// Copyright © 2025 CCP ehf.
#pragma once

#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// A minimal Tracy profiler client for use in unit tests.
// Connects to the Tracy profiler embedded in the test executable,
// receives and parses the event stream, and exposes the collected
// data so tests can make assertions about profiler activity.
class TracyTestClient
{
public:
    struct ZoneInfo
    {
        std::string name;
        std::string function;
        std::string source;
        uint32_t line = 0;
    };

    using ZoneStack = std::vector<ZoneInfo>;

    // State of a lockable announced via TracyCLockAnnounce, accumulated from
    // LockAnnounce / LockTerminate / LockWait / LockObtain / LockRelease events.
    // The source location (function/source/line) identifies the TracyCLockAnnounce
    // call site and is resolved asynchronously through server queries, so it may
    // be empty briefly after the announce event arrives.
    struct LockInfo
    {
        uint32_t id = 0;
        // Custom name set via TracyCLockCustomName (LockName event); falls back
        // to the srcloc name, which is empty for locks announced via the C API.
        std::string name;
        std::string function;
        std::string source;
        uint32_t line = 0;
        bool terminated = false;            // a LockTerminate event was received
        uint32_t holderThread = 0;          // thread currently holding the lock, 0 = none
        std::vector<uint32_t> waitingThreads; // threads between LockWait and LockObtain
        int waitCount = 0;    // LockWait events    (TracyCLockBeforeLock)
        int obtainCount = 0;  // LockObtain events  (TracyCLockAfterLock)
        int releaseCount = 0; // LockRelease events (TracyCLockAfterUnlock)
    };

    TracyTestClient();
    ~TracyTestClient();

    // Try to connect to the Tracy profiler at addr:port.
    // Retries until timeoutMs elapses. Returns true on success.
    bool Connect( const char* addr = "127.0.0.1", uint16_t port = 8086, int timeoutMs = 5000 );
    void Disconnect();
    bool IsConnected() const;

    int GetZoneBeginCount() const { return m_zoneBeginCount.load( std::memory_order_relaxed ); }
    int GetZoneEndCount() const { return m_zoneEndCount.load( std::memory_order_relaxed ); }

    // Returns all currently open zones across all threads and fibers (flattened).
    std::vector<ZoneInfo> GetZones() const;
    // Returns the zone stack currently open for the given thread (not including fiber zones).
    ZoneStack GetZonesForThread( uint32_t threadId ) const;
    // Returns the zone stack currently open for the named fiber.
    ZoneStack GetZonesForFiber( const std::string& fiberName ) const;

    std::vector<std::string> GetFiberNames() const;

    // Global event counters for each lock state transition.
    // Note: LockAnnounce/LockTerminate are deferred items in Tracy, so they are
    // replayed for previously announced locks on every new connection. Within a
    // process that runs several tests, these two counters therefore also include
    // locks announced before this client connected.
    int GetLockAnnounceCount() const { return m_lockAnnounceCount.load( std::memory_order_relaxed ); }
    int GetLockTerminateCount() const { return m_lockTerminateCount.load( std::memory_order_relaxed ); }
    int GetLockWaitCount() const { return m_lockWaitCount.load( std::memory_order_relaxed ); }
    int GetLockObtainCount() const { return m_lockObtainCount.load( std::memory_order_relaxed ); }
    int GetLockReleaseCount() const { return m_lockReleaseCount.load( std::memory_order_relaxed ); }

    // Returns all locks this client has seen (including terminated ones).
    std::vector<LockInfo> GetLocks() const;
    // Returns all announced locks that have not been terminated yet.
    std::vector<LockInfo> GetActiveLocks() const;
    // Looks up a single lock by its Tracy lock id.
    bool TryGetLock( uint32_t id, LockInfo& outLock ) const;

    TracyTestClient( const TracyTestClient& ) = delete;
    TracyTestClient& operator=( const TracyTestClient& ) = delete;

private:
    void RecvLoop();
    void ProcessDecompressedData( const char* data, int sz );
    void SendQueryLocked( uint8_t queryType, uint64_t ptr, uint32_t extra = 0 );

    // Returns a reference to the zone stack for the current thread/fiber context.
    // Must be called with m_dataMutex held.
    ZoneStack& CurrentStack( uint32_t thread );

    // Returns the LockInfo for the given lock id, creating it if necessary.
    // Must be called with m_dataMutex held.
    LockInfo& LockById( uint32_t id );

    // Queries the string behind ptr from the profiler and routes the reply into
    // the given LockInfo field. Must be called with m_dataMutex held.
    void RequestLockString( uint64_t ptr, uint32_t lockId, int field );

    // Opaque handles to Tracy types, allocated on heap to keep Tracy headers out of this header.
    void* m_socket = nullptr;    // tracy::Socket*
    void* m_lz4Stream = nullptr; // LZ4_streamDecode_t*

    // Ring buffer matching Tracy's decompression scheme:
    // must be 2 × TargetFrameSize (= 2 × 256 KiB) to serve as LZ4 dictionary.
    char* m_ringBuffer = nullptr;
    int m_bufferOffset = 0;

    std::thread m_recvThread;
    std::atomic<bool> m_connected{ false };
    std::atomic<bool> m_shutdown{ false };
    std::atomic<int> m_zoneBeginCount{ 0 };
    std::atomic<int> m_zoneEndCount{ 0 };
    std::atomic<int> m_lockAnnounceCount{ 0 };
    std::atomic<int> m_lockTerminateCount{ 0 };
    std::atomic<int> m_lockWaitCount{ 0 };
    std::atomic<int> m_lockObtainCount{ 0 };
    std::atomic<int> m_lockReleaseCount{ 0 };

    // Current thread established by ThreadContext events (recv thread only, no mutex needed).
    uint32_t m_currentThread = 0;

    mutable std::mutex m_dataMutex;
    std::mutex m_sendMutex;

    // Source location received from a SourceLocationPayload event,
    // to be consumed by the following ZoneBeginAllocSrcLoc event.
    ZoneInfo m_pendingZone;
    bool m_hasPendingZone = false;

    // Payload of the most recent SingleStringData event, to be consumed by the
    // following fat-pointer item (e.g. LockName). Recv thread only.
    std::string m_pendingSingleString;

    std::unordered_map<uint32_t, uint64_t> m_threadCurrentFiber;  // thread id → active fiber ptr (0 = none)
    std::unordered_map<uint32_t, ZoneStack> m_threadZoneStacks;    // thread id → zone stack
    std::unordered_map<uint64_t, ZoneStack> m_fiberZoneStacks;     // fiber ptr → zone stack
    std::unordered_map<uint64_t, std::string> m_fiberNames;        // fiber ptr → name
    std::unordered_set<uint64_t> m_queriedFibers;                  // ptrs already queried

    std::unordered_map<uint32_t, LockInfo> m_locks;                // lock id → state

    // SourceLocation replies carry no request pointer; the profiler answers
    // queries in order, so match replies FIFO against the announcing lock ids.
    std::deque<uint32_t> m_pendingLockSrcLocs;

    // String replies do echo the queried pointer. Several locks may share a
    // string (e.g. the source file), so each pointer maps to all destinations.
    struct PendingLockString
    {
        uint32_t lockId;
        int field; // 0 = name, 1 = function, 2 = source
    };
    std::unordered_map<uint64_t, std::vector<PendingLockString>> m_pendingLockStrings;
};
