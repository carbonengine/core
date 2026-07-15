// Copyright © 2026 CCP ehf.

#include <string>

#include "include/CcpAtomic.h"
#include "include/CcpMutex.h"
#include "include/CcpThread.h"
#if CCP_TELEMETRY_ENABLED
#include "tracy/TracyC.h"
#endif


namespace
{
	// Platform independent alias for the underlying lock object used by CcpMutex
#ifdef _WIN32
	#include <windows.h>
	using NativeMutex = CRITICAL_SECTION;
#else
	#include <pthread.h>
	using NativeMutex = pthread_mutex_t;
#endif

	// Small platform-specific helpers. The cross-platform CcpMutex methods below
	// call these so that no public method body needs to be duplicated per OS.
	void CreateNativeMutex( NativeMutex& mux, unsigned spinCount )
	{
#ifdef _WIN32
		InitializeCriticalSectionAndSpinCount( &mux, spinCount );
#else
		(void)spinCount; // pthreads has no equivalent
		pthread_mutexattr_t mutexAttr;
		pthread_mutexattr_init( &mutexAttr );
		pthread_mutexattr_settype( &mutexAttr, PTHREAD_MUTEX_RECURSIVE );

		pthread_mutex_init( &mux, &mutexAttr );
		pthread_mutexattr_destroy( &mutexAttr );
#endif
	}

	void DestroyNativeMutex( NativeMutex& mux )
	{
#ifdef _WIN32
		::DeleteCriticalSection( &mux );
#else
		pthread_mutex_destroy( &mux );
#endif
	}

	void LockNativeMutex( NativeMutex& mux )
	{
#ifdef _WIN32
		EnterCriticalSection( &mux );
#else
		pthread_mutex_lock( &mux );
#endif
	}

	void UnlockNativeMutex( NativeMutex& mux )
	{
#ifdef _WIN32
		LeaveCriticalSection( &mux );
#else
		pthread_mutex_unlock( &mux );
#endif
	}
}

// ---------------------------------------------------------------------------
// CcpMutex
// ---------------------------------------------------------------------------

struct CcpMutex::Private
{
#if CCP_TELEMETRY_ENABLED
	TracyCLockCtx telemetryLockContext;
#endif

	NativeMutex mutexHandle;

	const char* owner;
	const char* name;
};

CcpMutex::CcpMutex( const char* owner, const char* name, unsigned spinCount ) : m_impl( std::make_unique<Private>() )
{
	CreateNativeMutex( m_impl->mutexHandle, spinCount );
	SetOwner( owner );
	SetName( name );

	CcpRegisterMutex( *this, owner, name );
}

CcpMutex::~CcpMutex()
{
	DestroyNativeMutex( m_impl->mutexHandle );

#if CCP_TELEMETRY_ENABLED
	if ( CcpTelemetryLockTrackingIsEnabled() && m_impl->telemetryLockContext && CcpTelemetryIsConnected() )
	{
		TracyCLockTerminate( m_impl->telemetryLockContext );
	}
#endif
}

void CcpMutex::Acquire()
{
#if CCP_TELEMETRY_ENABLED
	bool notifyTracy{ false };
	if ( CcpTelemetryLockTrackingIsEnabled() && CcpTelemetryIsConnected() )
	{
		// Lazy initialization pattern because there are many CcpMutex instances which are created statically,
		// and therefore would never be announced to tracy.
		if ( !m_impl->telemetryLockContext )
		{
			const std::string lockName = std::string( m_impl->owner ) + "-" + m_impl->name;
			TracyCLockAnnounce( m_impl->telemetryLockContext );
			if ( m_impl->telemetryLockContext ) {
				TracyCLockCustomName( m_impl->telemetryLockContext, lockName.c_str(), lockName.size() );
			}
		}

		if ( m_impl->telemetryLockContext )
		{
			notifyTracy = TracyCLockBeforeLock( m_impl->telemetryLockContext );
		}
	}
#endif

	LockNativeMutex( m_impl->mutexHandle );

#if CCP_TELEMETRY_ENABLED
	if ( notifyTracy && CcpTelemetryLockTrackingIsEnabled() && CcpTelemetryIsConnected() )
	{
		TracyCLockAfterLock( m_impl->telemetryLockContext );
	}
#endif
}

