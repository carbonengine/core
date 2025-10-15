// Copyright © 2013 CCP ehf.

#include <CCPLog.h>

#include "include/CcpThread.h"
#include "include/CCPMemory.h"

namespace
{
	struct CreateThreadData
	{
		CcpThreadProc_t functionToCall;
		void* context;
	};
}

#if _WIN32

#include <map>

namespace
{
	std::map<CcpThreadHandle_t,CcpThreadId_t> s_handleToId;

	DWORD WINAPI ThreadProcHelper( void* context )
	{
		CreateThreadData* data = reinterpret_cast<CreateThreadData*>( context );
		CcpThreadProc_t f = data->functionToCall;
		void* ctx = data->context;
		CCP_DELETE data;

		return f( ctx );
	}
}

CcpThreadId_t CcpGetCurrentThreadId()
{
	return GetCurrentThreadId();
}

CcpThreadHandle_t CcpCreateThread( CcpThreadProc_t threadProc, void* context, CcpThreadPriority_t priority )
{
	CreateThreadData* data = CCP_NEW( "CcpCreateThread/data" ) CreateThreadData;
	data->functionToCall = threadProc;
	data->context = context;

	CcpThreadId_t id;
	CcpThreadHandle_t threadHandle = CreateThread( 0, 0, ThreadProcHelper, data, CREATE_SUSPENDED, &id );
	s_handleToId[threadHandle] = id;
	SetThreadPriority( threadHandle, priority );

	ResumeThread( threadHandle );

	return threadHandle;
}

CcpThreadId_t CcpGetThreadId( CcpThreadHandle_t handle )
{
	return s_handleToId[handle];
}

int CcpJoinThread( CcpThreadHandle_t threadHandle, uint32_t& result )
{
	return CcpJoinThreadWithTimeout( threadHandle, INFINITE, result );
}

int CcpJoinThreadWithTimeout( CcpThreadHandle_t threadHandle, uint32_t timeoutInMs, uint32_t& result )
{
	DWORD s = WaitForSingleObject( threadHandle, timeoutInMs );
	if( s == 0 )
	{
		GetExitCodeThread( threadHandle, (DWORD*)&result );
		::CloseHandle( threadHandle );
		return 0;
	}
	else
	{
		// TODO: Translate error code
		return s;
	}
}

bool CcpSetThreadPriority( CcpThreadHandle_t thread, CcpThreadPriority_t priority )
{
    return SetThreadPriority( thread, priority ) != 0;
}

void CcpThreadSleep( uint32_t sleepTimeInMs )
{
	Sleep( sleepTimeInMs );
}


void CcpKillThread( CcpThreadHandle_t threadHandle )
{
	TerminateThread( threadHandle, 0 );
}

void CcpSetThreadPriority( CcpThread& thread, CcpThreadPriority_t priority )
{
	if( thread.joinable() )
	{
		SetThreadPriority( thread.native_handle(), priority );
	}
}

bool CcpGetThreadTimes( int64_t& kernelTime, int64_t& userTime )
{
    FILETIME dummy;
    return GetThreadTimes( GetCurrentThread(), &dummy, &dummy, (LPFILETIME)&kernelTime, (LPFILETIME)&userTime ) != 0;
}

#elif __APPLE__

#include <sys/time.h>
#include "include/CCPAssert.h"
#include <mach/thread_act.h>

namespace
{
	void* ThreadProcHelper( void* context )
	{
		CreateThreadData* data = reinterpret_cast<CreateThreadData*>( context );
		CcpThreadProc_t f = data->functionToCall;
		void* ctx = data->context;
		CCP_DELETE data;

		return (void*)(ptrdiff_t)f( ctx );
	}
}

CcpThreadId_t CcpGetCurrentThreadId()
{
	return pthread_mach_thread_np(pthread_self());
}

CcpThreadHandle_t CcpCreateThread( CcpThreadProc_t threadProc, void* context, CcpThreadPriority_t priority )
{
	pthread_attr_t attr;
	if( pthread_attr_init( &attr ) != 0 )
	{
		return nullptr;
	}

	CreateThreadData* data = CCP_NEW( "CcpCreateThread/data" ) CreateThreadData;
	data->functionToCall = threadProc;
	data->context = context;

	CcpThreadHandle_t threadHandle;
	int s = pthread_create( &threadHandle, &attr, ThreadProcHelper, data );

    if( priority != CCP_THREAD_PRIORITY_NORMAL )
    {
        CcpSetThreadPriority( threadHandle, priority );
    }
    pthread_attr_destroy( &attr );

	if( s == 0 )
	{
		return threadHandle;
	}
	else
	{
		return nullptr;
	}
}

