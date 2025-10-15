// Copyright © 2025 CCP ehf.

#include "gtest/gtest.h"
#include "CcpCore.h"
#include <fstream>

namespace
{
bool EndsWith( const std::wstring& str, const std::wstring& suffix )
{
	return suffix.length() <= str.length() && str.substr( str.length() - suffix.length() ) == suffix;
}

#if _WIN32
const std::wstring ROOT_PATH = L"C:/";
#else
const std::wstring ROOT_PATH = L"/";
#endif

}

TEST( CcpFileUtils, CcpGetCurrentWorkingDirectoryIsNotEmpty )
{
	auto path = CcpGetCurrentWorkingDirectory();
	EXPECT_FALSE( path.empty() );
}

TEST( CcpFileUtils, CcpGetAbsolutePathSkipsEmptyPath )
{
	auto path = CcpGetAbsolutePath( L"" );
	EXPECT_TRUE( path.empty() );
}

TEST( CcpFileUtils, CcpGetAbsolutePathResolvesRelativePath )
{
	std::wstring name = L"blah";
	auto path = CcpGetAbsolutePath( name );
	EXPECT_TRUE( EndsWith( path, L"/" + name ) );
}

TEST( CcpFileUtils, CcpGetAbsolutePathRemovesDots )
{
	auto path1 = CcpGetAbsolutePath( L"blah" );
	EXPECT_EQ( path1, CcpGetAbsolutePath( L"./blah" ) );
	EXPECT_EQ( path1, CcpGetAbsolutePath( L"././blah" ) );
}

TEST( CcpFileUtils, CcpGetAbsolutePathRemovesConsequtiveSlashes )
{
	auto path1 = CcpGetAbsolutePath( L"./blah" );
	EXPECT_EQ( path1, CcpGetAbsolutePath( L".//blah" ) );
	EXPECT_EQ( path1, CcpGetAbsolutePath( L".///blah" ) );
}

TEST( CcpFileUtils, CcpGetAbsolutePathProcessesDoubleDots )
{
	auto path = CcpGetAbsolutePath( L"blah" );
	EXPECT_EQ( path, CcpGetAbsolutePath( L"foo/../blah" ) );
}

TEST( CcpFileUtils, CcpGetAbsolutePathResolvesAbsolutePath )
{
	auto path = ROOT_PATH + L"foo";
	EXPECT_EQ( path, CcpGetAbsolutePath( path ) );
}

TEST( CcpFileUtils, CcpGetAbsolutePathIgnoresInvalidDoubleDots )
{
	auto path = ROOT_PATH + L"..";
	EXPECT_EQ( ROOT_PATH, CcpGetAbsolutePath( path ) );
}

TEST( CcpFileUtils, CcpGetAbsolutePathResolvesToExistingFile )
{
	const char* tempFileName = "tempFileName";
	const char* checkString = "test string. can\'t possibly have it in another file";

	{
		std::ofstream tempFile( "tempFileName" );
		ASSERT_TRUE( tempFile.good() );
		tempFile << checkString;
	}


	auto path = CcpGetAbsolutePath( static_cast<const wchar_t*>( CA2W( tempFileName ) ) );

	{
		std::ifstream tempFile( CW2A( path.c_str() ) );
		EXPECT_TRUE( tempFile.good() );

		std::string contents( ( std::istreambuf_iterator<char>( tempFile ) ), std::istreambuf_iterator<char>() );
		EXPECT_EQ( checkString, contents );
	}

#if _WIN32
	_unlink( tempFileName );
#else
	unlink( tempFileName );
#endif
}

TEST( CcpFileUtils, CcpGetAbsolutePathResolvesToNewFile )
{
	const char* tempFileName = "tempFileName";
	const char* checkString = "different test string. can\'t possibly have it in another file";

	auto path = CcpGetAbsolutePath( static_cast<const wchar_t*>( CA2W( tempFileName ) ) );

	{
		std::ofstream tempFile( "tempFileName" );
		ASSERT_TRUE( tempFile.good() );
		tempFile << checkString;
	}

	{
		std::ifstream tempFile( CW2A( path.c_str() ) );
		EXPECT_TRUE( tempFile.good() );

		std::string contents( ( std::istreambuf_iterator<char>( tempFile ) ), std::istreambuf_iterator<char>() );
		EXPECT_EQ( checkString, contents );
	}

#if _WIN32
	_unlink( tempFileName );
#else
	unlink( tempFileName );
#endif
}
