// Copyright © 2013 CCP ehf.

#include "include/CcpSemaphore.h"
#include "include/CCPAssert.h"

#if CCP_TELEMETRY_ENABLED
#include "include/CcpTelemetry.h"
#endif

#ifdef _WIN32

// Fully qualified, preferred constructor.
CcpSemaphore::CcpSemaphore( const char* semaphoreName, uint32_t initialCount, uint32_t maximumCount )
{
	// Make sure to keep our own copy of the semaphoreName
	if (semaphoreName != nullptr)
	{
		const size_t strLen = std::strlen( semaphoreName ) + 1;
		m_semaphoreName = new char[strLen];
		strcpy_s( const_cast<char*>(m_semaphoreName), strLen, semaphoreName );
	}
	else
	{
		m_semaphoreName = nullptr;
	}

#if CCP_TELEMETRY_ENABLED
	EnsureTelemetryLockAnnounced();
#endif

	m_semaphore = ::CreateSemaphore( 0, initialCount, maximumCount, 0 );
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
	::CloseHandle( m_semaphore );
	delete[] m_semaphoreName;

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

	const auto result = ::WaitForSingleObject( m_semaphore, INFINITE ) == 0;

#if CCP_TELEMETRY_ENABLED
	if ( notifyTracy && result )
	{
		TracyCLockAfterLock( static_cast<TracyCLockCtx>( m_tracyLockContext ) );
	}
#endif
	return result;
}

bool CcpSemaphore::TimedWait( uint32_t timeout )
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

	const auto result = ::WaitForSingleObject( m_semaphore, timeout ) == 0;

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
	::ReleaseSemaphore( m_semaphore, 1, 0 );

#if CCP_TELEMETRY_ENABLED
	if ( m_tracyLockContext && CcpTelemetryLockTrackingIsEnabled() && CcpTelemetryIsConnected() )
	{
		TracyCLockAfterUnlock( static_cast<TracyCLockCtx>( m_tracyLockContext ) );
	}
#endif
}


#elif defined(__APPLE__)

#include <mach/semaphore.h>
#include <mach/mach.h>

// Fully qualified, preferred constructor. Note maximumCount is ignored.
CcpSemaphore::CcpSemaphore( const char* semaphoreName, uint32_t initialCount, uint32_t maximumCount )
{
	// Make sure to keep our own copy of the semaphoreName
	if (semaphoreName != nullptr)
	{
		const size_t strLen = std::strlen( semaphoreName ) + 1;
		m_semaphoreName = new char[strLen];
		strcpy_s( const_cast<char*>(m_semaphoreName), strLen, semaphoreName );
	}
	else
	{
		m_semaphoreName = nullptr;
	}

#if CCP_TELEMETRY_ENABLED
	EnsureTelemetryLockAnnounced();
#endif

	semaphore_create( current_task(), &m_semaphore, SYNC_POLICY_FIFO, initialCount );
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
    semaphore_destroy( current_task(), m_semaphore );
	delete[] m_semaphoreName;

#if CCP_TELEMETRY_ENABLED
	if ( m_tracyLockContext && CcpTelemetryLockTrackingIsEnabled() && CcpTelemetryIsConnected() )
	{
		TracyCLockTerminate( static_cast<TracyCLockCtx>( m_tracyLockContext ) );
	}
	m_tracyLockContext = nullptr;
#endif
}

#include <errno.h>

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

	const auto result = semaphore_wait( m_semaphore ) == KERN_SUCCESS;

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

	mach_timespec_t mts;
	mts.tv_sec = timeoutInMs / 1000;
	mts.tv_nsec = ( timeoutInMs % 1000 ) * 1000000;
	const auto result = semaphore_timedwait( m_semaphore, mts ) == KERN_SUCCESS;

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
    semaphore_signal( m_semaphore );

#if CCP_TELEMETRY_ENABLED
	if ( m_tracyLockContext && CcpTelemetryLockTrackingIsEnabled() && CcpTelemetryIsConnected() )
	{
		TracyCLockAfterUnlock( static_cast<TracyCLockCtx>( m_tracyLockContext ) );
	}
#endif
}

#else

// Fully qualified, preferred constructor. Note maximumCount is ignored
CcpSemaphore::CcpSemaphore( const char* semaphoreName, uint32_t initialCount, uint32_t maximumCount )
{
	// Make sure to keep our own copy of the semaphoreName
	if (semaphoreName != nullptr)
	{
		const size_t strLen = std::strlen( semaphoreName ) + 1;
		m_semaphoreName = new char[strLen];
		strcpy_s( const_cast<char*>(m_semaphoreName), strLen, semaphoreName );
	}
	else
	{
		m_semaphoreName = nullptr;
	}

#if CCP_TELEMETRY_ENABLED
	EnsureTelemetryLockAnnounced();
#endif

	sem_init( &m_semaphore, 0, initialCount );
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
	sem_destroy( &m_semaphore );
	delete[] m_semaphoreName;

#if CCP_TELEMETRY_ENABLED
	if ( m_tracyLockContext && CcpTelemetryLockTrackingIsEnabled() && CcpTelemetryIsConnected() )
	{
		TracyCLockTerminate( static_cast<TracyCLockCtx>( m_tracyLockContext ) );
	}
	m_tracyLockContext = nullptr;
#endif
}

#include <errno.h>

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

	const auto result = sem_wait( &m_semaphore ) == 0;

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

	timespec ts;
	ts.tv_sec = timeoutInMs / 1000;
	ts.tv_nsec = (timeoutInMs % 1000) * 1000000;
	const auto result = sem_timedwait( &m_semaphore, &ts ) == 0;

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
	sem_post( &m_semaphore );

#if CCP_TELEMETRY_ENABLED
	if ( m_tracyLockContext && CcpTelemetryLockTrackingIsEnabled() && CcpTelemetryIsConnected() )
	{
		TracyCLockAfterUnlock( static_cast<TracyCLockCtx>( m_tracyLockContext ) );
	}
#endif
}

#endif


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
