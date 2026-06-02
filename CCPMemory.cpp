// Copyright © 2025 CCP ehf.

#include "include/CCPMemory.h"
#include "include/CCPMemoryTracker.h"
#include "include/CCPAssert.h"
#include "include/CcpTelemetry.h"
#include "CcpMemoryTrackerMutex.h"

#ifdef __APPLE__
#include <malloc/malloc.h>
#include <mach/mach.h>
#include <mach/task_info.h>
#elif defined(_WIN32)
#include <psapi.h>
#else
#include <malloc.h>
#endif

// We need to initialize the memory system before any other static initializers are executed.
#ifdef _WIN32
#pragma warning(suppress:4073)
#pragma init_seg(lib)
#endif

#ifdef _WIN32
HANDLE s_heap = INVALID_HANDLE_VALUE;
#endif

// Should allocations be guarded? Note that this cannot be changed at runtime - the only
// way to enable is via a command line switch that is processed in a static initializer.
// Once any allocations have been made this cannot be changed - things will crash and burn.
bool s_guardAllocations = false;

CcpMemoryTrackerMutex& GetTrackingMutex()
{
	static CcpMemoryTrackerMutex s_trackingGuard;
	return s_trackingGuard;
}

#define IS_2_POW_N(X)   (((X)&(X-1)) == 0)
#define PTR_SZ          sizeof(void *)

// Note that the guards must maintain the alignment of the allocated block
const int CCP_MEMORY_GUARD_SIZE_FRONT = 64 - sizeof( size_t );
const int CCP_MEMORY_GUARD_SIZE_BACK = 64;
const int CCP_MEMORY_GUARD_SIZE = CCP_MEMORY_GUARD_SIZE_FRONT + CCP_MEMORY_GUARD_SIZE_BACK + sizeof( size_t );
const uint8_t CCP_MEMORY_GUARD_VALUE_FRONT = 0xee;
const uint8_t CCP_MEMORY_GUARD_VALUE_BACK = 0xef;

// Should memory tracking in Telemetry be enabled? Defaults to yes. This can skew results of
// timing analysis of functions that do a lot of allocations, such as the yaml parser.
// Set this to 0 to disable it - please don't check it in like that!
#define ENABLE_TELEMETRY_MEMORY_TRACKING CCP_TELEMETRY_ENABLED

#ifdef _WIN32
static bool ValidateHeap()
{
	if( !HeapLock( s_heap ) )
	{
		return false;
	}

	PROCESS_HEAP_ENTRY entry;
	entry.lpData = NULL;

	while( HeapWalk( s_heap, &entry ) )
	{
		if( entry.wFlags & PROCESS_HEAP_ENTRY_BUSY )
		{
			if( !HeapValidate( s_heap, 0, entry.lpData ) )
			{
				return false;
			}
		}
	}

	HeapUnlock( s_heap );

	return true;
}
#endif

void* CCPMallocWithTracking( size_t size, const char* name, const char* file, int line )
{
	CcpMemoryTrackerAutoMutex guard( GetTrackingMutex() );

	void* p = CCPMalloc( size );
	if( IsMemoryTrackingEnabled() && p )
	{
		MemoryTrackerAdd( p, size, name, file, line );
	}

	return p;
}

void* CCPCallocWithTracking( size_t nitems, size_t size, const char* name, const char* file, int line )
{
	CcpMemoryTrackerAutoMutex guard( GetTrackingMutex() );

	void* p = CCPCalloc( nitems, size );
	if( IsMemoryTrackingEnabled() && p )
	{
		MemoryTrackerAdd( p, size, name, file, line );
	}

	return p;
}

void* CCPAlignedMallocWithTracking( size_t size, size_t alignment, const char* name, const char* file, int line )
{
	CcpMemoryTrackerAutoMutex guard( GetTrackingMutex() );

	void* p = CCPAlignedMalloc( size, alignment );
	if( IsMemoryTrackingEnabled() && p )
	{
		MemoryTrackerAdd( p, size, name, file, line );
	}

	return p;
}

void CCPFreeWithTracking( void* p )
{
	CcpMemoryTrackerAutoMutex guard( GetTrackingMutex() );

	if( p )
	{
		if( !MemoryTrackerIsValid( p ) )
		{
			CCP_LOGERR( "CCPFreeWithTracking: Pointer 0x%p is not valid for the heap", p );
			CCP_DEBUG_BREAK();
		}

		MemoryTrackerRemove( p );

		CCPFree( p );
	}
}

