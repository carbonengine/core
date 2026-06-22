// Copyright © 2025 CCP ehf.

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <functional>
#include <future>
#include <mutex>
#include <thread>

#include <CcpCore.h>

// How can we test telemetry-related functionality to ensure our bookkeeping
// there is sane?
// The problem is that such tests need `ProfilerState::Started` in order to
// create a valid zone context for testing. This, in turn, needs
// `TracyIsConnected` to be true.
//
// A first thought may be to simply redefine the macro to always return true.
// However, this is not possible because it is set inside the Tracy header.
//
// The next idea, then, would be to mock the `Profiler` class. However, this
// also is not possible because the `Profiler` class is not virtual.
//
// This leads to the next idea of choosing the concrete `Profiler` class type
// based on a template parameter. This is not possible either because the macros
// exposed by Tracy would not honor any such template parameter.
//
// With all this in mind, there is another aspect to consider:
// If we wanted to inspect more of the functionality, then we almost certainly
// want to provide a test implementation of the tracy network protocol. Fortunately,
// tracy itself already provides many of the building blocks for this. So this
// includes the AI-written, but human-reviewed test client.
#include "TracyTestClient.h"

class CcpTelemetryTest : public ::testing::Test
{
protected:
	CcpTelemetryTest() = default;
	~CcpTelemetryTest() override = default;

	void SetUp() override
	{
		::testing::Test::SetUp();
		StartTelemetry();
		ConnectProfilerClient();
	}

	void TearDown() override
	{
		DisconnectProfilerClient();
		StopTelemetry();
		::testing::Test::TearDown();
	}

	void TickTelemetry( std::function<bool()> predicate = nullptr, std::chrono::milliseconds timeout = std::chrono::milliseconds( 500 ) )
	{
		const auto deadline = std::chrono::steady_clock::now() + timeout;
		while( std::chrono::steady_clock::now() < deadline && !( predicate && predicate() ) )
		{
			CcpTelemetryTick();
			std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );
		}
	}

	void StartTelemetry( std::string appName = "Telemetry Tests",
						 std::chrono::milliseconds duration = std::chrono::milliseconds::zero(),
						 bool trackMemory = true,
						 bool trackLocks = true )
	{
		CcpTelemetryConfig conf{ appName };
		conf.captureDuration = duration;
		conf.trackMemoryAllocations = trackMemory;
		conf.trackLocks = trackLocks;
		CcpStartTelemetry( conf );
		// It may appear weird that this checks `TracyIsStarted`, but the reason is that the internal state machine
		// in CcpTelemetry only advances to `CcpTelemetryIsStarted` once it _also_ has established a connection to
		// a profiler client.
		// So a logical next question is: Why not use `CcpTelemetryIsConnectionRequested` as predicate? The answer
		// there is that this is flaky when performing a reconnect in a test: the brief sleep window may be enough
		// to fully re-establish the connection between telemetry integration and profiler client. In that scenario,
		// the internal state machine would completely skip that state.
		// As such, we really only wait for the listen socket to be open, which is more or less what `TracyIsStarted`
		// represents.
		TickTelemetry( [] { return TracyIsStarted; }, std::chrono::seconds( 5 ) );
		EXPECT_TRUE( TracyIsStarted ) << "Could not start the telemetry integration";
	}

	void StopTelemetry()
	{
		CcpStopTelemetry();
		TickTelemetry( CcpTelemetryIsStopped, std::chrono::seconds( 5 ) );
		EXPECT_TRUE( CcpTelemetryIsStopped() ) << "Could not stop the telemetry integration";
	}

	void ConnectProfilerClient()
	{
		// Connect on a background thread so this thread can keep ticking Tracy.
		// The handshake requires both sides to run concurrently: Tracy's worker
		// sends data and may block on Send() until the client reads it.
		auto connectFuture = std::async( std::launch::async, [this] {
			return m_tracyClient.Connect();
		} );

		// Tick until CcpTelemetry recognises the connection and the client handshake completes.
		// Both must be true: if we stop ticking the moment CcpTelemetryIsConnected fires,
		// connectFuture.get() may block while Tracy still needs CcpTelemetryTick() to
		// finish the protocol exchange on our side.
		auto isConnected = [&] {
			return CcpTelemetryIsConnected() &&
				connectFuture.wait_for( std::chrono::milliseconds( 0 ) ) == std::future_status::ready;
		};
		TickTelemetry( isConnected, std::chrono::seconds( 5 ) );
		EXPECT_TRUE( connectFuture.get() ) << "Could not establish a connection between telemetry integration and profiler client";
	}

	void DisconnectProfilerClient()
	{
		m_tracyClient.Disconnect();
		TickTelemetry( [this] { return !m_tracyClient.IsConnected() && !TracyIsConnected; } );
		EXPECT_FALSE( TracyIsConnected );
	}

	bool ZoneExists( const std::string& zoneName )
	{
		auto tracyZones = m_tracyClient.GetZones();
		// CcpTelemetryEnterZone passes the zone name as the Tracy "function" field
		// (via the 6-param ___tracy_alloc_srcloc), so match against both fields.
		auto pred = [&zoneName]( const TracyTestClient::ZoneInfo& elem ) -> bool {
			return elem.function == zoneName;
		};

		// Check if the Zone exists in the list of Zones
		return tracyZones.end() != std::find_if( tracyZones.begin(), tracyZones.end(), pred );
	}

	// Helper for Raw lock tests, finding lock announced via TracyCLockAnnounce at a given line.
	// Locks are identified by their announce call site because absolute lock ids and announce counts
	// are not stable across tests: Tracy defers LockAnnounce/LockTerminate events and replays them
	// on every new connection, so locks from earlier tests reappear here.
	// For the same reason, when several locks match the call site (e.g. when a test is repeated
	// within one process), the most recently announced one wins.
	// Call it as a TickTelemetry predicate.
	bool TryGetLockAtLine( uint32_t line, TracyTestClient::LockInfo& outLock )
	{
		bool found = false;
		for( const auto& lock : m_tracyClient.GetAllLocks() )
		{
			// All tests are in this file, hence lock.source == __FILE__
			if( lock.line == line && lock.source == __FILE__ && ( !found || lock.id > outLock.id ) )
			{
				outLock = lock;
				found = true;
			}
		}
		return found;
	}

	// Helper for CcpMutex tests, finding  active locks by the given custom name.
	// CcpMutex names its lock "<owner>-<name>" via EnsureTelemetryLockAnnounced helper function.
	// Name arrives async shortly after announce event, call function from a TickTelemetry predicate.
	// When a test is repeated within one process, Tracy replays earlier (terminated) locks having
	// the same name, where the most recently announced lock wins by id.
	bool TryGetActiveLockNamed( const std::string& name, TracyTestClient::LockInfo& outLock )
	{
		bool found = false;
		for( const auto& lock : m_tracyClient.GetActiveLocks() )
		{
			if( lock.name == name && ( !found || lock.id > outLock.id ) )
			{
				outLock = lock;
				found = true;
			}
		}
		return found;
	}

	// Refreshes a previously identified lock by id.
	bool TryGetLockById( uint32_t lockId, TracyTestClient::LockInfo& outLock )
	{
		return m_tracyClient.TryGetLock( lockId, outLock );
	}

	const std::string expectedNoFiber;
	const std::string expectedFiberName1{ "TestFiber1" };
	const std::string expectedFiberName2{ "TestFiber2" };

	TracyTestClient m_tracyClient;
};


