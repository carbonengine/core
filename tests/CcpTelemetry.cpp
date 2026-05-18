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
		fprintf( stderr, "CcpTelemetryTest::SetUp() - Begin\n" ); fflush( stderr );  // TODO: Debug info, remove this
		CcpTelemetryConfig conf{ "Telemetry Tests" };
		EXPECT_EQ( conf.captureDuration, std::chrono::milliseconds::zero() );
		CcpStartTelemetry( conf );

		// Tick until the profiler's listen socket is up.
		while( !TracyIsStarted )
		{
			fprintf( stderr, "CcpTelemetryTest::SetUp() - while( !TracyIsStarted ) - TickTelemetry()\n" ); fflush( stderr );  // TODO: Debug info, remove this
			TickTelemetry();
		}

		// Connect on a background thread so this thread can keep ticking Tracy.
		// The handshake requires both sides to run concurrently: Tracy's worker
		// sends data and may block on Send() until the client reads it.
		auto connectFuture = std::async( std::launch::async, [this] {
			return m_tracyClient.Connect();
		} );

		// Tick until CcpTelemetry recognises the connection and enters Started state.
		while( !CcpTelemetryIsConnected() )
		{
			fprintf( stderr, "CcpTelemetryTest::SetUp() - while( !CcpTelemetryIsConnected ) - TickTelemetry()\n" ); fflush( stderr );  // TODO: Debug info, remove this
			TickTelemetry();
		}

		ASSERT_TRUE( connectFuture.get() ) << "Could not connect to Tracy profiler";
		fprintf( stderr, "CcpTelemetryTest::SetUp() - End\n" ); fflush( stderr );  // TODO: Debug info, remove this
	}

	void TearDown() override
	{
		fprintf( stderr, "CcpTelemetryTest::TearDown() - Begin\n" ); fflush( stderr );  // TODO: Debug info, remove this
		// m_tracyClient.Disconnect();  // TODO: Removed the explicit call to Disconnect.
		CcpStopTelemetry();
		fprintf( stderr, "CcpTelemetryTest::TearDown() - End\n" ); fflush( stderr );  // TODO: Debug info, remove this
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

TEST_F( CcpTelemetryTest, StartConnectStopDisconnectProfiling )
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
	EXPECT_EQ( 1, m_tracyClient.GetZoneEndCount() );
	EXPECT_TRUE( CcpTelemetryIsConnected() );
	EXPECT_TRUE( m_tracyClient.IsConnected() );

	// Now simulate "Stop Telemetry" operation. NOTE we're not disconnecting the TracyTestClient, only signaling the profiler to Stop
	CcpStopTelemetry();
	EXPECT_TRUE( m_tracyClient.IsConnected() ) << "Connection should still be true at this point because the TracyTestClient hasn't been disconnected";
	EXPECT_TRUE( TracyIsStarted ) << "Until we Tick TWICE, this should still be true (first for StopRequested, then for Stopped)";
	EXPECT_TRUE( TracyNoop ) << "Tracy should have a profiler available";
	EXPECT_TRUE( TracyIsConnected ) << "TracyTestClient should still be connected at this point";
	EXPECT_FALSE( CcpTelemetryIsStarted() ) << "Internal profiler state should no longer be: Started, it should be StopRequested";

	TickTelemetry(); // This should process the "StopRequested" state.
	EXPECT_FALSE( m_tracyClient.IsConnected() ) << "Now that the TracyTestClient is handling the 'kQueueTerminate' and setting m_shutdown=true, this should be FALSE";  //"Connection should still be true at this point because the TracyTestClient hasn't been disconnected";
	EXPECT_TRUE( TracyIsStarted ) << "Until we Tick ONE MORE TIME, this should still be true (this is for StopRequested, next Tick is for Stopped)";
	EXPECT_TRUE( TracyNoop ) << "Tracy should have a profiler available";
	EXPECT_TRUE( TracyIsConnected ) << "TracyTestClient should still be connected at this point";
	EXPECT_FALSE( CcpTelemetryIsStarted() ) << "Internal profiler state should no longer be: StopRequested, it should have transitioned to Stopped";

	// TODO: Figure out why TracyIsStarted only becomes FALSE if m_tracyClient.Disconnect() is called, but NOT just because we called tracy::ShutdownProfiler()

	m_tracyClient.Disconnect(); // Disconnect the TracyTestClient, meaning TracyNoop should return FALSE (and TracyIsConnected a segfault because tracy::GetProfiler() is invalid)

	fprintf( stderr, "CC0\n" ); fflush( stderr );  // TODO: Debug info, remove this
	TickTelemetry(); // This should process the "Stopped" state (2nd part of calling Stop Telemetry)
	fprintf( stderr, "CcpTelemetryTest::ReconnectAfterStop() - After TickTelemetry() #4\n" ); fflush( stderr );  // TODO: Debug info, remove this
	EXPECT_FALSE( m_tracyClient.IsConnected() ) << "By disconnecting the TracyTestClient, this should return false";
	fprintf( stderr, "CC1\n" ); fflush( stderr );  // TODO: Debug info, remove this
	EXPECT_FALSE( TracyNoop ) << "TracyNoop is returning false here, NOT because we called tracy::ShutdownProfiler(), but because we called m_tracyClient.Disconnect(). This is a bit surprising. <<<====";
	fprintf( stderr, "CC2\n" ); fflush( stderr );  // TODO: Debug info, remove this
	EXPECT_FALSE( TracyIsStarted ) << "Tracy should have fully stopped by now WHICH SHOULD have happened because of a call to tracy::ShutdownProfiler(), BUT is probably because of m_tracyClient.Disconnect() <<<====";
	fprintf( stderr, "CC3\n" ); fflush( stderr );  // TODO: Debug info, remove this
	//EXPECT_FALSE( TracyIsConnected ) << "Tracy should NOT be connected, because we've called m_tracyClient.Disconnect()";
	fprintf( stderr, "CC4 - We can't call TracyIsConnected here, because it will end up with a SEH exception\n" ); fflush( stderr );  // TODO: Debug info, remove this
	EXPECT_FALSE( CcpTelemetryIsStarted() ) << "Internal profiler state should no longer be: Started";
	fprintf( stderr, "CC5\n" ); fflush( stderr );  // TODO: Debug info, remove this

}

