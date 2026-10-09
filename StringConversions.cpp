// Copyright © 2013 CCP ehf.


#include "include/StringConversions.h"

std::wstring UTF8ToWide( const std::string& utf8String )
{
	return UTF8ToWide( utf8String.c_str() );
}

std::string WideToUTF8( const std::wstring& wideString )
{
	return WideToUTF8( wideString.c_str() );
}

#if _WIN32

std::wstring UTF8ToWide( const char* utf8String )
{
	return std::wstring( CA2W( utf8String, CP_UTF8 ) );
}

std::string WideToUTF8( const wchar_t* wideString )
{
	return std::string( CW2A( wideString, CP_UTF8 ) );
}

#else

#include <wchar.h>
#include <string.h>
#include "CcpMemory.h"


BlueConvertWideToAscii::BlueConvertWideToAscii( const wchar_t* src ) : m_converted( nullptr )
{
	Init( src );
}

BlueConvertWideToAscii::~BlueConvertWideToAscii()
{
	if( m_converted != m_buffer )
	{
		CCP_FREE( (void*)m_converted );
	}
}

void BlueConvertWideToAscii::Init( const wchar_t* src )
{
	size_t sizeNeeded = wcsrtombs( nullptr, &src, 0, nullptr );
	if( sizeNeeded == (size_t)-1 )
	{
		m_converted = m_buffer;
		strcpy( m_converted, "Invalid string" );
		return;
	}

	if( sizeNeeded >= BUFFER_SIZE )
	{
		m_converted = (char*)CCP_MALLOC( "ConvertWideToAscii", sizeNeeded + 1 );
	}
	else
	{
		m_converted = m_buffer;
	}
	wcsrtombs( m_converted, &src, sizeNeeded, nullptr );
	m_converted[sizeNeeded] = 0;
}

BlueConvertAsciiToWide::BlueConvertAsciiToWide( const char* src ) : m_converted( nullptr )
{
	Init( src );
}

BlueConvertAsciiToWide::~BlueConvertAsciiToWide()
{
	if( m_converted != m_buffer )
	{
		CCP_FREE( (void*)m_converted );
	}
}

void BlueConvertAsciiToWide::Init( const char* src )
{
	size_t srcLen = strlen( src );
	size_t sizeNeeded = mbsrtowcs( nullptr, &src, srcLen, nullptr );
	if( sizeNeeded == (size_t)-1 )
	{
		m_converted = m_buffer;
		wcscpy( m_converted, L"Invalid string" );
		return;
	}

	if( sizeNeeded >= BUFFER_SIZE )
	{
		m_converted = (wchar_t*)CCP_MALLOC( "ConvertAsciiToWide", (sizeNeeded + 1) * sizeof( wchar_t ) );
	}
	else
	{
		m_converted = m_buffer;
	}
	mbsrtowcs( m_converted, &src, srcLen, nullptr );
	m_converted[sizeNeeded] = 0;
}

#endif

#if __linux__
#include <iconv.h>

std::wstring UTF8ToWide(const char* utf8String)
{
	std::wstring result;

	const auto conversion = iconv_open("WCHAR_T", "UTF-8");
	if (conversion == reinterpret_cast<iconv_t>(-1))
	{
		CCP_LOGERR("Failed to create conversion algorithm from UTF-8 to WCHAR_T");
		return result;
	}

	std::string input = utf8String;
	auto inputSize= input.size();
	auto inputBuffer = input.data();

	std::vector<wchar_t> conversionBuffer(inputSize + 1);
	auto outputBuffer = reinterpret_cast<char*>(conversionBuffer.data());
	auto outputSize = conversionBuffer.size() * sizeof(wchar_t);

	if (const auto numConverted = iconv(conversion, &inputBuffer , &inputSize, &outputBuffer,&outputSize); numConverted == static_cast<size_t>(-1))
	{
		CCP_LOGERR("Failed to convert string from UTF-8 to WCHAR_T: [%s]", utf8String);
		iconv_close(conversion);
		return result;
	}

	result = conversionBuffer.data();

	iconv_close(conversion);

	return result;
}

std::string WideToUTF8( const wchar_t* wideString )
{
	std::string result;

	const auto conversion = iconv_open("UTF-8", "WCHAR_T");
	if (conversion == reinterpret_cast<iconv_t>(-1))
	{
		CCP_LOGERR("Failed to create conversion algorithm from UTF-8 to WCHAR_T");
		return result;
	}

	std::wstring input = wideString;
	auto inputSize= input.size() * sizeof(wchar_t);
	auto inputBuffer = reinterpret_cast<char*>(input.data());

	std::vector<char> conversionBuffer(inputSize + 1);
	auto outputBuffer = conversionBuffer.data();
	auto outputSize = conversionBuffer.size();

	if (const auto numConverted = iconv(conversion, &inputBuffer , &inputSize, &outputBuffer,&outputSize); numConverted == static_cast<size_t>(-1))
	{
		CCP_LOGERR("Failed to convert string from WCHAR_T to UTF-8: [%ls]", wideString);
		iconv_close(conversion);
		return result;
	}

	result = conversionBuffer.data();

	iconv_close(conversion);

	return result;
}
#endif