TEST_F( CcpTelemetryTest, TestFiberSwitching )
{
	CcpTelemetrySetActiveFiber( expectedFiberName1 );
	const auto& observedFiberName1 = CcpTelemetryGetActiveFiber();
	EXPECT_EQ( observedFiberName1, expectedFiberName1 );

	CcpTelemetrySetActiveFiber( expectedFiberName2 );
	const auto& observedFiberName2 = CcpTelemetryGetActiveFiber();
	EXPECT_EQ( observedFiberName2, expectedFiberName2 );

	CcpTelemetrySetActiveFiber( expectedFiberName1 );
	const auto& observedFiberName3 = CcpTelemetryGetActiveFiber();
	EXPECT_EQ( observedFiberName1.c_str(), observedFiberName3.c_str() );

	CcpTelemetrySetActiveFiber( "" );
	EXPECT_EQ( CcpTelemetryGetActiveFiber(), expectedNoFiber );
}

TEST_F( CcpTelemetryTest, RemovingActiveFiberClearsIt )
{
	CcpTelemetrySetActiveFiber( expectedFiberName1 );
	CcpTelemetryRemoveFiber( expectedFiberName1 );
	EXPECT_EQ( CcpTelemetryGetActiveFiber(), expectedNoFiber );
}

TEST_F( CcpTelemetryTest, SimpleZoneTest )
{
	EXPECT_TRUE( CcpTelemetryIsConnected() );

	static int key = 4711;
	const std::string zoneName{ "TestZone" };
	CcpTelemetryEnterZone( &key, zoneName.c_str(), __FILE__, __LINE__ );
	// Tracy's worker sleeps up to 10 ms between queue flushes, so give it
	// time to process and send the zone event before asserting.
	TickTelemetry( [this] { return m_tracyClient.GetZoneBeginCount() == 1; } );
	EXPECT_EQ( 1, m_tracyClient.GetZoneBeginCount() );
	EXPECT_TRUE( ZoneExists( zoneName ) );

	CcpTelemetryLeaveZone( &key );
	TickTelemetry( [this] { return m_tracyClient.GetZoneEndCount() == 1; } );
	EXPECT_EQ( 1, m_tracyClient.GetZoneEndCount() );
	EXPECT_FALSE( ZoneExists( zoneName ) );
}

