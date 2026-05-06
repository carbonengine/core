// Copyright © 2025 CCP ehf.
#include "TracyTestClient.h"

#include <cassert>
#include <chrono>
#include <cstring>
#include <thread>

#include <common/TracyProtocol.hpp>
#include <common/TracyQueue.hpp>
#include <common/TracySocket.hpp>
#include <common/tracy_lz4.hpp>

static constexpr int kReadTimeoutMs = 100;

TracyTestClient::TracyTestClient()
    : m_socket( new tracy::Socket() )
    , m_lz4Stream( tracy::LZ4_createStreamDecode() )
    , m_ringBuffer( new char[tracy::TargetFrameSize * 2] )
{
}

TracyTestClient::~TracyTestClient()
{
    Disconnect();
    delete static_cast<tracy::Socket*>( m_socket );
    tracy::LZ4_freeStreamDecode( static_cast<tracy::LZ4_streamDecode_t*>( m_lz4Stream ) );
    delete[] m_ringBuffer;
}

bool TracyTestClient::Connect( const char* addr, uint16_t port, int timeoutMs )
{
    auto& sock = *static_cast<tracy::Socket*>( m_socket );
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds( timeoutMs );

    // Retry until we connect or time out, since the profiler's listen socket may
    // not be ready immediately after TracyIsStarted becomes true.
    while( std::chrono::steady_clock::now() < deadline )
    {
        if( sock.ConnectBlocking( addr, port ) )
            break;
        std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
    }
    if( !sock.IsValid() )
        return false;

    // Send handshake shibboleth and protocol version.
    sock.Send( tracy::HandshakeShibboleth, tracy::HandshakeShibbolethSize );
    uint32_t proto = tracy::ProtocolVersion;
    sock.Send( &proto, sizeof( proto ) );

    // Receive handshake status.
    tracy::HandshakeStatus status;
    if( !sock.ReadRaw( &status, sizeof( status ), 2000 ) || status != tracy::HandshakeWelcome )
    {
        sock.Close();
        return false;
    }

    // Receive the welcome message.
    tracy::WelcomeMessage welcome;
    if( !sock.ReadRaw( &welcome, sizeof( welcome ), 5000 ) )
    {
        sock.Close();
        return false;
    }

    // With TRACY_ON_DEMAND the profiler sends an extra OnDemandPayloadMessage.
    tracy::OnDemandPayloadMessage onDemand;
    if( !sock.ReadRaw( &onDemand, sizeof( onDemand ), 5000 ) )
    {
        sock.Close();
        return false;
    }

    // Reset the LZ4 streaming context for the new connection.
    tracy::LZ4_setStreamDecode( static_cast<tracy::LZ4_streamDecode_t*>( m_lz4Stream ), nullptr, 0 );
    m_bufferOffset = 0;

    m_connected.store( true, std::memory_order_release );
    m_shutdown.store( false, std::memory_order_relaxed );
    m_recvThread = std::thread( &TracyTestClient::RecvLoop, this );
    return true;
}

void TracyTestClient::Disconnect()
{
    if( !m_connected.load( std::memory_order_acquire ) && !m_recvThread.joinable() )
        return;

    m_shutdown.store( true, std::memory_order_release );
    static_cast<tracy::Socket*>( m_socket )->Close();

    if( m_recvThread.joinable() )
        m_recvThread.join();

    m_connected.store( false, std::memory_order_release );
}

bool TracyTestClient::IsConnected() const
{
    return m_connected.load( std::memory_order_acquire );
}

std::vector<TracyTestClient::ZoneInfo> TracyTestClient::GetZones() const
{
    std::lock_guard<std::mutex> lock( m_dataMutex );
    return m_zones;
}

