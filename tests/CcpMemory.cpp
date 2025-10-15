// Copyright © 2025 CCP ehf.

#include "gtest/gtest.h"
#include "CcpCore.h"

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#if _WIN32
// We test unsafe memory patterns here, so we can't use safe CRT functions
#pragma warning( disable : 4996 )
#endif

namespace
{

const size_t RIDICULOUS_SIZE = ( (size_t)-1 ) << 1 >> 1;

bool IsAligned( void* pointer, size_t alignment )
{
	return ( reinterpret_cast<size_t>( pointer ) & ( alignment - 1 ) ) == 0;
}

template <size_t size, size_t newSize>
void TestRealloc()
{
	void* memory = CCPMalloc( size );
	EXPECT_NE( nullptr, memory );
	uint8_t pattern[size];
	for( size_t i = 0; i < size; ++i )
	{
		pattern[i] = static_cast<uint8_t>( rand() );
	}
	memcpy( memory, pattern, size );

	void* newMemory = CCPRealloc( memory, newSize );
	EXPECT_NE( nullptr, newMemory );
	EXPECT_EQ( 0, memcmp( newMemory, pattern, std::min( size, newSize ) ) );
	CCPFree( newMemory );
}

template <size_t size, size_t alignment, size_t newSize, size_t newAlignment>
void TestAlignedRealloc()
{
	void* memory = CCPAlignedMalloc( size, alignment );
	EXPECT_NE( nullptr, memory );
	uint8_t pattern[size];
	for( size_t i = 0; i < size; ++i )
	{
		pattern[i] = static_cast<uint8_t>( rand() );
	}
	memcpy( memory, pattern, size );

	void* newMemory = CCPAlignedRealloc( memory, newSize, newAlignment );
	EXPECT_NE( nullptr, newMemory );
	EXPECT_TRUE( IsAligned( newMemory, newAlignment ) );
	EXPECT_EQ( 0, memcmp( newMemory, pattern, std::min( size, newSize ) ) );
	CCPAlignedFree( newMemory );
}

std::vector<std::pair<CCP::LogType, std::string>> s_logMessages;

void LogCallback( CcpLogChannel_t& channel, CCP::LogType type, unsigned long userData, const char* message )
{
	s_logMessages.push_back( std::make_pair( type, std::string( message ) ) );
}

class LogHelper
{
public:
	LogHelper()
	{
		CCP::RegisterLogEcho( &LogCallback, CCP::LOGTYPE_ERR, true );
	}
	~LogHelper()
	{
		CCP::UnregisterLogEcho( &LogCallback );
		s_logMessages.clear();
	}

	bool HasMessage( CCP::LogType type, const char* substring )
	{
		for( auto it = s_logMessages.begin(); it != s_logMessages.end(); ++it )
		{
			if( it->first == type && it->second.find( substring ) != std::string::npos )
			{
				return true;
			}
		}
		return false;
	}
};

void SilencedFreeWithGuard( void* memory )
{
#ifdef _WIN32
	__try 
    {
#endif
		CCPFreeWithGuard( memory );
#ifdef _WIN32
	}
	__except( GetExceptionCode() == EXCEPTION_BREAKPOINT ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH )
	{
	}
#endif
}

}

TEST( CcpMemory, CanAllocateWithMalloc )
{
	const size_t size = 123;
	void* memory = CCPMalloc( size );
	EXPECT_NE( nullptr, memory );
	memset( memory, 0xdd, size );
	CCPFree( memory );
}

TEST( CcpMemory, CanFreeNullPointer )
{
	CCPFree( nullptr );
	// and nothing blows up
}

TEST( CcpMemory, MallocSilentlyFailsWhenOutOfMemory )
{
	void* memory = CCPMalloc( RIDICULOUS_SIZE );
	EXPECT_EQ( nullptr, memory );
}

TEST( CcpMemory, CanReallocateMemoryToAGreaterSize )
{
	TestRealloc<123, 640>();
}

TEST( CcpMemory, CanReallocateMemoryToASmallerSize )
{
	TestRealloc<123, 12>();
}

TEST( CcpMemory, CanAllocateWithAlignedMalloc )
{
	const size_t size = 123;
	const size_t alignment = 32;
	void* memory = CCPAlignedMalloc( size, alignment );
	EXPECT_NE( nullptr, memory );
	memset( memory, 0xdd, size );
	CCPAlignedFree( memory );
}

TEST( CcpMemory, AlignedMallocReturnsAlignedMemory )
{
	const size_t size = 123;
	const size_t alignment = 64;
	void* memory = CCPAlignedMalloc( size, alignment );
	EXPECT_NE( nullptr, memory );
	EXPECT_TRUE( IsAligned( memory, alignment ) );
	memset( memory, 0xdd, size );
	CCPAlignedFree( memory );
}

TEST( CcpMemory, CanFreeAlignedNullPointer )
{
	CCPAlignedFree( nullptr );
	// and nothing blows up
}

TEST( CcpMemory, AlignedMallocSilentlyFailsWhenOutOfMemory )
{
	const size_t alignment = 64;
	void* memory = CCPAlignedMalloc( RIDICULOUS_SIZE, alignment );
	EXPECT_EQ( nullptr, memory );
}

