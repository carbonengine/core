// Copyright © 2025 CCP ehf.

#include "gtest/gtest.h"
#include <cstdint>
#include <set>
#include <string>
#include <vector>
#include "CcpCore.h"

#include "TempFile.h"

class CCPMemoryTracker : public ::testing::Test
{
	protected:
		virtual void TearDown()
		{
			if ( IsMemoryTrackingEnabled() )
			{
				MemoryTrackerShutdown();
			}
		}
};

namespace 
{

bool ReadLine( FILE* f, std::string& result )
{
	result = "";
	char buffer[512];
	while( fgets( buffer, 512, f ) )
	{
		result += buffer;
		if( !result.empty() && result[result.length() - 1] == '\n' )
		{
			result.resize( result.size() - 1 );
			return true;
		}
	}
	return false;
}

}

TEST_F( CCPMemoryTracker, MemoryTrackerSizeIsZeroWithoutTracking )
{
	size_t s;
	void* p = CCP_MALLOC( "namedAllocation", 4096 );
	EXPECT_FALSE( MemoryTrackerGetSize( s ) );
	EXPECT_EQ( 0, s );
	CCP_FREE( p );
}

TEST_F( CCPMemoryTracker, MemoryTrackerTracksSize )
{
	MemoryTrackerInitialize();
	size_t s;
	void* p = CCP_MALLOC( "namedAllocation", 4096 );
	EXPECT_TRUE( MemoryTrackerGetSize( s ) );
	EXPECT_EQ( 4096, s );
	CCP_FREE( p );
	
	MemoryTrackerGetSize(s);
	EXPECT_EQ( 0, s );
}

TEST_F( CCPMemoryTracker, MemoryTrackerCountIsZeroWithoutTracking )
{
	size_t s;
	void* p = CCP_MALLOC( "namedAllocation", 4096 );
	EXPECT_FALSE( MemoryTrackerGetCount( s ) );
	EXPECT_EQ( 0, s );
	CCP_FREE( p );
}

TEST_F( CCPMemoryTracker, MemoryTrackerTracksCount )
{
	MemoryTrackerInitialize();
	size_t s;
	void* p = CCP_MALLOC( "namedAllocation", 4096 );
	EXPECT_TRUE( MemoryTrackerGetCount( s ) );
	EXPECT_EQ( 1, s );
	CCP_FREE( p );
	
	EXPECT_TRUE( MemoryTrackerGetCount( s ) );
	EXPECT_EQ( 0, s );
}

TEST_F( CCPMemoryTracker, MemoryTrackingEnabledTellsWhetherOrNotMemoryTrackingIsEnabled )
{
	EXPECT_EQ(false, IsMemoryTrackingEnabled());
	MemoryTrackerInitialize();
	EXPECT_EQ(true, IsMemoryTrackingEnabled());
	MemoryTrackerShutdown();
	EXPECT_EQ(false, IsMemoryTrackingEnabled());
}

TEST_F( CCPMemoryTracker, MemoryTrackerDoesNotCountFailedAllocations )
{
	MemoryTrackerInitialize();
	size_t s;
	void* p = CCP_MALLOC( "namedAllocation", ( (size_t)-1 ) << 1 >> 1 );
	EXPECT_EQ( nullptr, p );
	EXPECT_TRUE( MemoryTrackerGetCount( s ) );
	EXPECT_EQ( 0, s );
	CCP_FREE( p );
}

TEST_F( CCPMemoryTracker, MemoryTrackerReportsAllocatedMemoryAsValid )
{
	MemoryTrackerInitialize();
	void* p = CCP_MALLOC( "namedAllocation", 4096 );
	EXPECT_TRUE( MemoryTrackerIsValid( p ) );
	CCP_FREE( p );
}

TEST_F( CCPMemoryTracker, MemoryTrackerReportsFreedMemoryAsInvalid )
{
	MemoryTrackerInitialize();
	void* p = CCP_MALLOC( "namedAllocation", 4096 );
	CCP_FREE( p );
	EXPECT_FALSE( MemoryTrackerIsValid( p ) );
}

TEST_F( CCPMemoryTracker, MemoryTrackerReportsNonHeapMemoryAsInvalid )
{
	MemoryTrackerInitialize();
	int abc = 123;
	EXPECT_FALSE( MemoryTrackerIsValid( &abc ) );
}

TEST_F( CCPMemoryTracker, DISABLED_TextReportContainsAllocatedInformation )
{
	MemoryTrackerInitialize();

	SetCallStackCaptureEnabled( true );
	void* p = CCP_MALLOC( "namedAllocation", 4096 );
	SetCallStackCaptureEnabled( false );

	char filename[64];
	sprintf_s( filename, "ccpcoretest_%" CCP_SIZET_FORMAT, size_t( CcpGetCurrentProcessId() ) );

	MemoryTrackerDumpReportAsText( filename );
	ON_BLOCK_EXIT( [&] { 
#if _WIN32
		_unlink( filename );
#else
		unlink( filename );
#endif
	} );

	CCP_FREE( p );

	FILE* f;
	ASSERT_EQ( 0, fopen_s( &f, filename, "rt" ) );

	bool found = false;
	std::string line;
	while( ReadLine( f, line ) )
	{
		if( line.find( "namedAllocation" ) != std::string::npos )
		{
			found = true;
			break;
		}
	}
	fclose( f );

	ASSERT_TRUE( found );
	EXPECT_NE( std::string::npos, line.find( "4096" ) );
	EXPECT_NE( std::string::npos, line.find( "TextReportContainsAllocatedInformation" ) );
}


