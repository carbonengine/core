// Copyright © 2025 CCP ehf.

#include "gtest/gtest.h"
#include "CcpCore.h"


class CcpSecureCrt : public ::testing::Test 
{
protected:
    virtual void SetUp()
    {
#ifdef _WIN32
		m_oldHandler = _set_invalid_parameter_handler( &IgnoreInvalidParameterHandler );
#endif
    }
    virtual void TearDown()
    {
#ifdef _WIN32
		_set_invalid_parameter_handler( m_oldHandler );
#endif
    }
private:
#ifdef _WIN32
	static void IgnoreInvalidParameterHandler(
		const wchar_t* expression,
		const wchar_t* function, 
		const wchar_t* file, 
		unsigned int line, 
		uintptr_t pReserved )
	{
	}

	_invalid_parameter_handler m_oldHandler;
#endif
};

TEST_F( CcpSecureCrt, Function_snprintf_sPrintsString )
{
	char buffer[100];
	auto result = _snprintf_s( buffer, 50, "abc %s", "def" );

	EXPECT_EQ( 7, result );
	EXPECT_EQ( 0, strcmp( buffer, "abc def" ) );
}

TEST_F( CcpSecureCrt, Function_snprintf_sTruncatesStringToMaxLength )
{
	char buffer[100];
	auto result = _snprintf_s( buffer, 5, "abc %s", "def" );

	EXPECT_EQ( -1, result );
	EXPECT_EQ( 0, strcmp( buffer, "abc d" ) );
}

TEST_F( CcpSecureCrt, Function_snprintf_sReturnsEmptyStringOnOverflow )
{
	char buffer[5];
	auto result = _snprintf_s( buffer, 50, "abc %s", "def" );

	EXPECT_EQ( -1, result );
	EXPECT_EQ( 0, strcmp( buffer, "" ) );
}

TEST_F( CcpSecureCrt, Function_snprintf_sSucceedsWithMaxLengthSetToTruncate )
{
	char buffer[50];
	auto result = _snprintf_s( buffer, _TRUNCATE, "abc %s", "def" );

	EXPECT_EQ( 7, result );
	EXPECT_EQ( 0, strcmp( buffer, "abc def" ) );
}

TEST_F( CcpSecureCrt, Function_snprintf_sTruncatesWithMaxLengthSetToTruncate )
{
	char buffer[6];
	auto result = _snprintf_s( buffer, _TRUNCATE, "abc %s", "def" );

	EXPECT_EQ( -1, result );
	EXPECT_EQ( 0, strcmp( buffer, "abc d" ) );
}

TEST_F( CcpSecureCrt, Function_snprintf_sFailsWhenFormatIsNull )
{
	char buffer[6];
	auto result = _snprintf_s( buffer, _TRUNCATE, nullptr, "def" );
	EXPECT_EQ( -1, result );
}

TEST_F( CcpSecureCrt, Function_snprintf_sFailsWhenCountIsZero )
{
	char buffer[6];
	auto result = _snprintf_s( buffer, 0, "abc %s", "def" );
	EXPECT_EQ( -1, result );
	EXPECT_EQ( 0, buffer[0] );
}


TEST_F( CcpSecureCrt, Functionstrncpy_sCopiesString )
{
	char buffer[6];
	auto result = strncpy_s( buffer, 6, "abc", 5 );
	EXPECT_EQ( 0, result );
	EXPECT_EQ( 0, strcmp( buffer, "abc" ) );
}

TEST_F( CcpSecureCrt, TemplateFunctionstrncpy_sCopiesString )
{
	char buffer[6];
	auto result = strncpy_s( buffer, "abc", 5 );
	EXPECT_EQ( 0, result );
	EXPECT_EQ( 0, strcmp( buffer, "abc" ) );
}

TEST_F( CcpSecureCrt, Functionstrncpy_sTruncatesToMaxCountCharacters )
{
	char buffer[6];
	auto result = strncpy_s( buffer, 6, "abc", 2 );
	EXPECT_EQ( 0, result );
	EXPECT_EQ( 0, strcmp( buffer, "ab" ) );
}

