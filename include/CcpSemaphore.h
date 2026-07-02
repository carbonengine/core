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

#include "CcpTelemetry.h"

// Simple wrapper for a semaphore
class CARBON_CORE_API CcpSemaphore
{
public:
	// Preferred constructors, containing semaphoreName (an identifier used by telemetry tool)
	// Note: The preferred constructors intentionally DON'T use default parameters.
	// This is to avoid ambiguous overloads with the deprecated constructors below.
	// A constructor with (const char* semaphoreName, uint32_t initialCount=0, uint32_t maximumCount=1)
	// i.e. last two default parameters, would make calls such as `CcpSemaphore( 0, 1 )`,
	// on the deprecated constructor, ambiguous (because the literal `0` is a null pointer
	// convertible to `const char*`).
	// Splitting the overloads avoids that ambiguity while keeping source compatibility.
	CcpSemaphore( const char* semaphoreName, uint32_t initialCount, uint32_t maximumCount );
	explicit CcpSemaphore( const char* semaphoreName );

	[[deprecated( "Use `CcpSemaphore( const char* semaphoreName, ... )` instead" )]]
	CcpSemaphore();

	[[deprecated( "Use `CcpSemaphore( const char* semaphoreName, ... )` instead" )]]
	CcpSemaphore( uint32_t initialCount, uint32_t maximumCount );

	~CcpSemaphore();

	bool Wait();
	bool TimedWait( uint32_t timeoutInMs );
	void Signal();

	// Don't allow copy/assignment
	CcpSemaphore( const CcpSemaphore& ) = delete;
	CcpSemaphore& operator=( const CcpSemaphore& ) = delete;

private:
#if CCP_TELEMETRY_ENABLED
	// Lazily announce the semaphore to Tracy. Subsequent calls are no-ops once a context exists.
	void EnsureTelemetryLockAnnounced();

	// Opaque pointer to TracyCLockCtx, kept as void* so this header does not
	// need to pull in Tracy headers.
	void* m_tracyLockContext{ nullptr };
#endif

#ifdef _WIN32
	HANDLE m_semaphore;
#elif defined(__APPLE__)
	semaphore_t m_semaphore;
#else
	sem_t m_semaphore;
#endif

	const char* m_semaphoreName{ nullptr };
};
#endif // CcpSemaphore_h
