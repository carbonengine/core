// Copyright © 2025 CCP ehf.

#include "gtest/gtest.h"
#include "CcpCore/include/CCPCore.h"

namespace
{

template <typename R>
uint32_t TimeCallInMs( R call )
{
	auto start = CcpGetTimestamp();
	call();
	auto duration = CcpGetTimestamp() - start;

	auto scale = CcpGetTimestampFrequency();
	return uint32_t( double( duration ) / scale * 1000. );
}

template<typename R>
uint32_t MakeThread( void* context )
{
	( *static_cast<R*>( context ) )();
	return 0;
}

}

TEST( CcpSemaphore, DefaultConstructorCreatesNonSignaledSemaphore )
{
	CcpSemaphore s;
	
	auto duration = TimeCallInMs( [&] { s.TimedWait( 1000 ); } );
	EXPECT_GT( duration, 500 );
	EXPECT_LT( duration, 1500 );
}

TEST( CcpSemaphore, NoWaitOnSignaledSemaphore )
{
	CcpSemaphore s;
	s.Signal();
	auto duration = TimeCallInMs( [&] { s.TimedWait( 1000 ); } );
	EXPECT_LT( duration, 100 );
}

TEST( CcpSemaphore, NonZeroInitialCountCreatesSignaledSemaphore )
{
	CcpSemaphore s( 2, 100 );

	auto duration = TimeCallInMs( [&] { s.TimedWait( 1000 ); } );
	EXPECT_LT( duration, 100 );
}

TEST( CcpSemaphore, WaitDecreasesSemaphoreCountByOne )
{
	CcpSemaphore s( 2, 100 );

	auto duration = TimeCallInMs( [&] { s.TimedWait( 1000 ); } );
	EXPECT_LT( duration, 100 );
	duration = TimeCallInMs( [&] { s.TimedWait( 1000 ); } );
	EXPECT_LT( duration, 100 );

	duration = TimeCallInMs( [&] { s.TimedWait( 1000 ); } );
	EXPECT_GT( duration, 900 );
}

TEST( CcpSemaphore, CanWaitOnSemaphore )
{
	CcpSemaphore s;

	auto thread = [&] {
		CcpThreadSleep( 300 );
		s.Signal();
	};
	CcpCreateThread( &MakeThread<decltype( thread )>, &thread, CCP_THREAD_PRIORITY_NORMAL );

	auto duration = TimeCallInMs( [&] { s.Wait(); } );
	EXPECT_GT( duration, 200 );
	EXPECT_LT( duration, 1000 );
}
