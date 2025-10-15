// Copyright © 2013 CCP ehf.

#pragma once
#ifndef CcpProcess_h
#define CcpProcess_h

#include "carbon_core_export.h"
#include <cstdint>

#ifdef _WIN32

#include <windows.h>
typedef DWORD CcpProcessId_t;

#else

#include <sys/types.h>
#include <unistd.h>

typedef pid_t CcpProcessId_t;

#endif

CARBON_CORE_API CcpProcessId_t CcpGetCurrentProcessId();

// Time returned should be interpreted as 100-nanosecond intervals,
// however the time resolution between periodic timer updates is OS dependent.
// On my Windows machine the resolution is ca. 16ms, which is consistent with
// the thread time slice duration when measuring the duration of Sleep(1).
// https://learn.microsoft.com/en-us/windows/win32/procthread/multitasking
CARBON_CORE_API bool CcpGetProcessTimes( int64_t& kernelTime, int64_t& userTime );

#endif // CcpProcess_h
