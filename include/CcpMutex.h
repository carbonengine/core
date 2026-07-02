// Copyright © 2025 CCP ehf.

#ifndef CCPMUTEX_H
#define CCPMUTEX_H

#include <string>

#include "CcpAtomic.h"
#include "CcpTelemetry.h"

class CcpMutex
{
public:
	CARBON_CORE_API CcpMutex( const char* owner, const char* name, unsigned spinCount = 0 );
	CARBON_CORE_API ~CcpMutex();

	CARBON_CORE_API void Acquire();
	CARBON_CORE_API void Release();

	CARBON_CORE_API void SetOwner( const char* owner );
	CARBON_CORE_API void SetName( const char* name );

	// Don't allow copy/assignment
	CcpMutex( const CcpMutex& ) = delete;
	CcpMutex& operator=( const CcpMutex& ) = delete;

private:
#if CCP_TELEMETRY_ENABLED
	// Lazily announce mutex/lock as long as telemetry is connected and lock
	// tracking is enabled. Subsequent calls are no-ops once a context exists.
	void EnsureTelemetryLockAnnounced();

	// Opaque pointer to TracyCLockCtx, kept as void* so this header does not
	// need to pull in Tracy headers.
	void* m_tracyLockContext{ nullptr };
#endif

	// Opaque pointer to the platform-native mutex primitive
	// (CRITICAL_SECTION on Windows, pthread_mutex_t elsewhere).
	void* m_mutexHandle{ nullptr };

	// Owned copies of the owner/name strings.
	std::string m_owner;
	std::string m_name;
};


class CcpAutoMutex
{
public:
	CARBON_CORE_API explicit CcpAutoMutex( CcpMutex& m );
	CARBON_CORE_API ~CcpAutoMutex();

	// Release early
	CARBON_CORE_API void Release();

	CcpAutoMutex( const CcpAutoMutex& ) = delete;
	CcpAutoMutex& operator=( const CcpAutoMutex& ) = delete;

private:
	CcpMutex& m_mutex;
	bool m_released;
};


class CcpSpinLock
{
public:
	// Preferred constructor — accepts a name used to identify the spin-lock in Tracy.
	CARBON_CORE_API explicit CcpSpinLock( const char* spinLockName );

	[[deprecated( "Use `CcpSpinLock( const char* spinLockName )` instead" )]]
	CARBON_CORE_API CcpSpinLock();

	CARBON_CORE_API ~CcpSpinLock();

	CARBON_CORE_API void Acquire();
	CARBON_CORE_API void Release();

	CcpSpinLock( const CcpSpinLock& ) = delete;
	CcpSpinLock& operator=( const CcpSpinLock& ) = delete;

private:
#if CCP_TELEMETRY_ENABLED
	// Lazily announce the spin-lock to Tracy.
	void EnsureTelemetryLockAnnounced();

	// Opaque pointer to TracyCLockCtx, kept as void* so this header does not
	// need to pull in Tracy headers.
	void* m_tracyLockContext{ nullptr };
#endif

	CcpAtomic<uint32_t> m_lock;
	std::string m_spinLockName;
};


class CcpAutoSpinLock
{
public:
	CARBON_CORE_API explicit CcpAutoSpinLock( CcpSpinLock& m );
	CARBON_CORE_API ~CcpAutoSpinLock();

	// Release early
	CARBON_CORE_API void Release();

	CcpAutoSpinLock( const CcpAutoSpinLock& ) = delete;
	CcpAutoSpinLock& operator=( const CcpAutoSpinLock& ) = delete;

private:
	CcpSpinLock& m_mutex;
	bool m_released;
};

#endif
