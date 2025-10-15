// Copyright © 2025 CCP ehf.

#ifndef _CCP_MEMORY_H_
#define _CCP_MEMORY_H_

#include "carbon_core_export.h"
#include <new>
#include <cstddef>

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef CCP_MEMORY_DEBUG
	// Define this to 0/1 on a project-by-project basis (or globally)
	// to disable/enable memory tracking for a module.
	// If left undefined, the default value is given here.
	#define CCP_MEMORY_DEBUG 1
#endif

// By default, CCPMemory provides overrides for operator new/delete.
// This can be disabled by defining CCP_MEMORY_REPLACE_OPERATOR_NEW as 0.
#ifndef CCP_MEMORY_REPLACE_OPERATOR_NEW
	#define CCP_MEMORY_REPLACE_OPERATOR_NEW 1
#endif

//
// Macros that application code should use for allocating and freeing memory.
// The behavior of these macros changes depending on the setting of CCP_MEMORY_DEBUG
//

#if CCP_MEMORY_DEBUG
	#define CCP_MALLOC( name, size ) CCPMallocWithTracking( size, name, __FILE__, __LINE__ )
	#define CCP_CALLOC( name, items, size ) CCPCallocWithTracking( items, size, name, __FILE__, __LINE__ )
	#define CCP_ALIGNED_MALLOC( name, size, alignment ) CCPAlignedMallocWithTracking( size, alignment, name, __FILE__, __LINE__ )
	#define CCP_REALLOC( name, p, size ) CCPReallocWithTracking( p, size, name, __FILE__, __LINE__ )
	#define CCP_ALIGNED_REALLOC( name, p, size, alignment ) CCPAlignedReallocWithTracking( p, size, alignment, name, __FILE__, __LINE__ )
	#define CCP_STRDUP( name, s ) CCPStrDupWithTracking( s, name, __FILE__, __LINE__ )
	#define CCP_WSTRDUP( name, s ) CCPWStrDupWithTracking( s, name, __FILE__, __LINE__ )
	#define CCP_FREE( p ) CCPFreeWithTracking( p )
	#define CCP_ALIGNED_FREE( p ) CCPAlignedFreeWithTracking( p )
	#define CCP_MSIZE( p ) CCPMSizeWithTracking( p )

#else

	#define CCP_MALLOC( name, size ) CCPMalloc( size )
	#define CCP_CALLOC( name, items, size ) CCPCalloc( items, size )
	#define CCP_ALIGNED_MALLOC( name, size, alignment ) CCPAlignedMalloc( size, alignment )
	#define CCP_REALLOC( name, p, size ) CCPRealloc( p, size )
	#define CCP_ALIGNED_REALLOC( name, p, size, alignment ) CCPAlignedRealloc( p, size, alignment )
	#define CCP_STRDUP( name, s ) CCPStrDup( s )
	#define CCP_WSTRDUP( name, s ) CCPWStrDup( s )
	#define CCP_FREE( p ) CCPFree( p )
	#define CCP_ALIGNED_FREE( p ) CCPAlignedFree( p )
	#define CCP_MSIZE( p ) CCPMSize( p )

#endif


//
// The following defines the public interface of Blue - doesn't depend on the setting
// of CCP_MEMORY_DEBUG
//

CARBON_CORE_API void* CCPMalloc( size_t size );
CARBON_CORE_API void* CCPCalloc( size_t items, size_t size );
CARBON_CORE_API void* CCPMallocWithGuard( size_t size );
CARBON_CORE_API void* CCPCallocWithGuard( size_t items, size_t size );
CARBON_CORE_API void* CCPAlignedMalloc( size_t size, size_t alignment );
CARBON_CORE_API void* CCPRealloc( void* p, size_t size );
CARBON_CORE_API void* CCPAlignedRealloc( void* p, size_t size, size_t alignment );
CARBON_CORE_API void CCPFree( void* p );
CARBON_CORE_API void CCPFreeWithGuard( void* p );
CARBON_CORE_API void CCPAlignedFree( void* p );
CARBON_CORE_API char* CCPStrDup( const char* s );
CARBON_CORE_API wchar_t* CCPWStrDup( const wchar_t* s);
CARBON_CORE_API size_t CCPMSize( void* p);

CARBON_CORE_API void* CCPMallocWithTracking( size_t size, const char* name, const char* file, int line );
CARBON_CORE_API void* CCPCallocWithTracking( size_t items, size_t size, const char* name, const char* file, int line );
CARBON_CORE_API void* CCPAlignedMallocWithTracking( size_t size, size_t alignment, const char* name, const char* file, int line );
CARBON_CORE_API void* CCPReallocWithTracking( void* p, size_t size, const char* name, const char* file, int line );
CARBON_CORE_API void* CCPAlignedReallocWithTracking( void* p, size_t size, size_t alignment, const char* name, const char* file, int line );
CARBON_CORE_API void  CCPFreeWithTracking( void* p );
CARBON_CORE_API void CCPAlignedFreeWithTracking( void* p );
CARBON_CORE_API char* CCPStrDupWithTracking( const char* s, const char* name, const char* file, int line  );
CARBON_CORE_API wchar_t* CCPWStrDupWithTracking( const wchar_t* s, const char* name, const char* file, int line  );
CARBON_CORE_API size_t CCPMSizeWithTracking( void* p);

