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

	std::string_view input = utf8String;
	auto inputBufferRemainingByteCount = input.size();
	auto inputBuffer = const_cast<char*>(input.data()); // iconv will increment buffer

	auto outputBufferSize = input.size() * sizeof(wchar_t); // Size is in bytes
	result.resize(outputBufferSize);
	auto outputBuffer = reinterpret_cast<char*>(result.data()); // Calculate buffer position in case backing buffer is reallocated
	auto outputBufferRemainingByteCount = outputBufferSize;
	iconv(conversion, &inputBuffer , &inputBufferRemainingByteCount, &outputBuffer, &outputBufferRemainingByteCount);

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

	const std::wstring_view input = wideString;
	auto inputBufferRemainingByteCount = input.size() * sizeof(wchar_t);
	auto inputBuffer = const_cast<char*>(reinterpret_cast<const char*>(input.data())); // iconv will increment buffer

	// UTF8 code points require four bytes at most
	auto outputBufferSize = input.size() * 4; // Size is in bytes
	result.resize(outputBufferSize);
	auto outputBuffer = result.data(); // Calculate buffer position in case backing buffer is reallocated
	auto outputBufferRemainingByteCount = outputBufferSize;
	iconv(conversion, &inputBuffer , &inputBufferRemainingByteCount, &outputBuffer,&outputBufferRemainingByteCount);
	const auto bytesRead = outputBufferSize - outputBufferRemainingByteCount;

	iconv_close(conversion);

	// The result must be truncated to the number of character elements read into the output buffer
	result.resize(bytesRead);

	return result;
}
#endif
