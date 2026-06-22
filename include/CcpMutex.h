// Copyright © 2025 CCP ehf.

#ifndef CCPMUTEX_H
#define CCPMUTEX_H

#include <string>

#include "CcpAtomic.h"
#include "CcpTelemetry.h"

#ifdef _WIN32

class CcpMutex
{
public:
	CcpMutex( const char* owner, const char* name, unsigned spinCount = 0 )
    {
		InitializeCriticalSectionAndSpinCount( &m_mutex, spinCount );

		m_owner = owner;
		m_name = name;

#if CCP_TELEMETRY_ENABLED
		EnsureTelemetryLockAnnounced();
#endif

		CcpRegisterMutex( *this, owner, name );
    }

    ~CcpMutex()
    {
        ::DeleteCriticalSection( &m_mutex );
#if CCP_TELEMETRY_ENABLED
		// Only terminate if Lock tracking is turned on, we still have a live context AND telemetry is still connected.
		// If telemetry has been disconnected meanwhile, the context is already stale.
		if ( CcpTelemetryLockTrackingIsEnabled() && m_tracyLockContext && CcpTelemetryIsConnected() )
		{
			TracyCLockTerminate( m_tracyLockContext );
		}
		m_tracyLockContext = nullptr;
#endif
    }

    void Acquire()
    {
#if CCP_TELEMETRY_ENABLED
		EnsureTelemetryLockAnnounced();
		const bool emit = m_tracyLockContext && CcpTelemetryLockTrackingIsEnabled() && CcpTelemetryIsConnected();
		bool notifyTracy{false};
		if ( emit )
		{
			notifyTracy = TracyCLockBeforeLock( m_tracyLockContext );
		}
#endif
        EnterCriticalSection( &m_mutex);
#if CCP_TELEMETRY_ENABLED
		if ( notifyTracy )
		{
			TracyCLockAfterLock( m_tracyLockContext );
		}
#endif
    }

    void Release()
    {
        LeaveCriticalSection( &m_mutex );
#if CCP_TELEMETRY_ENABLED
		if ( m_tracyLockContext && CcpTelemetryLockTrackingIsEnabled() && CcpTelemetryIsConnected() )
		{
			TracyCLockAfterUnlock( m_tracyLockContext );
		}
#endif
    }

	void SetOwner( const char* owner )
	{
		m_owner = owner;
	}

	void SetName( const char* name )
	{
		m_name = name;
	}

    // Don't allow assignment
	CcpMutex( const CcpMutex& ) = delete;

private:
#if CCP_TELEMETRY_ENABLED
	// Lazily announce mutex/lock on first call to constructor/acquire
	// as long as telemetry is connected and lock tracking is enabled.
	// Giving it a custom display "<owner>-<name>" name.
	// Subsequent calls are no-ops as long as a valid context exists.
	void EnsureTelemetryLockAnnounced()
	{
		if ( m_tracyLockContext )
		{
			return; // already announced
		}
		if ( CcpTelemetryLockTrackingIsEnabled() && CcpTelemetryIsConnected() )
		{
			const std::string tracyLockName = std::string( m_owner ? m_owner : "<owner>" ) + "-" + ( m_name ? m_name : "<name>" );
			TracyCLockAnnounce( m_tracyLockContext );
			TracyCLockCustomName( m_tracyLockContext, tracyLockName.c_str(), tracyLockName.size() );
		}
	}

	TracyCLockCtx m_tracyLockContext{nullptr};
#endif
    CRITICAL_SECTION m_mutex;
	const char* m_owner;
	const char* m_name;
};



#else

#include <pthread.h>

class CcpMutex
{
public:
	CcpMutex( const char* owner, const char* name, unsigned spinCount = 0 )
	{
		pthread_mutexattr_t mutexAttr;
		pthread_mutexattr_init( &mutexAttr );
		pthread_mutexattr_settype( &mutexAttr, PTHREAD_MUTEX_RECURSIVE );
			
		pthread_mutex_init( &m_mutex, &mutexAttr );

		pthread_mutexattr_destroy( &mutexAttr );
		
		m_owner = owner;
		m_name = name;

#if CCP_TELEMETRY_ENABLED
		EnsureTelemetryLockAnnounced();
#endif

		CcpRegisterMutex( *this, owner, name );
	}

