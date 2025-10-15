// Copyright © 2025 CCP ehf.

#include <gtest/gtest.h>

#include <CcpCore.h>

class CcpTelemetryTest : public ::testing::Test
{
protected:
	CcpTelemetryTest() = default;
	~CcpTelemetryTest() override = default;

	void SetUp() override {
		CcpTelemetryConfig conf{ "Telemetry Tests" };
		EXPECT_EQ( conf.captureDuration, std::chrono::milliseconds::zero() );
		CcpStartTelemetry( conf );
		while ( !TracyIsStarted )
		{
			CcpTelemetryTick();
			std::this_thread::yield();
		}
	}

	void TearDown() override {
		CcpStopTelemetry();
	}

	const std::string expectedNoFiber;
	const std::string expectedFiberName{"TestFiber"};
	const std::string expectedFiberName2{"TestFiber"};
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