void CcpMutex::Release()
{
	UnlockNativeMutex( m_impl->mutexHandle );

#if CCP_TELEMETRY_ENABLED
	if ( m_impl->telemetryLockContext && CcpTelemetryLockTrackingIsEnabled() && CcpTelemetryIsConnected() )
	{
		TracyCLockAfterUnlock( m_impl->telemetryLockContext );
	}
#endif
}

void CcpMutex::SetOwner( const char* owner )
{
	m_impl->owner = owner ? owner : "<owner>";
}

void CcpMutex::SetName( const char* name )
{
	m_impl->name = name ? name : "<name>";
}

// ---------------------------------------------------------------------------
// CcpAutoMutex
// ---------------------------------------------------------------------------

CcpAutoMutex::CcpAutoMutex( CcpMutex& m )
	: m_mutex( m ), m_released( false )
{
	m_mutex.Acquire();
}

CcpAutoMutex::~CcpAutoMutex()
{
	if ( !m_released )
	{
		m_mutex.Release();
	}
}

void CcpAutoMutex::Release()
{
	m_mutex.Release();
	m_released = true;
}

// ---------------------------------------------------------------------------
// CcpSpinLock
// ---------------------------------------------------------------------------

struct CcpSpinLock::Private
{
#if CCP_TELEMETRY_ENABLED
	TracyCLockCtx telemetryLockContext;
#endif
	CcpAtomic<uint32_t> lock;
	const char* spinLockName;
};

CcpSpinLock::CcpSpinLock( const char* spinLockName )
	: m_impl( std::make_unique<Private>() )
{
	m_impl->spinLockName = spinLockName ? spinLockName : "<Unnamed Spinlock>";
}

// Deprecated default ctor — forwards to the named ctor using the class name.
CcpSpinLock::CcpSpinLock()
	: CcpSpinLock( "CcpSpinLock" )
{
}

CcpSpinLock::~CcpSpinLock()
{
#if CCP_TELEMETRY_ENABLED
	if ( CcpTelemetryLockTrackingIsEnabled() && m_impl->telemetryLockContext && CcpTelemetryIsConnected() )
	{
		TracyCLockTerminate( m_impl->telemetryLockContext );
	}
#endif
}

void CcpSpinLock::Acquire()
{
#if CCP_TELEMETRY_ENABLED
	bool notifyTracy{ false };
	if ( CcpTelemetryLockTrackingIsEnabled() && CcpTelemetryIsConnected() )
	{
		// Lazy initialization pattern because there are many CcpMutex instances which are created statically,
		// and therefore would never be announced to tracy.
		if ( !m_impl->telemetryLockContext )
		{
			TracyCLockAnnounce( m_impl->telemetryLockContext );
			if ( m_impl->telemetryLockContext ) {
				TracyCLockCustomName( m_impl->telemetryLockContext, m_impl->spinLockName, strlen( m_impl->spinLockName) )
			}
		}

		if ( m_impl->telemetryLockContext )
		{
			notifyTracy = TracyCLockBeforeLock( m_impl->telemetryLockContext );
		}
	}
#endif

	while ( true )
	{
		uint32_t expected = 0;
		if ( m_impl->lock.compare_exchange_strong( expected, 1 ) )
		{
			break;
		}
		CcpThreadYield();
	}

#if CCP_TELEMETRY_ENABLED
	if ( notifyTracy && CcpTelemetryLockTrackingIsEnabled() && CcpTelemetryIsConnected() )
	{
		TracyCLockAfterLock( m_impl->telemetryLockContext );
	}
#endif
}

void CcpSpinLock::Release()
{
	m_impl->lock = 0;

#if CCP_TELEMETRY_ENABLED
	if ( m_impl->telemetryLockContext && CcpTelemetryLockTrackingIsEnabled() && CcpTelemetryIsConnected() )
	{
		TracyCLockAfterUnlock( m_impl->telemetryLockContext );
	}
#endif
}

// ---------------------------------------------------------------------------
// CcpAutoSpinLock
// ---------------------------------------------------------------------------

CcpAutoSpinLock::CcpAutoSpinLock( CcpSpinLock& m )
	: m_mutex( m ), m_released( false )
{
	m_mutex.Acquire();
}

CcpAutoSpinLock::~CcpAutoSpinLock()
{
	if ( !m_released )
	{
		m_mutex.Release();
	}
}

void CcpAutoSpinLock::Release()
{
	m_mutex.Release();
	m_released = true;
}
