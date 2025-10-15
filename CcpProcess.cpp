// Copyright © 2013 CCP ehf.

#include "include/CcpProcess.h"

#if defined( _WIN32 )
CcpProcessId_t CcpGetCurrentProcessId()
{
	return GetCurrentProcessId();
}

bool CcpGetProcessTimes( int64_t& kernelTime, int64_t& userTime )
{
    FILETIME dummy;
	return GetProcessTimes( GetCurrentProcess(), &dummy, &dummy, (LPFILETIME)&kernelTime, (LPFILETIME)&userTime ) != 0;
}

#else

#include <sys/resource.h>

CcpProcessId_t CcpGetCurrentProcessId()
{
	return getpid();
}

bool CcpGetProcessTimes( int64_t& kernelTime, int64_t& userTime )
{
    struct rusage usage;
    if( !getrusage( RUSAGE_SELF, &usage ) )
    {
        kernelTime = int64_t( usage.ru_stime.tv_sec ) * 10000000 + int64_t( usage.ru_stime.tv_usec ) * 10;
        userTime = int64_t( usage.ru_utime.tv_sec ) * 10000000 + int64_t( usage.ru_utime.tv_usec ) * 10;
        return true;
    }
    return false;
}

#endif
