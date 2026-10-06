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
std::wstring UTF8ToWide(const char* utf8String)
{
	std::wstring result;
	const std::string input = std::forward<std::string>(utf8String);
	auto iterator = input.begin();

	while (iterator != input.end())
	{
		unsigned char character = *iterator;
		wchar_t codePoint = character; // Assume sizeof(wchar_t) = 4
		uint32_t continuationBytes = 0;

		if (character >= 0x7F) // Code point is not ASCII
		{
			if ((character & 0xE0) == 0xC0)
			{
				codePoint = character & 0x1F; // First five bits represent code point
				continuationBytes = 1;
			}
			else if ((character & 0xF0) == 0xE0)
			{
				codePoint = character & 0x0F; // First four bits represent code point
				continuationBytes = 2;
			}
			else if ((character & 0xF8) == 0xF0)
			{
				codePoint = character & 0x07; // First three bits represent code point
				continuationBytes = 3;
			}
			else
			{
				// Malformed byte
				codePoint = 0xFFFD;
			}
		}

		++iterator;

		if (continuationBytes)
		{
			for (int i = 0; i < continuationBytes; i++)
			{
				character = *iterator;
				if ((character & 0xC0) != 0x80)
				{
					// Malformed byte
					codePoint = 0xFFFD;
					break;
				}

				character &= 0x3F; // First six bits represent code point
				codePoint = (codePoint << 6) | character;
				++iterator;
			}
		}

		result.push_back(codePoint);
	}

	return result;
}

std::string WideToUTF8( const wchar_t* wideString )
{
	std::string result;
	const std::wstring input = std::forward<std::wstring>( wideString );

	// A unicode code point encoded in UTF-8 consists of one to four bytes, depending on the code point range
	for (const auto& character : input)
	{
		if (character < 0x80) // U+0000-U+007F range (1-byte)
		{
			result.push_back( static_cast<char>(character));
		}
		else if (character < 0x800) // U+0080–U+07FF range (2-byte)
		{
			result.push_back( static_cast<char>(0xC0 | (character >> 6 )));
			result.push_back( static_cast<char>(0x80 | (character & 0x3F)));
		}
		else if (character < 0x10000) // U+0800–U+FFFF range (3-byte)
		{
			result.push_back( static_cast<char>(0xE0 | (character >> 12)));
			result.push_back( static_cast<char>(0x80 | ((character >> 6) & 0x3F)));
			result.push_back( static_cast<char>(0x80 | (character & 0x3F)));
		}
		else // U+10000–U+10FFFF range (4-byte)
		{
			result.push_back( static_cast<char>(0xF0 | (character >> 18)));
			result.push_back( static_cast<char>(0x80 | ((character >> 12) & 0x3F)));
			result.push_back( static_cast<char>(0x80 | ((character >> 6) & 0x3F)));
			result.push_back( static_cast<char>(0x80 | (character & 0x3F)));
		}
	}

	return result;
}
#endif
