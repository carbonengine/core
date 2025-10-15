// Copyright © 2011 CCP ehf.

#pragma once
#ifndef CachedAllocator_h
#define CachedAllocator_h

#include "CCPMemory.h"

// CachedAllocator is a template class that provides cached allocations for its template argument.
//
// Cached allocations are intended to reduce the overhead of allocations through the generic
// memory allocation mechanism. Memory for multiple instances is allocated through CCP_MALLOC at
// once - a given amount at startup and if that runs out, further blocks are added. Memory is never
// returned - deleting an object with cached allocations means it goes on to the free list for
// this class. Allocations and deletions are therefore much faster.
//
// The usage of the allocator is simplified through the use of accompanying macros, USE_CACHED_ALLOCATOR
// and DEFINE_CACHED_ALLOCATOR. The first one must be put within the class definition, the second one
// somewhere in a cpp file to initialize the allocator.
//
// Example:
//
// MyClass.h:
// class MyClass
// {
// public:
//      ...
//      USE_CACHED_ALLOCATOR( MyClass )
//      ...
// };
//
// MyClass.cpp:
// ...
// DEFINE_CACHED_ALLOCTOR( MyClass, 1024, 256 );
//
// This sets MyClass up for using a cached allocator and sets up a pool with room for 1024 MyClass instances.
// Once those 1024 are used up (and whenever there are no instances available to be reused), memory for 256
// further instances is allocated.

#include "CCPAssert.h"

template <class T>
class CachedAllocator
{
public:
	static void Initialize( size_t initialAvailCount, size_t growBy, const char* name )
	{
		static CachedAllocator<T> instance( initialAvailCount, growBy, name );
		s_instance = &instance;
	}

	static CachedAllocator<T>* GetInstance()
	{
		// If there's no instance you probably are missing DEFINE_CACHED_ALLOCATOR
		CCP_ASSERT( s_instance ); 

		return s_instance;
	}

	CachedAllocator( size_t initialAvailCount, size_t growBy, const char* name ) : 
	m_name( name ),
		m_freeList( nullptr ),
		m_growBy( growBy ),
		m_initialCount( initialAvailCount ),
		m_allocatedCount( 0 ),
		m_usedCount( 0 ),
		m_blocks( nullptr ),
		m_blockCount( 0 )
	{
		GrowFreeList( initialAvailCount );
	}

	void* Allocate( size_t size )
	{
		CCP_ASSERT( size == sizeof( T ) );

		if( !m_freeList )
		{
			GrowFreeList( m_growBy );
		}

		void* p = m_freeList;
        if( m_freeList )
        {
            m_freeList = m_freeList->next;
            ++m_usedCount;
        }
		return p;
	}

	void Free( void* p, size_t size )
	{
		CCP_ASSERT( size == sizeof( T ) );

		Entry* entry = static_cast<Entry*>( p );
		entry->next = m_freeList;
		m_freeList = entry;
		--m_usedCount;
	}

	void GetMemoryUsage( size_t& used, size_t& allocated )
	{
		used = m_usedCount * sizeof( T );
		allocated = m_allocatedCount * sizeof( T );
	}