	~CcpMutex()
	{
		pthread_mutex_destroy( &m_mutex );
#if CCP_TELEMETRY_ENABLED
		// See the Windows variant for rationale.
		if ( CcpTelemetryLockTrackingIsEnabled() && m_tracyLockContext && CcpTelemetryIsConnected() )
		{
			TracyCLockTerminate( m_tracyLockContext );
		}
		m_tracyLockContext = nullptr;
#endif
	}

	void Acquire()
	{
#if CCP_TELEMETRY_ENABLED
		EnsureTelemetryLockAnnounced();
		const bool emit = m_tracyLockContext && CcpTelemetryLockTrackingIsEnabled() && CcpTelemetryIsConnected();
		bool notifyTracy{false};
		if ( emit )
		{
			notifyTracy = TracyCLockBeforeLock( m_tracyLockContext );
		}
#endif
		pthread_mutex_lock( &m_mutex);
#if CCP_TELEMETRY_ENABLED
		if ( notifyTracy )
		{
			TracyCLockAfterLock( m_tracyLockContext );
		}
#endif
	}

	void Release()
	{
		pthread_mutex_unlock( &m_mutex );
#if CCP_TELEMETRY_ENABLED
		if ( m_tracyLockContext && CcpTelemetryLockTrackingIsEnabled() && CcpTelemetryIsConnected() )
		{
			TracyCLockAfterUnlock( m_tracyLockContext );
		}
#endif
	}

	void SetOwner( const char* owner )
	{
		m_owner = owner;
	}

	void SetName( const char* name )
	{
		m_name = name;
	}

	// Don't allow assignment
	CcpMutex( const CcpMutex& other ) = delete;

private:
#if CCP_TELEMETRY_ENABLED
	// See the Windows variant above for documentation.
	void EnsureTelemetryLockAnnounced()
	{
		if ( m_tracyLockContext )
		{
			return; // already announced
		}
		if ( CcpTelemetryLockTrackingIsEnabled() && CcpTelemetryIsConnected() )
		{
			const std::string tracyLockName = std::string( m_owner ? m_owner : "<owner>" ) + "-" + ( m_name ? m_name : "<name>" );
			TracyCLockAnnounce( m_tracyLockContext );
			TracyCLockCustomName( m_tracyLockContext, tracyLockName.c_str(), tracyLockName.size() );
		}
	}

	TracyCLockCtx m_tracyLockContext{nullptr};
#endif
	pthread_mutex_t m_mutex;
	const char* m_owner;
	const char* m_name;
};

#endif // _WIN32

class CcpAutoMutex
{
public:
    CcpAutoMutex( CcpMutex& m ) : 
		m_mutex( m ),
		m_released(false)
    {
        m_mutex.Acquire();
    }

    ~CcpAutoMutex()
    {
		if( !m_released )
		{
			m_mutex.Release();
		}
    }

	// Release early
	void Release()
	{
		m_mutex.Release();
		m_released = true;
	}

private:
    CcpMutex& m_mutex;
	bool m_released;
};


class CcpSpinLock
{
public:
	CcpSpinLock()
		:m_lock( 0 )
	{
	}

    void Acquire()
    {
		while( true )
		{
			uint32_t expected = 0;
			if( m_lock.compare_exchange_strong( expected, 1 ) )
			{
				break;
			}
			CcpThreadYield();
		}
    }

    void Release()
    {
		m_lock = 0;
    }
private:
	CcpAtomic<uint32_t> m_lock;
};


class CcpAutoSpinLock
{
public:
    CcpAutoSpinLock( CcpSpinLock& m ) : 
		m_mutex( m ),
		m_released(false)
    {
        m_mutex.Acquire();
    }

    ~CcpAutoSpinLock()
    {
		if( !m_released )
		{
			m_mutex.Release();
		}
    }

	// Release early
	void Release()
	{
		m_mutex.Release();
		m_released = true;
	}

private:
    CcpSpinLock& m_mutex;
	bool m_released;
};

#endif