TEST_F( CcpTelemetryTest, StackedZones )
{
	// A stacked zone is a zone that has the same key as a previously created zone.
	static int key = 4711;
	CcpTelemetryEnterZone( &key, "TestZone", __FILE__, __LINE__ );
	CcpTelemetryEnterZone( &key, "TestZone2", __FILE__, __LINE__ );
	TickTelemetry( [this] { return m_tracyClient.GetZones().size() == 2; } );
	EXPECT_EQ( 2, m_tracyClient.GetZones().size() );
	EXPECT_TRUE( ZoneExists( "TestZone" ) );
	EXPECT_TRUE( ZoneExists( "TestZone2" ) );

	CcpTelemetryLeaveZone( &key );
	TickTelemetry( [this] { return m_tracyClient.GetZones().size() == 1; } );
	EXPECT_EQ( 1, m_tracyClient.GetZones().size() );
	EXPECT_TRUE( ZoneExists( "TestZone" ) );
	EXPECT_FALSE( ZoneExists( "TestZone2" ) ) << "TestZone2 should be gone";

	CcpTelemetryLeaveZone( &key );
	TickTelemetry( [this] { return m_tracyClient.GetZones().empty(); } );
	EXPECT_TRUE( m_tracyClient.GetZones().empty() );
}

TEST_F( CcpTelemetryTest, StartStopStartTelemetryWhileClientIsRunning )
{
	static int key1 = 1001;
	const std::string zoneName1{ "FirstZone" };
	CcpTelemetryEnterZone( &key1, zoneName1.c_str(), __FILE__, __LINE__ );
	TickTelemetry( [this, zoneName1] { return ZoneExists( zoneName1 ); } );
	EXPECT_TRUE( ZoneExists( zoneName1 ) );
	EXPECT_EQ( 1, m_tracyClient.GetZones().size() );
	EXPECT_EQ( 1, m_tracyClient.GetZoneBeginCount() );
	EXPECT_EQ( 0, m_tracyClient.GetZoneEndCount() );

	CcpTelemetryLeaveZone( &key1 );
	TickTelemetry( [this] { return m_tracyClient.GetZones().empty(); } );
	EXPECT_TRUE( m_tracyClient.GetZones().empty() );
	EXPECT_EQ( 1, m_tracyClient.GetZoneEndCount() );

	// Now simulate "Stop and Start Telemetry" operation
	StopTelemetry();
	EXPECT_TRUE( CcpTelemetryIsStopped() );
	StartTelemetry( "Telemetry Tests - 2nd Start" );
	// StartTelemetry only checks whether the profiler integration is started, but the running client also needs to reconnect.
	// Thus, wait until that has happened, which should be fast in this scenario.
	TickTelemetry( CcpTelemetryIsConnected );
	EXPECT_TRUE( CcpTelemetryIsStarted() );

	// Emit a new Zone, on the 2nd Start and validate
	static int key2 = 1002;
	const std::string zoneName2{ "SecondZone" };
	CcpTelemetryEnterZone( &key2, zoneName2.c_str(), __FILE__, __LINE__ );
	TickTelemetry( [this, zoneName2] { return ZoneExists( zoneName2 ); } );
	EXPECT_TRUE( ZoneExists( zoneName2 ) );
	EXPECT_FALSE( ZoneExists( zoneName1 ) ) << "FirstZone should not exist";
	EXPECT_EQ( 1, m_tracyClient.GetZones().size() );
	EXPECT_EQ( 2, m_tracyClient.GetZoneBeginCount() );
	EXPECT_EQ( 1, m_tracyClient.GetZoneEndCount() );

	CcpTelemetryLeaveZone( &key2 );
	TickTelemetry();
	EXPECT_TRUE( m_tracyClient.GetZones().empty() );
	EXPECT_EQ( 2, m_tracyClient.GetZoneEndCount() );
}

