// Copyright © 2013 CCP ehf.

#ifndef _CCPCRASH_H
#define _CCPCRASH_H

#include <CcpLog.h>

#if _MSC_VER
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE __attribute__((noinline))
#endif

static NOINLINE void CcpCrashOnPurpose()
#ifdef __clang__
	__attribute__( ( __analyzer_noreturn__ ) )
	__attribute__( ( optnone ) )
#endif
{
	CCP_LOGERR( "About to force the process to abort" );
	std::abort();
}

#endif
