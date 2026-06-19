// Copyright © 2025 CCP ehf.
#include "TracyTestClient.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstring>
#include <lz4.h>
#include <thread>

#include <tracy/common/TracyProtocol.hpp>
#include <tracy/common/TracyQueue.hpp>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
   using socket_t = SOCKET;
   static constexpr socket_t kInvalidSocket = INVALID_SOCKET;
#  define sock_close( s ) ::closesocket( s )
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/select.h>
#  include <sys/socket.h>
#  include <unistd.h>
   using socket_t = int;
   static constexpr socket_t kInvalidSocket = -1;
#  define sock_close( s ) ::close( s )
#endif

static constexpr int kReadTimeoutMs = 100;

// ---------------------------------------------------------------------------
// TCP socket (POSIX + Winsock)
// ---------------------------------------------------------------------------

namespace {

struct TcpSocket
{
    socket_t fd = kInvalidSocket;

    bool ConnectBlocking( const char* addr, uint16_t port )
    {
        fd = ::socket( AF_INET, SOCK_STREAM, 0 );
        if( fd == kInvalidSocket ) return false;
        struct sockaddr_in sa{};
        sa.sin_family = AF_INET;
        sa.sin_port   = htons( port );
        if( ::inet_pton( AF_INET, addr, &sa.sin_addr ) != 1 )
        {
            sock_close( fd ); fd = kInvalidSocket; return false;
        }
        if( ::connect( fd, reinterpret_cast<struct sockaddr*>( &sa ), sizeof( sa ) ) != 0 )
        {
            sock_close( fd ); fd = kInvalidSocket; return false;
        }
        return true;
    }

    void Send( const void* buf, int len )
    {
        ::send( fd, static_cast<const char*>( buf ), len, 0 );
    }

    bool ReadRaw( void* buf, int len, int timeoutMs )
    {
        auto* p = static_cast<char*>( buf );
        while( len > 0 )
        {
            fd_set fds;
            FD_ZERO( &fds );
            FD_SET( fd, &fds );
            struct timeval tv{};
            tv.tv_sec  = timeoutMs / 1000;
            tv.tv_usec = ( timeoutMs % 1000 ) * 1000;
            // nfds is ignored on Windows; on POSIX it must be fd + 1.
#ifdef _WIN32
            if( ::select( 0, &fds, nullptr, nullptr, &tv ) <= 0 ) return false;
#else
            if( ::select( fd + 1, &fds, nullptr, nullptr, &tv ) <= 0 ) return false;
#endif
            const int n = static_cast<int>( ::recv( fd, p, len, 0 ) );
            if( n <= 0 ) return false;
            p   += n;
            len -= n;
        }
        return true;
    }

    void Close()
    {
        if( fd != kInvalidSocket ) { sock_close( fd ); fd = kInvalidSocket; }
    }

