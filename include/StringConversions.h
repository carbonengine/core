// Copyright © 2013 CCP ehf.
//
// This file provides the CW2A and CA2W macros. On Windows we simply pull in
// the ATL macros - on other platforms we provide our own implemented with
// wcsrtombs and mbsrtowcs.
//

#pragma once

#include <string>
#include "carbon_core_export.h"

CARBON_CORE_API std::wstring UTF8ToWide( const char* utf8String );
CARBON_CORE_API std::wstring UTF8ToWide( const std::string& utf8String );
CARBON_CORE_API std::string WideToUTF8( const wchar_t* wideString );
CARBON_CORE_API std::string WideToUTF8( const std::wstring& wideString );

#if _WIN32
#include <atlbase.h>
#else

#include "CcpMacros.h"
#include <stdint.h>

class CARBON_CORE_API BlueConvertWideToAscii
{
public:
	BlueConvertWideToAscii( const wchar_t* src );
	~BlueConvertWideToAscii();

	operator const char*() const
	{
		return m_converted;
	}

private:
	void Init( const wchar_t* src );

private:
	static const uint32_t BUFFER_SIZE = 128;
	char* m_converted;
	char m_buffer[BUFFER_SIZE];
};


class CARBON_CORE_API BlueConvertAsciiToWide
{
public:
	BlueConvertAsciiToWide( const char* src );
	~BlueConvertAsciiToWide();

	operator const wchar_t*() const
	{
		return m_converted;
	}

private:
	void Init( const char* src );

private:
	static const uint32_t BUFFER_SIZE = 128;
	wchar_t* m_converted;
	wchar_t m_buffer[BUFFER_SIZE];
};

typedef BlueConvertWideToAscii CW2A;
typedef BlueConvertAsciiToWide CA2W;

#endif
