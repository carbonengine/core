// Copyright © 2013 CCP ehf.

#ifndef _CCPCRASH_H
#define _CCPCRASH_H

#include <CCPLog.h>

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
	CCP_LOGERR( "About to throw an exception that will kill the current process." );
#ifndef __clang_analyzer__

#if __APPLE__
	volatile int *i = nullptr;
	CCP_LOG( "This is how *0 looks like: %i", *i );
#else
	volatile int i = 0;
	int j = 1 / i;  // cppcheck-suppress zerodiv
	CCP_LOG( "This is how 1 / 0  looks like: %i", j );
#endif

#endif
}

#endif