TEST_F( CcpTelemetryTest, RawTracyLockCounters )
{
	TracyCLockCtx lockCtx;
	TracyTestClient::LockInfo lockInfo;
	std::string lockName = "CcpTelemetryTest-RawTracyLockCounters";
	EXPECT_EQ( 0, m_tracyClient.GetLockAnnounceCount() );
	EXPECT_EQ( 0, m_tracyClient.GetLockWaitCount() );
	EXPECT_EQ( 0, m_tracyClient.GetLockObtainCount() );
	EXPECT_EQ( 0, m_tracyClient.GetLockReleaseCount() );
	EXPECT_EQ( 0, m_tracyClient.GetLockTerminateCount() );

	// Announce Lock:
	const uint32_t announceLine = __LINE__ + 1;  // The line where we call TracyCLockAnnounce()
	TracyCLockAnnounce( lockCtx );
	TickTelemetry( [&] { return TryGetLockAtLine( announceLine, lockInfo ); } );
	EXPECT_EQ( "", lockInfo.name ) << "Name is only set in TracyCLockCustomName";
	EXPECT_EQ( announceLine, lockInfo.line );
	EXPECT_EQ( 1, m_tracyClient.GetLockAnnounceCount() );

	// Give the Lock a name:
	TracyCLockCustomName( lockCtx, lockName.c_str(), lockName.size() );
	TickTelemetry( [&] { return TryGetLockAtLine( announceLine, lockInfo ) && lockInfo.name == lockName; } );
	EXPECT_EQ( lockName, lockInfo.name );
	EXPECT_FALSE( lockInfo.terminated );
	EXPECT_EQ( 0, size(lockInfo.waitingThreads) );
	EXPECT_EQ( 0, lockInfo.waitCount );
	EXPECT_EQ( 0, lockInfo.obtainCount );
	EXPECT_EQ( 0, lockInfo.releaseCount );

	// Before Lock Acquire:
	const auto notifyTracy = TracyCLockBeforeLock( lockCtx );
	TickTelemetry( [&] { return TryGetLockAtLine( announceLine, lockInfo ) && (lockInfo.obtainCount == 1 || lockInfo.waitCount == 1); } );
	EXPECT_TRUE( notifyTracy );
	EXPECT_EQ( 1, lockInfo.waitCount + lockInfo.obtainCount ) << "Sum of Wait+Obtain needs to match";
	EXPECT_EQ( 1, m_tracyClient.GetLockObtainCount() + m_tracyClient.GetLockWaitCount() ) << "Sum of Wait+Obtain needs to match";

	// After Lock Acquire:
	TracyCLockAfterLock( lockCtx );
	TickTelemetry( [&] { return TryGetLockAtLine( announceLine, lockInfo ) && lockInfo.obtainCount == 1; } );
	EXPECT_EQ( 1, lockInfo.waitCount );
	EXPECT_EQ( 1, lockInfo.obtainCount );
	EXPECT_EQ( 0, lockInfo.releaseCount );
	EXPECT_EQ( 1, m_tracyClient.GetLockObtainCount() );
	EXPECT_EQ( 1, m_tracyClient.GetLockWaitCount() );

	// After Lock Release:
	TracyCLockAfterUnlock( lockCtx );
	TickTelemetry( [&] { return TryGetLockAtLine( announceLine, lockInfo ) && lockInfo.releaseCount == 1; } );
	EXPECT_EQ( 1, lockInfo.releaseCount );
	EXPECT_EQ( 1, m_tracyClient.GetLockReleaseCount() );

	// Remove the Lock:
	TracyCLockTerminate( lockCtx );
	TickTelemetry( [&] { return TryGetLockAtLine( announceLine, lockInfo ) && lockInfo.terminated; } );
	EXPECT_TRUE( lockInfo.terminated );
}

