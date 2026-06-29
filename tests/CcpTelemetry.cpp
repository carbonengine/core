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
	EXPECT_TRUE( lockInfo.waitingThreads.empty() );

	mutex.Release();
	TickTelemetry( [&] { return TryGetLockById( lockId, lockInfo ) && lockInfo.releaseCount == 1; } );
	EXPECT_EQ( 1, lockInfo.waitCount );
	EXPECT_EQ( 1, lockInfo.obtainCount );
	EXPECT_EQ( 1, lockInfo.releaseCount );
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
	EXPECT_EQ( 0u, lockInfo.holderThread ) << "No thread should hold the mutex after both A and B have released it";
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
	}

	TickTelemetry( [&] { return TryGetLockById( lockId, lockInfo ) && lockInfo.releaseCount == 1; } );
	EXPECT_EQ( 1, lockInfo.obtainCount );
	EXPECT_EQ( 1, lockInfo.releaseCount );
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

		// Do an "early" Release on the scoped guarded Auto mutex
		autoMutex.Release();
		TickTelemetry( [&] { return TryGetLockById( lockId, lockInfo ) && lockInfo.releaseCount == 1; } );
		EXPECT_EQ( 1, lockInfo.releaseCount );
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
	EXPECT_EQ( 0, secondLock.releaseCount );
	ASSERT_TRUE( TryGetLockById( firstLock.id, firstLock ) );
	EXPECT_EQ( 0, firstLock.obtainCount );

	secondMutex.Release();
	TickTelemetry( [&] { return TryGetLockById( secondLock.id, secondLock ) && secondLock.releaseCount == 1; } );
	EXPECT_EQ( 1, secondLock.releaseCount );
	EXPECT_EQ( 0, firstLock.releaseCount );
}

// ---------------------------------------------------------------------------
// CcpSpinLock / CcpAutoSpinLock
// ---------------------------------------------------------------------------
// CcpSpinLock announces a Tracy lock in EnsureTelemetryLockAnnounced and names it
// via TracyCLockCustomName using the spinLockName passed to the constructor.

TEST_F( CcpTelemetryTest, CcpSpinLockAnnounceAndTerminate )
{
	TracyTestClient::LockInfo lockInfo;
	const std::string lockName = "CcpSpinLockAnnounceAndTerminate";

	{
		CcpSpinLock spinLock( lockName.c_str() );

		TickTelemetry( [&] { return TryGetActiveLockNamed( lockName, lockInfo ); } );
		ASSERT_TRUE( TryGetActiveLockNamed( lockName, lockInfo ) );
		EXPECT_FALSE( lockInfo.terminated );
		EXPECT_EQ( lockName, lockInfo.name );
		EXPECT_TRUE( lockInfo.waitingThreads.empty() );
		EXPECT_EQ( 0, lockInfo.waitCount );
		EXPECT_EQ( 0, lockInfo.obtainCount );
		EXPECT_EQ( 0, lockInfo.releaseCount );
	}

	// Destroying the spin-lock terminates its Tracy lock.
	const uint32_t lockId = lockInfo.id;
	TickTelemetry( [&] { return TryGetLockById( lockId, lockInfo ) && lockInfo.terminated; } );
	EXPECT_TRUE( lockInfo.terminated );
}

TEST_F( CcpTelemetryTest, CcpSpinLockAcquireAndRelease )
{
	const std::string lockName = "CcpSpinLockAcquireAndRelease";
	CcpSpinLock spinLock( lockName.c_str() );

	TracyTestClient::LockInfo lockInfo;
	TickTelemetry( [&] { return TryGetActiveLockNamed( lockName, lockInfo ); } );
	ASSERT_TRUE( TryGetActiveLockNamed( lockName, lockInfo ) );
	const uint32_t lockId = lockInfo.id;

	spinLock.Acquire();
	TickTelemetry( [&] { return TryGetLockById( lockId, lockInfo ) && lockInfo.obtainCount == 1; } );
	EXPECT_EQ( 1, lockInfo.waitCount );
	EXPECT_EQ( 1, lockInfo.obtainCount );
	EXPECT_EQ( 0, lockInfo.releaseCount );

	spinLock.Release();
	TickTelemetry( [&] { return TryGetLockById( lockId, lockInfo ) && lockInfo.releaseCount == 1; } );
	EXPECT_EQ( 1, lockInfo.obtainCount );
	EXPECT_EQ( 1, lockInfo.releaseCount );
}

