// Copyright © 2026 CCP ehf.

#include <string>

#include "include/CcpMutex.h"
#include "include/CcpTelemetry.h"
#include "include/CcpThread.h"


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
	NativeMutex* CreateNativeMutex( unsigned spinCount )
	{
		auto* mux = new NativeMutex;
#ifdef _WIN32
		InitializeCriticalSectionAndSpinCount( mux, spinCount );
#else
		(void)spinCount; // pthreads has no equivalent
		pthread_mutexattr_t mutexAttr;
		pthread_mutexattr_init( &mutexAttr );
		pthread_mutexattr_settype( &mutexAttr, PTHREAD_MUTEX_RECURSIVE );

		pthread_mutex_init( mux, &mutexAttr );
		pthread_mutexattr_destroy( &mutexAttr );
#endif
		return mux;
	}

	void DestroyNativeMutex( NativeMutex* mux )
	{
		if ( !mux )
		{
			return;
		}
#ifdef _WIN32
		::DeleteCriticalSection( mux );
#else
		pthread_mutex_destroy( mux );
#endif
		delete mux;
	}

	void LockNativeMutex( NativeMutex* mux )
	{
#ifdef _WIN32
		EnterCriticalSection( mux );
#else
		pthread_mutex_lock( mux );
#endif
	}

	void UnlockNativeMutex( NativeMutex* mux )
	{
#ifdef _WIN32
		LeaveCriticalSection( mux );
#else
		pthread_mutex_unlock( mux );
#endif
	}
}


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

// ---------------------------------------------------------------------------
// CcpMutex
// ---------------------------------------------------------------------------

CcpMutex::CcpMutex( const char* owner, const char* name, unsigned spinCount )
	: m_mutexHandle( CreateNativeMutex( spinCount ) ),
	  m_owner( owner ? owner : "" ),
	  m_name( name ? name : "" )
{
#if CCP_TELEMETRY_ENABLED
	EnsureTelemetryLockAnnounced();
#endif

	CcpRegisterMutex( *this, owner, name );
}

CcpMutex::~CcpMutex()
{
	DestroyNativeMutex( static_cast<NativeMutex*>( m_mutexHandle ) );
	m_mutexHandle = nullptr;

#if CCP_TELEMETRY_ENABLED
	NotifyTelemetryLockTerminated( m_tracyLockContext );
	m_tracyLockContext = nullptr;
#endif
}

void CcpMutex::Acquire()
{
#if CCP_TELEMETRY_ENABLED
	EnsureTelemetryLockAnnounced();
	const bool notifyTracy = NotifyTelemetryBeforeLock( m_tracyLockContext );
#endif

	LockNativeMutex( static_cast<NativeMutex*>( m_mutexHandle ) );

#if CCP_TELEMETRY_ENABLED
	NotifyTelemetryAfterLock( notifyTracy, m_tracyLockContext );
#endif
}

void CcpMutex::Release()
{
	UnlockNativeMutex( static_cast<NativeMutex*>( m_mutexHandle ) );

#if CCP_TELEMETRY_ENABLED
	NotifyTelemetryAfterUnlock( m_tracyLockContext );
#endif
}

void CcpMutex::SetOwner( const char* owner )
{
	m_owner = owner ? owner : "";
}

void CcpMutex::SetName( const char* name )
{
	m_name = name ? name : "";
}

#if CCP_TELEMETRY_ENABLED
void CcpMutex::EnsureTelemetryLockAnnounced()
{
	if ( m_tracyLockContext )
	{
		return; // already announced
	}
	if ( CcpTelemetryLockTrackingIsEnabled() && CcpTelemetryIsConnected() )
	{
		TracyCLockCtx ctx = static_cast<TracyCLockCtx>( m_tracyLockContext );
		const std::string lockName = ( m_owner.empty() ? std::string( "<owner>" ) : m_owner ) + "-" +
		                             ( m_name.empty()  ? std::string( "<name>" )  : m_name );
		TracyCLockAnnounce( ctx );
		TracyCLockCustomName( ctx, lockName.c_str(), lockName.size() );
		m_tracyLockContext = ctx;
	}
}
#endif

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

CcpSpinLock::CcpSpinLock( const char* spinLockName )
	: m_lock( 0 ),
	  m_spinLockName( spinLockName ? spinLockName : "" )
{
#if CCP_TELEMETRY_ENABLED
	EnsureTelemetryLockAnnounced();
#endif
}

// Deprecated default ctor — forwards to the named ctor using the class name.
CcpSpinLock::CcpSpinLock()
	: CcpSpinLock( "CcpSpinLock" )
{
}

CcpSpinLock::~CcpSpinLock()
{
#if CCP_TELEMETRY_ENABLED
	NotifyTelemetryLockTerminated( m_tracyLockContext );
	m_tracyLockContext = nullptr;
#endif
}

void CcpSpinLock::Acquire()
{
#if CCP_TELEMETRY_ENABLED
	EnsureTelemetryLockAnnounced();
	const bool notifyTracy = NotifyTelemetryBeforeLock( m_tracyLockContext );
#endif

	while ( true )
	{
		uint32_t expected = 0;
		if ( m_lock.compare_exchange_strong( expected, 1 ) )
		{
			break;
		}
		CcpThreadYield();
	}

#if CCP_TELEMETRY_ENABLED
	NotifyTelemetryAfterLock( notifyTracy, m_tracyLockContext );
#endif
}

void CcpSpinLock::Release()
{
	m_lock = 0;

#if CCP_TELEMETRY_ENABLED
	NotifyTelemetryAfterUnlock( m_tracyLockContext );
#endif
}

#if CCP_TELEMETRY_ENABLED
void CcpSpinLock::EnsureTelemetryLockAnnounced()
{
	if ( m_tracyLockContext )
	{
		return; // already announced
	}
	if ( CcpTelemetryLockTrackingIsEnabled() && CcpTelemetryIsConnected() )
	{
		TracyCLockCtx ctx = static_cast<TracyCLockCtx>( m_tracyLockContext );
		const std::string lockName = m_spinLockName.empty() ? std::string( "CcpSpinLock" ) : m_spinLockName;
		TracyCLockAnnounce( ctx );
		TracyCLockCustomName( ctx, lockName.c_str(), lockName.size() );
		m_tracyLockContext = ctx;
	}
}
#endif

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

