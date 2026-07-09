// Copyright © 2025 CCP ehf.

#ifndef CCPMUTEX_H
#define CCPMUTEX_H

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
	struct Private;
	std::unique_ptr<Private> m_impl;
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
	struct Private;
	std::unique_ptr<Private> m_impl;
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
