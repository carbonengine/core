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
		// Lazily announce on first Acquire/Release; this also handles the case where
		// the mutex is created before telemetry is connected.
		EnsureTracyLockState();
#endif

		CcpRegisterMutex( *this, owner, name );
    }

    ~CcpMutex()
    {
        ::DeleteCriticalSection( &m_mutex );
#if CCP_TELEMETRY_ENABLED
		// Only terminate if we still have a live context AND telemetry is still connected.
		// If telemetry has been disconnected meanwhile, the context is already stale.
		if ( m_tracyLockContext && CcpTelemetryIsConnected() )
		{
			TracyCLockTerminate( m_tracyLockContext );
		}
		m_tracyLockContext = nullptr;
#endif
    }

    void Acquire()
    {
#if CCP_TELEMETRY_ENABLED
		EnsureTracyLockState();
		bool notifyTracy{false};
		if ( m_tracyLockContext )
		{
			notifyTracy = TracyCLockBeforeLock( m_tracyLockContext );
		}
#endif
        EnterCriticalSection( &m_mutex);
#if CCP_TELEMETRY_ENABLED
		if ( notifyTracy && m_tracyLockContext )
		{
			TracyCLockAfterLock( m_tracyLockContext );
		}
#endif
    }

    void Release()
    {
        LeaveCriticalSection( &m_mutex );
#if CCP_TELEMETRY_ENABLED
		EnsureTracyLockState();
		if ( m_tracyLockContext )
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
	// Synchronizes m_tracyLockContext with the current telemetry connection state.
	// - If telemetry is connected and we don't yet have a context, announce one.
	// - If telemetry is disconnected but we still have a (now stale) context, drop it
	//   so that a future reconnect will produce a fresh, valid context.
	// After this returns, all other Tracy calls in Acquire/Release can rely on a
	// single, fast null-check of m_tracyLockContext.
	void EnsureTracyLockState()
	{
		const bool connected = CcpTelemetryIsConnected();
		if ( m_tracyLockContext )
		{
			if ( !connected )
			{
				// Telemetry disconnected; drop the stale context quickly so the next
				// connect produces a fresh announce/name.
				m_tracyLockContext = nullptr;
			}
		}
		else if ( connected )
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
		// Lazily announce on first Acquire/Release; this also handles the case where
		// the mutex is created before telemetry is connected.
		EnsureTracyLockState();
#endif

		CcpRegisterMutex( *this, owner, name );
	}

	~CcpMutex()
	{
		pthread_mutex_destroy( &m_mutex );
#if CCP_TELEMETRY_ENABLED
		// Only terminate if we still have a live context AND telemetry is still connected.
		// If telemetry has been disconnected meanwhile, the context is already stale.
		if ( m_tracyLockContext && CcpTelemetryIsConnected() )
		{
			TracyCLockTerminate( m_tracyLockContext );
		}
		m_tracyLockContext = nullptr;
#endif
	}

	void Acquire()
	{
#if CCP_TELEMETRY_ENABLED
		EnsureTracyLockState();
		bool notifyTracy{false};
		if ( m_tracyLockContext )
		{
			notifyTracy = TracyCLockBeforeLock( m_tracyLockContext );
		}
#endif
		pthread_mutex_lock( &m_mutex);
#if CCP_TELEMETRY_ENABLED
		if ( notifyTracy && m_tracyLockContext )
		{
			TracyCLockAfterLock( m_tracyLockContext );
		}
#endif
	}

	void Release()
	{
		pthread_mutex_unlock( &m_mutex );
#if CCP_TELEMETRY_ENABLED
		EnsureTracyLockState();
		if ( m_tracyLockContext )
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
	void EnsureTracyLockState()
	{
		const bool connected = CcpTelemetryIsConnected();
		if ( m_tracyLockContext )
		{
			if ( !connected )
			{
				m_tracyLockContext = nullptr;
			}
		}
		else if ( connected )
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