// A locks, B waits, A unlocks, B locks, B unlocks.
TEST_F( CcpTelemetryTest, RawTracyLockOneWaitingThread )
{
	TracyCLockCtx lockCtx;
	TracyTestClient::LockInfo lockInfo;
	std::string lockName = "CcpTelemetryTest-RawTracyLockOneWaitingThread";

	const uint32_t announceLine = __LINE__ + 1;
	TracyCLockAnnounce( lockCtx );
	TracyCLockCustomName( lockCtx, lockName.c_str(), lockName.size() );
	TickTelemetry( [&] { return TryGetLockAtLine( announceLine, lockInfo ) && !lockInfo.terminated; } );
	ASSERT_TRUE( TryGetLockAtLine( announceLine, lockInfo ) );

	std::mutex mutex;
	auto lockMutex = [&] {
		const auto notifyTracy = TracyCLockBeforeLock( lockCtx );
		mutex.lock();
		if( notifyTracy )
		{
			TracyCLockAfterLock( lockCtx );
		}
	};
	auto unlockMutex = [&] {
		mutex.unlock();
		TracyCLockAfterUnlock( lockCtx );
	};

	// Thread A acquires the lock and holds it until we tell it to release.
	// The locking must happen on worker threads so that this thread can keep
	// ticking the telemetry while the lock is being held / waited on.
	std::atomic<bool> releaseA{ false };
	std::thread threadA( [&] {
		lockMutex();
		while( !releaseA.load() )
			std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
		unlockMutex();
	} );

	TickTelemetry( [&] { return TryGetLockAtLine( announceLine, lockInfo ) && lockInfo.holderThread != 0; } );
	const uint32_t threadAId = lockInfo.holderThread;
	EXPECT_NE( 0u, threadAId );
	EXPECT_EQ( 1, lockInfo.waitCount );
	EXPECT_EQ( 1, lockInfo.obtainCount );
	EXPECT_TRUE( lockInfo.waitingThreads.empty() );

	// Thread B blocks on the lock while A is holding it.
	std::thread threadB( [&] {
		lockMutex();
		unlockMutex();
	} );

	TickTelemetry( [&] { return TryGetLockAtLine( announceLine, lockInfo ) && lockInfo.waitingThreads.size() == 1; } );
	EXPECT_EQ( 1u, lockInfo.waitingThreads.size() );
	const uint32_t threadBId = lockInfo.waitingThreads.empty() ? 0 : lockInfo.waitingThreads.front();
	EXPECT_NE( 0u, threadBId );
	EXPECT_NE( threadAId, threadBId );
	EXPECT_EQ( threadAId, lockInfo.holderThread ) << "A should still hold the lock while B waits";
	EXPECT_EQ( 2, lockInfo.waitCount );
	EXPECT_EQ( 1, lockInfo.obtainCount );
	EXPECT_EQ( 0, lockInfo.releaseCount );

	// Let A unlock; B then obtains and releases the lock.
	releaseA.store( true );
	threadA.join();
	threadB.join();

	TickTelemetry( [&] { return TryGetLockAtLine( announceLine, lockInfo ) && lockInfo.releaseCount == 2; } );
	EXPECT_EQ( 2, lockInfo.waitCount );
	EXPECT_EQ( 2, lockInfo.obtainCount );
	EXPECT_EQ( 2, lockInfo.releaseCount );
	EXPECT_EQ( 0u, lockInfo.holderThread );
	EXPECT_TRUE( lockInfo.waitingThreads.empty() );

	TracyCLockTerminate( lockCtx );
	TickTelemetry( [&] { return TryGetLockAtLine( announceLine, lockInfo ) && lockInfo.terminated; } );
	EXPECT_TRUE( lockInfo.terminated );
}

// A locks, B waits, C waits, A unlocks, B and C lock/unlock in turn.
TEST_F( CcpTelemetryTest, RawTracyLockMultipleWaitingThreads )
{
	TracyCLockCtx lockCtx;
	TracyTestClient::LockInfo lockInfo;
	std::string lockName = "CcpTelemetryTest-RawTracyLockMultipleWaitingThreads";

	const uint32_t announceLine = __LINE__ + 1;
	TracyCLockAnnounce( lockCtx );
	TracyCLockCustomName( lockCtx, lockName.c_str(), lockName.size() );
	TickTelemetry( [&] { return TryGetLockAtLine( announceLine, lockInfo ) && !lockInfo.terminated; } );
	ASSERT_TRUE( TryGetLockAtLine( announceLine, lockInfo ) );

	std::mutex mutex;
	auto lockMutex = [&] {
		const auto notifyTracy = TracyCLockBeforeLock( lockCtx );
		mutex.lock();
		if( notifyTracy )
		{
			TracyCLockAfterLock( lockCtx );
		}
	};
	auto unlockMutex = [&] {
		mutex.unlock();
		TracyCLockAfterUnlock( lockCtx );
	};

	std::atomic<bool> releaseA{ false };
	std::thread threadA( [&] {
		lockMutex();
		while( !releaseA.load() )
			std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
		unlockMutex();
	} );

	TickTelemetry( [&] { return TryGetLockAtLine( announceLine, lockInfo ) && lockInfo.holderThread != 0; } );
	const uint32_t threadAId = lockInfo.holderThread;
	EXPECT_NE( 0u, threadAId );

	// B and C block on the lock one after the other while A is holding it.
	auto waiterBody = [&] {
		lockMutex();
		unlockMutex();
	};
	std::thread threadB( waiterBody );
	TickTelemetry( [&] { return TryGetLockAtLine( announceLine, lockInfo ) && lockInfo.waitingThreads.size() == 1; } );
	std::thread threadC( waiterBody );
	TickTelemetry( [&] { return TryGetLockAtLine( announceLine, lockInfo ) && lockInfo.waitingThreads.size() == 2; } );

	EXPECT_EQ( 2u, lockInfo.waitingThreads.size() );
	EXPECT_EQ( threadAId, lockInfo.holderThread ) << "A should still hold the lock while B and C wait";
	EXPECT_EQ( 3, lockInfo.waitCount );
	EXPECT_EQ( 1, lockInfo.obtainCount );
	EXPECT_EQ( 0, lockInfo.releaseCount );
	if( lockInfo.waitingThreads.size() == 2 )
	{
		EXPECT_NE( lockInfo.waitingThreads[0], lockInfo.waitingThreads[1] );
		EXPECT_NE( threadAId, lockInfo.waitingThreads[0] );
		EXPECT_NE( threadAId, lockInfo.waitingThreads[1] );
	}

	// Let A unlock; B and C then obtain and release the lock in whichever
	// order the OS wakes them up.
	releaseA.store( true );
	threadA.join();
	threadB.join();
	threadC.join();

	TickTelemetry( [&] { return TryGetLockAtLine( announceLine, lockInfo ) && lockInfo.releaseCount == 3; } );
	EXPECT_EQ( 3, lockInfo.waitCount );
	EXPECT_EQ( 3, lockInfo.obtainCount );
	EXPECT_EQ( 3, lockInfo.releaseCount );
	EXPECT_EQ( 0u, lockInfo.holderThread );
	EXPECT_TRUE( lockInfo.waitingThreads.empty() );

	TracyCLockTerminate( lockCtx );
	TickTelemetry( [&] { return TryGetLockAtLine( announceLine, lockInfo ) && lockInfo.terminated; } );
	EXPECT_TRUE( lockInfo.terminated );
}