void CCPAlignedFreeWithTracking( void* p )
{
	CcpMemoryTrackerAutoMutex guard( GetTrackingMutex() );

	if( p )
	{
		if( IsMemoryTrackingEnabled() )
		{
			MemoryTrackerRemove( p );
		}
		CCPAlignedFree( p );
	}
}

void* CCPReallocWithTracking( void* p, size_t size, const char* name, const char* file, int line )
{
	CcpMemoryTrackerAutoMutex guard( GetTrackingMutex() );

	if( p && !MemoryTrackerIsValid( p ) )
	{
		CCP_DEBUG_BREAK();
	}

	void* pNew = CCPRealloc( p, size );

	if( IsMemoryTrackingEnabled() )
	{
		if( p )
		{
			MemoryTrackerRemove( p );
		}

		if( pNew )
		{
			MemoryTrackerAdd( pNew, size, name, file, line );
		}
	}

	return pNew;
}

void* CCPAlignedReallocWithTracking( void* p, size_t size, size_t alignment, const char* name, const char* file, int line )
{
	CcpMemoryTrackerAutoMutex guard( GetTrackingMutex() );

	if( p && !MemoryTrackerIsValid( p ) )
	{
		CCP_DEBUG_BREAK();
	}

	void* pNew = CCPAlignedRealloc( p, size, alignment );

	if( IsMemoryTrackingEnabled() )
	{
		if( p )
		{
			MemoryTrackerRemove( p );
		}

		if( pNew )
		{
			MemoryTrackerAdd( pNew, size, name, file, line );
		}
	}

	return pNew;
}

size_t CCPMSizeWithTracking( void *p )
{
	CcpMemoryTrackerAutoMutex guard( GetTrackingMutex() );

	if( !MemoryTrackerIsValid( p ) )
	{
		CCP_LOGERR( "CCPMSizeWithTracking: Pointer 0x%p is not valid for the heap", p );
		CCP_DEBUG_BREAK();
	}

	return CCPMSize( p );
}

char* CCPStrDupWithTracking( const char* s, const char* name,  const char* file, int line  )
{
	CcpMemoryTrackerAutoMutex guard( GetTrackingMutex() );

	if( !s )
	{
		return NULL;
	}

	size_t size = strlen(s) + 1;
	char* s2 = (char*)CCPMallocWithTracking( size, name, file, line );
	strcpy_s( s2, size, s );
	return s2;
}

wchar_t* CCPWStrDupWithTracking( const wchar_t* s, const char* name, const char* file, int line )
{
	CcpMemoryTrackerAutoMutex guard( GetTrackingMutex() );

	if( !s )
	{
		return NULL;
	}

	size_t size = wcslen( s ) + 1;
	wchar_t* s2 = (wchar_t*)CCPMallocWithTracking( size * sizeof(wchar_t), name, file, line );
	if (s2)
		wcscpy_s( s2, size, s );
	return s2;
}

static inline size_t CalcSizeHelper( size_t items, size_t size )
{
	auto ret = items * size;
	if ( ret == 0 || ( items != ret / size ) )
	{
		return 0;
	}
	return ret;
}

#ifdef _WIN32
//memory usage statistic
#ifdef _WIN64
LONGLONG s_memuse = 0;
static inline void UpdateCount(SSIZE_T s)
{
	InterlockedExchangeAdd64(&s_memuse, (LONGLONG)s);
}
#else
LONG s_memuse = 0;
static inline void UpdateCount(SSIZE_T s)
{
	InterlockedExchangeAdd(&s_memuse, (LONG)s);
}
#endif

static inline void UpdateCount(void *p, bool inc)
{
	if (!p)
		return;
	SSIZE_T s = HeapSize(s_heap, 0, p);
	if (!inc)
		s = -s;
	UpdateCount(s);
}

static inline void* CcpPlatformMalloc( size_t size )
{
	void* p = HeapAlloc( s_heap, 0, size );
	UpdateCount( size );
#if ENABLE_TELEMETRY_MEMORY_TRACKING
	CcpTelemetryTrackAllocation( p, size );
#endif
	return p;
}