TEST( CcpMemory, CanReallocateAlignedMemoryToAGreaterSize )
{
	TestAlignedRealloc<123, 32, 640, 64>();
}

TEST( CcpMemory, CanReallocateAlignedMemoryToASmallerSize )
{
	TestAlignedRealloc<123, 64, 12, 32>();
}

TEST( CcpMemory, CanAllocateWithMallocWithGuard )
{
	const size_t size = 123;
	void* memory = CCPMallocWithGuard( size );
	EXPECT_NE( nullptr, memory );
	memset( memory, 0xdd, size );
	CCPFreeWithGuard( memory );
}

TEST( CcpMemory, CanFreeWithGuardNullPointer )
{
	CCPFreeWithGuard( nullptr );
	// and nothing blows up
}

TEST( CcpMemory, MallocWithGuardSilentlyFailsWhenOutOfMemory )
{
	void* memory = CCPMallocWithGuard( RIDICULOUS_SIZE );
	EXPECT_EQ( nullptr, memory );
}

TEST( CcpMemory, CallocWithGuardSilentlyFailsWhenOutOfMemory )
{
	void* memory = CCPCallocWithGuard( 0, RIDICULOUS_SIZE );
	EXPECT_EQ( nullptr, memory );
	memory = CCPCallocWithGuard( 213412, RIDICULOUS_SIZE );
	EXPECT_EQ( nullptr, memory );
	memory = CCPCallocWithGuard( RIDICULOUS_SIZE, RIDICULOUS_SIZE );
	EXPECT_EQ( nullptr, memory );
	memory = CCPCallocWithGuard( RIDICULOUS_SIZE, 123456789 );
	EXPECT_EQ( nullptr, memory );
	memory = CCPCallocWithGuard( RIDICULOUS_SIZE, 0 );
	EXPECT_EQ( nullptr, memory );
}

TEST( CcpMemory, CanDetectInvalidMemoryAccessPastMemoryBlock )
{

	const size_t size = 123;
	void* memory = CCPMallocWithGuard( size );
	EXPECT_NE( nullptr, memory );
	strcpy( reinterpret_cast<char*>( memory ) + size, "findme" );

	LogHelper logHelper;
	SilencedFreeWithGuard( memory );

	EXPECT_TRUE( logHelper.HasMessage( CCP::LOGTYPE_ERR, "after block at" ) );
}

TEST( CcpMemory, CanDetectInvalidMemoryAccessBeforeMemoryBlock )
{

	const size_t size = 123;
	void* memory = CCPMallocWithGuard( size );
	EXPECT_NE( nullptr, memory );
	strcpy( reinterpret_cast<char*>( memory ) - 16, "findme" );
    {
        LogHelper logHelper;
        SilencedFreeWithGuard( memory );

        EXPECT_TRUE( logHelper.HasMessage( CCP::LOGTYPE_ERR, "in front of block at" ) );
    }

    memory = CCPCallocWithGuard( 1, size );
    EXPECT_NE( nullptr, memory );
    strcpy( reinterpret_cast<char*>( memory ) - 16, "findme" );
    {
        LogHelper logHelper;
        SilencedFreeWithGuard( memory );

        EXPECT_TRUE( logHelper.HasMessage( CCP::LOGTYPE_ERR, "in front of block at" ) );
    }
}

TEST( CcpMemory, CanDuplicateString )
{
	const char* string = "just a string";
	auto duplicate = CCPStrDup( string );
	EXPECT_NE( nullptr, duplicate );
	EXPECT_NE( string, duplicate );

	EXPECT_EQ( 0, strcmp( string, duplicate ) );
	CCPFree( duplicate );
}

TEST( CcpMemory, CanDuplicateWideString )
{
	const wchar_t* string = L"just a string";
	auto duplicate = CCPWStrDup( string );
	EXPECT_NE( nullptr, duplicate );
	EXPECT_NE( string, duplicate );

	EXPECT_EQ( 0, wcscmp( string, duplicate ) );
	CCPFree( duplicate );
}

TEST( CcpMemory, CanGetAllocatedMemorySize )
{
	const size_t size = 123;
	void* memory = CCPMalloc( size );
	EXPECT_NE( nullptr, memory );
	EXPECT_GE( CCPMSize( memory ), size );
	memset( memory, 0xdd, size );
	CCPFree( memory );
}


TEST( CcpMemory, CanGetTotalMallocSize )
{
	size_t initial = CCPMallocUsage();

	const size_t size = 123;
	void* memory = CCPMalloc( size );
	EXPECT_NE( nullptr, memory );

	// Different platforms yield different results here,
	// e.g. on macOS the allocated amount of memory is
	// determined by `malloc_good_size( requested_amount )`
	// Therefore, just check for current usage being greater
	// than initial.
	EXPECT_GE( CCPMallocUsage(), initial );

	CCPFree( memory );
	EXPECT_EQ( CCPMallocUsage(), initial );

	memory = CCPCalloc( 2, size );
	// same as above, platform differences
	EXPECT_GE( CCPMallocUsage(), initial );

	CCPFree( memory );
	EXPECT_EQ( CCPMallocUsage(), initial );
}
