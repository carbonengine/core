// Copyright © 2025 CCP ehf.

#pragma once

#ifndef TRACKABLECONTAINER_H
#define TRACKABLECONTAINER_H

#include "CCPMemory.h"
#include "CcpSecureCrt.h"
#ifdef _MSC_VER
	#include <xmemory>
#endif

template<class T>
class NamedStdAllocator : public std::allocator<T> {
public:
	NamedStdAllocator()
	{
#if CCP_MEMORY_DEBUG
		strcpy_s( m_name, sizeof( m_name ), "<Unknown>" );
#endif
	}

	NamedStdAllocator( const char* name )
	{
#if CCP_MEMORY_DEBUG
		strcpy_s( m_name, sizeof( m_name ), name );
#endif
	}

	NamedStdAllocator( const NamedStdAllocator<T>& other )
	{
#if CCP_MEMORY_DEBUG
		strcpy_s( m_name, sizeof( m_name ), other.m_name );
#endif
	}
	
	template<class OtherType>
	NamedStdAllocator(const NamedStdAllocator<OtherType>& other )
	{
#if CCP_MEMORY_DEBUG
		strcpy_s( m_name, sizeof( m_name ), other.m_name );
#endif
	}

	void SetName( const char* name )
	{
#if CCP_MEMORY_DEBUG
		strcpy_s( m_name, sizeof( m_name ), name );
#endif
	}

	T * allocate(size_t count, const void *hint = 0)
	{
		// If CCP_MEMORY_DEBUG is not enabled, the name is omitted
		return static_cast<T*>( CCP_MALLOC( m_name, count * sizeof( T ) ) );
	}

	void deallocate( T* ptr, size_t )
	{
		CCP_FREE( ptr );
	}

#if CCP_MEMORY_DEBUG
	char m_name[64];
#endif
public:
	template<class OtherType>
	struct rebind
	{	// convert an allocator<_Ty> to an allocator <_Other>
		typedef NamedStdAllocator<OtherType> other;
	};
};

// Some subclassing of frequently used stl containers, to use NamedStdAllocator.

#include <vector>
template<
	class T,
	class Alloc = NamedStdAllocator<T>
>
class TrackableStdVector : public std::vector<T, Alloc>
{
public:
	explicit TrackableStdVector( const char* name ) 
		: std::vector<T, Alloc>( NamedStdAllocator<T>( name ) )
	{
	}

	TrackableStdVector( const char* name, size_t size, const T& t = T() ) 
		: std::vector<T, Alloc>( size, 
								 t,
								 NamedStdAllocator<T>( name ) )
	{
	}
};

#include <deque>
template<
	class Key,
	class Alloc = NamedStdAllocator<Key>
>
class TrackableStdDeque : public std::deque<Key, Alloc>
{
public:
	explicit TrackableStdDeque( const char* name ) : std::deque<Key, Alloc>( NamedStdAllocator<Key>( name ) )
	{
	}

	TrackableStdDeque( const char* name, size_t size) : std::deque<Key, Alloc>( NamedStdAllocator<Key>( name ), size )
	{
	}
};

#include <stack>
template<
	class Key,
	class Alloc = NamedStdAllocator<Key>
>
class TrackableStdStack : public std::stack<Key, TrackableStdDeque<Key, Alloc> >
{
public:
	explicit TrackableStdStack( const char* name = "<unknown stack>" ) : std::stack<Key, TrackableStdDeque<Key, Alloc> >( TrackableStdDeque<Key, Alloc>( name ) )
	{
	}

	void SetName( const char* name )
	{
		// looks like we can rely on accessing c here -- 23.6.5.2 in n3337.
		std::stack<Key, TrackableStdDeque<Key, Alloc> >::c.get_allocator().SetName( name );
	}
};

#include <list>
template<
	class Type,
	class Alloc = NamedStdAllocator<Type> >