static inline void* CcpPlatformCalloc( size_t items, size_t size )
{
	auto bytes = CalcSizeHelper( items, size );
	if (bytes == 0)
	{
		return nullptr;
	}
	void* p = HeapAlloc( s_heap, HEAP_ZERO_MEMORY, bytes );
	UpdateCount( bytes );
#if ENABLE_TELEMETRY_MEMORY_TRACKING
	CcpTelemetryTrackAllocation( p, bytes );
#endif
	return p;
}

static inline void CcpPlatformFree( void* p )
{
#if ENABLE_TELEMETRY_MEMORY_TRACKING
	CcpTelemetryTrackDeallocation( p );
#endif
	UpdateCount( p, false );
	HeapFree( s_heap, 0, p );
}

#else

std::atomic<size_t> s_memuse( 0 );


static inline void* CcpPlatformMalloc( size_t size )
{
#if defined(__ANDROID__)
	size += sizeof( size_t );
#endif
	void* p = malloc( size );
    if( p )
    {
		auto realSize = CCPMSize( p );
        s_memuse += realSize;

#if ENABLE_TELEMETRY_MEMORY_TRACKING
    	CcpTelemetryTrackAllocation( p, realSize );
#endif
    }
#if defined(__ANDROID__)
	if( p )
	{
		*reinterpret_cast<size_t*>( p ) = size - sizeof( size_t );
		p = reinterpret_cast<size_t*>( p ) + 1;
	}
#endif
    return p;
}

static inline void* CcpPlatformCalloc( size_t items, size_t size )
{
	void* p = calloc( items, size );
	if ( p )
	{
		auto realSize = CCPMSize( p );
		s_memuse += realSize;

#if ENABLE_TELEMETRY_MEMORY_TRACKING
		CcpTelemetryTrackAllocation( p, realSize );
#endif
	}
	return p;
}

static inline void CcpPlatformFree( void* p )
{
#if defined(__ANDROID__)
	p = reinterpret_cast<size_t*>( p ) - 1;
#endif
#if ENABLE_TELEMETRY_MEMORY_TRACKING
	CcpTelemetryTrackDeallocation( p );
#endif
    s_memuse -= CCPMSize( p );
	free( p );
}

#endif

void *WriteMemoryGuard(size_t orgSize, void *p) {
    uintptr_t p0 = (uintptr_t)p;

    // Write original size requested
    *(size_t*)p0 = orgSize;

    uintptr_t pg = p0 + sizeof( size_t );

    // Write guard bytes in front
    memset( (void*)pg, CCP_MEMORY_GUARD_VALUE_FRONT, CCP_MEMORY_GUARD_SIZE_FRONT );

    uintptr_t p1 = (uintptr_t)p0 + CCP_MEMORY_GUARD_SIZE_FRONT + sizeof( size_t );
    uintptr_t p2 = p1 + orgSize;

    // Write guard bytes in back
    memset( (void*)p2, CCP_MEMORY_GUARD_VALUE_BACK, CCP_MEMORY_GUARD_SIZE_BACK );

    p = (void*)p1;
    return p;
}


void* CCPMallocWithGuard( size_t size )
{
	size_t orgSize = size;
	size += CCP_MEMORY_GUARD_SIZE;

	void* p = CcpPlatformMalloc( size );
	if( !p )
	{
		CCP_LOGERR( "Failed to allocate %zd bytes of memory", size );
	}

	if( p )
	{
        p = WriteMemoryGuard(orgSize, p);
	}
	return p;
}

void* CCPMalloc( size_t size )
{
	if( s_guardAllocations )
	{
		return CCPMallocWithGuard( size );
	}
	else
	{
		void* p = CcpPlatformMalloc( size );
		if( !p )
		{
			CCP_LOGERR( "Failed to allocate %zd bytes of memory", size );
		}
		return p;
	}
}

void* CCPCallocWithGuard(size_t items, size_t size )
{
	size_t orgSize = CalcSizeHelper( items, size );
	if ( orgSize == 0 ) {
		return nullptr;
	}
	size_t sizeWithGuard = orgSize + CCP_MEMORY_GUARD_SIZE;

	// Simply pretend it's one large item we allocate here, otherwise
	// calculating the guard size / offsets gets really complicated.
	void* p = CcpPlatformCalloc( 1, sizeWithGuard );
	if( !p )
	{
		CCP_LOGERR( "Failed to allocate %zd bytes of memory", size );
	}

	if( p )
	{
        p = WriteMemoryGuard(orgSize, p);

    }
	return p;
}

