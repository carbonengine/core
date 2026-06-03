// Copyright © 2025 CCP ehf.

#include <gtest/gtest.h>

#include <future>

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
		SetUp(true);
	}

	void SetUp(bool doTestClientConnect)
	{
		CcpTelemetryConfig conf{ "Telemetry Tests" };
		EXPECT_EQ( conf.captureDuration, std::chrono::milliseconds::zero() );
		CcpStartTelemetry( conf );

		// Tick until the profiler's listen socket is up.
		while( !TracyIsStarted )
		{
			TickTelemetry();
		}

		// Connect on a background thread so this thread can keep ticking Tracy.
		// The handshake requires both sides to run concurrently: Tracy's worker
		// sends data and may block on Send() until the client reads it.
		auto connectFuture = doTestClientConnect
			? std::async( std::launch::async, [this] { return m_tracyClient.Connect(); } )
			: std::async( std::launch::deferred, [] { return true; } );

		// Tick until CcpTelemetry recognises the connection and enters Started state.
		while( !CcpTelemetryIsConnected() )
		{
			TickTelemetry();
		}

		ASSERT_TRUE( connectFuture.get() ) << "Could not connect to Tracy profiler";
	}

	void TearDown() override
	{
		// m_tracyClient.Disconnect();  // Remove explicit call to Disconnect() because current implementation does NOT call tracy::ShutdownProfiler().
		CcpStopTelemetry();
		::testing::Test::TearDown();
	}

	void TickTelemetry( std::chrono::milliseconds duration = std::chrono::milliseconds( 500 ) )
	{
		const auto deadline = std::chrono::steady_clock::now() + duration;
		while( std::chrono::steady_clock::now() < deadline )
		{
			CcpTelemetryTick();
			std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );
		}
	}

	const std::string expectedNoFiber;
	const std::string expectedFiberName{ "TestFiber" };
	const std::string expectedFiberName2{ "TestFiber" };

	TracyTestClient m_tracyClient;
};

TEST_F( CcpTelemetryTest, TestFiberSwitching )
{
	CcpTelemetrySetActiveFiber( expectedFiberName );
	const auto& observedFiberName1 = CcpTelemetryGetActiveFiber();
	EXPECT_EQ( observedFiberName1, expectedFiberName );
	CcpTelemetrySetActiveFiber( expectedFiberName2 );
	const auto& observedFiberName2 = CcpTelemetryGetActiveFiber();
	EXPECT_EQ( observedFiberName2, expectedFiberName2 );
	const auto& observedFiberName3 = CcpTelemetryGetActiveFiber();
	CcpTelemetrySetActiveFiber( expectedFiberName );
	EXPECT_EQ( observedFiberName1.c_str(), observedFiberName3.c_str() );
	CcpTelemetrySetActiveFiber( "" );
	EXPECT_EQ( CcpTelemetryGetActiveFiber(), expectedNoFiber );
}

TEST_F( CcpTelemetryTest, RemovingActiveFiberClearsIt )
{
	CcpTelemetrySetActiveFiber( expectedFiberName );
	CcpTelemetryRemoveFiber( expectedFiberName );
	EXPECT_EQ( CcpTelemetryGetActiveFiber(), expectedNoFiber );
}

TEST_F( CcpTelemetryTest, SimpleZoneTest )
{
	static int key = 4711;
	const std::string zoneName{ "TestZone" };
	EXPECT_TRUE( CcpTelemetryIsConnected() );
	CcpTelemetryEnterZone( &key, zoneName.c_str(), __FILE__, __LINE__ );

	// Tracy's worker sleeps up to 10 ms between queue flushes, so give it
	// time to process and send the zone event before asserting.
	TickTelemetry();

	EXPECT_EQ( 1, m_tracyClient.GetZoneBeginCount() );
	auto tracyZones = m_tracyClient.GetZones();
	// CcpTelemetryEnterZone passes the zone name as the Tracy "function" field
	// (via the 6-param ___tracy_alloc_srcloc), so match against both fields.
	auto pred = [&zoneName]( const TracyTestClient::ZoneInfo& elem ) -> bool {
		return elem.function == zoneName;
	};
	EXPECT_NE( tracyZones.end(), std::find_if( tracyZones.begin(), tracyZones.end(), pred ) );

	CcpTelemetryLeaveZone( &key );
	TickTelemetry();
	EXPECT_EQ( 1, m_tracyClient.GetZoneEndCount() );
	tracyZones = m_tracyClient.GetZones();
	EXPECT_EQ( tracyZones.end(), std::find_if( tracyZones.begin(), tracyZones.end(), pred ) );
}

