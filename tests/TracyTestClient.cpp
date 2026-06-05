// Copyright © 2025 CCP ehf.
#include "TracyTestClient.h"

#include <cassert>
#include <chrono>
#include <cstring>
#include <lz4.h>
#include <thread>

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
// Tracy wire-protocol constants and types (Tracy 0.13.1)
// Derived from TracyProtocol.hpp / TracyQueue.hpp — no Tracy headers needed.
// ---------------------------------------------------------------------------

static constexpr uint32_t kProtocolVersion         = 76;
static constexpr uint32_t kTargetFrameSize         = 256 * 1024;
static constexpr unsigned kLZ4Size                 = kTargetFrameSize + ( kTargetFrameSize / 255 ) + 16;
static constexpr int      kHandshakeShibbolethSize = 8;
static const char         kHandshakeShibboleth[kHandshakeShibbolethSize] = { 'T', 'r', 'a', 'c', 'y', 'P', 'r', 'f' };

enum HandshakeStatus : uint8_t
{
    HandshakePending          = 0,
    HandshakeWelcome          = 1,
    HandshakeProtocolMismatch = 2,
    HandshakeNotAvailable     = 3,
    HandshakeDropped          = 4,
};

#pragma pack( push, 1 )

struct WelcomeMessage
{
    double   timerMul;
    int64_t  initBegin;
    int64_t  initEnd;
    uint64_t resolution;
    uint64_t epoch;
    uint64_t exectime;
    uint64_t pid;
    int64_t  samplingPeriod;
    uint8_t  flags;
    uint8_t  cpuArch;
    char     cpuManufacturer[12];
    uint32_t cpuId;
    char     programName[64];
    char     hostInfo[1024];
};

struct OnDemandPayloadMessage
{
    uint64_t frames;
    uint64_t currentTime;
};

// Only the server-query value we actually emit.
static constexpr uint8_t kServerQueryFiberName = 7;

struct ServerQueryPacket
{
    uint8_t  type;
    uint64_t ptr;
    uint32_t extra;
};

// Minimal queue-item structs — only fields accessed in ProcessDecompressedData.
struct QueueHeader        { uint8_t  idx; };
struct QueueThreadContext { uint32_t thread; };

struct QueueFiberEnter
{
    int64_t  time;
    uint64_t fiber;
    uint32_t thread;
    int32_t  groupHint;
};

struct QueueFiberLeave
{
    int64_t  time;
    uint32_t thread;
};

struct QueueStringTransfer { uint64_t ptr; };

// QueueItem matches Tracy's 32-byte packed union layout.
struct QueueItem
{
    QueueHeader hdr;
    union {
        QueueThreadContext  threadCtx;
        QueueFiberEnter     fiberEnter;
        QueueFiberLeave     fiberLeave;
        QueueStringTransfer stringTransfer;
        char                _pad[31];
    };
};
static_assert( sizeof( QueueItem ) == 32, "QueueItem size mismatch" );

#pragma pack( pop )

// QueueType index constants (QueueType enum : uint8_t from TracyQueue.hpp).
static constexpr uint8_t kQueueZoneBeginAllocSrcLoc          =   7;
static constexpr uint8_t kQueueZoneBeginAllocSrcLocCallstack =   8;
static constexpr uint8_t kQueueZoneBegin                     =  15;
static constexpr uint8_t kQueueZoneBeginCallstack            =  16;
static constexpr uint8_t kQueueZoneEnd                       =  17;
static constexpr uint8_t kQueueFiberEnter                    =  58;
static constexpr uint8_t kQueueFiberLeave                    =  59;
static constexpr uint8_t kQueueTerminate                     =  60;
static constexpr uint8_t kQueueThreadContext                 =  62;
static constexpr uint8_t kQueueSingleStringData              =  99;
static constexpr uint8_t kQueueSecondStringData              = 100;
static constexpr uint8_t kQueueStringDataFirst               = 104; // indices >= this carry QueueStringTransfer
static constexpr uint8_t kQueueSourceLocationPayload         = 107;
static constexpr uint8_t kQueueFrameImageData                = 111;
static constexpr uint8_t kQueueSymbolCode                    = 114;
static constexpr uint8_t kQueueSourceCode                    = 115;
static constexpr uint8_t kQueueFiberName                     = 116;
static constexpr uint8_t kQueueNumTypes                      = 117;