void* CCPCalloc( size_t items, size_t size )
{
	if( s_guardAllocations )
	{
		return CCPCallocWithGuard( items, size );
	}
	else
	{
		void* p = CcpPlatformCalloc( items, size );
		if( !p )
		{
			CCP_LOGERR( "Failed to allocate %zd bytes of memory", size );
		}
		return p;
	}
}

void CCPFreeWithGuard( void* p )
{
	if( !p )
	{
		return;
	}

	// Calculate the pointer that was originally allocated
	uintptr_t p0 = (uintptr_t)p - CCP_MEMORY_GUARD_SIZE_FRONT - sizeof( size_t );

	// Get the size
	size_t orgSize = *(size_t*)p0;

	if( orgSize == 0xdddddddd )
	{
		CCP_LOGERR( "Freeing a block that was already freed at 0x%p", p );
		CCP_DEBUG_BREAK();
	}

	uintptr_t pg = p0 + sizeof( size_t );

	uintptr_t p2 = (uintptr_t)p + orgSize;

	uint8_t* test = (uint8_t*)pg;
	for( int i = 0; i < CCP_MEMORY_GUARD_SIZE_FRONT; ++i )
	{
		if( *test != CCP_MEMORY_GUARD_VALUE_FRONT )
		{
			CCP_LOGERR( "Write outside of allocated memory detected in front of block at 0x%p", p );
			CCP_DEBUG_BREAK();
			break;
		}

		++test;
	}

	test = (uint8_t*)p2;
	for( int i = 0; i < CCP_MEMORY_GUARD_SIZE_BACK; ++i )
	{
		if( *test != CCP_MEMORY_GUARD_VALUE_BACK )
		{
			CCP_LOGERR( "Write outside of allocated memory detected after block at 0x%p", p );
			CCP_DEBUG_BREAK();
			break;
		}

		++test;
	}

	memset( (void*)p0, 0xdd, orgSize + CCP_MEMORY_GUARD_SIZE );

	// Set 'p' to the pointer originally allocated before the call to HeapFree below
	p = (void*)p0;

	CcpPlatformFree( p );
}

void CCPFree( void* p )
{
	if( p )
	{
		if( s_guardAllocations )
		{
			CCPFreeWithGuard( p );
		}
		else
		{
			CcpPlatformFree( p );
		}
	}
}

void* CCPAlignedMalloc( size_t size, size_t alignment )
{
	// Aligned allocation is done by simply allocating a larger block to allow for potentially
	// misaligned memory coming from the underlying allocation.

	// |s|___w___|p|_________data block__________|_w_|
	//
	// s -> Pointer to start of the block allocated by malloc.
	// p -> Value of s.
	// w -> Wasted memory.

	CCP_ASSERT( IS_2_POW_N( alignment ) );

	alignment = (alignment > PTR_SZ ? alignment : PTR_SZ) -1;

	uintptr_t ptr = (uintptr_t)CCPMalloc( PTR_SZ + alignment + size );
	if( !ptr )
	{
		return NULL;
	}

	uintptr_t retPtr = ((ptr + PTR_SZ + alignment) & ~alignment);
	((uintptr_t *)retPtr)[-1] = ptr;

	return (void *)retPtr;
}

void CCPAlignedFree( void* p )
{
	if( p )
	{
		uintptr_t* ptr = (uintptr_t*)p;
		void* allocatedPtr = (void*)ptr[-1];

		if( s_guardAllocations )
		{
			if( (uintptr_t)allocatedPtr == 0xdddddddd )
			{
				CCP_LOGERR( "Freeing an aligned block that was already freed at 0x%p", p );
				CCP_DEBUG_BREAK();
                return;
			}
		}
		CCPFree( allocatedPtr );
	}
}

char* CCPStrDup( const char* s  )
{
	if( !s )
	{
		return NULL;
	}

	size_t size = strlen(s) + 1;
	char* s2 = (char*)CCPMalloc( size );
	if( s2 )
	{
		strcpy_s( s2, size, s );
	}
	return s2;
}

wchar_t* CCPWStrDup( const wchar_t* s)
{
	if( !s )
	{
		return NULL;
	}
	size_t size = wcslen( s ) + 1;
	wchar_t* s2 = (wchar_t*)CCPMalloc( size*sizeof(wchar_t) );
	if( s2 )
	{
		wcscpy_s( s2, size, s );
	}
	return s2;
}

