// Copyright © 2025 CCP ehf.

#include "gtest/gtest.h"
#include "CcpTime.h"
#include "CcpThread.h"


TEST( CcpTime, TimeDoesNotRunBackwards )
{
	uint64_t last = CcpGetTimestamp();

	for( int i = 0; i < 10; ++i )
	{
		uint64_t now = CcpGetTimestamp();
		EXPECT_LE(last, now);
		last = now;
	}
}

TEST( CcpTime, CanGetCurrentTime )
{
	ASSERT_NE( 0, TimeNow() );
}

TEST( CcpTime, TimeHasMoreThanMSResolution )
{
	auto start = TimeNow();
	CcpThreadSleep( 2 );
	ASSERT_NE( start, TimeNow() );
}

TEST( CcpTime, CanConvertTimeToDateTime )
{
	auto now = TimeNow();
	CcpDateTime dt = {};
	ASSERT_TRUE( TimeAsDateTime( dt, now ) );
	ASSERT_GT( dt.year, 2000 );
	ASSERT_GT( dt.month, 0 );
}

TEST( CcpTime, CanConvertDateTimeToTime )
{
	CcpDateTime dt = { 2020, 6, 0, 5 };
	CcpTime time = 0;
	ASSERT_TRUE( TimeFromDateTime( time, dt ) );
	ASSERT_GT( time, 0 );
}