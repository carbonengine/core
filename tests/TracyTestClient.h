// Copyright © 2025 CCP ehf.
#pragma once

#include <atomic>
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

    TracyTestClient();
    ~TracyTestClient();

    // Try to connect to the Tracy profiler at addr:port.
    // Retries until timeoutMs elapses. Returns true on success.
    bool Connect( const char* addr = "127.0.0.1", uint16_t port = 8086, int timeoutMs = 5000 );
    void Disconnect();
    bool IsConnected() const;

    int GetZoneBeginCount() const { return m_zoneBeginCount.load( std::memory_order_relaxed ); }
    int GetZoneEndCount() const { return m_zoneEndCount.load( std::memory_order_relaxed ); }

    std::vector<ZoneInfo> GetZones() const;
    std::vector<std::string> GetFiberNames() const;

    TracyTestClient( const TracyTestClient& ) = delete;
    TracyTestClient& operator=( const TracyTestClient& ) = delete;

private:
    void RecvLoop();
    void ProcessDecompressedData( const char* data, int sz );
    void SendQueryLocked( uint8_t queryType, uint64_t ptr, uint32_t extra = 0 );

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

    mutable std::mutex m_dataMutex;
    std::mutex m_sendMutex;

    // Source location received from a SourceLocationPayload event,
    // to be consumed by the following ZoneBeginAllocSrcLoc event.
    ZoneInfo m_pendingZone;
    bool m_hasPendingZone = false;

    std::vector<ZoneInfo> m_zones;
    std::unordered_map<uint64_t, std::string> m_fiberNames; // fiber ptr → name
    std::unordered_set<uint64_t> m_queriedFibers;           // ptrs already queried
};
