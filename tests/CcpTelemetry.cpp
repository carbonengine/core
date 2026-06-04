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
		StartTelemetry();
		ConnectTelemetry();
	}

	void TearDown() override
	{
		// Do NOT explicit call m_tracyClient.Disconnect(), unless we change actual implementation to call tracy::ShutdownProfiler().
		StopTelemetry();
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

	void StartTelemetry(std::string appName = "Telemetry Tests",
	                    std::chrono::milliseconds duration = std::chrono::milliseconds::zero(),
	                    bool trackMemory = false)
	{
		CcpTelemetryConfig conf{appName};
		conf.captureDuration = duration;
		conf.trackMemoryAllocations = trackMemory;
		CcpStartTelemetry(conf);

		// Tick at least once or until the profiler's listen socket is up.
		do
		{
			TickTelemetry();
		}
		while (!TracyIsStarted);
	}

	void StopTelemetry()
	{
		CcpStopTelemetry();
		// Tick at least once or until we're stopped
		do
		{
			TickTelemetry();
		}
		while (!CcpTelemetryIsStopped());
	}

	void ConnectTelemetry()
	{
		// Connect on a background thread so this thread can keep ticking Tracy.
		// The handshake requires both sides to run concurrently: Tracy's worker
		// sends data and may block on Send() until the client reads it.
		auto connectFuture = std::async( std::launch::async, [this] {
			return m_tracyClient.Connect();
		} );

		// Tick until CcpTelemetry recognises the connection and enters Started state.
		while( !CcpTelemetryIsConnected() )
		{
			TickTelemetry();
		}
		ASSERT_TRUE( connectFuture.get() ) << "Could not connect to Tracy profiler";
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

	// Switching to a new name
	CcpTelemetrySetActiveFiber( expectedFiberName2 );
	const auto& observedFiberName2 = CcpTelemetryGetActiveFiber();
	EXPECT_EQ( observedFiberName2, expectedFiberName2 );

	// Switching back to the original name
	CcpTelemetrySetActiveFiber( expectedFiberName1 );
	const auto& observedFiberName3 = CcpTelemetryGetActiveFiber();
	EXPECT_EQ( observedFiberName1.c_str(), observedFiberName3.c_str() );

	// Switching to the "Root name" (no name)
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
	TickTelemetry();
	EXPECT_EQ( 1, m_tracyClient.GetZoneBeginCount() );
	EXPECT_TRUE( ZoneExists( zoneName ) );

	CcpTelemetryLeaveZone( &key );
	TickTelemetry();
	EXPECT_EQ( 1, m_tracyClient.GetZoneEndCount() );
	EXPECT_FALSE( ZoneExists( zoneName ) );
}

TEST_F( CcpTelemetryTest, StackedZones )
{
	// A stacked zone is a zone that has the same key as a previously created zone.
	static int key = 4711;
	CcpTelemetryEnterZone( &key, "TestZone", __FILE__, __LINE__ );
	CcpTelemetryEnterZone( &key, "TestZone2", __FILE__, __LINE__ );
	TickTelemetry();
	EXPECT_EQ( 2, m_tracyClient.GetZones().size() );
	EXPECT_TRUE( ZoneExists( "TestZone" ) );
	EXPECT_TRUE( ZoneExists( "TestZone2" ) );

	CcpTelemetryLeaveZone( &key );
	TickTelemetry();
	EXPECT_EQ( 1, m_tracyClient.GetZones().size() );
	EXPECT_TRUE( ZoneExists( "TestZone" ) );
	EXPECT_FALSE( ZoneExists( "TestZone2" ) ) << "TestZone2 should be gone";

	CcpTelemetryLeaveZone( &key );
	TickTelemetry();
	EXPECT_TRUE( m_tracyClient.GetZones().empty() );
}

TEST_F( CcpTelemetryTest, ReStartAfterStop )
{
	static int key1 = 1001;
	const std::string zoneName1{ "FirstZone" };
	CcpTelemetryEnterZone( &key1, zoneName1.c_str(), __FILE__, __LINE__ );
	TickTelemetry();
	EXPECT_TRUE( ZoneExists( zoneName1 ) );
	EXPECT_EQ( 1, m_tracyClient.GetZones().size() );
	EXPECT_EQ( 1, m_tracyClient.GetZoneBeginCount() );
	EXPECT_EQ( 0, m_tracyClient.GetZoneEndCount() );

	CcpTelemetryLeaveZone( &key1 );
	TickTelemetry();
	EXPECT_TRUE( m_tracyClient.GetZones().empty() );
	EXPECT_EQ( 1, m_tracyClient.GetZoneEndCount() );

	// Now simulate "Stop and Start Telemetry" operation
	StopTelemetry();
	EXPECT_TRUE( CcpTelemetryIsStopped() );
	StartTelemetry( "Telemetry Tests - 2nd Start" );
	EXPECT_TRUE( CcpTelemetryIsStarted() );

	// Emit a new Zone, on the 2nd Start and validate
	static int key2 = 1002;
	const std::string zoneName2{ "SecondZone" };
	CcpTelemetryEnterZone( &key2, zoneName2.c_str(), __FILE__, __LINE__ );
	TickTelemetry();
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

