// Copyright © 2025 CCP ehf.

#ifndef _CCPMEMORYTRACKER_H_
#define _CCPMEMORYTRACKER_H_

#include <cstdio>

#include "carbon_core_export.h"

CARBON_CORE_API void MemoryTrackerInitialize();
CARBON_CORE_API void MemoryTrackerShutdown();

// The following functions are used by the memory allocation functions in CCPMemory.h

CARBON_CORE_API void MemoryTrackerDumpReportAsText( const char* filename );
CARBON_CORE_API void MemoryTrackerDumpReportAsBinary( const char* filename );
CARBON_CORE_API bool IsMemoryTrackingEnabled();
CARBON_CORE_API void MemoryTrackerAdd( void* p, size_t size, const char* pName, const char* pFileName, int line );
CARBON_CORE_API void MemoryTrackerRemove( void* p );
CARBON_CORE_API bool MemoryTrackerIsValid( void* p );

CARBON_CORE_API bool MemoryTrackerGetCount( size_t& count );
CARBON_CORE_API bool MemoryTrackerGetSize( size_t& size );
CARBON_CORE_API void SetCallStackCaptureEnabled( bool enabled );
CARBON_CORE_API bool IsCallstackCaptureEnabled();

CARBON_CORE_API void MemoryTrackerSummaryReportToFile( FILE* file );

#ifdef _WIN32
CARBON_CORE_API size_t GetHeapSizeWithHeapWalk( HANDLE heap );
CARBON_CORE_API HANDLE MemoryTrackerGetHeapForTracking();
#endif

#endif