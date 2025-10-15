// Copyright © 2025 CCP ehf.

#include "include/CCPHash.h"

/// See http://www.isthe.com/chongo/tech/comp/fnv/ for a description of the FNV1 hash algorithm
/// and the specific values used here.

const unsigned int FNV1_INITIAL_VALUE = 2166136261U;
const unsigned int FNV1_PRIME = 16777619U;


unsigned int CcpHashFNV1( const void* pInput, size_t nLength )
{
	return CcpHashFNV1( pInput, nLength, FNV1_INITIAL_VALUE );
}

unsigned int CcpHashFNV1( const void* pInput, size_t nLength, unsigned int inputHash )
{
	const int8_t* p = static_cast<const int8_t*>( pInput );
	const int8_t* pEnd = p + nLength;

	unsigned int hash = inputHash;
	while( p < pEnd )
	{
		hash *= FNV1_PRIME;
		hash ^= *p;
		++p;
	}
	return hash;
}

