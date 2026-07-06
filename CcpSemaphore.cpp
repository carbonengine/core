// Copyright © 2013 CCP ehf.

#include "include/CcpSemaphore.h"

#include "tracy/TracyC.h"

// OS specific includes:
#ifdef _WIN32
// Nothing specific
	using NativeHandle = HANDLE;
#elif defined(__APPLE__)
#include <mach/semaphore.h>
#include <mach/mach.h>
	using NativeHandle = semaphore_t;
#else
	using NativeHandle = sem_t;
#endif

struct CcpSemaphore::Private
{
#if CCP_TELEMETRY_ENABLED
	// Lazily announce the semaphore to Tracy. Subsequent calls are no-ops once a context exists.
	void EnsureTelemetryLockAnnounced();

	// Opaque pointer to TracyCLockCtx, kept as void* so this header does not
	// need to pull in Tracy headers.
	TracyCLockCtx telemetryLockContext;
#endif

	NativeHandle semaphore;

	const char* semaphoreName{ nullptr };
};

// Fully qualified, preferred constructor.
CcpSemaphore::CcpSemaphore( const char* semaphoreName, uint32_t initialCount, uint32_t maximumCount ) : m_impl( std::make_unique<Private>() )
{
	// Make sure to keep our own copy of the semaphoreName
	if (semaphoreName != nullptr)
	{
		const size_t strLen = std::strlen( semaphoreName ) + 1;
		char* copy = new char[strLen];
		strcpy_s( copy, strLen, semaphoreName );
		m_impl->semaphoreName = copy;
	}
	else
	{
		m_impl->semaphoreName = nullptr;
	}

	// OS specific implementation:
#ifdef _WIN32
	m_impl->semaphore = ::CreateSemaphore( 0, initialCount, maximumCount, 0 );
#elif defined(__APPLE__)
	semaphore_create( current_task(), &m_impl->semaphore, SYNC_POLICY_FIFO, initialCount );
#else
	sem_init( &m_impl->semaphore, 0, initialCount );
#endif
}

namespace
{
	void AnnounceSemaphoreToTelemetry( TracyCLockCtx& ctx, const char* name )
	{
		// Lazy initialization pattern because there are many instances which are created statically,
		// and therefore would never be announced to tracy.
		if ( !ctx )
		{
			TracyCLockAnnounce( ctx );
			if ( ctx ) {
				TracyCLockCustomName( ctx, name, strlen( name ) );
			}
		}
	}
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
	::CloseHandle( m_impl->semaphore );
#elif defined(__APPLE__)
	semaphore_destroy( current_task(), m_impl->semaphore );
#else
	sem_destroy( &m_impl->semaphore );
#endif

	delete[] m_impl->semaphoreName;

#if CCP_TELEMETRY_ENABLED
	if ( CcpTelemetryLockTrackingIsEnabled() && m_impl->telemetryLockContext && CcpTelemetryIsConnected() )
	{
		TracyCLockTerminate( m_impl->telemetryLockContext );
	}
#endif
}

bool CcpSemaphore::Wait()
{
#if CCP_TELEMETRY_ENABLED
	bool notifyTracy{ false };
	if ( CcpTelemetryLockTrackingIsEnabled() && CcpTelemetryIsConnected() )
	{
		AnnounceSemaphoreToTelemetry( m_impl->telemetryLockContext, m_impl->semaphoreName );

		if ( m_impl->telemetryLockContext )
		{
			notifyTracy = TracyCLockBeforeLock( m_impl->telemetryLockContext );
		}
	}
#endif

	// OS specific implementation:
#ifdef _WIN32
	const bool result = ::WaitForSingleObject( m_semaphore, INFINITE ) == 0;
#elif defined(__APPLE__)
	const bool result = semaphore_wait( m_impl->semaphore ) == KERN_SUCCESS;
#else
	const bool result = sem_wait( &m_semaphore ) == 0;
#endif

#if CCP_TELEMETRY_ENABLED
	if ( notifyTracy && CcpTelemetryLockTrackingIsEnabled() && CcpTelemetryIsConnected() )
	{
		TracyCLockAfterLock( m_impl->telemetryLockContext );
	}
#endif
	return result;
}

bool CcpSemaphore::TimedWait( uint32_t timeoutInMs )
{
#if CCP_TELEMETRY_ENABLED
	bool notifyTracy{ false };
	if ( CcpTelemetryLockTrackingIsEnabled() && CcpTelemetryIsConnected() )
	{
		AnnounceSemaphoreToTelemetry( m_impl->telemetryLockContext, m_impl->semaphoreName );

		if ( m_impl->telemetryLockContext )
		{
			notifyTracy = TracyCLockBeforeLock( m_impl->telemetryLockContext );
		}
	}
#endif

	// OS specific implementation:
#ifdef _WIN32
	const bool result = ::WaitForSingleObject( m_semaphore, timeoutInMs ) == 0;
#elif defined(__APPLE__)
	mach_timespec_t mts;
	mts.tv_sec = timeoutInMs / 1000;
	mts.tv_nsec = ( timeoutInMs % 1000 ) * 1000000;
	const bool result = semaphore_timedwait( m_impl->semaphore, mts ) == KERN_SUCCESS;
#else
	timespec ts;
	ts.tv_sec = timeoutInMs / 1000;
	ts.tv_nsec = (timeoutInMs % 1000) * 1000000;
	const bool result = sem_timedwait( &m_semaphore, &ts ) == 0;
#endif

#if CCP_TELEMETRY_ENABLED
	if ( notifyTracy && CcpTelemetryLockTrackingIsEnabled() && CcpTelemetryIsConnected() )
	{
		TracyCLockAfterLock( m_impl->telemetryLockContext );
	}
#endif
	return result;
}

void CcpSemaphore::Signal()
{
	// OS specific implementation:
#ifdef _WIN32
	::ReleaseSemaphore( m_impl->semaphore, 1, 0 );
#elif defined(__APPLE__)
	semaphore_signal( m_impl->semaphore );
#else
	sem_post( &m_impl->semaphore );
#endif

#if CCP_TELEMETRY_ENABLED
	if ( CcpTelemetryLockTrackingIsEnabled() && CcpTelemetryIsConnected() )
	{
		AnnounceSemaphoreToTelemetry( m_impl->telemetryLockContext, m_impl->semaphoreName );

		if ( m_impl->telemetryLockContext )
		{
			TracyCLockAfterUnlock( m_impl->telemetryLockContext );
		}
	}
#endif
}