TEST_F( CcpSecureCrt, Functionstrncpy_sFailsWhenBufferIsTooShort )
{
	char buffer[6];
	auto result = strncpy_s( buffer, 3, "abc", 3 );
	EXPECT_EQ( ERANGE, result );
	EXPECT_EQ( 0, strcmp( buffer, "" ) );
}

TEST_F( CcpSecureCrt, Functionstrncpy_sTruncatesWhenCountIsTruncateAndBufferIsTooShort )
{
	char buffer[6];
	auto result = strncpy_s( buffer, 3, "abc", _TRUNCATE );
	EXPECT_EQ( STRUNCATE, result );
	EXPECT_EQ( 0, strcmp( buffer, "ab" ) );
}

TEST_F( CcpSecureCrt, Functionstrncpy_sFailsWhenDestinationIsNull )
{
	auto result = strncpy_s( nullptr, 3, "abc", 1 );
	EXPECT_EQ( EINVAL, result );
}

TEST_F( CcpSecureCrt, Functionstrncpy_sFailsWhenSourceIsNull )
{
	char buffer[6] = "def";
	auto result = strncpy_s( buffer, 3, nullptr, 1 );
	EXPECT_EQ( EINVAL, result );
	EXPECT_EQ( 0, buffer[0] );
}

TEST_F( CcpSecureCrt, Functionstrncpy_sFailsWhenBufferSizeIsZero )
{
	char buffer[6] = "def";
	auto result = strncpy_s( buffer, 0, "abc", 1 );
	EXPECT_EQ( EINVAL, result );
	EXPECT_EQ( 0, strcmp( buffer, "def" ) );
}


TEST_F( CcpSecureCrt, Functionstrcpy_sCopiesString )
{
	char buffer[6];
	auto result = strcpy_s( buffer, 6, "abc" );
	EXPECT_EQ( 0, result );
	EXPECT_EQ( 0, strcmp( buffer, "abc" ) );
}

TEST_F( CcpSecureCrt, TemplatedFunctionstrcpy_sCopiesString )
{
	char buffer[6];
	auto result = strcpy_s( buffer, "abc" );
	EXPECT_EQ( 0, result );
	EXPECT_EQ( 0, strcmp( buffer, "abc" ) );
}

TEST_F( CcpSecureCrt, Functionstrcpy_sFailsWhenDestinationIsTooShort )
{
	char buffer[6];
	auto result = strcpy_s( buffer, 2, "abc" );
	EXPECT_EQ( ERANGE, result );
	EXPECT_EQ( 0, strcmp( buffer, "" ) );
}

TEST_F( CcpSecureCrt, Functionstrcpy_sFailsWhenDestinationIsNull )
{
	auto result = strcpy_s( nullptr, 10, "abc" );
	EXPECT_EQ( EINVAL, result );
}

TEST_F( CcpSecureCrt, Functionstrcpy_sFailsWhenSourceIsNull )
{
	char buffer[6];
	auto result = strcpy_s( buffer, 2, nullptr );
	EXPECT_EQ( EINVAL, result );
	EXPECT_EQ( 0, strcmp( buffer, "" ) );
}


TEST_F( CcpSecureCrt, Functionsprintf_sWritesToString )
{
	char buffer[32];
	auto result = sprintf_s( buffer, "abc %s", "def" );
	EXPECT_EQ( 7, result );
	EXPECT_EQ( 0, strcmp( buffer, "abc def" ) );
}

TEST_F( CcpSecureCrt, Functionsprintf_sFailsWhenBufferIsTooShort )
{
	char buffer[6] = "aaa";
	auto result = sprintf_s( buffer, "abc %s", "def" );
	EXPECT_EQ( -1, result );
	EXPECT_EQ( 0, strcmp( buffer, "" ) );
}

