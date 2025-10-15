// Copyright © 2025 CCP ehf.

#include "gtest/gtest.h"
#include "CcpCore.h"

TEST( CcpAtomicTest, DefaultConstructorDoesNotInitializeContents )
{
	char buffer[sizeof( CcpAtomic<uint32_t> )];
	memset( buffer, 0xaf, sizeof( buffer ) );
	CcpAtomic<uint32_t>* a = new( buffer )CcpAtomic<uint32_t>;
	ASSERT_EQ( 0xafafafaf, uint32_t( *a ) );
}

TEST( CcpAtomicTest, CopyConstructorInitializesCorrectly )
{
	CcpAtomic<uint32_t> a( 123 );
	ASSERT_EQ( 123, uint32_t( a ) );
}

TEST( CcpAtomicTest, AssignmentStoresValue )
{
	CcpAtomic<uint32_t> a( 123 );
	a = 456;
	ASSERT_EQ( 456, uint32_t( a ) );
}

TEST( CcpAtomicTest, PreIncrementIncrementsValue )
{
	CcpAtomic<uint32_t> a( 123 );
	++a;
	ASSERT_EQ( 124, uint32_t( a ) );
}

TEST( CcpAtomicTest, PreIncrementReturnsIncrementedValue )
{
	CcpAtomic<uint32_t> a( 123 );
	ASSERT_EQ( 124, ++a );
}

TEST( CcpAtomicTest, PostIncrementIncrementsValue )
{
	CcpAtomic<uint32_t> a( 123 );
	a++;
	ASSERT_EQ( 124, uint32_t( a ) );
}

TEST( CcpAtomicTest, PostIncrementReturnsOriginalValue )
{
	CcpAtomic<uint32_t> a( 123 );
	ASSERT_EQ( 123, a++ );
}

TEST( CcpAtomicTest, PreDecrementDecrementsValue )
{
	CcpAtomic<uint32_t> a( 123 );
	--a;
	ASSERT_EQ( 122, uint32_t( a ) );
}

TEST( CcpAtomicTest, PreDecrementReturnsDecrementedValue )
{
	CcpAtomic<uint32_t> a( 123 );
	ASSERT_EQ( 122, --a );
}

TEST( CcpAtomicTest, PostDecrementDecrementsValue )
{
	CcpAtomic<uint32_t> a( 123 );
	a--;
	ASSERT_EQ( 122, uint32_t( a ) );
}

TEST( CcpAtomicTest, PostDecrementReturnsOriginalValue )
{
	CcpAtomic<uint32_t> a( 123 );
	ASSERT_EQ( 123, a-- );
}

TEST( CcpAtomicTest, AssignmentWithAddIncrementsValue )
{
	CcpAtomic<uint32_t> a( 123 );
	a += 111;
	ASSERT_EQ( 234, a );
}

TEST( CcpAtomicTest, AssignmentWithAddReturnsIncrementedValue )
{
	CcpAtomic<uint32_t> a( 123 );
	ASSERT_EQ( 234, a += 111 );
}

TEST( CcpAtomicTest, StoreStoresValue )
{
	CcpAtomic<uint32_t> a( 123 );
	a.store( 456 );
	ASSERT_EQ( 456, uint32_t( a ) );
}