CARBON_CORE_API size_t CCPMallocUsage();
CARBON_CORE_API size_t CCPMSize( void* p); //works with "tracking" too


// special code for the C++ new and delete operators

#if CCP_MEMORY_DEBUG
	//New and delete.  Use placement new for the "new" and regular delete for the delete, because
	//there is no placement delete syntax in c++
	//see http://en.wikipedia.org/wiki/Placement_syntax for placement delete info.
	#define CCP_NEW( name ) new( name, __FILE__, __LINE__ )
	#define CCP_DELETE delete

	inline void* operator new( size_t size, const char* name, const char* file, int line )
	{
		return CCPMallocWithTracking( size, name, file, line );
	}
	inline void* operator new[]( size_t size, const char* name, const char* file, int line )
	{
		return CCPMallocWithTracking( size, name, file, line );
	}

	//matching placement delete used by the compiler.
	inline void operator delete (void *p, const char* name, const char* file, int line)
	{
		return CCPFreeWithTracking( p );
	}
	inline void operator delete[] (void *p, const char* name, const char* file, int line)
	{
		return CCPFreeWithTracking( p );
	}

#else

	#define CCP_NEW( name ) new
	#define CCP_DELETE delete

#endif


// Scope guard doesn't play well with the macros, so provide a smart pointer object
// for malloc'ed buffers.
class CcpMallocBuffer
{
public:
	CcpMallocBuffer()
	: m_p( NULL )
	, m_size( 0 )
	{}

	CcpMallocBuffer( const char* name, size_t size )
	{
		m_size = size;
		m_p = static_cast<char*>( CCP_MALLOC( name, m_size ) );
		if( m_p == nullptr )
		{
			m_size = 0;
		}
	}

	~CcpMallocBuffer()
	{
		clear();
	}

	CcpMallocBuffer( CcpMallocBuffer&& other )
	{
		m_p = other.m_p;
		other.m_p = nullptr;
		m_size = other.m_size;
		other.m_size = 0;
	}

	CcpMallocBuffer& operator=( CcpMallocBuffer&& other )
	{
		if( this != &other )
		{
			clear();
			m_p = other.m_p;
			other.m_p = nullptr;
			m_size = other.m_size;
			other.m_size = 0;
		}
		return *this;
	}

	void resize( const char* name, size_t size )
	{
		if( m_size == size )
		{
			return;
		}
		void* newP = CCP_REALLOC( name, m_p, size );
		if( newP )
		{
			m_size = size;
			m_p = static_cast<char*>( newP );
		}
		else
		{
			// out of memory
			clear();
		}
	}

	char* get() const
	{
		return m_p;
	}

	size_t size() const
	{
		return m_size;
	}

	operator bool() const
	{
		return m_p != NULL;
	}

	bool empty() const
	{
		return m_p == NULL;
	}

	void clear()
	{
		CCP_FREE( m_p );
		m_p = NULL;
		m_size = 0;
	}

	void swap( CcpMallocBuffer& other )
	{
		char *t = m_p;
		m_p = other.m_p;
		other.m_p = t;

		size_t s = m_size;
		m_size = other.m_size;
		other.m_size = s;
	}

private:
	char*	m_p;
	size_t	m_size;

	CcpMallocBuffer( CcpMallocBuffer& );
	CcpMallocBuffer& operator=( const CcpMallocBuffer& );
};

class CcpAlignedMallocBuffer
{
public:
	CcpAlignedMallocBuffer()
	: m_p( NULL )
	, m_size( 0 )
	, m_alignment( 0 )
	{}

	CcpAlignedMallocBuffer( const char* name, size_t size, size_t alignment )
	{
		m_size = size;
		m_alignment = alignment;
		m_p = static_cast<char*>( CCP_ALIGNED_MALLOC( name, m_size, m_alignment ) );
		if( m_p == nullptr )
		{
			m_size = 0;
		}
	}

	~CcpAlignedMallocBuffer()
	{
		clear();
	}

	CcpAlignedMallocBuffer( CcpAlignedMallocBuffer&& other )
	{
		m_p = other.m_p;
		other.m_p = nullptr;
		m_size = other.m_size;
		m_alignment = other.m_alignment;
		other.m_size = 0;
	}

	CcpAlignedMallocBuffer& operator=( CcpAlignedMallocBuffer&& other )
	{
		if( this != &other )
		{
			clear();
			m_p = other.m_p;
			other.m_p = nullptr;
			m_size = other.m_size;
			m_alignment = other.m_alignment;
			other.m_size = 0;
		}
		return *this;
	}

	void resize( const char* name, size_t size )
	{
		if( m_size == size )
		{
			return;
		}
		void* newP = CCP_ALIGNED_REALLOC( name, m_p, size, m_alignment );
		if( newP )
		{
			m_size = size;
			m_p = static_cast<char*>( newP );
		}
		else
		{
			// out of memory
			clear();
		}
	}

