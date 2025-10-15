// Copyright © 2013 CCP ehf.
//
// Wrapper for std::atomic. Since at the time of writing not all platforms we'd
// like to support provide std::atomic we 

#pragma once
#ifndef CcpAtomic_h
#define CcpAtomic_h

#ifdef _MSC_VER
	#if _MSC_VER >= 1700
		#define CCP_HAS_ATOMIC 1
	#endif
#endif

#ifdef __GNUG__
	#if __GNUC__ == 4 && __GNUC_MINOR__ >= 7
		#define CCP_HAS_ATOMIC 1
	#endif
#endif

#ifdef __clang__
#define CCP_HAS_ATOMIC 1
#endif

#if CCP_HAS_ATOMIC

#include <atomic>
#define CcpAtomic std::atomic

#else

#ifdef _WIN32

template <typename T>
class CcpAtomic
{};

template <>
class CcpAtomic<uint32_t>
{
public:
	CcpAtomic()
	{
	}

	CcpAtomic( uint32_t initialValue )
	{
		store( initialValue );
	}

	uint32_t operator=( uint32_t desired )
	{
		uint32_t oldValue = InterlockedExchange( &m_value, desired );
		return oldValue;
	}

	// Pre-increment
	uint32_t operator++()
	{
		uint32_t oldValue = InterlockedIncrement( &m_value );
		return oldValue;
	}

	// Post-increment
	uint32_t operator++(int)
	{
		uint32_t oldValue = InterlockedIncrement( &m_value );
		return oldValue - 1;
	}

	// Pre-decrement
	uint32_t operator--()
	{
		uint32_t oldValue = InterlockedDecrement( &m_value );
		return oldValue;
	}

	// Post-decrement
	uint32_t operator--(int)
	{
		uint32_t oldValue = InterlockedDecrement( &m_value );
		return oldValue + 1;
	}

	operator uint32_t() const
	{
		uint32_t curValue = InterlockedCompareExchange( const_cast<uint32_t*>( &m_value ), 0, 0 );
		return curValue;
	}

	uint32_t operator+=( uint32_t arg )
	{
		uint32_t oldValue = InterlockedExchangeAdd( &m_value, arg );
		return oldValue + arg;
	}

	void store( uint32_t val )
	{
		InterlockedExchange( &m_value, val );
	}

	bool compare_exchange_strong( uint32_t& expected, uint32_t desired )
	{
		auto prev = InterlockedCompareExchange( &m_value, desired, expected );
		if( prev == expected )
		{
			return true;
		}
		expected = prev;
		return false;
	}

private:
	uint32_t m_value;

private:
	CcpAtomic(const CcpAtomic&);	// not defined
	CcpAtomic& operator=(const CcpAtomic&);	// not defined
};
#endif

#endif

#endif // CcpAtomic_h