void* CCPRealloc( void* p, size_t size )
{
	if( !p )
	{
		p = CCPMalloc( size );
	}
	else
	{
		void* pNew = CCPMalloc( size );
		if( pNew )
		{
			size_t oldSize = CCPMSize( p );
			size_t toCopy = oldSize;
			if( size < oldSize )
			{
				toCopy = size;
			}
			memcpy( pNew, p, toCopy );

			CCPFree( p );
			p = pNew;
		}
		else
		{
			return nullptr;
		}
	}

	return p;
}

void* CCPAlignedRealloc( void* p, size_t size, size_t alignment )
{
	if( !p )
	{
		p = CCPAlignedMalloc( size, alignment );
	}
	else
	{
		uintptr_t* ptr = (uintptr_t*)p;
		void* allocatedPtr = (void*)ptr[-1];
		size_t oldSize = CCPMSize(allocatedPtr);
		auto offset = static_cast<uint8_t*>( p ) - static_cast<uint8_t*>( allocatedPtr );
		oldSize -= offset;

		void* pNew = CCPAlignedMalloc( size, alignment );
		if( pNew )
		{
			size_t toCopy = oldSize;
			if( size < oldSize )
			{
				toCopy = size;
			}
			memcpy( pNew, p, toCopy );

			CCPAlignedFree( p );
			p = pNew;
		}
		else
		{
			return nullptr;
		}
	}

	return p;
}

size_t CCPMSize( void *p )
{
	if( s_guardAllocations )
	{
		// Calculate the pointer that was originally allocated
		uintptr_t p0 = (uintptr_t)p - CCP_MEMORY_GUARD_SIZE_FRONT - sizeof( size_t );

		// Get the size
		size_t orgSize = *(size_t*)p0;

		return orgSize;
	}
	else
	{
#ifdef _WIN32
		return HeapSize( s_heap, 0, p );
#elif defined(__APPLE__)
		return malloc_size( p );
#elif defined(__ANDROID__)
        return reinterpret_cast<size_t*>( p )[-1];
#else
		return malloc_usable_size( p );
#endif
	}
}

size_t CCPMallocUsage()
{

	return size_t( s_memuse );
}

#ifdef _WIN32

class CcpMemoryInitializer
{
public:
	CcpMemoryInitializer()
	{
#ifdef _MSC_VER
		// sigh. simply use the crt heap - we occasionally run into issues where
		// things are allocated in one heap, freed on another, usually in
		// std::string.
		s_heap = (HANDLE)_get_heap_handle();
#else
		s_heap = HeapCreate( 0, 0, 0 );
#endif

#ifdef _MSC_VER
		ULONG heapFragValue = 2;
		HeapSetInformation( s_heap, HeapCompatibilityInformation, &heapFragValue, sizeof( heapFragValue ) );
#endif

		const wchar_t* cmdLine = GetCommandLineW();

		const wchar_t* found = wcsstr( cmdLine, L"/memoryGuards" );
		if( found )
		{
			s_guardAllocations = true;
		}
		found = wcsstr( cmdLine, L"/memoryTracking" );
		if( found )
		{
			MemoryTrackerInitialize();
		}
	}
};

static CcpMemoryInitializer initializeCcpMemory;

#endif

bool CcpGetProcessMemoryInfo( CcpProcessMemoryInfo& result )
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS mc;
    if( GetProcessMemoryInfo( GetCurrentProcess(), &mc, sizeof(mc)) )
    {
		result.pageFaultCount = mc.PageFaultCount;
		result.workingSetSize = mc.WorkingSetSize;
		result.pageFileUsage = mc.PagefileUsage;
        return true;
    }
#elif defined(__APPLE__)
	task_vm_info_data_t vmInfo;
	mach_msg_type_number_t infoCount = TASK_VM_INFO_COUNT;
	if( task_info( mach_task_self(), TASK_VM_INFO, (task_info_t)&vmInfo, &infoCount ) == KERN_SUCCESS )
	{
		result.workingSetSize = size_t( vmInfo.resident_size );
		result.pageFileUsage = size_t( vmInfo.phys_footprint );
	}
	task_events_info_data_t vmEvents;
	infoCount = TASK_EVENTS_INFO_COUNT;
	if ( task_info( mach_task_self(), TASK_EVENTS_INFO, (task_info_t)&vmEvents, &infoCount ) == KERN_SUCCESS )
	{
		result.pageFaultCount = size_t( vmEvents.faults );
		return true;
	}
#endif
    return false;
}
