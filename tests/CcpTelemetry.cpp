// Copyright © 2025 CCP ehf.

#include <gtest/gtest.h>

#include <future>

#include <CcpCore.h>

#include "TracyTestClient.h"

class CcpTelemetryTest : public ::testing::Test
{
protected:
	CcpTelemetryTest() = default;
	~CcpTelemetryTest() override = default;

	void SetUp() override
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

	void TearDown() override
	{
		m_tracyClient.Disconnect();
		CcpStopTelemetry();
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
