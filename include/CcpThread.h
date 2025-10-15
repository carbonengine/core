// Copyright © 2013 CCP ehf.

#pragma once
#ifndef CcpThread_h
#define CcpThread_h

#include "carbon_core_export.h"
#include <cstdint>
#include <type_traits>
#include <exception>
#include <algorithm>
#include "CCPMemory.h"

#if _WIN32
	#include <windows.h>
    typedef DWORD CcpThreadId_t;
	typedef HANDLE CcpThreadHandle_t;

	inline void CcpThreadYield()
	{
		_mm_pause();
	}
#elif __APPLE__
	#include <pthread.h>
	#include <sched.h>
    typedef mach_port_t CcpThreadId_t;
	typedef pthread_t CcpThreadHandle_t;

	inline void CcpThreadYield()
	{
		sched_yield();
	}
#else
#error "Unsupported platform!"
#endif

typedef uint32_t (*CcpThreadProc_t)( void* context );

enum CcpThreadPriority_t
{
	CCP_THREAD_PRIORITY_LOWEST = -2,
	CCP_THREAD_PRIORITY_BELOW_NORMAL,
	CCP_THREAD_PRIORITY_NORMAL,
	CCP_THREAD_PRIORITY_ABOVE_NORMAL,
	CCP_THREAD_PRIORITY_HIGHEST
};

CARBON_CORE_API CcpThreadId_t CcpGetCurrentThreadId();
CARBON_CORE_API CcpThreadHandle_t CcpCreateThread( CcpThreadProc_t threadProc, void* context, CcpThreadPriority_t priority );
CARBON_CORE_API CcpThreadId_t CcpGetThreadId( CcpThreadHandle_t handle );
CARBON_CORE_API void CcpThreadSleep( uint32_t sleepTimeInMs );

// TODO: Error codes
CARBON_CORE_API int CcpJoinThread( CcpThreadHandle_t threadHandle, uint32_t& result );
CARBON_CORE_API int CcpJoinThreadWithTimeout( CcpThreadHandle_t threadHandle, uint32_t timeoutInMs, uint32_t& result );

CARBON_CORE_API void CcpKillThread( CcpThreadHandle_t threadHandle );

CARBON_CORE_API bool CcpSetThreadPriority( CcpThreadHandle_t thread, CcpThreadPriority_t priority );

// Time returned should be interpreted as 100-nanosecond intervals,
// however the time resolution between periodic timer updates is OS dependent.
// On my Windows machine the resolution is ca. 16ms, which is consistent with
// the thread time slice duration when measuring the duration of Sleep(1).
// https://learn.microsoft.com/en-us/windows/win32/procthread/multitasking
CARBON_CORE_API bool CcpGetThreadTimes( int64_t& kernelTime, int64_t& userTime );

#if defined(_MSC_VER) && _MSC_VER <= 1700

class CcpThread
{
public:
	CcpThread();
	CcpThread( CcpThread&& other );

	template <typename Function>
	explicit CcpThread( Function&& function )
		:m_id( 0 )
	{
		auto b = std::bind( std::forward<Function>( function ) );
		auto params = CCP_NEW( "CcpThread::params" ) decltype( b )( std::move( b ) );
		m_thread = CreateThread( nullptr, 0, &ThreadProc<decltype( b )>, params, 0, &m_id );
	}


	template <typename Function, typename Arg1>
	explicit CcpThread( Function&& function, Arg1&& arg1 )
		:m_id( 0 )
	{
		auto b = std::bind( std::forward<Function>( function ), std::forward<Arg1>( arg1 ) );
		auto params = CCP_NEW( "CcpThread::params" ) decltype( b )( std::move( b ) );
		m_thread = CreateThread( nullptr, 0, &ThreadProc<decltype( b )>, params, 0, &m_id );
	}

	template <typename Function, typename Arg1, typename Arg2>
	explicit CcpThread( Function&& function, Arg1&& arg1, Arg2&& arg2 )
		:m_id( 0 )
	{
		auto b = std::bind( std::forward<Function>( function ), std::forward<Arg1>( arg1 ), std::forward<Arg2>( arg2 ) );
		auto params = CCP_NEW( "CcpThread::params" ) decltype( b )( std::move( b ) );
		m_thread = CreateThread( nullptr, 0, &ThreadProc<decltype( b )>, params, 0, &m_id );
	}

	~CcpThread();

	CcpThread& operator=( CcpThread&& other );

	bool joinable() const;
	DWORD get_id() const;
	HANDLE native_handle();
	void join();
	void detach();
	void swap( CcpThread& other );
private:
	CcpThread( const CcpThread& ) /* = delete */;


	template <typename Callable>
	static DWORD WINAPI ThreadProc( LPVOID context )
	{
		Callable* callable = static_cast<Callable*>( context );
		( *callable )();
		CCP_DELETE callable;
		return 0;
	}

	HANDLE m_thread;
	DWORD m_id;
};

#else 

#include <thread>
typedef std::thread CcpThread;

#endif

CARBON_CORE_API void CcpSetThreadPriority( CcpThread& thread, CcpThreadPriority_t priority );

#endif // CcpThread_h