// ---------------------------------------------------------------------------
// CcpMutex / CcpAutoMutex
// ---------------------------------------------------------------------------
// CcpMutex announces a Tracy lock in the EnsureTelemetryLockAnnounced helper function
// and names it "<owner>-<name>" via TracyCLockCustomName.
// It reports wait/obtain around EnterCriticalSection in Acquire(), release in
// Release() and terminates in destructor.
// The custom name is what currently identifies a CcpMutex lock; see TryGetActiveLockNamed.

TEST_F( CcpTelemetryTest, CcpMutexAnnounceAndTerminate )
{
	TracyTestClient::LockInfo lockInfo;

	// Scope the CcpMutex so we can see what happens on destruction.
	{
		std::string lockName = "CcpTelemetryTest-CcpMutexAnnounceAndTerminate";
		CcpMutex mutex( "CcpTelemetryTest", "CcpMutexAnnounceAndTerminate" );

		// The custom name arrives almost immediately, but the source location
		// resolves through extra server-query round trips; wait for both.
		TickTelemetry( [&] { return TryGetActiveLockNamed( lockName, lockInfo ) && !lockInfo.source.empty(); } );
		ASSERT_TRUE( TryGetActiveLockNamed( lockName, lockInfo ) );
		EXPECT_FALSE( lockInfo.terminated );
		// The owner and name passed to CcpMutex arrive combined as the custom lock name.
		EXPECT_EQ( lockName, lockInfo.name );
		// The announce site is the EnsureTelemetryLockAnnounced helper function
		EXPECT_EQ( "EnsureTelemetryLockAnnounced", lockInfo.function );
		EXPECT_EQ( 0u, lockInfo.holderThread );
		EXPECT_TRUE( lockInfo.waitingThreads.empty() );
		EXPECT_EQ( 0, lockInfo.waitCount );
		EXPECT_EQ( 0, lockInfo.obtainCount );
		EXPECT_EQ( 0, lockInfo.releaseCount );
	}

	// Destroying the mutex terminates its lock.
	const uint32_t lockId = lockInfo.id;
	TickTelemetry( [&] { return TryGetLockById( lockId, lockInfo ) && lockInfo.terminated; } );
	EXPECT_TRUE( lockInfo.terminated );
}