TEST_F( CcpTelemetryTest, StartConnectStopProfiling )
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
	EXPECT_EQ( 1, m_tracyClient.GetZoneEndCount() );
	EXPECT_TRUE( CcpTelemetryIsConnected() );
	EXPECT_TRUE( m_tracyClient.IsConnected() );

	// Now simulate "Stop Telemetry" operation. NOTE we're not disconnecting the TracyTestClient, only signaling the profiler to Stop
	CcpStopTelemetry();
	EXPECT_TRUE( m_tracyClient.IsConnected() ) << "Connection should still be true at this point because the TracyTestClient hasn't been disconnected";
	EXPECT_TRUE( TracyIsStarted ) << "Until we Tick TWICE, this should still be true (first for StopRequested, then for Stopped)";
	EXPECT_TRUE( TracyNoop ) << "Tracy should have a profiler available";
	EXPECT_TRUE( TracyIsConnected ) << "TracyTestClient should still be connected at this point";
	EXPECT_FALSE( CcpTelemetryIsStarted() ) << "Internal profiler state should no longer be: Started, it should be StopRequested";

	TickTelemetry(); // This should process the "StopRequested" state.
	EXPECT_FALSE( m_tracyClient.IsConnected() ) << "Now that the TracyTestClient is handling the 'kQueueTerminate' and setting m_shutdown=true, this should be FALSE";  //"Connection should still be true at this point because the TracyTestClient hasn't been disconnected";
	EXPECT_TRUE( TracyIsStarted ) << "Until we Tick ONE MORE TIME, this should still be true (this is for StopRequested, next Tick is for Stopped)";
	EXPECT_TRUE( TracyNoop ) << "Tracy should have a profiler available";
	EXPECT_TRUE( TracyIsConnected ) << "TracyTestClient should still be connected at this point";
	EXPECT_FALSE( CcpTelemetryIsStarted() ) << "Internal profiler state should no longer be: StopRequested, it should have transitioned to Stopped";

	// TODO: Figure out why TracyIsStarted only becomes FALSE if m_tracyClient.Disconnect() is called, but NOT just because we called tracy::ShutdownProfiler()

	// m_tracyClient.Disconnect(); // NOT calling m_tracyClient.Disconnect() here on purpose in the TracyTestClient.
	// We should be able to Stop Profiling, by calling tracy::ShutdownProfiler(), without having to disconnect the Tracy GUI or TracyTestClient first

	fprintf( stderr, "CC0\n" ); fflush( stderr );  // TODO: Debug info, remove this
	TickTelemetry(std::chrono::milliseconds(1000)); // This should process the "Stopped" state (2nd part of calling Stop Telemetry)
	TickTelemetry(std::chrono::milliseconds(1000)); // Giving it more time to flush the queue.
	fprintf( stderr, "CcpTelemetryTest::ReconnectAfterStop() - After TickTelemetry() #4\n" ); fflush( stderr );  // TODO: Debug info, remove this
	EXPECT_FALSE( m_tracyClient.IsConnected() ) << "By disconnecting the TracyTestClient (because of the 'kQueueTerminate' handling setting m_shutdown=true), this should return false";
	fprintf( stderr, "CC1\n" ); fflush( stderr );  // TODO: Debug info, remove this
	EXPECT_TRUE( TracyNoop ) << "For some reason, even though we've Stopped Telemetry, Tracy is still available, this is ODD.  <<<====";
	fprintf( stderr, "CC2\n" ); fflush( stderr );  // TODO: Debug info, remove this
	EXPECT_FALSE( TracyIsStarted ) << "I would have thought that this should be FALSE here, because we've called tracy::GetProfiler().RequestShutdown() -> which in turn should on the next tick process Stopped state and call ShutdownProfiler(). BUT this never becomes true [TracyIsStarted && tracy::GetProfiler().HasShutdownFinished()]  <<<=====";
	fprintf( stderr, "CC3\n" ); fflush( stderr );  // TODO: Debug info, remove this
	EXPECT_FALSE( TracyIsConnected ) << "Tracy should NOT be connected, because the TracyTestClient should have been disconnected because of kQueueTerminate";
	fprintf( stderr, "CC4\n" ); fflush( stderr );  // TODO: Debug info, remove this
	EXPECT_FALSE( CcpTelemetryIsStarted() ) << "Internal profiler state should no longer be: Started";
	fprintf( stderr, "CC5\n" ); fflush( stderr );  // TODO: Debug info, remove this
}

TEST_F( CcpTelemetryTest, StartConnectDisconnectProfiling )
{

}

TEST_F( CcpTelemetryTest, StartConnectDisconnectStopProfiling )
{

}

TEST_F( CcpTelemetryTest, ReconnectAfterStop )
{
	// Now simulate "Start Telemetry" operation again...

	// Enter and Leave a different zone...
}