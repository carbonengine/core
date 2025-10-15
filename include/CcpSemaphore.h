// Copyright © 2013 CCP ehf.

#pragma once

#ifndef CcpSemaphore_h
#define CcpSemaphore_h

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach/task.h>
#else
#include <semaphore.h>
#endif

#include "carbon_core_export.h"

// Simple wrapper for a semaphore
class CARBON_CORE_API CcpSemaphore
{
public:
	CcpSemaphore();
	CcpSemaphore( uint32_t initialCount, uint32_t maximumCount );
	~CcpSemaphore();

	bool Wait();
	bool TimedWait( uint32_t timeoutInMs );
	void Signal();

private:
#ifdef _WIN32
	HANDLE m_semaphore;
#elif defined(__APPLE__)
    semaphore_t m_semaphore;
#else
	sem_t m_semaphore;
#endif

};
#endif // CcpSemaphore_h