TEST_F( CcpTelemetryTest, CcpMutexAcquireAndRelease )
{
	std::string lockName = "CcpTelemetryTest-CcpMutexAcquireAndRelease";
	CcpMutex mutex( "CcpTelemetryTest", "CcpMutexAcquireAndRelease" );

	TracyTestClient::LockInfo lockInfo;
	TickTelemetry( [&] { return TryGetActiveLockNamed( lockName, lockInfo ); } );
	ASSERT_TRUE( TryGetActiveLockNamed( lockName, lockInfo ) );
	const uint32_t lockId = lockInfo.id;

	mutex.Acquire();
	TickTelemetry( [&] { return TryGetLockById( lockId, lockInfo ) && lockInfo.obtainCount == 1; } );
	EXPECT_EQ( 1, lockInfo.waitCount );
	EXPECT_EQ( 1, lockInfo.obtainCount );
	EXPECT_EQ( 0, lockInfo.releaseCount );
	EXPECT_NE( 0u, lockInfo.holderThread );
	EXPECT_TRUE( lockInfo.waitingThreads.empty() );

	mutex.Release();
	TickTelemetry( [&] { return TryGetLockById( lockId, lockInfo ) && lockInfo.releaseCount == 1; } );
	EXPECT_EQ( 1, lockInfo.waitCount );
	EXPECT_EQ( 1, lockInfo.obtainCount );
	EXPECT_EQ( 1, lockInfo.releaseCount );
	EXPECT_EQ( 0u, lockInfo.holderThread );
}

// A acquires the CcpMutex, B waits, A releases, B acquires and releases.
TEST_F( CcpTelemetryTest, CcpMutexContentionAcrossThreads )
{
	std::string lockName = "CcpTelemetryTest-CcpMutexContentionAcrossThreads";
	CcpMutex mutex( "CcpTelemetryTest", "CcpMutexContentionAcrossThreads" );

	TracyTestClient::LockInfo lockInfo;
	TickTelemetry( [&] { return TryGetActiveLockNamed( lockName, lockInfo ); } );
	ASSERT_TRUE( TryGetActiveLockNamed( lockName, lockInfo ) );
	const uint32_t lockId = lockInfo.id;

	// Thread A acquires the mutex and holds it until we tell it to release.
	// The locking must happen on worker threads so that this thread can keep
	// ticking the telemetry while the mutex is being held / waited on.
	std::atomic<bool> releaseA{ false };
	std::thread threadA( [&] {
		mutex.Acquire();
		while( !releaseA.load() )
			std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
		mutex.Release();
	} );

	TickTelemetry( [&] { return TryGetLockById( lockId, lockInfo ) && lockInfo.holderThread != 0; } );
	const uint32_t threadAId = lockInfo.holderThread;
	EXPECT_NE( 0u, threadAId );
	EXPECT_EQ( 1, lockInfo.waitCount );
	EXPECT_EQ( 1, lockInfo.obtainCount );
	EXPECT_TRUE( lockInfo.waitingThreads.empty() );

	// Thread B blocks on the mutex while A is holding it.
	std::thread threadB( [&] {
		mutex.Acquire();
		mutex.Release();
	} );

	TickTelemetry( [&] { return TryGetLockById( lockId, lockInfo ) && lockInfo.waitingThreads.size() == 1; } );
	EXPECT_EQ( 1u, lockInfo.waitingThreads.size() );
	const uint32_t threadBId = lockInfo.waitingThreads.empty() ? 0 : lockInfo.waitingThreads.front();
	EXPECT_NE( 0u, threadBId );
	EXPECT_NE( threadAId, threadBId );
	EXPECT_EQ( threadAId, lockInfo.holderThread ) << "A should still hold the mutex while B waits";
	EXPECT_EQ( 2, lockInfo.waitCount );
	EXPECT_EQ( 1, lockInfo.obtainCount );
	EXPECT_EQ( 0, lockInfo.releaseCount );

	// Let A release; B then acquires and releases the mutex.
	releaseA.store( true );
	threadA.join();
	threadB.join();

	TickTelemetry( [&] { return TryGetLockById( lockId, lockInfo ) && lockInfo.releaseCount == 2; } );
	EXPECT_EQ( 2, lockInfo.waitCount );
	EXPECT_EQ( 2, lockInfo.obtainCount );
	EXPECT_EQ( 2, lockInfo.releaseCount );
	EXPECT_EQ( 0u, lockInfo.holderThread );
	EXPECT_TRUE( lockInfo.waitingThreads.empty() );
}

