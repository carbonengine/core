// Copyright © 2025 CCP ehf.

#include "gtest/gtest.h"
#include "CCPHash.h"

TEST ( CCPHash, TestHashEmptyString )
{
	unsigned int expected = 2166136261;
	unsigned int actual = CcpHashFNV1( "", 0 );
	EXPECT_EQ( expected, actual );
}

TEST ( CCPHash, TestHashesCorrectly )
{
	unsigned int expected = 974921845;
	unsigned int actual = CcpHashFNV1( "Hash", 4 );
	EXPECT_EQ( expected, actual );
}

TEST ( CCPHash, TestConcatenateHashes )
{
	unsigned int expected = CcpHashFNV1( "HashTest", 8 );
	unsigned int actual = CcpHashFNV1( "Test", 4, CcpHashFNV1( "Hash", 4 ) );
	EXPECT_EQ( expected, actual );
}