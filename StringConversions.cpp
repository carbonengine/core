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

#endif