// Wire-format byte size of each queue item indexed by QueueType.
// Mirrors QueueDataSize[] from TracyQueue.hpp with #pragma pack(1) struct sizes.
static const size_t kQueueDataSize[kQueueNumTypes] = {
     1,  //   0 ZoneText
     1,  //   1 ZoneName
     9,  //   2 Message                    {int64_t}
    12,  //   3 MessageColor               {int64_t, 3×uint8_t}
     9,  //   4 MessageCallstack
    12,  //   5 MessageColorCallstack
     9,  //   6 MessageAppInfo
     9,  //   7 ZoneBeginAllocSrcLoc       {int64_t}
     9,  //   8 ZoneBeginAllocSrcLocCallstack
     1,  //   9 CallstackSerial
     1,  //  10 Callstack
     1,  //  11 CallstackAlloc
    13,  //  12 CallstackSample            {int64_t, uint32_t}
    13,  //  13 CallstackSampleContextSwitch
    10,  //  14 FrameImage                 {uint32_t, uint16_t, uint16_t, uint8_t}
    17,  //  15 ZoneBegin                  {int64_t, uint64_t}
    17,  //  16 ZoneBeginCallstack
     9,  //  17 ZoneEnd                    {int64_t}
    17,  //  18 LockWait                   {uint32_t, uint32_t, int64_t}
    17,  //  19 LockObtain
    13,  //  20 LockRelease                {uint32_t, int64_t}
    17,  //  21 LockSharedWait
    17,  //  22 LockSharedObtain
    17,  //  23 LockSharedRelease          {uint32_t, int64_t, uint32_t}
     5,  //  24 LockName                   {uint32_t}
    27,  //  25 MemAlloc                   {int64_t, uint32_t, uint64_t, char[6]}
    27,  //  26 MemAllocNamed
    21,  //  27 MemFree                    {int64_t, uint32_t, uint64_t}
    21,  //  28 MemFreeNamed
    27,  //  29 MemAllocCallstack
    27,  //  30 MemAllocCallstackNamed
    21,  //  31 MemFreeCallstack
    21,  //  32 MemFreeCallstackNamed
    21,  //  33 MemDiscard
    21,  //  34 MemDiscardCallstack
    24,  //  35 GpuZoneBegin               {int64_t, uint32_t, uint16_t, uint8_t, uint64_t}
    24,  //  36 GpuZoneBeginCallstack
    16,  //  37 GpuZoneBeginAllocSrcLoc    {int64_t, uint32_t, uint16_t, uint8_t}
    16,  //  38 GpuZoneBeginAllocSrcLocCallstack
    16,  //  39 GpuZoneEnd
    24,  //  40 GpuZoneBeginSerial
    24,  //  41 GpuZoneBeginCallstackSerial
    16,  //  42 GpuZoneBeginAllocSrcLocSerial
    16,  //  43 GpuZoneBeginAllocSrcLocCallstackSerial
    16,  //  44 GpuZoneEndSerial
    25,  //  45 PlotDataInt                {uint64_t, int64_t, int64_t}
    21,  //  46 PlotDataFloat              {uint64_t, int64_t, float}
    25,  //  47 PlotDataDouble             {uint64_t, int64_t, double}
    23,  //  48 ContextSwitch              {int64_t, 2×uint32_t, 4×uint8_t, 2×int8_t}
    16,  //  49 ThreadWakeup               {int64_t, uint32_t, uint8_t, 2×int8_t}
    12,  //  50 GpuTime                    {int64_t, uint16_t, uint8_t}
     2,  //  51 GpuContextName             {uint8_t}
    10,  //  52 GpuAnnotationName          {int64_t, uint8_t}
    10,  //  53 CallstackFrameSize         {uint64_t, uint8_t}
    13,  //  54 SymbolInformation          {uint32_t, uint64_t}
     1,  //  55 ExternalNameMetadata       (not wire-transferred)
     1,  //  56 SymbolCodeMetadata         (not wire-transferred)
     1,  //  57 SourceCodeMetadata         (not wire-transferred)
    25,  //  58 FiberEnter                 {int64_t, uint64_t, uint32_t, int32_t}
    13,  //  59 FiberLeave                 {int64_t, uint32_t}
     1,  //  60 Terminate
     1,  //  61 KeepAlive
     5,  //  62 ThreadContext              {uint32_t}
    26,  //  63 GpuCalibration             {int64_t, int64_t, int64_t, uint8_t}
    18,  //  64 GpuTimeSync                {int64_t, int64_t, uint8_t}
     1,  //  65 Crash
    17,  //  66 CrashReport                {int64_t, uint64_t}
     5,  //  67 ZoneValidation             {uint32_t}
     4,  //  68 ZoneColor                  {3×uint8_t}
     9,  //  69 ZoneValue                  {uint64_t}
    17,  //  70 FrameMarkMsg               {int64_t, uint64_t}
    17,  //  71 FrameMarkMsgStart
    17,  //  72 FrameMarkMsgEnd
    13,  //  73 FrameVsync                 {int64_t, uint32_t}
    32,  //  74 SourceLocation             {3×uint64_t, uint32_t, 3×uint8_t}
    22,  //  75 LockAnnounce               {uint32_t, int64_t, uint64_t, uint8_t}
    13,  //  76 LockTerminate              {uint32_t, int64_t}
    17,  //  77 LockMark                   {2×uint32_t, uint64_t}
    17,  //  78 MessageLiteral             {int64_t, uint64_t}
    20,  //  79 MessageLiteralColor        {int64_t, 3×uint8_t, uint64_t}
    17,  //  80 MessageLiteralCallstack
    20,  //  81 MessageLiteralColorCallstack
    28,  //  82 GpuNewContext              {2×int64_t, uint32_t, float, 3×uint8_t}
    17,  //  83 CallstackFrame             {uint32_t, uint64_t, uint32_t}
    13,  //  84 SysTimeReport              {int64_t, float}
    25,  //  85 SysPowerReport             {3×int64_t/uint64_t}
    17,  //  86 TidToPid                   {2×uint64_t}
    17,  //  87 HwSampleCpuCycle           {uint64_t, int64_t}
    17,  //  88 HwSampleInstructionRetired
    17,  //  89 HwSampleCacheReference
    17,  //  90 HwSampleCacheMiss
    17,  //  91 HwSampleBranchRetired
    17,  //  92 HwSampleBranchMiss
    16,  //  93 PlotConfig                 {uint64_t, 3×uint8_t, uint32_t}
    18,  //  94 ParamSetup                 {uint32_t, uint64_t, uint8_t, int32_t}
     1,  //  95 AckServerQueryNoop
     5,  //  96 AckSourceCodeNotAvailable  {uint32_t}
     1,  //  97 AckSymbolCodeNotAvailable
    17,  //  98 CpuTopology                {4×uint32_t}
     1,  //  99 SingleStringData           (variable-length; header only in fixed table)
     1,  // 100 SecondStringData           (variable-length; header only in fixed table)
     9,  // 101 MemNamePayload             {uint64_t}
     9,  // 102 ThreadGroupHint            {uint32_t, int32_t}
    24,  // 103 GpuZoneAnnotation          {int64_t, double, uint32_t, uint16_t, uint8_t}
    // indices >= kQueueStringDataFirst carry QueueStringTransfer + variable string
     9,  // 104 StringData                 {QueueStringTransfer}
     9,  // 105 ThreadName
     9,  // 106 PlotName
     9,  // 107 SourceLocationPayload
     9,  // 108 CallstackPayload
     9,  // 109 CallstackAllocPayload
     9,  // 110 FrameName
     9,  // 111 FrameImageData
     9,  // 112 ExternalName
     9,  // 113 ExternalThreadName
     9,  // 114 SymbolCode
     9,  // 115 SourceCode
     9,  // 116 FiberName
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// TracyTestClient
// ---------------------------------------------------------------------------

TracyTestClient::TracyTestClient()
    : m_socket( new TcpSocket() )
    , m_lz4Stream( LZ4_createStreamDecode() )
    , m_ringBuffer( new char[kTargetFrameSize * 2] )
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
    sock.Send( kHandshakeShibboleth, kHandshakeShibbolethSize );
    uint32_t proto = kProtocolVersion;
    sock.Send( &proto, sizeof( proto ) );

    // Receive handshake status.
    HandshakeStatus status;
    if( !sock.ReadRaw( &status, sizeof( status ), 2000 ) || status != HandshakeWelcome )
    {
        sock.Close();
        return false;
    }

    // Receive the welcome message.
    WelcomeMessage welcome;
    if( !sock.ReadRaw( &welcome, sizeof( welcome ), 5000 ) )
    {
        sock.Close();
        return false;
    }

    // With TRACY_ON_DEMAND the profiler sends an extra OnDemandPayloadMessage.
    OnDemandPayloadMessage onDemand;
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

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void TracyTestClient::SendQueryLocked( uint8_t queryType, uint64_t ptr, uint32_t extra )
{
    ServerQueryPacket pkt;
    pkt.type  = queryType;
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
    std::unique_ptr<char[]> lz4Buf( new char[kLZ4Size] );

    while( !m_shutdown.load( std::memory_order_relaxed ) )
    {
        // Each LZ4 frame is prefixed by its compressed size.
        uint32_t compressedSz = 0;
        if( !sock.ReadRaw( &compressedSz, sizeof( compressedSz ), kReadTimeoutMs ) )
            continue;

        if( compressedSz > static_cast<uint32_t>( kLZ4Size ) )
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
            static_cast<int>( compressedSz ), static_cast<int>( kTargetFrameSize ) );
        if( decompressedSz < 0 )
            break; // decompression error

        ProcessDecompressedData( dst, decompressedSz );

        m_bufferOffset += decompressedSz;
        if( m_bufferOffset > static_cast<int>( kTargetFrameSize * 2 ) )
            m_bufferOffset = 0;
    }

    // Close the socket so Tracy's worker thread sees the connection drop and
    // can finish its own shutdown sequence. This is necessary whether we exit
    // because kQueueTerminate was received or because Disconnect() set m_shutdown.
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

// Parse the decompressed byte stream and update internal state.
void TracyTestClient::ProcessDecompressedData( const char* data, int sz )
{
    const char* ptr = data;
    const char* const end = data + sz;

    while( ptr < end )
    {
        const auto* item = reinterpret_cast<const QueueItem*>( ptr );
        const uint8_t idx = item->hdr.idx;

        if( idx >= kQueueStringDataFirst )
        {
            // String transfer item: fixed header + QueueStringTransfer, followed by
            // a length-prefixed string payload.
            if( ptr + sizeof( QueueHeader ) + sizeof( QueueStringTransfer ) > end )
                break;
            const uint64_t strPtr = item->stringTransfer.ptr;
            ptr += sizeof( QueueHeader ) + sizeof( QueueStringTransfer );

            if( idx == kQueueFrameImageData ||
                idx == kQueueSymbolCode      ||
                idx == kQueueSourceCode )
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

                if( idx == kQueueSourceLocationPayload )
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
                else if( idx == kQueueFiberName )
                {
                    std::string name( ptr, strSz );
                    std::lock_guard<std::mutex> lock( m_dataMutex );
                    m_fiberNames[strPtr] = std::move( name );
                }

                ptr += strSz;
            }
        }
        else
        {
            // Fixed-size item (or SingleStringData / SecondStringData special cases).
            if( idx == kQueueSingleStringData || idx == kQueueSecondStringData )
            {
                ptr += sizeof( QueueHeader );
                if( ptr + sizeof( uint16_t ) > end ) return;
                uint16_t strSz = 0;
                std::memcpy( &strSz, ptr, sizeof( strSz ) );
                ptr += sizeof( strSz );
                if( ptr + strSz > end ) return;
                ptr += strSz;
            }
            else
            {
                if( idx >= kQueueNumTypes ) return;
                const size_t itemSz = kQueueDataSize[idx];
                if( ptr + itemSz > end ) return;

                switch( idx )
                {
                case kQueueThreadContext:
                    m_currentThread = item->threadCtx.thread;
                    break;

                case kQueueZoneBeginAllocSrcLoc:
                case kQueueZoneBeginAllocSrcLocCallstack:
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

                case kQueueZoneBegin:
                case kQueueZoneBeginCallstack:
                    m_zoneBeginCount.fetch_add( 1, std::memory_order_relaxed );
                    break;

                case kQueueZoneEnd:
                {
                    m_zoneEndCount.fetch_add( 1, std::memory_order_relaxed );
                    const uint32_t thread = m_currentThread;
                    std::lock_guard<std::mutex> lock( m_dataMutex );
                    auto& stack = CurrentStack( thread );
                    if( !stack.empty() )
                        stack.pop_back();
                    break;
                }

                case kQueueFiberEnter:
                {
                    const uint64_t fiberPtr = item->fiberEnter.fiber;
                    const uint32_t thread   = item->fiberEnter.thread;
                    std::lock_guard<std::mutex> lock( m_dataMutex );
                    m_threadCurrentFiber[thread] = fiberPtr;
                    if( m_queriedFibers.insert( fiberPtr ).second )
                        SendQueryLocked( kServerQueryFiberName, fiberPtr );
                    break;
                }

                case kQueueFiberLeave:
                {
                    const uint32_t thread = item->fiberLeave.thread;
                    std::lock_guard<std::mutex> lock( m_dataMutex );
                    m_threadCurrentFiber[thread] = 0;
                    break;
                }

                case kQueueTerminate:
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
