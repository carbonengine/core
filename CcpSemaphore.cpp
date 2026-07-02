// Copyright © 2013 CCP ehf.

#include "include/CcpSemaphore.h"


#if CCP_TELEMETRY_ENABLED
namespace
{
	// Helper functions to notifying Telemetry tool of a change in lock state:
	void NotifyTelemetryLockTerminated( void* lockContext )
	{
		if ( CcpTelemetryLockTrackingIsEnabled() && lockContext && CcpTelemetryIsConnected() )
		{
			TracyCLockTerminate( static_cast<TracyCLockCtx>( lockContext ) );
		}
	}

	bool NotifyTelemetryBeforeLock( void* lockContext )
	{
		const bool emit = lockContext && CcpTelemetryLockTrackingIsEnabled() && CcpTelemetryIsConnected();
		bool notifyTracy{ false };
		if ( emit )
		{
			notifyTracy = TracyCLockBeforeLock( static_cast<TracyCLockCtx>( lockContext ) );
		}
		return notifyTracy;
	}

	void NotifyTelemetryAfterLock( const bool notifyTracy, void* lockContext )
	{
		if ( notifyTracy )
		{
			TracyCLockAfterLock( static_cast<TracyCLockCtx>( lockContext ) );
		}
	}

	void NotifyTelemetryAfterUnlock( void* lockContext )
	{
		if ( lockContext && CcpTelemetryLockTrackingIsEnabled() && CcpTelemetryIsConnected() )
		{
			TracyCLockAfterUnlock( static_cast<TracyCLockCtx>( lockContext ) );
		}
	}
}
#endif

// OS specific includes:
#ifdef _WIN32
// Nothing specific
#elif defined(__APPLE__)
#include <mach/semaphore.h>
#include <mach/mach.h>
#include <errno.h>  // Refactoring note: Original implementation included this after destructor and before Wait()
#else
#include <errno.h>  // Refactoring note: Original implementation included this after destructor and before Wait()
#endif


// Fully qualified, preferred constructor.
CcpSemaphore::CcpSemaphore( const char* semaphoreName, uint32_t initialCount, uint32_t maximumCount )
{
	// Make sure to keep our own copy of the semaphoreName
	if (semaphoreName != nullptr)
	{
		const size_t strLen = std::strlen( semaphoreName ) + 1;
		char* copy = new char[strLen];
		strcpy_s( copy, strLen, semaphoreName );
		m_semaphoreName = copy;
	}
	else
	{
		m_semaphoreName = nullptr;
	}

#if CCP_TELEMETRY_ENABLED
	EnsureTelemetryLockAnnounced();
#endif

	// OS specific implementation:
#ifdef _WIN32
	m_semaphore = ::CreateSemaphore( 0, initialCount, maximumCount, 0 );
#elif defined(__APPLE__)
	semaphore_create( current_task(), &m_semaphore, SYNC_POLICY_FIFO, initialCount );
#else
	sem_init( &m_semaphore, 0, initialCount );
#endif
}

// Preferred constructor, with default value overloads (see header file for details)
CcpSemaphore::CcpSemaphore( const char* semaphoreName )
	: CcpSemaphore( semaphoreName, 0, 1 )
{
}

CcpSemaphore::CcpSemaphore()
	: CcpSemaphore( "CcpSemaphore", 0, 1 )
{
}

CcpSemaphore::CcpSemaphore( uint32_t initialCount, uint32_t maximumCount )
	: CcpSemaphore( "CcpSemaphore", initialCount, maximumCount )
{
}

CcpSemaphore::~CcpSemaphore()
{
	// OS specific implementation:
#ifdef _WIN32
	::CloseHandle( m_semaphore );
#elif defined(__APPLE__)
	semaphore_destroy( current_task(), m_semaphore );
#else
	sem_destroy( &m_semaphore );
#endif

	delete[] m_semaphoreName;

#if CCP_TELEMETRY_ENABLED
	NotifyTelemetryLockTerminated( m_tracyLockContext );
	m_tracyLockContext = nullptr;
#endif
}

bool CcpSemaphore::Wait()
{
#if CCP_TELEMETRY_ENABLED
	EnsureTelemetryLockAnnounced();
	const bool notifyTracy = NotifyTelemetryBeforeLock( m_tracyLockContext );
#endif

	// OS specific implementation:
#ifdef _WIN32
	const bool result = ::WaitForSingleObject( m_semaphore, INFINITE ) == 0;
#elif defined(__APPLE__)
	const bool result = semaphore_wait( m_semaphore ) == KERN_SUCCESS;
#else
	const bool result = sem_wait( &m_semaphore ) == 0;
#endif

#if CCP_TELEMETRY_ENABLED
	NotifyTelemetryAfterLock( notifyTracy && result, m_tracyLockContext );
#endif
	return result;
}

bool CcpSemaphore::TimedWait( uint32_t timeoutInMs )
{
#if CCP_TELEMETRY_ENABLED
	EnsureTelemetryLockAnnounced();
	const bool notifyTracy = NotifyTelemetryBeforeLock( m_tracyLockContext );
#endif

	// OS specific implementation:
#ifdef _WIN32
	const bool result = ::WaitForSingleObject( m_semaphore, timeoutInMs ) == 0;
#elif defined(__APPLE__)
	mach_timespec_t mts;
	mts.tv_sec = timeoutInMs / 1000;
	mts.tv_nsec = ( timeoutInMs % 1000 ) * 1000000;
	const bool result = semaphore_timedwait( m_semaphore, mts ) == KERN_SUCCESS;
#else
	timespec ts;
	ts.tv_sec = timeoutInMs / 1000;
	ts.tv_nsec = (timeoutInMs % 1000) * 1000000;
	const bool result = sem_timedwait( &m_semaphore, &ts ) == 0;
#endif

#if CCP_TELEMETRY_ENABLED
	NotifyTelemetryAfterLock( notifyTracy && result, m_tracyLockContext );
#endif
	return result;
}

void CcpSemaphore::Signal()
{
	// OS specific implementation:
#ifdef _WIN32
	::ReleaseSemaphore( m_semaphore, 1, 0 );
#elif defined(__APPLE__)
	semaphore_signal( m_semaphore );
#else
	sem_post( &m_semaphore );
#endif

#if CCP_TELEMETRY_ENABLED
	NotifyTelemetryAfterUnlock( m_tracyLockContext );
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
		const char* name = m_semaphoreName ? m_semaphoreName : "CcpSemaphore";
		TracyCLockCustomName( ctx, name, strlen( name ) );
		m_tracyLockContext = ctx;
	}
}
#endif