class TrackableStdList : public std::list<Type, Alloc>
{
public:
	explicit TrackableStdList( const char* name ) : std::list<Type, Alloc>( NamedStdAllocator<Type>( name ) )
	{
	}
};

#include <set>
template<
	class Key,
	class Traits = std::less<Key>,
	class Alloc = NamedStdAllocator<Key>
>
class TrackableStdSet : public std::set<Key, Traits, Alloc>
{
public:
	explicit TrackableStdSet( const char* name ) : std::set<Key, Traits, Alloc>( Traits(), NamedStdAllocator<Key>( name ) )
	{
	}
};

template<
	class Key,
	class Traits = std::less<Key>,
	class Alloc = NamedStdAllocator<Key> >
class TrackableStdMultiset : public std::multiset<Key, Traits, Alloc>
{
public:
	explicit TrackableStdMultiset( const char* name ) : std::multiset<Key, Traits, Alloc>( Traits(), NamedStdAllocator<Key>( name ) )
	{
	}
};

#include <map>
template<
	class Key,
	class Type, 
	class Traits = std::less<Key>,
	class Alloc = NamedStdAllocator< std::pair<const Key, Type> > >
class TrackableStdMap : public std::map<Key, Type, Traits, Alloc>
{
public:
	explicit TrackableStdMap( const char* name ) : std::map<Key, Type, Traits, Alloc>( Traits(), NamedStdAllocator<Key>( name ) )
	{
	}
};

#include <functional>
#include <unordered_set>

template<
	class Key,
	class Hasher = std::hash<Key>,
	class Pred = std::equal_to<Key>,
	class Alloc = NamedStdAllocator<Key> >
class TrackableStdHashSet : public std::unordered_set<Key, Hasher, Pred, Alloc>
{
public:
	explicit TrackableStdHashSet( const char* name ) : std::unordered_set<Key, Hasher, Pred, Alloc>( 1, Hasher(), Pred(), NamedStdAllocator<Key>( name ) )
	{
	}
};

#include <unordered_map>

template<
	class Key,
	class Type,
	class Hasher = std::hash<Key>,
class Alloc = NamedStdAllocator<std::pair<const Key, Type>> >
class TrackableStdHashMap : public std::unordered_map<Key, Type, Hasher, std::equal_to<Key>, Alloc>
{
public:
	explicit TrackableStdHashMap( const char* name ) : 
		std::unordered_map<Key, Type, Hasher, std::equal_to<Key>, Alloc>( 10, Hasher(), std::equal_to<Key>(), NamedStdAllocator<Key>( name ) )
	{
	}
};

template<	class Kty,
			class Ty,
			class Hasher = std::hash<Kty>,
			class Keyeq  = std::equal_to<Kty>,
			class Alloc  = NamedStdAllocator<std::pair<const Kty, Ty> > >
class TrackableStdUnorderedMap : public std::unordered_map<Kty, Ty, Hasher, Keyeq, Alloc>
{
public:
	explicit TrackableStdUnorderedMap( const char* name ) : std::unordered_map<Kty, Ty, Hasher, Keyeq, Alloc>( 3, Hasher(), Keyeq(), NamedStdAllocator<Kty>( name ) )
	{
	}
};

template <typename T>
struct TrackableDelete
{
	TrackableDelete()
	{
	}

	template<class T2>
	TrackableDelete( const TrackableDelete<T2>& )
	{
	}

	void operator()( T *ptr ) const
	{
		if( 0 < sizeof( T ) )
		{
			CCP_DELETE ptr;
		}
	}
};

template<class T>
struct TrackableDelete<T[]>
{
	TrackableDelete()
	{
	}

	template<class U> 
	TrackableDelete<T[]>( const TrackableDelete<U[]>& )
	{
	}

	void operator()( T *ptr ) const
	{
		if( 0 < sizeof( T ) )
		{
			CCP_DELETE[] ptr;
		}
	}
};

#endif