TEST_F( CcpTelemetryTest, CcpAutoMutexScopeLocking )
{
	std::string lockName = "CcpTelemetryTest-CcpAutoMutexScopeLocking";
	CcpMutex mutex( "CcpTelemetryTest", "CcpAutoMutexScopeLocking" );

	TracyTestClient::LockInfo lockInfo;
	TickTelemetry( [&] { return TryGetActiveLockNamed( lockName, lockInfo ); } );
	ASSERT_TRUE( TryGetActiveLockNamed( lockName, lockInfo ) );
	const uint32_t lockId = lockInfo.id;

	{
		CcpAutoMutex autoMutex( mutex );

		TickTelemetry( [&] { return TryGetLockById( lockId, lockInfo ) && lockInfo.obtainCount == 1; } );
		EXPECT_EQ( 1, lockInfo.waitCount );
		EXPECT_EQ( 1, lockInfo.obtainCount );
		EXPECT_EQ( 0, lockInfo.releaseCount );
		EXPECT_NE( 0u, lockInfo.holderThread );
	}

	TickTelemetry( [&] { return TryGetLockById( lockId, lockInfo ) && lockInfo.releaseCount == 1; } );
	EXPECT_EQ( 1, lockInfo.obtainCount );
	EXPECT_EQ( 1, lockInfo.releaseCount );
	EXPECT_EQ( 0u, lockInfo.holderThread );
}

TEST_F( CcpTelemetryTest, CcpAutoMutexEarlyRelease )
{
	std::string lockName = "CcpTelemetryTest-CcpAutoMutexEarlyRelease";
	CcpMutex mutex( "CcpTelemetryTest", "CcpAutoMutexEarlyRelease" );

	TracyTestClient::LockInfo lockInfo;
	TickTelemetry( [&] { return TryGetActiveLockNamed( lockName, lockInfo ); } );
	ASSERT_TRUE( TryGetActiveLockNamed( lockName, lockInfo ) );
	const uint32_t lockId = lockInfo.id;

	{
		CcpAutoMutex autoMutex( mutex );

		TickTelemetry( [&] { return TryGetLockById( lockId, lockInfo ) && lockInfo.obtainCount == 1; } );
		EXPECT_EQ( 1, lockInfo.obtainCount );
		EXPECT_NE( 0u, lockInfo.holderThread );

		autoMutex.Release();
		TickTelemetry( [&] { return TryGetLockById( lockId, lockInfo ) && lockInfo.releaseCount == 1; } );
		EXPECT_EQ( 1, lockInfo.releaseCount );
		EXPECT_EQ( 0u, lockInfo.holderThread );
	}

	// Destroying the CcpAutoMutex after the early release must not release the
	// mutex a second time. Tick a little longer to give a (faulty) second
	// release event a chance to arrive before asserting it did not.
	TickTelemetry( nullptr, std::chrono::milliseconds( 100 ) );
	ASSERT_TRUE( TryGetLockById( lockId, lockInfo ) );
	EXPECT_EQ( 1, lockInfo.obtainCount );
	EXPECT_EQ( 1, lockInfo.releaseCount );
}

TEST_F( CcpTelemetryTest, MultipleCcpMutexesAnnounceDistinctLocks )
{
	// The custom lock names make the two mutexes distinguishable even though
	// they share the same announce call site in CcpMutex.h.
	CcpMutex firstMutex( "CcpTelemetryTest", "MultiTestFirstMutex" );
	CcpMutex secondMutex( "CcpTelemetryTest", "MultiTestSecondMutex" );
	std::string firstLockName = "CcpTelemetryTest-MultiTestFirstMutex";
	std::string secondLockName = "CcpTelemetryTest-MultiTestSecondMutex";

	// resolved) announce source location, so wait for that to settle too.
	TracyTestClient::LockInfo firstLock;
	TracyTestClient::LockInfo secondLock;
	TickTelemetry( [&] {
		return TryGetActiveLockNamed( firstLockName, firstLock ) &&
			TryGetActiveLockNamed( secondLockName, secondLock );
	} );
	ASSERT_TRUE( TryGetActiveLockNamed( firstLockName, firstLock ) );
	ASSERT_TRUE( TryGetActiveLockNamed( secondLockName, secondLock ) );
	EXPECT_NE( firstLock.id, secondLock.id );

	// Each mutex drives its own lock: acquiring the second must not affect the first.
	secondMutex.Acquire();
	TickTelemetry( [&] { return TryGetLockById( secondLock.id, secondLock ) && secondLock.obtainCount == 1; } );
	EXPECT_EQ( 1, secondLock.obtainCount );
	EXPECT_NE( 0u, secondLock.holderThread );
	ASSERT_TRUE( TryGetLockById( firstLock.id, firstLock ) );
	EXPECT_EQ( 0, firstLock.obtainCount );
	EXPECT_EQ( 0u, firstLock.holderThread );

	secondMutex.Release();
	TickTelemetry( [&] { return TryGetLockById( secondLock.id, secondLock ) && secondLock.releaseCount == 1; } );
	EXPECT_EQ( 1, secondLock.releaseCount );
	EXPECT_EQ( 0u, secondLock.holderThread );
}
