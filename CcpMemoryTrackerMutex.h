// Copyright © 2014 CCP ehf.


#pragma once
#ifndef CcpMemoryTrackerMutex_h
#define CcpMemoryTrackerMutex_h

// Note that we can't use the regular CcpMutex for the memory tracking as it allocates memory when
// registering itself for Telemetry.
#ifdef _WIN32

#include <winbase.h>
#include "CcpMutex.h"

class CcpMemoryTrackerMutex
{
public:
	CcpMemoryTrackerMutex()
	{
		InitializeCriticalSectionAndSpinCount( &m_mutex, 0 );
	}

	~CcpMemoryTrackerMutex()
	{
		// We are intentionally _not_ deleting the CRITICAL_SECTION here.
		// The CcpMemoryTrackerMutex is used as a singleton, static variable,
		// and as such there can be crashes on shutdown if this static variable gets cleaned up
		// before other static variables which free memory during their destructors.
	}

	void Acquire()
	{
		EnterCriticalSection( &m_mutex);
	}

	void Release()
	{
		LeaveCriticalSection( &m_mutex );
	}

private:
	// Don't allow assignment
	CcpMemoryTrackerMutex( const CcpMutex& other )
	{
	}


private:
	CRITICAL_SECTION m_mutex;
};
#else

#include "CcpMutex.h"
#include <pthread.h>

class CcpMemoryTrackerMutex
{
public:
	CcpMemoryTrackerMutex()
	{
		pthread_mutexattr_t mutexAttr;
		pthread_mutexattr_init( &mutexAttr );
		pthread_mutexattr_settype( &mutexAttr, PTHREAD_MUTEX_RECURSIVE );

		pthread_mutex_init( &m_mutex, &mutexAttr );

		pthread_mutexattr_destroy( &mutexAttr );
	}

	~CcpMemoryTrackerMutex()
	{
		pthread_mutex_destroy( &m_mutex );
	}

	void Acquire()
	{
		pthread_mutex_lock( &m_mutex);
	}

	void Release()
	{
		pthread_mutex_unlock( &m_mutex );
	}

private:
	// Don't allow assignment
	CcpMemoryTrackerMutex( const CcpMutex& other )
	{
	}


private:
	pthread_mutex_t m_mutex;
};

#endif

class CcpMemoryTrackerAutoMutex
{
public:
	CcpMemoryTrackerAutoMutex( CcpMemoryTrackerMutex& m ) : 
	  m_mutex( m )
	  {
		  m_mutex.Acquire();
	  }

	  ~CcpMemoryTrackerAutoMutex()
	  {
		  m_mutex.Release();
	  }

private:
	CcpMemoryTrackerMutex& m_mutex;
};



#endif // CcpMemoryTrackerMutex_h