TEST_F( CcpSecureCrt, Functionsprintf_sFailsWhenFormatIsNull )
{
	char buffer[6] = "aaa";
	auto result = sprintf_s( buffer, nullptr );
	EXPECT_EQ( -1, result );
	EXPECT_EQ( 0, strcmp( buffer, "aaa" ) );
}


TEST_F( CcpSecureCrt, Functionwcscpy_sCopiesString )
{
	wchar_t buffer[6];
	auto result = wcscpy_s( buffer, 6, L"abc" );
	EXPECT_EQ( 0, result );
	EXPECT_EQ( 0, wcscmp( buffer, L"abc" ) );
}

TEST_F( CcpSecureCrt, Functionwcscpy_sFailsWhenDestinationIsTooShort )
{
	wchar_t buffer[6];
	auto result = wcscpy_s( buffer, 2, L"abc" );
	EXPECT_EQ( ERANGE, result );
	EXPECT_EQ( 0, wcscmp( buffer, L"" ) );
}

TEST_F( CcpSecureCrt, Functionwcscpy_sFailsWhenDestinationIsNull )
{
	auto result = wcscpy_s( nullptr, 10, L"abc" );
	EXPECT_EQ( EINVAL, result );
}

TEST_F( CcpSecureCrt, Functionwcscpy_sFailsWhenSourceIsNull )
{
	wchar_t buffer[6];
	auto result = wcscpy_s( buffer, 2, nullptr );
	EXPECT_EQ( EINVAL, result );
	EXPECT_EQ( 0, wcscmp( buffer, L"" ) );
}


template <typename Callable>
void VariadicArgumentsHelper( Callable callable, ... )
{
	va_list args;
    va_start( args, callable );
    callable( args );
    va_end( args );
}

TEST_F( CcpSecureCrt, Functionvsprintf_sWritesToString )
{
    VariadicArgumentsHelper( [&]( va_list args ) {
        char buffer[32];
        auto result = vsprintf_s( buffer, 32, "abc", args );
        EXPECT_EQ( 3, result );
        EXPECT_EQ( 0, strcmp( buffer, "abc" ) );
    } );
}

TEST_F( CcpSecureCrt, TemplatedFunctionvsprintf_sWritesToString )
{
    VariadicArgumentsHelper( [&]( va_list args ) {
        char buffer[32];
        auto result = vsprintf_s( buffer, "abc", args );
        EXPECT_EQ( 3, result );
        EXPECT_EQ( 0, strcmp( buffer, "abc" ) );
    } );
}

TEST_F( CcpSecureCrt, Functionvsprintf_sFailsWhenBufferIsTooShort )
{
    VariadicArgumentsHelper( [&]( va_list args ) {
        char buffer[6] = "aaa";
        auto result = vsprintf_s( buffer, 2, "abc", args );
        EXPECT_EQ( -1, result );
        EXPECT_EQ( 0, strcmp( buffer, "" ) );
    } );
}

TEST_F( CcpSecureCrt, Functionvsprintf_sFailsWhenFormatIsNull )
{
    VariadicArgumentsHelper( [&]( va_list args ) {
        char buffer[6] = "aaa";
        auto result = vsprintf_s( buffer, 6, nullptr, args );
        EXPECT_EQ( -1, result );
        EXPECT_EQ( 0, strcmp( buffer, "aaa" ) );
    } );
}



TEST_F( CcpSecureCrt, Functionvsnprintf_sPrintsString )
{
    VariadicArgumentsHelper( [&]( va_list args ) {
        char buffer[100];
        auto result = vsnprintf_s( buffer, 100, 50, "abc", args );

        EXPECT_EQ( 3, result );
        EXPECT_EQ( 0, strcmp( buffer, "abc" ) );
    } );
}

TEST_F( CcpSecureCrt, TemplateFunctionvsnprintf_sPrintsString )
{
    VariadicArgumentsHelper( [&]( va_list args ) {
        char buffer[100];
        auto result = vsnprintf_s( buffer, 50, "abc", args );

        EXPECT_EQ( 3, result );
        EXPECT_EQ( 0, strcmp( buffer, "abc" ) );
    } );
}