TEST_F( CCPMemoryTracker, TextSummaryReportContainsAllocatedInformation )
{
	MemoryTrackerInitialize();

	void* p10 = CCP_MALLOC( "namedAllocation1", 4096 );
	void* p20 = CCP_MALLOC( "namedAllocation2", 72 );
	void* p21 = CCP_MALLOC( "namedAllocation2", 123 );

	char filename[64];
	sprintf_s( filename, "ccpcoretest_%" CCP_SIZET_FORMAT, size_t( CcpGetCurrentProcessId() ) );

	TempFile temp;
	MemoryTrackerSummaryReportToFile( temp );

	CCP_FREE( p10 );
	CCP_FREE( p20 );
	CCP_FREE( p21 );


	auto contents = temp.GetContents();
	size_t offset = 0;
	std::string line1, line2;
	while( offset < contents.length() )
	{
		size_t end = contents.find( '\n', offset );
		if( end == std::string::npos )
		{
			end = contents.length();
		}
		auto line = contents.substr( offset, end - offset );
		if( line.find( "namedAllocation1" ) != std::string::npos )
		{
			line1 = line;
		}
		if( line.find( "namedAllocation2" ) != std::string::npos )
		{
			line2 = line;
		}
		offset = end + 1;
	}

	ASSERT_FALSE( line1.empty() );
	EXPECT_NE( std::string::npos, line1.find( "4096" ) );

	ASSERT_FALSE( line2.empty() );
	EXPECT_NE( std::string::npos, line2.find( "195" ) );
}

// The memory tracker keeps allocation names in an internal StringTable: a
// contiguous array of (id, string) entries that is documented to be sorted by
// ascending id and searched with a binary search. The binary report dumps that
// array verbatim, so it is the public view of the table's invariants.
// This test checks that every entry's id is the FNV-1 hash of its string, that
// entries are strictly ascending by id (no duplicates), and that every name
// added through the tracker is present.
TEST_F( CCPMemoryTracker, BinaryReportStringTableIsSortedByIdAndConsistent )
{
	MemoryTrackerInitialize();

	static const int kCount = 64;
	static char blocks[kCount];
	std::vector<std::string> names;
	for( int i = 0; i < kCount; ++i )
	{
		names.push_back( "stringTableEntry-" + std::to_string( i ) );
	}
	for( int i = 0; i < kCount; ++i )
	{
		MemoryTrackerAdd( &blocks[i], 1, names[i].c_str(), "StringTableTest.cpp", i );
	}
	// Adding the same names again must only bump reference counts.
	for( int i = 0; i < kCount; ++i )
	{
		MemoryTrackerRemove( &blocks[i] );
		MemoryTrackerAdd( &blocks[i], 1, names[i].c_str(), "StringTableTest.cpp", i );
	}

	char filename[64];
	sprintf_s( filename, "ccpcoretest_st_%" CCP_SIZET_FORMAT, size_t( CcpGetCurrentProcessId() ) );
	MemoryTrackerDumpReportAsBinary( filename );
	ON_BLOCK_EXIT( [&] {
#if _WIN32
		_unlink( filename );
#else
		unlink( filename );
#endif
	} );

	for( int i = 0; i < kCount; ++i )
	{
		MemoryTrackerRemove( &blocks[i] );
	}

	// Layout written by CcpMemoryTracker::ReportBinary:
	// size_t mapSize, unsigned int numStrings,
	// then numStrings x { unsigned int id, size_t len, char[len] }.
	FILE* f;
	ASSERT_EQ( 0, fopen_s( &f, filename, "rb" ) );
	std::vector<unsigned int> ids;
	std::vector<std::string> strings;
	size_t mapSize = 0;
	unsigned int numStrings = 0;
	bool readOk = fread( &mapSize, sizeof( size_t ), 1, f ) == 1 &&
				  fread( &numStrings, sizeof( unsigned int ), 1, f ) == 1;
	for( unsigned int i = 0; readOk && i < numStrings; ++i )
	{
		unsigned int id = 0;
		size_t len = 0;
		readOk = fread( &id, sizeof( unsigned int ), 1, f ) == 1 &&
				 fread( &len, sizeof( size_t ), 1, f ) == 1 && len < 4096;
		std::string s( len, '\0' );
		if( readOk && len > 0 )
		{
			readOk = fread( &s[0], len, 1, f ) == 1;
		}
		ids.push_back( id );
		strings.push_back( s );
	}
	fclose( f );
	ASSERT_TRUE( readOk );

	// kCount names + "module" + the file name, plus whatever else the tracker saw.
	ASSERT_GE( numStrings, unsigned( kCount + 2 ) );

	std::set<std::string> seen;
	for( unsigned int i = 0; i < numStrings; ++i )
	{
		EXPECT_EQ( CcpHashFNV1( strings[i].c_str(), strings[i].size() ), ids[i] ) << "entry " << i << " '" << strings[i] << "'";
		if( i > 0 )
		{
			EXPECT_LT( ids[i - 1], ids[i] ) << "string table not strictly ascending by id at entry " << i;
		}
		EXPECT_TRUE( seen.insert( strings[i] ).second ) << "duplicate entry '" << strings[i] << "'";
	}
	for( const std::string& name : names )
	{
		EXPECT_EQ( 1u, seen.count( name ) ) << "missing '" << name << "'";
	}
}