    bool IsValid() const { return fd != kInvalidSocket; }
};

// ---------------------------------------------------------------------------
// Tracy wire-protocol definitions (handshake, protocol structs, queue item
// layouts and sizes) come from Tracy's public headers TracyProtocol.hpp and
// TracyQueue.hpp. Only those header-only definitions can be reused: the
// socket and LZ4 implementations are compiled into TracyClient but are not
// exported from the DLL on Windows, hence the local TcpSocket and lz4 usage.
// ---------------------------------------------------------------------------

constexpr uint8_t QueueIdx( tracy::QueueType type )
{
    return static_cast<uint8_t>( type );
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// TracyTestClient
// ---------------------------------------------------------------------------

TracyTestClient::TracyTestClient()
    : m_socket( new TcpSocket() )
    , m_lz4Stream( LZ4_createStreamDecode() )
    , m_ringBuffer( new char[tracy::TargetFrameSize * 3 + 1] )
{
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup( MAKEWORD( 2, 2 ), &wsaData );
#endif
}

TracyTestClient::~TracyTestClient()
{
    Disconnect();
    delete static_cast<TcpSocket*>( m_socket );
    LZ4_freeStreamDecode( static_cast<LZ4_streamDecode_t*>( m_lz4Stream ) );
    delete[] m_ringBuffer;
#ifdef _WIN32
    WSACleanup();
#endif
}

bool TracyTestClient::Connect( const char* addr, uint16_t port, int timeoutMs )
{
    auto& sock = *static_cast<TcpSocket*>( m_socket );
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
    LZ4_setStreamDecode( static_cast<LZ4_streamDecode_t*>( m_lz4Stream ), nullptr, 0 );
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
    std::vector<ZoneInfo> result;
    for( const auto& [tid, stack] : m_threadZoneStacks )
        result.insert( result.end(), stack.begin(), stack.end() );
    for( const auto& [fptr, stack] : m_fiberZoneStacks )
        result.insert( result.end(), stack.begin(), stack.end() );
    return result;
}

TracyTestClient::ZoneStack TracyTestClient::GetZonesForThread( uint32_t threadId ) const
{
    std::lock_guard<std::mutex> lock( m_dataMutex );
    auto it = m_threadZoneStacks.find( threadId );
    if( it == m_threadZoneStacks.end() )
        return {};
    return it->second;
}

TracyTestClient::ZoneStack TracyTestClient::GetZonesForFiber( const std::string& fiberName ) const
{
    std::lock_guard<std::mutex> lock( m_dataMutex );
    for( const auto& [ptr, name] : m_fiberNames )
    {
        if( name == fiberName )
        {
            auto it = m_fiberZoneStacks.find( ptr );
            if( it != m_fiberZoneStacks.end() )
                return it->second;
        }
    }
    return {};
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

std::vector<TracyTestClient::LockInfo> TracyTestClient::GetAllLocks() const
{
    std::lock_guard<std::mutex> lock( m_dataMutex );
    std::vector<LockInfo> result;
    result.reserve( m_locks.size() );
    for( const auto& [id, info] : m_locks )
        result.push_back( info );
    return result;
}

std::vector<TracyTestClient::LockInfo> TracyTestClient::GetActiveLocks() const
{
    std::lock_guard<std::mutex> lock( m_dataMutex );
    std::vector<LockInfo> result;
    for( const auto& [id, info] : m_locks )
    {
        if( !info.terminated )
            result.push_back( info );
    }
    return result;
}

bool TracyTestClient::TryGetLock( uint32_t id, LockInfo& outLock ) const
{
    std::lock_guard<std::mutex> lock( m_dataMutex );
    auto it = m_locks.find( id );
    if( it == m_locks.end() )
        return false;
    outLock = it->second;
    return true;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void TracyTestClient::SendQueryLocked( uint8_t queryType, uint64_t ptr, uint32_t extra )
{
    tracy::ServerQueryPacket pkt;
    pkt.type  = static_cast<tracy::ServerQuery>( queryType );
    pkt.ptr   = ptr;
    pkt.extra = extra;
    std::lock_guard<std::mutex> lock( m_sendMutex );
    static_cast<TcpSocket*>( m_socket )->Send( &pkt, static_cast<int>( sizeof( pkt ) ) );
}

// Receive loop: reads LZ4-compressed frames and decompresses them.
void TracyTestClient::RecvLoop()
{
    auto& sock = *static_cast<TcpSocket*>( m_socket );
    auto* lz4  = static_cast<LZ4_streamDecode_t*>( m_lz4Stream );
    std::unique_ptr<char[]> lz4Buf( new char[tracy::LZ4Size] );

    while( !m_shutdown.load( std::memory_order_relaxed ) )
    {
        // Each LZ4 frame is prefixed by its compressed size.
        uint32_t compressedSz = 0;
        if( !sock.ReadRaw( &compressedSz, sizeof( compressedSz ), kReadTimeoutMs ) )
            continue;

        if( compressedSz > static_cast<uint32_t>( tracy::LZ4Size ) )
        {
            fprintf( stderr, "Corrupt frame: %zu\n", static_cast<size_t>( compressedSz ) ); fflush( stderr );
            break;
        }

        if( !sock.ReadRaw( lz4Buf.get(), static_cast<int>( compressedSz ), kReadTimeoutMs ) )
        {
            fprintf( stderr, "ReadRaw failed to read compressed data\n" ); fflush( stderr );
            break;
        }

        // Decompress into the ring buffer using the streaming context so that
        // the previous block acts as the LZ4 dictionary.
        char* dst = m_ringBuffer + m_bufferOffset;
        const int decompressedSz = LZ4_decompress_safe_continue(
            lz4, lz4Buf.get(), dst,
            static_cast<int>( compressedSz ), static_cast<int>( tracy::TargetFrameSize ) );
        if( decompressedSz < 0 )
            break; // decompression error

        ProcessDecompressedData( dst, decompressedSz );

        m_bufferOffset += decompressedSz;
        if( m_bufferOffset > static_cast<int>( tracy::TargetFrameSize * 2 ) )
            m_bufferOffset = 0;
    }

    // Close the socket so Tracy's worker thread sees the connection drop and
    // can finish its own shutdown sequence. This is necessary whether we exit
    // because QueueType::Terminate was received or because Disconnect() set m_shutdown.
    // TcpSocket::Close() is idempotent, so a double-close from Disconnect() is safe.
    sock.Close();
    m_connected.store( false, std::memory_order_release );
}

TracyTestClient::ZoneStack& TracyTestClient::CurrentStack( uint32_t thread )
{
    auto fiberIt = m_threadCurrentFiber.find( thread );
    if( fiberIt != m_threadCurrentFiber.end() && fiberIt->second != 0 )
        return m_fiberZoneStacks[fiberIt->second];
    return m_threadZoneStacks[thread];
}

TracyTestClient::LockInfo& TracyTestClient::GetOrCreateLockById( uint32_t id )
{
    auto& info = m_locks[id];
    info.id = id;
    return info;
}

void TracyTestClient::RequestLockString( uint64_t ptr, uint32_t lockId, int field )
{
    auto& pending = m_pendingLockStrings[ptr];
    // Several locks can share a string pointer (e.g. the source file); query the
    // profiler only once per pointer while a reply is outstanding.
    const bool alreadyQueried = !pending.empty();
    pending.push_back( { lockId, field } );
    if( !alreadyQueried )
        SendQueryLocked( tracy::ServerQueryString, ptr );
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

        if( idx >= QueueIdx( tracy::QueueType::StringData ) )
        {
            // String transfer item: fixed header + QueueStringTransfer, followed by
            // a length-prefixed string payload.
            if( ptr + sizeof( tracy::QueueHeader ) + sizeof( tracy::QueueStringTransfer ) > end )
                break;
            const uint64_t strPtr = item->stringTransfer.ptr;
            ptr += sizeof( tracy::QueueHeader ) + sizeof( tracy::QueueStringTransfer );

            if( idx == QueueIdx( tracy::QueueType::FrameImageData ) ||
                idx == QueueIdx( tracy::QueueType::SymbolCode )     ||
                idx == QueueIdx( tracy::QueueType::SourceCode ) )
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

                if( idx == QueueIdx( tracy::QueueType::SourceLocationPayload ) )
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
                        m_pendingZone          = {};
                        m_pendingZone.function = function;
                        m_pendingZone.source   = source;
                        m_pendingZone.line     = line;
                        if( nameLen > 0 )
                            m_pendingZone.name = std::string( p, nameLen );
                        m_hasPendingZone = true;
                    }
                }
                else if( idx == QueueIdx( tracy::QueueType::FiberName ) )
                {
                    std::string name( ptr, strSz );
                    std::lock_guard<std::mutex> lock( m_dataMutex );
                    m_fiberNames[strPtr] = std::move( name );
                }
                else if( idx == QueueIdx( tracy::QueueType::StringData ) )
                {
                    // Reply to a ServerQueryString we sent while resolving a
                    // lock source location; strPtr echoes the queried pointer.
                    std::lock_guard<std::mutex> lock( m_dataMutex );
                    auto pendingIt = m_pendingLockStrings.find( strPtr );
                    if( pendingIt != m_pendingLockStrings.end() )
                    {
                        const std::string value( ptr, strSz );
                        for( const auto& target : pendingIt->second )
                        {
                            auto& info = GetOrCreateLockById( target.lockId );
                            switch( target.field )
                            {
                            case 0: info.name = value; break;
                            case 1: info.function = value; break;
                            case 2: info.source = value; break;
                            default: break;
                            }
                        }
                        m_pendingLockStrings.erase( pendingIt );
                    }
                }

                ptr += strSz;
            }
        }
        else
        {
            // Fixed-size item (or SingleStringData / SecondStringData special cases).
            if( idx == QueueIdx( tracy::QueueType::SingleStringData ) ||
                idx == QueueIdx( tracy::QueueType::SecondStringData ) )
            {
                ptr += sizeof( tracy::QueueHeader );
                if( ptr + sizeof( uint16_t ) > end ) return;
                uint16_t strSz = 0;
                std::memcpy( &strSz, ptr, sizeof( strSz ) );
                ptr += sizeof( strSz );
                if( ptr + strSz > end ) return;
                // Remember the payload: fat-pointer items (e.g. LockName) are
                // preceded by a SingleStringData event carrying their string.
                if( idx == QueueIdx( tracy::QueueType::SingleStringData ) )
                    m_pendingSingleString.assign( ptr, strSz );
                ptr += strSz;
            }
            else
            {
                if( idx >= QueueIdx( tracy::QueueType::NUM_TYPES ) ) return;
                const size_t itemSz = tracy::QueueDataSize[idx];
                if( ptr + itemSz > end ) return;

                switch( item->hdr.type )
                {
                case tracy::QueueType::ThreadContext:
                    m_currentThread = item->threadCtx.thread;
                    break;

                case tracy::QueueType::ZoneBeginAllocSrcLoc:
                case tracy::QueueType::ZoneBeginAllocSrcLocCallstack:
                {
                    m_zoneBeginCount.fetch_add( 1, std::memory_order_relaxed );
                    const uint32_t thread = m_currentThread;
                    std::lock_guard<std::mutex> lock( m_dataMutex );
                    if( m_hasPendingZone )
                    {
                        CurrentStack( thread ).push_back( m_pendingZone );
                        m_hasPendingZone = false;
                    }
                    break;
                }

                case tracy::QueueType::ZoneBegin:
                case tracy::QueueType::ZoneBeginCallstack:
                    m_zoneBeginCount.fetch_add( 1, std::memory_order_relaxed );
                    break;

                case tracy::QueueType::ZoneEnd:
                {
                    m_zoneEndCount.fetch_add( 1, std::memory_order_relaxed );
                    const uint32_t thread = m_currentThread;
                    std::lock_guard<std::mutex> lock( m_dataMutex );
                    auto& stack = CurrentStack( thread );
                    if( !stack.empty() )
                        stack.pop_back();
                    break;
                }

                case tracy::QueueType::LockAnnounce:
                {
                    m_lockAnnounceCount.fetch_add( 1, std::memory_order_relaxed );
                    const uint32_t lockId = item->lockAnnounce.id;
                    const uint64_t srcloc = item->lockAnnounce.lckloc;
                    std::lock_guard<std::mutex> lock( m_dataMutex );
                    GetOrCreateLockById( lockId );
                    // Resolve the announce call site. The reply carries no request
                    // pointer, so remember which lock the next reply belongs to.
                    m_pendingLockSrcLocs.push_back( lockId );
                    SendQueryLocked( tracy::ServerQuerySourceLocation, srcloc );
                    break;
                }

                case tracy::QueueType::LockTerminate:
                {
                    m_lockTerminateCount.fetch_add( 1, std::memory_order_relaxed );
                    std::lock_guard<std::mutex> lock( m_dataMutex );
                    GetOrCreateLockById( item->lockTerminate.id ).terminated = true;
                    break;
                }

                case tracy::QueueType::LockWait:
                {
                    m_lockWaitCount.fetch_add( 1, std::memory_order_relaxed );
                    std::lock_guard<std::mutex> lock( m_dataMutex );
                    auto& info = GetOrCreateLockById( item->lockWait.id );
                    ++info.waitCount;
                    info.waitingThreads.push_back( item->lockWait.thread );
                    break;
                }

                case tracy::QueueType::LockObtain:
                {
                    m_lockObtainCount.fetch_add( 1, std::memory_order_relaxed );
                    const uint32_t thread = item->lockObtain.thread;
                    std::lock_guard<std::mutex> lock( m_dataMutex );
                    auto& info = GetOrCreateLockById( item->lockObtain.id );
                    ++info.obtainCount;
                    info.holderThread = thread;
                    auto& waiting = info.waitingThreads;
                    auto waitIt = std::find( waiting.begin(), waiting.end(), thread );
                    if( waitIt != waiting.end() )
                        waiting.erase( waitIt );
                    break;
                }

                case tracy::QueueType::LockRelease:
                {
                    m_lockReleaseCount.fetch_add( 1, std::memory_order_relaxed );
                    std::lock_guard<std::mutex> lock( m_dataMutex );
                    auto& info = GetOrCreateLockById( item->lockRelease.id );
                    ++info.releaseCount;
                    info.holderThread = 0;
                    break;
                }

                case tracy::QueueType::LockName:
                {
                    std::lock_guard<std::mutex> lock( m_dataMutex );
                    GetOrCreateLockById( item->lockName.id ).name = m_pendingSingleString;
                    break;
                }

                case tracy::QueueType::SourceLocation:
                {
                    std::lock_guard<std::mutex> lock( m_dataMutex );
                    if( !m_pendingLockSrcLocs.empty() )
                    {
                        const uint32_t lockId = m_pendingLockSrcLocs.front();
                        m_pendingLockSrcLocs.pop_front();
                        GetOrCreateLockById( lockId ).line = item->srcloc.line;
                        if( item->srcloc.name != 0 )
                            RequestLockString( item->srcloc.name, lockId, 0 );
                        if( item->srcloc.function != 0 )
                            RequestLockString( item->srcloc.function, lockId, 1 );
                        if( item->srcloc.file != 0 )
                            RequestLockString( item->srcloc.file, lockId, 2 );
                    }
                    break;
                }

                case tracy::QueueType::FiberEnter:
                {
                    const uint64_t fiberPtr = item->fiberEnter.fiber;
                    const uint32_t thread   = item->fiberEnter.thread;
                    std::lock_guard<std::mutex> lock( m_dataMutex );
                    m_threadCurrentFiber[thread] = fiberPtr;
                    if( m_queriedFibers.insert( fiberPtr ).second )
                        SendQueryLocked( tracy::ServerQueryFiberName, fiberPtr );
                    break;
                }

                case tracy::QueueType::FiberLeave:
                {
                    const uint32_t thread = item->fiberLeave.thread;
                    std::lock_guard<std::mutex> lock( m_dataMutex );
                    m_threadCurrentFiber[thread] = 0;
                    break;
                }

                case tracy::QueueType::Terminate:
                {
                    m_shutdown.store( true, std::memory_order_release );
                    break;
                }

                default:
                    break;
                }

                ptr += itemSz;
            }
        }
    }
}
