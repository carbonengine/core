// Copyright © 2013 CCP ehf.

#include <cstring>

#include "include/CcpSemaphore.h"
#include "include/CCPAssert.h"
#include "include/CcpTelemetry.h"

// ---------------------------------------------------------------------------
// Platform-specific primitives (Create / Destroy / Wait / TimedWait / Signal)
// ---------------------------------------------------------------------------

#ifdef _WIN32

namespace
{
	HANDLE CreateNativeSemaphore( uint32_t initialCount, uint32_t maximumCount )
	{
		return ::CreateSemaphore( 0, initialCount, maximumCount, 0 );
	}

	void DestroyNativeSemaphore( HANDLE sem )
	{
		::CloseHandle( sem );
	}

	bool WaitNativeSemaphore( HANDLE sem )
	{
		return ::WaitForSingleObject( sem, INFINITE ) == 0;
	}

	bool TimedWaitNativeSemaphore( HANDLE sem, uint32_t timeoutInMs )
	{
		return ::WaitForSingleObject( sem, timeoutInMs ) == 0;
	}

	void SignalNativeSemaphore( HANDLE sem )
	{
		::ReleaseSemaphore( sem, 1, 0 );
	}
}

#elif defined(__APPLE__)

#include <mach/semaphore.h>
#include <mach/mach.h>

namespace
{
	semaphore_t CreateNativeSemaphore( uint32_t initialCount, uint32_t /*maximumCount*/ )
	{
		semaphore_t sem;
		semaphore_create( current_task(), &sem, SYNC_POLICY_FIFO, initialCount );
		return sem;
	}

	void DestroyNativeSemaphore( semaphore_t sem )
	{
		semaphore_destroy( current_task(), sem );
	}

	bool WaitNativeSemaphore( semaphore_t sem )
	{
		return semaphore_wait( sem ) == KERN_SUCCESS;
	}

	bool TimedWaitNativeSemaphore( semaphore_t sem, uint32_t timeoutInMs )
	{
		mach_timespec_t mts;
		mts.tv_sec  = timeoutInMs / 1000;
		mts.tv_nsec = ( timeoutInMs % 1000 ) * 1000000;
		return semaphore_timedwait( sem, mts ) == KERN_SUCCESS;
	}

	void SignalNativeSemaphore( semaphore_t sem )
	{
		semaphore_signal( sem );
	}
}

#else

#include <errno.h>

namespace
{
	sem_t CreateNativeSemaphore( uint32_t initialCount, uint32_t /*maximumCount*/ )
	{
		sem_t sem;
		sem_init( &sem, 0, initialCount );
		return sem;
	}

	void DestroyNativeSemaphore( sem_t& sem )
	{
		sem_destroy( &sem );
	}

	bool WaitNativeSemaphore( sem_t& sem )
	{
		return sem_wait( &sem ) == 0;
	}

	bool TimedWaitNativeSemaphore( sem_t& sem, uint32_t timeoutInMs )
	{
		timespec ts;
		ts.tv_sec  = timeoutInMs / 1000;
		ts.tv_nsec = ( timeoutInMs % 1000 ) * 1000000;
		return sem_timedwait( &sem, &ts ) == 0;
	}

	void SignalNativeSemaphore( sem_t& sem )
	{
		sem_post( &sem );
	}
}

#endif

// ---------------------------------------------------------------------------
// CcpSemaphore (cross-platform)
// ---------------------------------------------------------------------------

CcpSemaphore::CcpSemaphore( const char* semaphoreName, uint32_t initialCount, uint32_t maximumCount )
	: m_semaphore( CreateNativeSemaphore( initialCount, maximumCount ) ),
	  m_semaphoreName( semaphoreName )
{
#if CCP_TELEMETRY_ENABLED
	EnsureTelemetryLockAnnounced();
#endif
}

// Deprecated default constructor — forwards to the preferred semaphore named ctor instead
CcpSemaphore::CcpSemaphore()
	: CcpSemaphore( "CcpSemaphore", 0, 1 )
{
}

// Deprecated constructor — forwards to the preferred semaphore named ctor instead
CcpSemaphore::CcpSemaphore( uint32_t initialCount, uint32_t maximumCount )
	: CcpSemaphore( "CcpSemaphore", initialCount, maximumCount )
{
}

CcpSemaphore::~CcpSemaphore()
{
	DestroyNativeSemaphore( m_semaphore );

#if CCP_TELEMETRY_ENABLED
	if ( m_tracyLockContext && CcpTelemetryLockTrackingIsEnabled() && CcpTelemetryIsConnected() )
	{
		TracyCLockTerminate( static_cast<TracyCLockCtx>( m_tracyLockContext ) );
	}
	m_tracyLockContext = nullptr;
#endif
}

bool CcpSemaphore::Wait()
{
#if CCP_TELEMETRY_ENABLED
	EnsureTelemetryLockAnnounced();
	const bool emit = m_tracyLockContext && CcpTelemetryLockTrackingIsEnabled() && CcpTelemetryIsConnected();
	bool notifyTracy{ false };
	if ( emit )
	{
		notifyTracy = TracyCLockBeforeLock( static_cast<TracyCLockCtx>( m_tracyLockContext ) );
	}
#endif
	const bool result = WaitNativeSemaphore( m_semaphore );
#if CCP_TELEMETRY_ENABLED
	if ( notifyTracy && result )
	{
		TracyCLockAfterLock( static_cast<TracyCLockCtx>( m_tracyLockContext ) );
	}
#endif
	return result;
}

bool CcpSemaphore::TimedWait( uint32_t timeoutInMs )
{
#if CCP_TELEMETRY_ENABLED
	EnsureTelemetryLockAnnounced();
	const bool emit = m_tracyLockContext && CcpTelemetryLockTrackingIsEnabled() && CcpTelemetryIsConnected();
	bool notifyTracy{ false };
	if ( emit )
	{
		notifyTracy = TracyCLockBeforeLock( static_cast<TracyCLockCtx>( m_tracyLockContext ) );
	}
#endif
	const bool result = TimedWaitNativeSemaphore( m_semaphore, timeoutInMs );
#if CCP_TELEMETRY_ENABLED
	if ( notifyTracy && result )
	{
		TracyCLockAfterLock( static_cast<TracyCLockCtx>( m_tracyLockContext ) );
	}
#endif
	return result;
}

void CcpSemaphore::Signal()
{
	SignalNativeSemaphore( m_semaphore );
#if CCP_TELEMETRY_ENABLED
	if ( m_tracyLockContext && CcpTelemetryLockTrackingIsEnabled() && CcpTelemetryIsConnected() )
	{
		TracyCLockAfterUnlock( static_cast<TracyCLockCtx>( m_tracyLockContext ) );
	}
#endif
}

#if CCP_TELEMETRY_ENABLED
void CcpSemaphore::EnsureTelemetryLockAnnounced()
{
	if ( m_tracyLockContext )
	{
		return; // already announced
	}
	if ( CcpTelemetryLockTrackingIsEnabled() && CcpTelemetryIsConnected() )
	{
		TracyCLockCtx ctx = static_cast<TracyCLockCtx>( m_tracyLockContext );
		TracyCLockAnnounce( ctx );
		TracyCLockCustomName( ctx, m_semaphoreName, strlen( m_semaphoreName ) );
		m_tracyLockContext = ctx;
	}
}
#endif