int CcpJoinThread( CcpThreadHandle_t threadHandle, uint32_t& result )
{
	void* threadResult = nullptr;
	int s = pthread_join( threadHandle, &threadResult );
	if( s == 0 )
	{
		result = uint32_t( uintptr_t( threadResult ) );
		return 0;
	}
	else
	{
		// TODO: Translate error code
		return s;
	}
}

namespace
{

    struct JoinTimeoutData
    {
        CcpThreadHandle_t threadHandle;
        bool done;
        void* threadResult;
    };
    
    void* JoinTimeoutHelper( void* arg )
    {
        JoinTimeoutData* data = static_cast<JoinTimeoutData*>( arg );
        
        pthread_join( data->threadHandle, &data->threadResult );
        data->done = true;
        return nullptr;
    }
    
    uint32_t GetTicks()
    {
        timeval tv;
        
        gettimeofday( &tv, nullptr );
        return uint32_t( tv.tv_usec / 1000 + tv.tv_sec * 1000 );
    }
}

int CcpJoinThreadWithTimeout( CcpThreadHandle_t threadHandle, uint32_t timeoutInMs, uint32_t& result )
{
    uint32_t start = GetTicks();
    
    JoinTimeoutData data;
    data.threadHandle = threadHandle;
    data.done = false;
    
    CcpThreadHandle_t id;
    if( pthread_create( &id, nullptr, &JoinTimeoutHelper, &data ) != 0 )
    {
        return -1;
    }
    do
    {
        if( data.done )
        {
            break;
        }
        CcpThreadSleep( 10 );
    }
    while( GetTicks() - start < timeoutInMs );
    if( !data.done )
    {
        pthread_cancel( id );
        return 1;
    }
    pthread_join( id, nullptr );
    result = uint32_t( uintptr_t( data.threadResult ) );
    return 0;
}

CcpThreadId_t CcpGetThreadId( CcpThreadHandle_t handle )
{
    return pthread_mach_thread_np(handle);
}

bool CcpSetThreadPriority( CcpThreadHandle_t thread, CcpThreadPriority_t priority )
{
    int policy;
    sched_param param;
    if( pthread_getschedparam( thread, &policy, &param ) )
    {
        return false;
    }
    
    auto minPriority = sched_get_priority_min( policy );
    auto maxPriority = sched_get_priority_max( policy );
    auto normalPriority = ( minPriority + maxPriority ) / 2;

    switch( priority )
    {
    case CCP_THREAD_PRIORITY_LOWEST:
        param.sched_priority = minPriority;
        break;
    case CCP_THREAD_PRIORITY_BELOW_NORMAL:
        param.sched_priority = ( minPriority + normalPriority ) / 2;
        break;
    case CCP_THREAD_PRIORITY_HIGHEST:
        param.sched_priority = maxPriority;
        break;
    case CCP_THREAD_PRIORITY_ABOVE_NORMAL:
        param.sched_priority = ( maxPriority + normalPriority ) / 2;
        break;
    default:
        param.sched_priority = normalPriority;
    }
    return pthread_setschedparam( thread, policy, &param ) == 0;
}

void CcpThreadSleep( uint32_t sleepTimeInMs )
{
	timespec ts;
	ts.tv_sec = sleepTimeInMs / 1000;
	ts.tv_nsec = (sleepTimeInMs % 1000) * 1000000;
	nanosleep( &ts, nullptr );
}

void CcpKillThread( CcpThreadHandle_t threadHandle )
{
    pthread_cancel( threadHandle );
}

void CcpSetThreadPriority( CcpThread& thread, CcpThreadPriority_t priority )
{
    if( !thread.joinable() )
    {
        return;
    }
    CcpSetThreadPriority( thread.native_handle(), priority );
}

bool CcpGetThreadTimes( int64_t& kernelTime, int64_t& userTime )
{
    mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
    thread_basic_info_data_t info;
	mach_port_t port = pthread_mach_thread_np( pthread_self() );
	if( !MACH_PORT_VALID( port ) )
	{
		return false;
	}
	int returnCode = thread_info( port, THREAD_BASIC_INFO, (thread_info_t)&info, &count );
    if( returnCode != KERN_SUCCESS )
    {
		return false;
	}
    userTime = int64_t( info.user_time.seconds ) * 10000000 + int64_t( info.user_time.microseconds ) * 10;
    kernelTime = int64_t( info.system_time.seconds ) * 10000000 + int64_t( info.system_time.microseconds ) * 10;
    return true;
}

#endif