	void Compact()
	{
		if( m_blockCount < 2 )
		{
			return;
		}

		size_t* counts = CCP_NEW( "CachedAllocator::Compact::counts" ) size_t[m_blockCount];
		if( !counts )
		{
			return;
		}
		for( size_t i = 0; i < m_blockCount; ++i )
		{
			counts[i] = 0;
		}
		auto entry = m_freeList;
		while( entry )
		{
			for( size_t i = 1; i < m_blockCount; ++i )
			{
				if( IsEntryInBlock( entry, m_blocks[i] ) )
				{
					++counts[i];
					break;
				}
			}
			entry = entry->next;
		}
		size_t newBlockCount = 1;
		for( size_t i = 1; i < m_blockCount; ++i )
		{
			if( counts[i] == m_growBy )
			{
				RemoveBlock( m_blocks[i] );
			}
			else
			{
				++newBlockCount;
			}
		}
		if( newBlockCount != m_blockCount )
		{
			uint8_t** newBlocks = static_cast<uint8_t**>( CCP_MALLOC( m_name, newBlockCount * sizeof( uint8_t* ) ) );
			size_t dest = 0;
			for( size_t i = 0; i < m_blockCount; ++i )
			{
				if( i == 0 || counts[i] < m_growBy )
				{
					newBlocks[dest++] = m_blocks[i];
				}
				else
				{
					CCP_FREE( m_blocks[i] );
					m_allocatedCount -= m_growBy;
				}
			}
			CCP_FREE( m_blocks );
			m_blocks = newBlocks;
			m_blockCount = newBlockCount;
		}
		CCP_FREE( counts );
	}

private:
	struct Entry
	{
		Entry* next;
	};

	bool IsEntryInBlock( const Entry* entry, const uint8_t* block ) const
	{
		size_t size = m_growBy * sizeof( T );
		auto pointer = reinterpret_cast<const uint8_t*>( entry );
		return pointer >= block && pointer < block + size;
	}

	void RemoveBlock( const uint8_t* block )
	{
		while( m_freeList && IsEntryInBlock( m_freeList, block ) )
		{
			m_freeList = m_freeList->next;
		}
        if( m_freeList )
        {
            auto entry = m_freeList;
            while( entry->next )
            {
                if( IsEntryInBlock( entry->next, block ) )
                {
                    entry->next = entry->next->next;
                }
                else
                {
                    entry = entry->next;
                }
            }
        }
	}

	void GrowFreeList( size_t growBy )
	{
		size_t sizeToAllocate = sizeof( T ) * growBy;
		void* block = CCP_MALLOC( m_name, sizeToAllocate );

		CCP_ASSERT( block );
		if( !block )
		{
			return;
		}

		auto newBlocks = static_cast<uint8_t**>( CCP_REALLOC( m_name, m_blocks, ( m_blockCount + 1 ) * sizeof( uint8_t* ) ) );
		CCP_ASSERT( newBlocks );
		if( !newBlocks )
		{
			CCP_FREE( block );
			return;
		}
		m_blocks = newBlocks;
		m_blocks[m_blockCount] = static_cast<uint8_t*>( block );
		++m_blockCount;

		uintptr_t p = (uintptr_t)block;

		for( size_t i = 0; i < growBy; ++i )
		{
			Entry* entry = reinterpret_cast<Entry*>( p );
			entry->next = m_freeList;
			m_freeList = entry;

			p += sizeof( T );
		}
		m_allocatedCount += growBy;
	}

	static CachedAllocator<T>* s_instance;

	const char* m_name;
	Entry* m_freeList;
	size_t m_growBy;
	size_t m_initialCount;
	size_t m_allocatedCount;
	size_t m_usedCount;
	size_t m_blockCount;
	uint8_t** m_blocks;
};


template<class T> CachedAllocator<T>* CachedAllocator<T>::s_instance = nullptr;


#define USE_CACHED_ALLOCATOR( T ) \
void* operator new( size_t size ) \
{ \
	return CachedAllocator<T>::GetInstance()->Allocate( size ); \
} \
\
void operator delete( void* p, size_t size ) \
{ \
	CachedAllocator<T>::GetInstance()->Free( p, size ); \
}

#define DEFINE_CACHED_ALLOCATOR( T, initialAvailCount, growBy ) \
class CachedAllocatorInitializer##T \
{ \
public:\
	CachedAllocatorInitializer##T() \
	{ \
		CachedAllocator<T>::Initialize( initialAvailCount, growBy, "cached " #T ); \
	} \
}; \
template<> CachedAllocator<T>* CachedAllocator<T>::s_instance = nullptr; \
CachedAllocatorInitializer##T s_cachedAllocatorInitializer##T;

#endif