TEST_F( CcpTelemetryTest, CcpAutoSpinLockScopeLocking )
{
	const std::string lockName = "CcpAutoSpinLockScopeLocking";
	CcpSpinLock spinLock( lockName.c_str() );

	TracyTestClient::LockInfo lockInfo;
	TickTelemetry( [&] { return TryGetActiveLockNamed( lockName, lockInfo ); } );
	ASSERT_TRUE( TryGetActiveLockNamed( lockName, lockInfo ) );
	const uint32_t lockId = lockInfo.id;

	{
		CcpAutoSpinLock autoSpinLock( spinLock );

		TickTelemetry( [&] { return TryGetLockById( lockId, lockInfo ) && lockInfo.obtainCount == 1; } );
		EXPECT_EQ( 1, lockInfo.obtainCount );
		EXPECT_EQ( 0, lockInfo.releaseCount );
	}

	TickTelemetry( [&] { return TryGetLockById( lockId, lockInfo ) && lockInfo.releaseCount == 1; } );
	EXPECT_EQ( 1, lockInfo.releaseCount );
}

TEST_F( CcpTelemetryTest, CcpAutoSpinLockEarlyRelease )
{
	const std::string lockName = "CcpAutoSpinLockEarlyRelease";
	CcpSpinLock spinLock( lockName.c_str() );

	TracyTestClient::LockInfo lockInfo;
	TickTelemetry( [&] { return TryGetActiveLockNamed( lockName, lockInfo ); } );
	ASSERT_TRUE( TryGetActiveLockNamed( lockName, lockInfo ) );
	const uint32_t lockId = lockInfo.id;

	{
		CcpAutoSpinLock autoSpinLock( spinLock );

		TickTelemetry( [&] { return TryGetLockById( lockId, lockInfo ) && lockInfo.obtainCount == 1; } );
		EXPECT_EQ( 1, lockInfo.obtainCount );
		EXPECT_EQ( 0, lockInfo.releaseCount );

		// Do an "early" Release on the scoped guarded Auto lock
		autoSpinLock.Release();
		TickTelemetry( [&] { return TryGetLockById( lockId, lockInfo ) && lockInfo.releaseCount == 1; } );
		EXPECT_EQ( 1, lockInfo.releaseCount );
	}

	// Destruction after early release must not double-release.
	TickTelemetry( nullptr, std::chrono::milliseconds( 100 ) );
	ASSERT_TRUE( TryGetLockById( lockId, lockInfo ) );
	EXPECT_EQ( 1, lockInfo.obtainCount );
	EXPECT_EQ( 1, lockInfo.releaseCount );
}


// ---------------------------------------------------------------------------
// CcpSemaphore
// ---------------------------------------------------------------------------
// CcpSemaphore announces a Tracy lock in EnsureTelemetryLockAnnounced and names it
// via TracyCLockCustomName using the semaphoreName passed to the constructor.
// Wait() maps to BeforeLock/AfterLock and Signal() maps to AfterUnlock.
/*
TEST_F( CcpTelemetryTest, CcpSemaphoreAnnounceAndTerminate )
{
	TracyTestClient::LockInfo lockInfo;
	const std::string lockName = "CcpSemaphoreAnnounceAndTerminate";

	{
		CcpSemaphore semaphore( lockName.c_str() );

		TickTelemetry( [&] { return TryGetActiveLockNamed( lockName, lockInfo ); } );
		ASSERT_TRUE( TryGetActiveLockNamed( lockName, lockInfo ) );
		EXPECT_FALSE( lockInfo.terminated );
		EXPECT_EQ( lockName, lockInfo.name );
		EXPECT_EQ( 0, lockInfo.waitCount );
		EXPECT_EQ( 0, lockInfo.obtainCount );
		EXPECT_EQ( 0, lockInfo.releaseCount );
	}

	const uint32_t lockId = lockInfo.id;
	TickTelemetry( [&] { return TryGetLockById( lockId, lockInfo ) && lockInfo.terminated; } );
	EXPECT_TRUE( lockInfo.terminated );
}

TEST_F( CcpTelemetryTest, CcpSemaphoreTimedWaitTimesOut )
{
	const std::string lockName = "CcpSemaphoreTimedWaitTimesOut";
	CcpSemaphore semaphore( lockName.c_str(), 0, 1 );

	TracyTestClient::LockInfo lockInfo;
	TickTelemetry( [&] { return TryGetActiveLockNamed( lockName, lockInfo ); } );
	ASSERT_TRUE( TryGetActiveLockNamed( lockName, lockInfo ) );
	const uint32_t lockId = lockInfo.id;

	// No signal beforehand — TimedWait should time out and report a wait without an obtain.
	EXPECT_FALSE( semaphore.TimedWait( 10 ) );
	TickTelemetry( [&] { return TryGetLockById( lockId, lockInfo ) && lockInfo.waitCount == 1; } );
	EXPECT_EQ( 1, lockInfo.waitCount );
	EXPECT_EQ( 0, lockInfo.obtainCount );
	EXPECT_EQ( 0, lockInfo.releaseCount );
}

TEST_F( CcpTelemetryTest, CcpSemaphoreWaitsForSignalAcrossThreads )
{
	const std::string lockName = "CcpSemaphoreWaitsForSignalAcrossThreads";
	CcpSemaphore semaphore( lockName.c_str(), 0, 1 );

	TracyTestClient::LockInfo lockInfo;
	TickTelemetry( [&] { return TryGetActiveLockNamed( lockName, lockInfo ); } );
	ASSERT_TRUE( TryGetActiveLockNamed( lockName, lockInfo ) );
	const uint32_t lockId = lockInfo.id;

	// Worker thread blocks on Wait(); main thread keeps ticking telemetry then signals.
	std::thread waiter( [&] {
		EXPECT_TRUE( semaphore.Wait() );
	} );

	TickTelemetry( [&] { return TryGetLockById( lockId, lockInfo ) && lockInfo.waitCount >= 1; } );
	EXPECT_GE( lockInfo.waitCount, 1 );
	EXPECT_EQ( 0, lockInfo.obtainCount );

	semaphore.Signal();
	waiter.join();

	TickTelemetry( [&] { return TryGetLockById( lockId, lockInfo ) && lockInfo.obtainCount == 1; } );
	EXPECT_EQ( 1, lockInfo.obtainCount );
	EXPECT_EQ( 1, lockInfo.releaseCount );
}
*/