TEST_F( CcpTelemetryTest, StackedZones )
{
	// A stacked zone is a zone that has the same key as a previously created zone.
	static int key = 4711;
	CcpTelemetryEnterZone( &key, "TestZone", __FILE__, __LINE__ );
	CcpTelemetryEnterZone( &key, "TestZone2", __FILE__, __LINE__ );
	TickTelemetry();
	auto tracyZones = m_tracyClient.GetZones();
	EXPECT_EQ( 2, tracyZones.size() );
	CcpTelemetryLeaveZone( &key );
	TickTelemetry();
	tracyZones = m_tracyClient.GetZones();
	EXPECT_EQ( 1, tracyZones.size() );
	CcpTelemetryLeaveZone( &key );
	TickTelemetry();
	EXPECT_TRUE( m_tracyClient.GetZones().empty() );
}

TEST_F( CcpTelemetryTest, ReStartAfterStop )
{
	// Setup takes care of connecting to the TracyTestClient
	EXPECT_TRUE( m_tracyClient.IsConnected() );

	static int key1 = 1001;
	const std::string zoneName1{ "FirstZone" };
	CcpTelemetryEnterZone( &key1, zoneName1.c_str(), __FILE__, __LINE__ );

	TickTelemetry();
	auto tracyZones = m_tracyClient.GetZones();
	auto pred = [&zoneName1]( const TracyTestClient::ZoneInfo& elem ) -> bool {
		return elem.function == zoneName1;
	};
	EXPECT_NE( tracyZones.end(), std::find_if( tracyZones.begin(), tracyZones.end(), pred ) );
	EXPECT_EQ( 1, tracyZones.size() );
	EXPECT_EQ( 1, m_tracyClient.GetZoneBeginCount() );
	EXPECT_EQ( 0, m_tracyClient.GetZoneEndCount() );

	CcpTelemetryLeaveZone( &key1 );

	TickTelemetry();
	EXPECT_TRUE( m_tracyClient.GetZones().empty() );
	EXPECT_EQ( 1, m_tracyClient.GetZoneEndCount() );
	EXPECT_TRUE( CcpTelemetryIsConnected() );
	EXPECT_TRUE( m_tracyClient.IsConnected() );

	// Now simulate "Stop Telemetry" operation and Tick until we're in "Stopped" state
	CcpStopTelemetry();
	TickTelemetry(); // This processes the "StopRequested" state.
	TickTelemetry(); // This processes the "Stopped" state.
	EXPECT_TRUE( m_tracyClient.IsConnected() ) << "Connection should still be true at this point because the TracyTestClient hasn't been disconnected";
	EXPECT_FALSE( CcpTelemetryIsStarted() ) << "Internal profiler state should have changed: Started->StopRequested->Stopped";

	// Simulate a new call to StartTelemetry
	SetUp( false );
	EXPECT_TRUE( m_tracyClient.IsConnected() ) << "Connection should still be true because the TracyTestClient hasn't never been disconnected";
	EXPECT_TRUE( CcpTelemetryIsStarted() ) << "Internal profiler state should have changed: Started->StopRequested->Stopped";

	// Emit a new Zone, on the 2nd Start and validate
	static int key2 = 1002;
	const std::string zoneName2{ "SecondZone" };
	CcpTelemetryEnterZone( &key2, zoneName2.c_str(), __FILE__, __LINE__ );

	TickTelemetry();
	auto tracyZones2ndStart = m_tracyClient.GetZones();
	auto pred2nd = [&zoneName2]( const TracyTestClient::ZoneInfo& elem ) -> bool {
		return elem.function == zoneName2;
	};
	EXPECT_NE( tracyZones2ndStart.end(), std::find_if( tracyZones2ndStart.begin(), tracyZones2ndStart.end(), pred2nd ) );
	EXPECT_EQ( 1, tracyZones2ndStart.size() );
	EXPECT_EQ( 2, m_tracyClient.GetZoneBeginCount() ) << "The total Begin Zone count should be 2, even after Stop/Start";
	EXPECT_EQ( 1, m_tracyClient.GetZoneEndCount() ) << "The total End Zone count should be 1 at this point, because of the FirstZone has ended";

	CcpTelemetryLeaveZone( &key2 );

	TickTelemetry();
	EXPECT_TRUE( m_tracyClient.GetZones().empty() );
	EXPECT_EQ( 2, m_tracyClient.GetZoneEndCount() ) << "The total End Zone count should be 2, FirstZone (before the Stop) and SecondZone from after the Stop/Start";;
	EXPECT_TRUE( CcpTelemetryIsConnected() );
	EXPECT_TRUE( m_tracyClient.IsConnected() );
}