std::vector<std::string> TracyTestClient::GetFiberNames() const
{
    std::lock_guard<std::mutex> lock( m_dataMutex );
    std::vector<std::string> names;
    names.reserve( m_fiberNames.size() );
    for( const auto& [ptr, name] : m_fiberNames )
        names.push_back( name );
    return names;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void TracyTestClient::SendQueryLocked( uint8_t queryType, uint64_t ptr, uint32_t extra )
{
    tracy::ServerQueryPacket pkt;
    pkt.type = static_cast<tracy::ServerQuery>( queryType );
    pkt.ptr = ptr;
    pkt.extra = extra;
    std::lock_guard<std::mutex> lock( m_sendMutex );
    static_cast<tracy::Socket*>( m_socket )->Send( &pkt, tracy::ServerQueryPacketSize );
}

// Receive loop: reads LZ4-compressed frames and decompresses them.
void TracyTestClient::RecvLoop()
{
    auto& sock = *static_cast<tracy::Socket*>( m_socket );
    auto* lz4 = static_cast<tracy::LZ4_streamDecode_t*>( m_lz4Stream );
    std::unique_ptr<char[]> lz4Buf( new char[tracy::LZ4Size] );

    while( !m_shutdown.load( std::memory_order_relaxed ) )
    {
        // Each LZ4 frame is prefixed by its compressed size.
        tracy::lz4sz_t compressedSz = 0;
        if( !sock.ReadRaw( &compressedSz, sizeof( compressedSz ), kReadTimeoutMs ) ) {
            continue;
        }

        if( compressedSz > static_cast<tracy::lz4sz_t>( tracy::LZ4Size ) ) {
            fprintf( stderr, "Corrupt frame: %zu\n", static_cast<size_t>(compressedSz) ); fflush(stderr);
            break; // corrupt frame
        }

        if( !sock.ReadRaw( lz4Buf.get(), static_cast<int>( compressedSz ), kReadTimeoutMs ) ) {
            fprintf(stderr, "ReadRaw failed to read compressed data\n"); fflush(stderr);
            break;
        }

        // Decompress into the ring buffer using the streaming context so that
        // the previous block acts as the LZ4 dictionary.
        char* dst = m_ringBuffer + m_bufferOffset;
        const int decompressedSz = tracy::LZ4_decompress_safe_continue(
            lz4, lz4Buf.get(), dst, static_cast<int>( compressedSz ), tracy::TargetFrameSize );
        if( decompressedSz < 0 )
            break; // decompression error

        ProcessDecompressedData( dst, decompressedSz );

        m_bufferOffset += decompressedSz;
        if( m_bufferOffset > tracy::TargetFrameSize * 2 )
            m_bufferOffset = 0;
    }

    m_connected.store( false, std::memory_order_release );
}

// Parse the decompressed byte stream and update internal state.
void TracyTestClient::ProcessDecompressedData( const char* data, int sz )
{
    const char* ptr = data;
    const char* const end = data + sz;

    while( ptr < end )
    {
        const auto* item = reinterpret_cast<const tracy::QueueItem*>( ptr );
        const uint8_t idx = item->hdr.idx;

        if( idx >= static_cast<uint8_t>( tracy::QueueType::StringData ) )
        {
            // String transfer item: fixed header + QueueStringTransfer, followed by
            // a length-prefixed string payload.
            if( ptr + sizeof( tracy::QueueHeader ) + sizeof( tracy::QueueStringTransfer ) > end )
                break;
            const uint64_t strPtr = item->stringTransfer.ptr;
            ptr += sizeof( tracy::QueueHeader ) + sizeof( tracy::QueueStringTransfer );

            if( item->hdr.type == tracy::QueueType::FrameImageData ||
                item->hdr.type == tracy::QueueType::SymbolCode ||
                item->hdr.type == tracy::QueueType::SourceCode )
            {
                // Large binary payload with uint32_t length prefix.
                if( ptr + sizeof( uint32_t ) > end ) break;
                uint32_t strSz = 0;
                std::memcpy( &strSz, ptr, sizeof( strSz ) );
                ptr += sizeof( strSz );
                if( ptr + strSz > end ) break;
                ptr += strSz;
            }
            else
            {
                // Normal string payload with uint16_t length prefix.
                if( ptr + sizeof( uint16_t ) > end ) break;
                uint16_t strSz = 0;
                std::memcpy( &strSz, ptr, sizeof( strSz ) );
                ptr += sizeof( strSz );
                if( ptr + strSz > end ) break;

                switch( item->hdr.type )
                {
                case tracy::QueueType::SourceLocationPayload:
                {
                    // The profiler sends this immediately before ZoneBeginAllocSrcLoc.
                    // Format: [uint32_t color][uint32_t line][function\0][source\0][name]
                    if( strSz >= 9 )
                    {
                        const char* p = ptr;
                        p += 4; // skip color
                        uint32_t line = 0;
                        std::memcpy( &line, p, 4 );
                        p += 4;
                        const char* function = p;
                        p += std::strlen( function ) + 1;
                        const char* source = p;
                        p += std::strlen( source ) + 1;
                        const size_t nameLen = static_cast<size_t>( strSz ) - static_cast<size_t>( p - ptr );

                        std::lock_guard<std::mutex> lock( m_dataMutex );
                        m_pendingZone = {};
                        m_pendingZone.function = function;
                        m_pendingZone.source = source;
                        m_pendingZone.line = line;
                        if( nameLen > 0 )
                            m_pendingZone.name = std::string( p, nameLen );
                        m_hasPendingZone = true;
                    }
                    break;
                }
                case tracy::QueueType::FiberName:
                {
                    std::string name( ptr, strSz );
                    std::lock_guard<std::mutex> lock( m_dataMutex );
                    m_fiberNames[strPtr] = std::move( name );
                    break;
                }
                default:
                    break;
                }

                ptr += strSz;
            }
        }
        else
        {
            // Fixed-size item (or SingleStringData / SecondStringData special cases).
            switch( item->hdr.type )
            {
            case tracy::QueueType::SingleStringData:
            case tracy::QueueType::SecondStringData:
            {
                ptr += sizeof( tracy::QueueHeader );
                if( ptr + sizeof( uint16_t ) > end ) return;
                uint16_t strSz = 0;
                std::memcpy( &strSz, ptr, sizeof( strSz ) );
                ptr += sizeof( strSz );
                if( ptr + strSz > end ) return;
                ptr += strSz;
                break;
            }
            default:
            {
                const size_t itemSz = tracy::QueueDataSize[idx];
                if( ptr + itemSz > end ) return;

                switch( item->hdr.type )
                {
                case tracy::QueueType::ZoneBeginAllocSrcLoc:
                case tracy::QueueType::ZoneBeginAllocSrcLocCallstack:
                {
                    m_zoneBeginCount.fetch_add( 1, std::memory_order_relaxed );
                    std::lock_guard<std::mutex> lock( m_dataMutex );
                    if( m_hasPendingZone )
                    {
                        m_zones.push_back( m_pendingZone );
                        m_hasPendingZone = false;
                    }
                    break;
                }
                case tracy::QueueType::ZoneBegin:
                case tracy::QueueType::ZoneBeginCallstack:
                    m_zoneBeginCount.fetch_add( 1, std::memory_order_relaxed );
                    break;

                case tracy::QueueType::ZoneEnd:
                    m_zoneEndCount.fetch_add( 1, std::memory_order_relaxed );
                    break;

                case tracy::QueueType::FiberEnter:
                {
                    // Query the fiber name if we haven't already.
                    const uint64_t fiberPtr = item->fiberEnter.fiber;
                    std::lock_guard<std::mutex> lock( m_dataMutex );
                    if( m_queriedFibers.insert( fiberPtr ).second )
                    {
                        SendQueryLocked(
                            static_cast<uint8_t>( tracy::ServerQueryFiberName ), fiberPtr );
                    }
                    break;
                }

                default:
                    break;
                }

                ptr += itemSz;
                break;
            }
            }
        }
    }
}