// ---------------------------------------------------------------------------
// Tests for deprecated (but still used) Lock object constructors of any type
// Explicitly disable deprecation warnings, ONLY for the duration of those tests.
// ---------------------------------------------------------------------------
#if defined( _MSC_VER )
	#pragma warning( push )
	#pragma warning( disable : 4996 )
#elif defined( __clang__ ) || defined( __GNUC__ )
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

TEST_F( CcpTelemetryTest, DeprecatedCcpSpinLockDefaultConstructor )
{
	const std::string expectedLockName = "CcpSpinLock";
	CcpSpinLock spinLock;

	TracyTestClient::LockInfo lockInfo;
	TickTelemetry( [&] { return TryGetActiveLockNamed( expectedLockName, lockInfo ); } );
	ASSERT_TRUE( TryGetActiveLockNamed( expectedLockName, lockInfo ) );
	EXPECT_EQ( expectedLockName, lockInfo.name );
}

/*
TEST_F( CcpTelemetryTest, DeprecatedCcpSemaphoreDefaultConstructor )
{
	const std::string expectedLockName = "CcpSemaphore";
	CcpSemaphore semaphore;

	TracyTestClient::LockInfo lockInfo;
	TickTelemetry( [&] { return TryGetActiveLockNamed( expectedLockName, lockInfo ); } );
	ASSERT_TRUE( TryGetActiveLockNamed( expectedLockName, lockInfo ) );
	EXPECT_EQ( expectedLockName, lockInfo.name );

	// Default initialCount is 0 — TimedWait must time out.
	EXPECT_FALSE( semaphore.TimedWait( 10 ) );
}

TEST_F( CcpTelemetryTest, DeprecatedCcpSemaphoreParamConstructor )
{
	// With initialCount == 3 the first three Wait()/TimedWait() calls
	// should succeed without blocking. The fourth should time out.
	const std::string expectedLockName = "CcpSemaphore";
	const uint32_t initialCount = 3;
	const uint32_t maximumCount = 5;
	CcpSemaphore semaphore( initialCount, maximumCount );

	TracyTestClient::LockInfo lockInfo;
	TickTelemetry( [&] { return TryGetActiveLockNamed( expectedLockName, lockInfo ); } );
	ASSERT_TRUE( TryGetActiveLockNamed( expectedLockName, lockInfo ) );
	EXPECT_EQ( expectedLockName, lockInfo.name );

	// initialCount slots are already signaled — these should not block.
	for ( uint32_t i = 0; i < initialCount; ++i )
	{
		EXPECT_TRUE( semaphore.TimedWait( 10 ) ) << "TimedWait #" << i << " should succeed";
	}
	// One more — count is now drained, must time out.
	EXPECT_FALSE( semaphore.TimedWait( 500 ) );
}
*/

#if defined( _MSC_VER )
	#pragma warning( pop )
#elif defined( __clang__ ) || defined( __GNUC__ )
	#pragma GCC diagnostic pop
#endif


