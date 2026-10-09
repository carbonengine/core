// Copyright © 2025 CCP ehf.

#include "gtest/gtest.h"
#include "CcpCore.h"

#include <filesystem>
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

class CcpTempFileUtilityTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		std::string name = ::testing::UnitTest::GetInstance()->current_test_info()->name();
		tempFilePath = name.append("_tmp");

		if (exists(tempFilePath))
		{
			ASSERT_FALSE(std::remove(tempFilePath.string().c_str()));
		}

		ASSERT_FALSE(exists(tempFilePath));
	}

	void TearDown() override
	{
		if (exists(tempFilePath))
		{
			ASSERT_FALSE(std::remove(tempFilePath.string().c_str()));
		}

		ASSERT_FALSE(exists(tempFilePath));
	}

	std::filesystem::path tempFilePath;
};

TEST_F(CcpTempFileUtilityTest, CcpGetAbsolutePathResolvesToExistingFile)
{
	const char* checkString = "test string. can\'t possibly have it in another file";

	{
		std::ofstream tempFile( tempFilePath );
		ASSERT_TRUE( tempFile.good() );
		tempFile << checkString;
	}


	auto path = CcpGetAbsolutePath( tempFilePath.wstring());

	{
		std::ifstream tempFile( CW2A( path.c_str() ) );
		EXPECT_TRUE( tempFile.good() );

		std::string contents( ( std::istreambuf_iterator<char>( tempFile ) ), std::istreambuf_iterator<char>() );
		EXPECT_EQ( checkString, contents );
	}
}

TEST_F(CcpTempFileUtilityTest, CcpGetAbsolutePathResolvesToNewFile)
{
	const char* checkString = "different test string. can\'t possibly have it in another file";

	auto path = CcpGetAbsolutePath( tempFilePath.wstring());
	{
		std::ofstream tempFile( tempFilePath );
		ASSERT_TRUE( tempFile.good() );
		tempFile << checkString;
	}

	{
		std::ifstream tempFile( CW2A( path.c_str() ) );
		EXPECT_TRUE( tempFile.good() );

		std::string contents( ( std::istreambuf_iterator<char>( tempFile ) ), std::istreambuf_iterator<char>() );
		EXPECT_EQ( checkString, contents );
	}
}

TEST_F(CcpTempFileUtilityTest, CanOpenFile)
{
	std::ofstream tempFile(tempFilePath);

	ASSERT_TRUE(tempFile.good());
	tempFile.flush();
	tempFile.close();

	const auto fd1 = CcpOpenFile(tempFilePath.wstring().c_str(), CCP_OM_READONLY, CCP_SM_READSHARING);
	ASSERT_GT(fd1, 0);

	const auto fd2 = CcpOpenFile(tempFilePath.wstring().c_str(), CCP_OM_READONLY, CCP_SM_READSHARING);
	ASSERT_GT(fd2, 0);

	ASSERT_FALSE(close(fd1));
	ASSERT_FALSE(close(fd2));
}

TEST_F(CcpTempFileUtilityTest, CanOpenFile_Failure)
{
	const auto fd = CcpOpenFile(tempFilePath.wstring().c_str(), CCP_OM_READONLY, CCP_SM_READSHARING);

	ASSERT_LE(fd, 0);
}

TEST_F(CcpTempFileUtilityTest, CanOpenFileForWriting)
{
	const auto fd = CcpOpenFile(tempFilePath.wstring().c_str(), CCP_OM_READWRITE, CCP_SM_READSHARING);

	ASSERT_GT(fd, 0);
	ASSERT_TRUE(std::filesystem::exists(tempFilePath));

	ASSERT_FALSE(close(fd));
}

TEST_F(CcpTempFileUtilityTest, CanCreateFile)
{
	const auto fd = CcpCreateFile(tempFilePath.wstring().c_str(), CCP_SM_NOSHARING);

	ASSERT_GT(fd, 0);
	ASSERT_TRUE(std::filesystem::exists(tempFilePath));

	ASSERT_FALSE(close(fd));
}

TEST_F(CcpTempFileUtilityTest, CanCreateFile_Failure)
{
	const auto fd = CcpCreateFile(tempFilePath.wstring().c_str(), CCP_SM_NOSHARING);

	ASSERT_GT(fd, 0);
	ASSERT_TRUE(std::filesystem::exists(tempFilePath));

	ASSERT_EQ(CcpCreateFile(tempFilePath.wstring().c_str(), CCP_SM_NOSHARING), -1);

	ASSERT_FALSE(close(fd));
}

TEST(CcpFileUtils, CanGetExecutablePath)
{
	auto path = CcpExecutablePath();
	ASSERT_FALSE(path.empty());
}
