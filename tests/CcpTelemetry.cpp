// Copyright © 2025 CCP ehf.

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <functional>
#include <future>
#include <mutex>
#include <thread>

#include <tracy/Tracy.hpp>

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

	// Helper for CaptureMasks to make sure it only has a single bit set to 1
	bool IsSCaptureMaskSingleBit( uint64_t mask )
	{
		return mask != 0 && ( mask & ( mask - 1 ) ) == 0;
	}

	// Helper for find a CaptureMask by name from the list of already registered CaptureMasks
	bool TryGetCaptureMaskNamed( const std::string& name, CcpCaptureMaskInfo& info )
	{
		// CaptureMask is stored lower-case, make sure we compare it as such.
		std::string lowerName( name );
		std::transform( lowerName.begin(), lowerName.end(), lowerName.begin(), []( unsigned char c ) { return static_cast<char>( std::tolower( c ) ); } );

		for( const CcpCaptureMaskInfo& mask : CcpGetRegisteredCaptureMasks() )
		{
			if( mask.name == lowerName )
			{
				info = mask;
				return true;
			}
		}
		return false;
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

TEST_F( CcpTelemetryTest, RemainingCaptureDuration )
{
	// The test fixture starts telemetry without a specific capture duration.
	// So the remaining capture duration is 0 milliseconds, no matter how long we tick.
	EXPECT_EQ( CcpTelemetryRemainingCaptureDuration(), std::chrono::milliseconds( 0 ) );
	TickTelemetry( nullptr, std::chrono::milliseconds( 100 ) );
	EXPECT_EQ( CcpTelemetryRemainingCaptureDuration(), std::chrono::milliseconds( 0 ) );
	StopTelemetry();

	// Now start a timed capture
	constexpr auto captureDuration = std::chrono::milliseconds( 500 );
	constexpr auto waitDuration = std::chrono::milliseconds( 100 );
	StartTelemetry( "Telemetry Tests", captureDuration );
	TickTelemetry( [] { return CcpTelemetryIsConnected(); } );
	// Compare to lower-equal because some milliseconds may have passed to perform the whole connection handshake and so on.
	EXPECT_LE( CcpTelemetryRemainingCaptureDuration().count(), captureDuration.count() );
	// A short tick to observe that remaining time counts down
	TickTelemetry( nullptr, waitDuration );
	EXPECT_LE( CcpTelemetryRemainingCaptureDuration().count(), ( captureDuration - waitDuration ).count() );
	// Wait for the remaining time to expire
	TickTelemetry( nullptr, captureDuration );
	EXPECT_EQ( CcpTelemetryRemainingCaptureDuration(), std::chrono::milliseconds( 0 ) );
	EXPECT_TRUE( CcpTelemetryIsStopped() );
}

TEST_F( CcpTelemetryTest, SimpleZoneTest )
{
	EXPECT_TRUE( CcpTelemetryIsConnected() );

	static int key = 4711;
	const std::string zoneName{ "TestZone" };
	CcpTelemetryEnterZone( &key, zoneName.c_str(), __FILE__, __LINE__ );  // Original deprecated version
	// Tracy's worker sleeps up to 10 ms between queue flushes, so give it
	// time to process and send the zone event before asserting.
	TickTelemetry( [this] { return m_tracyClient.GetZoneBeginCount() == 1; } );
	EXPECT_EQ( 1, m_tracyClient.GetZoneBeginCount() );
	EXPECT_TRUE( ZoneExists( zoneName ) );

	// The default assigned color in CcpTelemetryEnterZone() for TMCM_CPP is CcpColor::Yellow, make sure it is present
	const auto zones = m_tracyClient.GetZones();
	EXPECT_EQ( 1, zones.size() );
	EXPECT_EQ( static_cast<uint32_t>( CcpColor::Yellow ), zones.front().color );

	CcpTelemetryLeaveZone( &key );
	TickTelemetry( [this] { return m_tracyClient.GetZoneEndCount() == 1; } );
	EXPECT_EQ( 1, m_tracyClient.GetZoneEndCount() );
	EXPECT_FALSE( ZoneExists( zoneName ) );
}

TEST_F( CcpTelemetryTest, StackedZones )
{
	// A stacked zone is a zone that has the same key as a previously created zone.
	static int key = 4711;
	CcpTelemetryEnterZone( &key, "TestZone", __FILE__, __LINE__ ); // Original deprecated version
	CcpTelemetryEnterZone( &key, TMCM_CPP, "TestZone2", __FILE__, __LINE__ ); // New CaptureMaskBit version
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
	CcpTelemetryEnterZone( &key1, zoneName1.c_str(), __FILE__, __LINE__ ); // Original deprecated version
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
	CcpTelemetryEnterZone( &key2, TMCM_GENERAL, zoneName2.c_str(), __FILE__, __LINE__ );  // New CaptureMaskBit version (CcpColor::SteelBlue)
	TickTelemetry( [this, zoneName2] { return ZoneExists( zoneName2 ); } );
	EXPECT_TRUE( ZoneExists( zoneName2 ) );
	EXPECT_FALSE( ZoneExists( zoneName1 ) ) << "FirstZone should not exist";

	const auto zones = m_tracyClient.GetZones();
	EXPECT_EQ( 1, zones.size() );
	EXPECT_EQ( static_cast<uint32_t>( CcpColor::SteelBlue ), zones.front().color ) << "Default color for CaptureMask TMCM_GENERAL should be CcpColor::SteelBlue";
	EXPECT_EQ( 2, m_tracyClient.GetZoneBeginCount() );
	EXPECT_EQ( 1, m_tracyClient.GetZoneEndCount() );

	CcpTelemetryLeaveZone( &key2 );
	TickTelemetry();
	EXPECT_TRUE( m_tracyClient.GetZones().empty() );
	EXPECT_EQ( 2, m_tracyClient.GetZoneEndCount() );
}

TEST_F( CcpTelemetryTest, TelemetryZoneLegacyConstructor )
{
	// Test where captureMask is in the Active list
	CcpSetActiveCaptureMask( TMCM_GENERAL | TMCM_CPP );
	{
		// Make sure the "legacy deprecated" TelemetryZone constructor respects overwrites of color
		TelemetryZone activeZone( TMCM_CPP, "LegacyZoneIsInActiveList", __FILE__, __LINE__, CcpColor::Red ); // Overwrite the default CcpColor::Yellow of TMCM_CPP
		TickTelemetry( [this] { return m_tracyClient.GetZoneBeginCount() == 1; } );
		EXPECT_EQ( 1, m_tracyClient.GetZoneBeginCount() );
		const auto zones = m_tracyClient.GetZones();
		EXPECT_EQ( 1, zones.size() );
		EXPECT_EQ( "LegacyZoneIsInActiveList", zones.front().function );
		EXPECT_EQ( static_cast<uint32_t>( CcpColor::Red ), zones.front().color );
		EXPECT_EQ( 0, m_tracyClient.GetZoneEndCount() );
	}
	// Now the activeZone has gone out of scope, so the zone should have ended
	TickTelemetry( [this] { return m_tracyClient.GetZoneEndCount() == 1; } );
	EXPECT_EQ( 1, m_tracyClient.GetZoneEndCount() );
	EXPECT_TRUE( m_tracyClient.GetZones().empty() );

	// Test where captureMask is NOT in the Active list
	CcpSetActiveCaptureMask( {"NotRegisteredCaptureMask", "ClearingPreviousGeneralAndCpp", "FromTheActiveCaptureMaskList"} );
	{
		TelemetryZone inactiveZone( TMCM_CPP, "LegacyZoneIsNotInActiveList", __FILE__, __LINE__, CcpColor::Blue );
		TickTelemetry( [this] { return m_tracyClient.GetZoneBeginCount() == 1; } );
		EXPECT_EQ( 1, m_tracyClient.GetZoneBeginCount() ) << "Because TMCM_CPP is now no longer in the ACTIVE CaptureMask list, we should not have emitted an event for it";
		EXPECT_EQ( 0, m_tracyClient.GetZones().size() ) << "Zone list should therefore be empty";
	}
	TickTelemetry( nullptr, std::chrono::milliseconds( 100 ) );
	EXPECT_EQ( 1, m_tracyClient.GetZoneBeginCount() ) << "Inactive legacy zone must not emit ZoneBegin, count should stay at 1";
	EXPECT_EQ( 1, m_tracyClient.GetZoneEndCount() ) << "Inactive legacy zone must not emit ZoneEnd, count should stay at 1";
	EXPECT_TRUE( m_tracyClient.GetZones().empty() );
}

TEST_F( CcpTelemetryTest, TelemetryZoneCaptureMaskBitConstructor )
{
	// Register a new component CaptureMask
	const uint64_t componentMaskBit = CcpRegisterCaptureMask( "TelemetryZoneCaptureMaskBitConstructor", CcpColor::Orange );
	EXPECT_TRUE( IsSCaptureMaskSingleBit( componentMaskBit ) );

	// Test where componentMaskBit is in the Active list
	CcpSetActiveCaptureMask( componentMaskBit );
	{
		TelemetryZone activeZone( CaptureMaskBit, componentMaskBit, "Active_TelemetryZoneCaptureMaskBitConstructor", __FILE__, __LINE__ );
		TickTelemetry( [this] { return m_tracyClient.GetZoneBeginCount() == 1; } );
		EXPECT_EQ( 1, m_tracyClient.GetZoneBeginCount() );
		const auto zones = m_tracyClient.GetZones();
		EXPECT_EQ( 1, zones.size() );
		EXPECT_EQ( "Active_TelemetryZoneCaptureMaskBitConstructor", zones.front().function );
		EXPECT_EQ( static_cast<uint32_t>( CcpColor::Orange ), zones.front().color )	<< "Color should come from the registered CaptureMask entry";
	}
	TickTelemetry( [this] { return m_tracyClient.GetZoneEndCount() == 1; } );
	EXPECT_EQ( 1, m_tracyClient.GetZoneEndCount() );
	EXPECT_TRUE( m_tracyClient.GetZones().empty() );

	// Test where componentMaskBit is NOT in the Active list - overwrite the Active list with only "general"
	CcpSetActiveCaptureMask( TMCM_GENERAL );
	EXPECT_EQ( 0, CcpGetActiveCaptureMask() & componentMaskBit );
	{
		TelemetryZone inactiveZone( CaptureMaskBit, componentMaskBit, "Inactive_TelemetryZoneCaptureMaskBitConstructor", __FILE__, __LINE__ );
		TickTelemetry( [this] { return m_tracyClient.GetZoneBeginCount() == 1; } );
		EXPECT_EQ( 1, m_tracyClient.GetZoneBeginCount() ) << "Because TMCM_GENERAL is now no longer in the ACTIVE CaptureMask list, we should not have emitted an event for it";
		EXPECT_EQ( 0, m_tracyClient.GetZones().size() ) << "Zone list should therefore be empty";
	}
	TickTelemetry( nullptr, std::chrono::milliseconds( 100 ) );
	EXPECT_EQ( 1, m_tracyClient.GetZoneBeginCount() ) << "Inactive CaptureMaskBit zone must not emit ZoneBegin";
	EXPECT_EQ( 1, m_tracyClient.GetZoneEndCount() ) << "Inactive CaptureMaskBit zone must not emit ZoneEnd";
	EXPECT_TRUE( m_tracyClient.GetZones().empty() );
}

TEST_F( CcpTelemetryTest, CcpTelemetryEnterZoneCaptureMaskBit )
{
	// Register a new component CaptureMask
	const uint64_t componentMaskBit = CcpRegisterCaptureMask( "CcpTelemetryEnterZoneCaptureMaskBit", CcpColor::LimeGreen );
	EXPECT_TRUE( IsSCaptureMaskSingleBit( componentMaskBit ) );

	// Test where componentMaskBit is in the Active list
	CcpSetActiveCaptureMask( componentMaskBit );
	static int activeKey = 8001;
	CcpTelemetryEnterZone( &activeKey, componentMaskBit, "Active_CcpTelemetryEnterZoneCaptureMaskBit", __FILE__, __LINE__ );
	TickTelemetry( [this] { return m_tracyClient.GetZoneBeginCount() == 1; } );
	EXPECT_EQ( 1, m_tracyClient.GetZoneBeginCount() );
	const auto zones = m_tracyClient.GetZones();
	EXPECT_EQ( 1, zones.size() );
	EXPECT_EQ( "Active_CcpTelemetryEnterZoneCaptureMaskBit", zones.front().function );
	EXPECT_EQ( static_cast<uint32_t>( CcpColor::LimeGreen ), zones.front().color );
	EXPECT_EQ( 0, m_tracyClient.GetZoneEndCount() );

	CcpTelemetryLeaveZone( &activeKey );
	TickTelemetry( [this] { return m_tracyClient.GetZoneEndCount() == 1; } );
	EXPECT_EQ( 1, m_tracyClient.GetZoneEndCount() );

	// Test where componentMaskBit is NOT in the Active list - overwrite the Active list with only "general"
	CcpSetActiveCaptureMask( TMCM_GENERAL );
	EXPECT_EQ( 0, CcpGetActiveCaptureMask() & componentMaskBit );
	static int inactiveKey = 8002;
	CcpTelemetryEnterZone( &inactiveKey, componentMaskBit, "Inactive_CcpTelemetryEnterZoneCaptureMaskBit", __FILE__, __LINE__ );
	TickTelemetry( nullptr, std::chrono::milliseconds( 100 ) );
	EXPECT_EQ( 1, m_tracyClient.GetZoneBeginCount() ) << "Inactive CaptureMaskBit enter-zone must not emit ZoneBegin";
	EXPECT_EQ( 1, m_tracyClient.GetZoneEndCount() ) << "EndZone count should still be 1";
	EXPECT_TRUE( m_tracyClient.GetZones().empty() );

	CcpTelemetryLeaveZone( &inactiveKey );
	TickTelemetry( nullptr, std::chrono::milliseconds( 100 ) );
	EXPECT_EQ( 1, m_tracyClient.GetZoneEndCount() ) << "Inactive CaptureMaskBit enter-zone must not emit ZoneEnd";
}


// ---------------------------------------------------------------------------
// CcpMutex / CcpAutoMutex
// ---------------------------------------------------------------------------

TEST_F( CcpTelemetryTest, CcpMutexAnnounceAndTerminate )
{
	TracyTestClient::LockInfo lockInfo;

	// Scope the CcpMutex so we can see what happens on destruction.
	{
		std::string lockName = "CcpTelemetryTest-CcpMutexAnnounceAndTerminate";
		CcpMutex mutex( "CcpTelemetryTest", "CcpMutexAnnounceAndTerminate" );
		// Because `CcpMutex` lazily announces itself to tracy, we use `CcpAutoMutex` to automatically acquire and release the mutex.
		CcpAutoMutex autoMutex( mutex );

		// The custom name arrives almost immediately, but the source location
		// resolves through extra server-query round trips; wait for both.
		TickTelemetry( [&] { return TryGetActiveLockNamed( lockName, lockInfo ) && !lockInfo.source.empty(); } );
		EXPECT_TRUE( TryGetActiveLockNamed( lockName, lockInfo ) );
		EXPECT_FALSE( lockInfo.terminated );
		// The owner and name passed to CcpMutex arrive combined as the custom lock name.
		EXPECT_EQ( lockName, lockInfo.name );
		EXPECT_TRUE( lockInfo.waitingThreads.empty() );
		EXPECT_EQ( 1, lockInfo.waitCount );
		EXPECT_EQ( 1, lockInfo.obtainCount );
		EXPECT_EQ( 0, lockInfo.releaseCount );
	}

	// Destroying the mutex terminates its lock.
	const uint32_t lockId = lockInfo.id;
	TickTelemetry( [&] { return TryGetLockById( lockId, lockInfo ) && lockInfo.terminated; } );
	EXPECT_EQ( 1, lockInfo.releaseCount );
	EXPECT_TRUE( lockInfo.terminated );
}

TEST_F( CcpTelemetryTest, CcpMutexAcquireAndRelease )
{
	std::string lockName = "CcpTelemetryTest-CcpMutexAcquireAndRelease";
	CcpMutex mutex( "CcpTelemetryTest", "CcpMutexAcquireAndRelease" );

	TracyTestClient::LockInfo lockInfo;
	mutex.Acquire();
	TickTelemetry( [&] { return TryGetActiveLockNamed( lockName, lockInfo ) && lockInfo.obtainCount == 1; } );
	EXPECT_TRUE( TryGetActiveLockNamed( lockName, lockInfo ) );
	EXPECT_EQ( 1, lockInfo.waitCount );
	EXPECT_EQ( 1, lockInfo.obtainCount );
	EXPECT_EQ( 0, lockInfo.releaseCount );
	EXPECT_TRUE( lockInfo.waitingThreads.empty() );

	const uint32_t lockId = lockInfo.id;
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

	TickTelemetry( [&] { return TryGetActiveLockNamed( lockName, lockInfo ) && lockInfo.holderThread != 0; } );
	const uint32_t lockId = lockInfo.id;
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

TEST_F( CcpTelemetryTest, MultipleCcpMutexesAnnounceDistinctLocks )
{
	CcpMutex firstMutex( "CcpTelemetryTest", "MultiTestFirstMutex" );
	CcpMutex secondMutex( "CcpTelemetryTest", "MultiTestSecondMutex" );
	std::string firstLockName = "CcpTelemetryTest-MultiTestFirstMutex";
	std::string secondLockName = "CcpTelemetryTest-MultiTestSecondMutex";

	firstMutex.Acquire();
	secondMutex.Acquire();

	TracyTestClient::LockInfo firstLock;
	TracyTestClient::LockInfo secondLock;
	TickTelemetry( [&] {
		return TryGetActiveLockNamed( firstLockName, firstLock ) &&
			TryGetActiveLockNamed( secondLockName, secondLock );
	} );
	EXPECT_TRUE( TryGetActiveLockNamed( firstLockName, firstLock ) );
	EXPECT_TRUE( TryGetActiveLockNamed( secondLockName, secondLock ) );
	EXPECT_NE( firstLock.id, secondLock.id );

	secondMutex.Release();
	firstMutex.Release();
}

// ---------------------------------------------------------------------------
// CcpSpinLock / CcpAutoSpinLock
// ---------------------------------------------------------------------------

TEST_F( CcpTelemetryTest, CcpSpinLockAnnounceAndTerminate )
{
	TracyTestClient::LockInfo lockInfo;
	const std::string lockName = "CcpSpinLockAnnounceAndTerminate";

	{
		CcpSpinLock spinLock( lockName.c_str() );
		CcpAutoSpinLock autoSpinLock( spinLock );

		TickTelemetry( [&] { return TryGetActiveLockNamed( lockName, lockInfo ); } );
		EXPECT_TRUE( TryGetActiveLockNamed( lockName, lockInfo ) );
		EXPECT_FALSE( lockInfo.terminated );
		EXPECT_EQ( lockName, lockInfo.name );
		EXPECT_TRUE( lockInfo.waitingThreads.empty() );
		EXPECT_EQ( 1, lockInfo.waitCount );
		EXPECT_EQ( 1, lockInfo.obtainCount );
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

	spinLock.Acquire();
	TickTelemetry( [&] { return TryGetActiveLockNamed( lockName, lockInfo ); } );
	EXPECT_TRUE( TryGetActiveLockNamed( lockName, lockInfo ) );
	const uint32_t lockId = lockInfo.id;
	EXPECT_EQ( 1, lockInfo.waitCount );
	EXPECT_EQ( 1, lockInfo.obtainCount );
	EXPECT_EQ( 0, lockInfo.releaseCount );

	spinLock.Release();
	TickTelemetry( [&] { return TryGetLockById( lockId, lockInfo ) && lockInfo.releaseCount == 1; } );
	EXPECT_EQ( 1, lockInfo.obtainCount );
	EXPECT_EQ( 1, lockInfo.releaseCount );
}

// ---------------------------------------------------------------------------
// CcpSemaphore
// ---------------------------------------------------------------------------

TEST_F( CcpTelemetryTest, CcpSemaphoreAnnounceAndTerminate )
{
	TracyTestClient::LockInfo lockInfo;
	const std::string lockName = "CcpSemaphoreAnnounceAndTerminate";
	{
		CcpSemaphore semaphore( lockName.c_str() );

		std::thread waiter( [&semaphore] { semaphore.Wait(); } );
		TickTelemetry( [&] { return TryGetActiveLockNamed( lockName, lockInfo ) && lockInfo.name == lockName; } );
		EXPECT_TRUE( TryGetActiveLockNamed( lockName, lockInfo ) ) << "lockName: " << lockName << " lockInfo.name: " << lockInfo.name;
		EXPECT_FALSE( lockInfo.terminated );
		EXPECT_EQ( lockName, lockInfo.name );
		EXPECT_EQ( 1, lockInfo.waitCount );
		EXPECT_EQ( 0, lockInfo.obtainCount );
		EXPECT_EQ( 0, lockInfo.releaseCount );

		std::thread signaler( [&semaphore] { semaphore.Signal(); } );

		waiter.join();
		signaler.join();

		TickTelemetry( [&] { return TryGetLockById( lockInfo.id, lockInfo ) && lockInfo.obtainCount == 1; } );
		EXPECT_EQ( 1, lockInfo.obtainCount );
	}

	TickTelemetry( [&] { return TryGetLockById( lockInfo.id, lockInfo ) && lockInfo.terminated; } );
	EXPECT_TRUE( lockInfo.terminated );
}

TEST_F( CcpTelemetryTest, CcpSemaphoreTimedWaitTimesOut )
{
	const std::string lockName = "CcpSemaphoreTimedWaitTimesOut";
	CcpSemaphore semaphore( lockName.c_str(), 0, 1 );

	TracyTestClient::LockInfo lockInfo;

	// No signal beforehand — TimedWait should time out and report a wait without an obtain.
	EXPECT_FALSE( semaphore.TimedWait( 10 ) );
	TickTelemetry( [&] { return TryGetActiveLockNamed( lockName, lockInfo ) && lockInfo.waitCount == 1 && lockInfo.obtainCount == 1; } );
	EXPECT_TRUE( TryGetActiveLockNamed( lockName, lockInfo ) ) << "lockName: " << lockName << " lockInfo.name: " << lockInfo.name;
	EXPECT_EQ( 1, lockInfo.waitCount );
	EXPECT_EQ( 1, lockInfo.obtainCount );
	EXPECT_EQ( 0, lockInfo.releaseCount );
}

// ---------------------------------------------------------------------------
// CaptureMask tests:
// ---------------------------------------------------------------------------

TEST_F( CcpTelemetryTest, CaptureMaskRegisterReturnsNonDefaultBit )
{
	const uint64_t maskBit = CcpRegisterCaptureMask( "CaptureMaskRegisterReturnsNonDefaultBit" );
	EXPECT_TRUE( IsSCaptureMaskSingleBit( maskBit ) );
	// TMCM_GENERAL and TMCM_CPP are registered as the default CaptureMasks,
	// so their maskBits must never be handed out to other components.
	EXPECT_EQ( 0, maskBit & ( TMCM_GENERAL | TMCM_CPP ) );
}

TEST_F( CcpTelemetryTest, CaptureMaskRegisterIsCaseInsensitive )
{
	const uint64_t firstBit = CcpRegisterCaptureMask( "CaptureMaskRegisterIsCaseInsensitive" );
	const uint64_t secondBit = CcpRegisterCaptureMask( "capturemaskregisteriscaseinsensitive" );
	EXPECT_TRUE( IsSCaptureMaskSingleBit( firstBit ) );
	EXPECT_EQ( firstBit, secondBit );

	CcpCaptureMaskInfo info;
	TryGetCaptureMaskNamed( "CAPTUREmaskREGISTERisCASEinsensitive", info );
	EXPECT_EQ( firstBit, info.maskBit );
}

TEST_F( CcpTelemetryTest, CaptureMaskRegisterDistinctBitPerName )
{
	const uint64_t maskBitA = CcpRegisterCaptureMask( "CaptureMaskRegisterDistinctBitPerName_A" );
	const uint64_t maskBitB = CcpRegisterCaptureMask( "CaptureMaskRegisterDistinctBitPerName_B", CcpColor::Tomato );
	EXPECT_TRUE( IsSCaptureMaskSingleBit( maskBitA ) );
	EXPECT_TRUE( IsSCaptureMaskSingleBit( maskBitB ) );
	EXPECT_NE( maskBitA, maskBitB );
}

TEST_F( CcpTelemetryTest, CaptureMaskRegisterRejectEmpty )
{
	EXPECT_EQ( 0, CcpRegisterCaptureMask( "" ) );
}

TEST_F( CcpTelemetryTest, CaptureMaskDefaultsAreRegistered )
{
	// The default CaptureMasks must be available from the start.
	CcpCaptureMaskInfo info;
	EXPECT_TRUE( TryGetCaptureMaskNamed( "general", info ) );
	EXPECT_EQ( static_cast<uint64_t>( TMCM_GENERAL ), info.maskBit );
	EXPECT_EQ( CcpColor::SteelBlue, info.color );  // CcpColor::SteelBlue is the default assigned color for TMCM_GENERAL
	EXPECT_TRUE( TryGetCaptureMaskNamed( "cpp", info ) );
	EXPECT_EQ( static_cast<uint64_t>( TMCM_CPP ), info.maskBit );

	// Registering a CaptureMask for the same name should result in the same maskBit returned,
	// but a color change should be allowed.
	const uint64_t reRegisterMaskBit = CcpRegisterCaptureMask( "GENERAL", CcpColor::OrangeRed );
	TryGetCaptureMaskNamed( "general", info );
	EXPECT_EQ( static_cast<uint64_t>( TMCM_GENERAL ), reRegisterMaskBit );
	EXPECT_EQ( static_cast<uint64_t>( TMCM_GENERAL ), info.maskBit );
	EXPECT_EQ( CcpColor::OrangeRed, info.color );
}

TEST_F( CcpTelemetryTest, CaptureMaskAutoColorIsPickedFromCandidateList )
{
	CcpRegisterCaptureMask( "CaptureMaskAutoColorIsPickedFromCandidateList" );

	CcpCaptureMaskInfo info;
	EXPECT_TRUE( TryGetCaptureMaskNamed( "CaptureMaskAutoColorIsPickedFromCandidateList", info ) );
	const auto begin = std::begin( ColorUtil::s_awailableNamedColors );
	const auto end = std::end( ColorUtil::s_awailableNamedColors );
	EXPECT_NE( end, std::find( begin, end, info.color ) );
}

TEST_F( CcpTelemetryTest, SetActiveCaptureMaskByBits )
{
	uint64_t newBits = 0;
	newBits |= CcpRegisterCaptureMask( "SetActiveCaptureMaskByBits_A" );
	newBits |= CcpRegisterCaptureMask( "SetActiveCaptureMaskByBits_B" );

	uint64_t registeredCaptureMaskBits = 0;
	auto registeredCaptureMasks = CcpGetRegisteredCaptureMasks();
	for ( auto captureMask : registeredCaptureMasks )
	{
		EXPECT_TRUE( IsSCaptureMaskSingleBit( captureMask.maskBit ) ) << "Each entry should only contain one bit set";
		registeredCaptureMaskBits |= captureMask.maskBit;
	}
	EXPECT_TRUE( (newBits & registeredCaptureMaskBits) == newBits ) << "All bits in combined newBits should be present in combined registered CaptureMask bits ";
	EXPECT_FALSE( (newBits & registeredCaptureMaskBits) == registeredCaptureMaskBits ) << "Registered bits should contain the additional 'general' and 'cpp' bits";

	// newBits excludes the default "general" and "ccp" that are present in registeredCaptureMaskBits
	CcpSetActiveCaptureMask( newBits );
	EXPECT_EQ( newBits, CcpGetActiveCaptureMask() );

	// Overwrite the active CaptureMask
	CcpSetActiveCaptureMask( TMCM_GENERAL );
	EXPECT_EQ( static_cast<uint64_t>( TMCM_GENERAL ), CcpGetActiveCaptureMask() );
}

TEST_F( CcpTelemetryTest, SetActiveCaptureMaskByNames )
{
	const std::string registeredName = "SetActiveCaptureMaskByNames_RegisteredName";
	const std::string newName = "SetActiveCaptureMaskByNames_NewName";
	const std::string pendingName = "SetActiveCaptureMaskByNames_PendingName";
	const std::vector<std::string> activeMaskList = { registeredName, pendingName };

	// Only register one name
	const uint64_t registeredBit = CcpRegisterCaptureMask( registeredName );
	EXPECT_TRUE( IsSCaptureMaskSingleBit( registeredBit ) );
	CcpSetActiveCaptureMask( activeMaskList );
	EXPECT_EQ( registeredBit, CcpGetActiveCaptureMask() ) << "ActiveCaptureMask should only contain the registered bit, not the pending one";

	// Register the new name
	const uint64_t newBit = CcpRegisterCaptureMask( newName );
	EXPECT_TRUE( IsSCaptureMaskSingleBit( newBit ) );
	EXPECT_EQ( registeredBit, CcpGetActiveCaptureMask() ) << "ActiveCaptureMask should still only contain the registered bit, not the new or pending one";

	// Now add the pending one
	const uint64_t pendingBit = CcpRegisterCaptureMask( pendingName );
	EXPECT_TRUE( IsSCaptureMaskSingleBit( pendingBit ) );
	const uint64_t activeBits = registeredBit | pendingBit;
	EXPECT_EQ( activeBits, CcpGetActiveCaptureMask() ) << "ActiveCaptureMask should now contain both the registered and pending bits";

	// Deal with the special "all" case
	CcpSetActiveCaptureMask( std::vector<std::string>{ "all" } );
	EXPECT_EQ( UINT64_MAX, CcpGetActiveCaptureMask() ) << "ActiveCaptureMask should be all bits set when using the special 'all' alias";
	CcpRegisterCaptureMask( "SetActiveCaptureMaskByNames_AfterAll" );
	EXPECT_EQ( UINT64_MAX, CcpGetActiveCaptureMask() );

	// Overwrite the active CaptureMask
	CcpSetActiveCaptureMask( TMCM_CPP );
	EXPECT_EQ( static_cast<uint64_t>( TMCM_CPP ), CcpGetActiveCaptureMask() );
	EXPECT_TRUE( IsSCaptureMaskSingleBit( CcpGetActiveCaptureMask() ) );
}

