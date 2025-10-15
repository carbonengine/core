// Copyright © 2025 CCP ehf.

#include "gtest/gtest.h"
#include "CCPCallstack.h"
#include <memory>

#include "TempFile.h"

#if defined(_MSC_VER)
#define NO_INLINE __declspec(noinline)
#else
#define NO_INLINE __attribute__((noinline))
#endif

namespace
{

NO_INLINE size_t CaptureAndReturnDepth( CCPCallstack& cs )
{
	cs.Capture();
	return cs.GetDepth();
}

struct MyStruct
{
	static NO_INLINE size_t CaptureAndReturnDepth( CCPCallstack& cs )
	{
		cs.Capture();
		return cs.GetDepth();
	}
};

size_t CountCharacters( char character, std::string::iterator begin, std::string::iterator end )
{
	size_t count = 0;
	for( auto it = begin; it != end; ++it )
	{
		if( *it == character )
		{
			++count;
		}
	}
	return count;
}

}

TEST( Callstack, UncapturedCallstackIsEmpty )
{
	CCPCallstack cs;
	ASSERT_EQ( 0, cs.GetDepth() );
}

TEST( Callstack, CallstackInInnerFunctionIsDeeperThanInOutterFunction )
{
	CCPCallstack cs1;
	cs1.Capture();
    
	CCPCallstack cs2;
	auto depth = CaptureAndReturnDepth( cs2 );
    
	ASSERT_EQ( cs1.GetDepth() + 1, depth );
}

TEST( Callstack, CanLoadAndReleaseSymbols )
{
    CCPCallstack::LoadSymbols();
    CCPCallstack::ReleaseSymbols();
    
	CCPCallstack cs1;
	cs1.Capture();
    
	CCPCallstack cs2;
	auto depth = CaptureAndReturnDepth( cs2 );
    
	ASSERT_EQ( cs1.GetDepth() + 1, depth );
}

TEST( Callstack, DepthIsEqualToItemCount )
{
	CCPCallstack cs;
	cs.Capture();

	size_t depth = 0;
	cs.EnumerateAddresses( [&]( size_t ) { ++depth; } );

	ASSERT_EQ( cs.GetDepth(), depth );
}

TEST( Callstack, DISABLED_StackSymbolsContainCallerFunctionName )
{
	CCPCallstack cs;
	CaptureAndReturnDepth( cs );

	bool found = false;
	cs.EnumerateSymbols( [&]( size_t, const char* name ) 
	{ 
		found |= strstr( name, "CaptureAndReturnDepth" ) != nullptr;
	} );

	ASSERT_TRUE( found );
}

TEST( Callstack, DISABLED_StackSymbolsAreNotGarbled )
{
	CCPCallstack cs;
	MyStruct::CaptureAndReturnDepth( cs );

	bool found = false;
	cs.EnumerateSymbols( [&]( size_t, const char* name ) 
	{ 
		found |= strstr( name, "MyStruct::CaptureAndReturnDepth" ) != nullptr;
	} );

	ASSERT_TRUE( found );
}

TEST( Callstack, CanDumpStackIntoFile )
{
	CCPCallstack cs;
	cs.Capture();

	TempFile temp;
	ASSERT_NE( nullptr, temp.GetFile() );

	cs.DumpRaw( temp );

	size_t length = ftell( temp );
	ASSERT_GT( length, 0 );
}

TEST( Callstack, DumpContainsRightNumberOfRecords )
{
	CCPCallstack cs;
	cs.Capture();

	TempFile temp;
	ASSERT_NE( nullptr, temp.GetFile() );

	cs.DumpRaw( temp );

	auto data = temp.GetContents();

	size_t comas = CountCharacters( ',', data.begin(), data.end() );
	EXPECT_EQ( cs.GetDepth(), comas );
}

TEST( Callstack, CanDumpSymbolsIntoFile )
{
	CCPCallstack cs;
	cs.Capture();

	TempFile temp;
	ASSERT_NE( nullptr, temp.GetFile() );

	cs.DumpWithSymbols( temp );

	size_t length = ftell( temp );
	ASSERT_GT( length, 0 );
}

TEST( Callstack, DISABLED_SymbolDumpContainsFunctionName )
{
	CCPCallstack cs;
	CaptureAndReturnDepth( cs );

	TempFile temp;
	ASSERT_NE( nullptr, temp.GetFile() );

	cs.DumpWithSymbols( temp );

	auto data = temp.GetContents();
	EXPECT_NE( std::string::npos, data.find( "CaptureAndReturnDepth" ) );
}
