// Copyright © 2013 CCP ehf.

#include "include/CcpSemaphore.h"
#include "include/CCPAssert.h"

#ifdef _WIN32

CcpSemaphore::CcpSemaphore()
{
	m_semaphore = ::CreateSemaphore( 0, 0, 1, 0 );
}

CcpSemaphore::CcpSemaphore( uint32_t initialCount, uint32_t maximumCount )
{
	m_semaphore = ::CreateSemaphore( 0, initialCount, maximumCount, 0 );
}


CcpSemaphore::~CcpSemaphore()
{
	::CloseHandle( m_semaphore );
}

bool CcpSemaphore::Wait()
{
	return ::WaitForSingleObject( m_semaphore, INFINITE ) == 0;
}

bool CcpSemaphore::TimedWait( uint32_t timeout )
{
	return ::WaitForSingleObject( m_semaphore, timeout ) == 0;
}

void CcpSemaphore::Signal()
{
	::ReleaseSemaphore( m_semaphore, 1, 0 );
}

#elif defined(__APPLE__)

#include <mach/semaphore.h>
#include <mach/mach.h>

CcpSemaphore::CcpSemaphore()
{
    semaphore_create( current_task(), &m_semaphore, SYNC_POLICY_FIFO, 0 );
}

CcpSemaphore::CcpSemaphore( uint32_t initialCount, uint32_t maximumCount )
{
    semaphore_create( current_task(), &m_semaphore, SYNC_POLICY_FIFO, initialCount );
}


CcpSemaphore::~CcpSemaphore()
{
    semaphore_destroy( current_task(), m_semaphore );
}

#include <errno.h>

bool CcpSemaphore::Wait()
{
    return semaphore_wait( m_semaphore ) == KERN_SUCCESS;
}

bool CcpSemaphore::TimedWait( uint32_t timeoutInMs )
{
	mach_timespec_t mts;
    mts.tv_sec = timeoutInMs / 1000;
    mts.tv_nsec = ( timeoutInMs % 1000 ) * 1000000;
    
    return semaphore_timedwait( m_semaphore, mts ) == KERN_SUCCESS;
}

void CcpSemaphore::Signal()
{
    semaphore_signal( m_semaphore );
}

#else

CcpSemaphore::CcpSemaphore()
{
	sem_init( &m_semaphore, 0, 0 );
}

CcpSemaphore::CcpSemaphore( uint32_t initialCount, uint32_t maximumCount )
{
	sem_init( &m_semaphore, 0, initialCount );
}


CcpSemaphore::~CcpSemaphore()
{
	sem_destroy( &m_semaphore );
}

#include <errno.h>

bool CcpSemaphore::Wait()
{
    if( sem_wait( &m_semaphore ) == 0 )
    {
        return true;
    }
    return false;
}

bool CcpSemaphore::TimedWait( uint32_t timeoutInMs )
{
    timespec ts;
    ts.tv_sec = timeoutInMs / 1000;
    ts.tv_nsec = (timeoutInMs % 1000) * 1000000;
    if( sem_timedwait( &m_semaphore, &ts ) == 0 )
    {
        return true;
    }
    return false;
}

void CcpSemaphore::Signal()
{
	sem_post( &m_semaphore );
}

#endif
