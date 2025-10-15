// Copyright © 2025 CCP ehf.

#include "gtest/gtest.h"
#include "CcpThread.h"
#include "CcpTime.h"
#include <functional> 

namespace
{

uint32_t TestThreadProc( void* ctx )
{
	return 0;
}

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

TEST ( CcpThread, SpawnAndJoinMultipleThreads )
{
	CcpThreadHandle_t threadHandles[10];
	uint32_t threadData[10];
	for( int i = 0; i < 10; ++i )
	{
		threadData[i] = i;
		threadHandles[i] = CcpCreateThread( TestThreadProc, &threadData[i], CCP_THREAD_PRIORITY_NORMAL );
	}

	for( int i = 0; i < 10; ++i )
	{
		uint32_t result = 0;
		int retVal = CcpJoinThread( threadHandles[i], result );
		EXPECT_EQ( 0, retVal );
	}
}

TEST ( CcpThread, SpawnAndJoinThread )
{
	CcpThreadHandle_t threadHandle;
	uint32_t threadData;
	uint32_t result = 123;
	
	threadHandle = CcpCreateThread( TestThreadProc, &threadData, CCP_THREAD_PRIORITY_NORMAL );
	int retVal = CcpJoinThread( threadHandle, result );
	EXPECT_EQ( 0, retVal );
	EXPECT_EQ( 0, result );
	
}

TEST ( CcpThread, GetThreadId )
{
	CcpThreadHandle_t threadHandle;
	uint32_t threadData;
	uint32_t result;

	threadHandle = CcpCreateThread( TestThreadProc, &threadData, CCP_THREAD_PRIORITY_HIGHEST );
	CcpThreadId_t actual = CcpGetThreadId(threadHandle);
	CcpJoinThread( threadHandle, result );
	EXPECT_NE( CcpThreadId_t( 0 ), actual );
}

TEST( CcpThread, CanJoinThreadWithTimeout )
{
	auto thread = [&] {
		CcpThreadSleep( 1000 );
	};

	auto handle = CcpCreateThread( &MakeThread<decltype( thread )>, &thread,  CCP_THREAD_PRIORITY_NORMAL );
	uint32_t result;
	auto time = TimeCallInMs( [&] { CcpJoinThreadWithTimeout( handle, 100, result ); } );
	EXPECT_LT( 50u, time );
	EXPECT_GT( 500u, time );
}

TEST( CcpThread, TimeoutWhenJoiningThreadDoesNotChangeResult )
{
	auto thread = [&] {
		CcpThreadSleep( 1000 );
	};

	auto handle = CcpCreateThread( &MakeThread<decltype( thread )>, &thread,  CCP_THREAD_PRIORITY_NORMAL );
	uint32_t result = 0xfaafaa;
	auto time = TimeCallInMs( [&] { CcpJoinThreadWithTimeout( handle, 100, result ); } );
	EXPECT_EQ( 0xfaafaa, result );
}

TEST( CcpThread, JoiningThreadSetsThreadResult )
{
	auto thread = [&] {
		CcpThreadSleep( 200 );
	};

	auto handle = CcpCreateThread( &MakeThread<decltype( thread )>, &thread,  CCP_THREAD_PRIORITY_NORMAL );
	uint32_t result = 0xfaafaa;
	auto time = TimeCallInMs( [&] { CcpJoinThreadWithTimeout( handle, 1000, result ); } );
	EXPECT_GT( 500u, time );
	EXPECT_EQ( 0, result );
}

#ifndef __ANDROID__

// Somehow on some machines this test corrupts all subsequent CreateThread calls
TEST( CcpThread, DISABLED_CanKillThread )
{
	bool reachedEnd = false;
	auto thread = [&] {
		CcpThreadSleep( 500 );
		reachedEnd = true;
	};

	auto handle = CcpCreateThread( &MakeThread<decltype( thread )>, &thread,  CCP_THREAD_PRIORITY_NORMAL );
	auto time = TimeCallInMs( [&] { CcpKillThread( handle ); } );
	EXPECT_GT( 10u, time );
	EXPECT_FALSE( reachedEnd );
}

#endif

namespace
{

int s_inThread;

void TestThreadProc0()
{
	++s_inThread;
}

void TestThreadProc1( int x )
{
	s_inThread = x;
}

void TestThreadProc2( int x, const char* )
{
	s_inThread = x;
}

void TestThreadProc3( const std::unique_ptr<int>& x )
{
	s_inThread = *x;
}

struct TestStruct
{
	void f0()
	{
		++s_inThread;
	}
	void f1( int x )
	{
		s_inThread = x;
	}
};

}

TEST( CcpThread, ThreadWithoutFunctionIsNotJoinable )
{
	CcpThread t;
	EXPECT_FALSE( t.joinable() );
}

TEST( CcpThread, ThreadWithFunctionIsJoinable )
{
	CcpThread t( &TestThreadProc0 );
	EXPECT_TRUE( t.joinable() );
	t.join();

}

TEST( CcpThread, ThreadCallsFunction )
{
	s_inThread = 0;
	CcpThread t( &TestThreadProc0 );
	t.join();
	EXPECT_EQ( 1, s_inThread );
}

TEST( CcpThread, ThreadCallsFunctionWithArgument )
{
	s_inThread = 0;
	CcpThread t( &TestThreadProc1, 123 );
	t.join();
	EXPECT_EQ( 123, s_inThread );
}

TEST( CcpThread, ThreadCallsMemberFunction )
{
	s_inThread = 0;
	TestStruct s;
	CcpThread t( &TestStruct::f0, s );
	t.join();
	EXPECT_EQ( 1, s_inThread );
}

TEST( CcpThread, ThreadCallsMemberFunctionWithArgument )
{
	s_inThread = 0;
	TestStruct s;
	CcpThread t( &TestStruct::f1, s, 123 );
	t.join();
	EXPECT_EQ( 123, s_inThread );
}

TEST( CcpThread, ThreadCallsFunctionWithArgument2 )
{
	s_inThread = 0;
	CcpThread t( &TestThreadProc2, 123, (const char*)"abc" );
	t.join();
	EXPECT_EQ( 123, s_inThread );
}

TEST( CcpThread, ThreadMovesArguments )
{
	s_inThread = 0;
	std::unique_ptr<int> x( new int( 123 ) );
	CcpThread t( &TestThreadProc3, std::cref( x ) );
	t.join();
	EXPECT_EQ( 123, s_inThread );
}

TEST( CcpThread, ThreadCallsMemberFunctionForPointer )
{
	s_inThread = 0;
	TestStruct s;
	CcpThread t( &TestStruct::f0, &s );
	t.join();
	EXPECT_EQ( 1, s_inThread );
}

TEST( CcpThread, CanGetThreadTimes )
{
	int64_t kernelTime = -1;
	int64_t userTime = -1;
	bool success = CcpGetThreadTimes( kernelTime, userTime );
	EXPECT_TRUE( success );
	EXPECT_GE( kernelTime, 0 );
	EXPECT_GE( userTime, 0);
}
