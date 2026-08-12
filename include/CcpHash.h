// Copyright © 2025 CCP ehf.

#pragma once

#ifndef _CCPHASH_H_
#define _CCPHASH_H_

#include "carbon_core_export.h"

/// See http://www.isthe.com/chongo/tech/comp/fnv/ for a description of the FNV1 hash algorithm.

// Hashes a block of memory
CARBON_CORE_API unsigned int CcpHashFNV1( const void* pInput, size_t nLength );

// Hashes a block of memory using a specified inputHash.  This can be used to concatenate hashes
//	For example: CcpHashFNV1("HashTest", 8) == CcpHashFNV1("Test", 4, CcpHashFNV1("Hash", 4))
CARBON_CORE_API unsigned int CcpHashFNV1( const void* pInput, size_t nLength, unsigned int inputHash );

#endif