	char* get() const
	{
		return m_p;
	}

	size_t size() const
	{
		return m_size;
	}

	size_t alignment() const
	{
		return m_alignment;
	}

	operator bool() const
	{
		return m_p != NULL;
	}

	bool empty() const
	{
		return m_p == NULL;
	}

	void clear()
	{
		CCP_ALIGNED_FREE( m_p );
		m_p = NULL;
		m_size = 0;
	}

	void swap( CcpAlignedMallocBuffer& other )
	{
		char *t = m_p;
		m_p = other.m_p;
		other.m_p = t;

		size_t s = m_size;
		m_size = other.m_size;
		other.m_size = s;

		size_t a = m_alignment;
		m_alignment = other.m_alignment;
		other.m_alignment = a;
	}
	char* m_p;
private:
	size_t m_size;
	size_t m_alignment;
	CcpAlignedMallocBuffer( CcpMallocBuffer& );
	CcpAlignedMallocBuffer& operator=( const CcpAlignedMallocBuffer& );
};

//A Boost library "user allocator" that allocates using these macros
//see: http://www.boost.org/doc/libs/1_40_0/libs/pool/doc/interfaces/user_allocator.html

struct CCPUserAllocator
{
	typedef size_t size_type;
	typedef ptrdiff_t difference_type;

	static char * malloc(const size_type bytes)
	{
		return (char*)CCP_MALLOC( "CCPUserAllocator", bytes);
	}
	static void free(char * const block)
	{
		CCP_FREE(block);
	}
};

// Base classes that override new and delete to use CCP_MALLOC
// Inherit from this, and "new myclass" will use the blue allocator.
#include <new>
class CcpNewAllocator
{
public:
	void *operator new(size_t s)
	{
		void *r = CCP_MALLOC("CcpNewAllocator", s);
		if (!r)
			throw std::bad_alloc();
		return r;
	}
	void *operator new(size_t s, const std::nothrow_t &)
	{
		return CCP_MALLOC("CcpNewAllocator", s);
	}
	void operator delete(void *p)
	{
		CCP_FREE(p);
	}
	void operator delete(void *p, const std::nothrow_t &)
	{
		CCP_FREE(p);
	}
	void *operator new[](size_t s)
	{
		void *r = CCP_MALLOC("CcpNewAllocator", s);
		if (!r)
			throw std::bad_alloc();
		return r;
	}
	void *operator new[] (size_t s, const std::nothrow_t &)
	{
		return CCP_MALLOC("CcpNewAllocator", s);
	}
	void operator delete[] (void *p)
	{
		CCP_FREE(p);
	}
	void operator delete[] (void *p, const std::nothrow_t &)
	{
		CCP_FREE(p);
	}
};

// A template based version that can take string arguments for the allocation
// Use like this:
//
// char alloc_str[] = "my/alloc/str";
// class MyClass : public CcpNewAllocator<alloc_str>
// {...};
//
template <const char *NAME>
class CcpNewAllocatorStr
{
public:
	void *operator new(size_t s)
	{
		void *r = CCP_MALLOC(NAME, s);
#ifdef _CPPUNWIND
		if (!r)
			throw std::bad_alloc(NAME);
#endif
		return r;
	}
	void *operator new(size_t s, std::nothrow_t)
	{
		return CCP_MALLOC(NAME, s);
	}
	void operator delete(void *p, size_t)
	{
		CCP_FREE(p);
	}
	void operator delete(void *p, size_t, std::nothrow_t)
	{
		CCP_FREE(p);
	}
	void *operator new[] (size_t s)
	{
		void *r = CCP_MALLOC(NAME, s);
#ifdef _CPPUNWIND
		if (!r)
			throw std::bad_alloc(NAME);
#endif
		return r;
	}
	void *operator new[] (size_t s, std::nothrow_t)
	{
		return CCP_MALLOC(NAME, s);
	}
	void operator delete[] (void *p, size_t)
	{
		CCP_FREE(p);
	}
	void operator delete[] (void *p, size_t, std::nothrow_t)
	{
		CCP_FREE(p);
	}
};

// A convenience macro to define New allocators
#if CCP_MEMORY_DEBUG
#define DEFINE_CCP_NEW_ALLOCATOR(Name, Str)\
	char Name ## _Str[] = #Str;\
	typedef CcpNewAllocatorStr<Name ## _Str> Name;
#else
#define DEFINE_CCP_NEW_ALLOCATOR(Name, Str)\
	typedef CcpNewAllocator Name;
#endif

struct CARBON_CORE_API CcpProcessMemoryInfo {
	size_t workingSetSize{0};
	size_t pageFileUsage{0};
	size_t pageFaultCount{0};
};

CARBON_CORE_API bool CcpGetProcessMemoryInfo( CcpProcessMemoryInfo& result );

CARBON_CORE_DEPRECATED_EXPORT bool CcpGetProcessMemoryInfo( size_t& workingSetSize, size_t& pageFileUsage );

#endif
