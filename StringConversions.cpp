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
#include "CCPMemory.h"


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

#ifndef __APPLE__

// UTF-8 <-> wchar_t (UTF-32 on Linux) conversions. On Apple these two
// overloads are implemented in StringConversions.mm using CoreFoundation.
std::wstring UTF8ToWide( const char* utf8String )
{
	std::wstring result;
	const unsigned char* s = reinterpret_cast<const unsigned char*>( utf8String );
	while( *s )
	{
		uint32_t codePoint;
		int continuationBytes;
		if( *s < 0x80 )
		{
			codePoint = *s;
			continuationBytes = 0;
		}
		else if( ( *s & 0xE0 ) == 0xC0 )
		{
			codePoint = *s & 0x1F;
			continuationBytes = 1;
		}
		else if( ( *s & 0xF0 ) == 0xE0 )
		{
			codePoint = *s & 0x0F;
			continuationBytes = 2;
		}
		else if( ( *s & 0xF8 ) == 0xF0 )
		{
			codePoint = *s & 0x07;
			continuationBytes = 3;
		}
		else
		{
			++s;
			result.push_back( wchar_t( 0xFFFD ) );
			continue;
		}
		++s;
		for( ; continuationBytes; --continuationBytes )
		{
			if( ( *s & 0xC0 ) != 0x80 )
			{
				codePoint = 0xFFFD;
				break;
			}
			codePoint = ( codePoint << 6 ) | ( *s & 0x3F );
			++s;
		}
		result.push_back( wchar_t( codePoint ) );
	}
	return result;
}

std::string WideToUTF8( const wchar_t* wideString )
{
	std::string result;
	for( const wchar_t* p = wideString; *p; ++p )
	{
		uint32_t c = uint32_t( *p );
		if( c < 0x80 )
		{
			result.push_back( char( c ) );
		}
		else if( c < 0x800 )
		{
			result.push_back( char( 0xC0 | ( c >> 6 ) ) );
			result.push_back( char( 0x80 | ( c & 0x3F ) ) );
		}
		else if( c < 0x10000 )
		{
			result.push_back( char( 0xE0 | ( c >> 12 ) ) );
			result.push_back( char( 0x80 | ( ( c >> 6 ) & 0x3F ) ) );
			result.push_back( char( 0x80 | ( c & 0x3F ) ) );
		}
		else
		{
			result.push_back( char( 0xF0 | ( c >> 18 ) ) );
			result.push_back( char( 0x80 | ( ( c >> 12 ) & 0x3F ) ) );
			result.push_back( char( 0x80 | ( ( c >> 6 ) & 0x3F ) ) );
			result.push_back( char( 0x80 | ( c & 0x3F ) ) );
		}
	}
	return result;
}

#endif // !__APPLE__

#endif