TEST_F( CcpSecureCrt, Functionvsnprintf_sTruncatesStringToMaxLength )
{
    VariadicArgumentsHelper( [&]( va_list args ) {
        char buffer[100];
        auto result = vsnprintf_s( buffer, 100, 5, "abc def", args );

        EXPECT_EQ( -1, result );
        EXPECT_EQ( 0, strcmp( buffer, "abc d" ) );
    } );
}

TEST_F( CcpSecureCrt, Functionvsnprintf_sReturnsEmptyStringOnOverflow )
{
    VariadicArgumentsHelper( [&]( va_list args ) {
        char buffer[5];
        auto result = vsnprintf_s( buffer, 5, 50, "abc def", args );

        EXPECT_EQ( -1, result );
        EXPECT_EQ( 0, strcmp( buffer, "" ) );
    } );
}

TEST_F( CcpSecureCrt, Functionvsnprintf_sSucceedsWithMaxLengthSetToTruncate )
{
    VariadicArgumentsHelper( [&]( va_list args ) {
        char buffer[50];
        auto result = vsnprintf_s( buffer, 50, _TRUNCATE, "abc def", args );

        EXPECT_EQ( 7, result );
        EXPECT_EQ( 0, strcmp( buffer, "abc def" ) );
    } );
}

TEST_F( CcpSecureCrt, Functionvsnprintf_sTruncatesWithMaxLengthSetToTruncate )
{
    VariadicArgumentsHelper( [&]( va_list args ) {
        char buffer[6];
        auto result = vsnprintf_s( buffer, 6, _TRUNCATE, "abc def", args );

        EXPECT_EQ( -1, result );
        EXPECT_EQ( 0, strcmp( buffer, "abc d" ) );
    } );
}

TEST_F( CcpSecureCrt, Functionvsnprintf_sFailsWhenFormatIsNull )
{
    VariadicArgumentsHelper( [&]( va_list args ) {
        char buffer[6];
        auto result = vsnprintf_s( buffer, 6, _TRUNCATE, nullptr, args );
        EXPECT_EQ( -1, result );
    } );
}

TEST_F( CcpSecureCrt, Functionvsnprintf_sFailsWhenCountIsZero )
{
    VariadicArgumentsHelper( [&]( va_list args ) {
        char buffer[6];
        auto result = vsnprintf_s( buffer, 6, 0, "abc def", args );
        EXPECT_EQ( -1, result );
        EXPECT_EQ( 0, buffer[0] );
    } );
}


TEST_F( CcpSecureCrt, Function_vscprintfCalculatesStringLength )
{
    VariadicArgumentsHelper( [&]( va_list args ) {
        auto result = _vscprintf( "abc def", args );
        EXPECT_EQ( 7, result );
    } );
}

TEST_F( CcpSecureCrt, Function_vscprintfFailsWhenFormatIsNull )
{
    VariadicArgumentsHelper( [&]( va_list args ) {
        auto result = _vscprintf( nullptr, args );
        EXPECT_EQ( -1, result );
    } );
}


TEST_F( CcpSecureCrt, Functiongmtime_sReturnsTime )
{
	time_t t;
	time( &t );

	tm result;
	auto code = gmtime_s( &result, &t );

	EXPECT_EQ( 0, code );
	EXPECT_LT( 2013, result.tm_year + 1900 );
	EXPECT_GT( 2100, result.tm_year + 1900 );
}

TEST_F( CcpSecureCrt, Functiongmtime_sFailsIfSourceIsNull )
{
	tm result;
	result.tm_year = 66;
	auto code = gmtime_s( &result, nullptr );

	EXPECT_EQ( EINVAL, code );
	EXPECT_EQ( -1, result.tm_year );
}

TEST_F( CcpSecureCrt, Functiongmtime_sFailsIfDestinationIsNull )
{
	time_t t;
	time( &t );

	auto code = gmtime_s( nullptr, &t );

	EXPECT_EQ( EINVAL, code );
}
