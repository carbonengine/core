// Copyright © 2013 CCP ehf.

#pragma once
#ifndef CcpSecureCrt_h
#define CcpSecureCrt_h

#ifndef _MSC_VER

#include <cstdarg>
#include <ctime>
#include <algorithm>

#include "carbon_core_export.h"

#if !defined(_TRUNCATE)
#define _TRUNCATE ((size_t)-1)
#define EINVAL          22
#define ERANGE          34
#define STRUNCATE       80
#endif

template< size_t sizeOfBuffer >
int _snprintf_s( char (&buffer)[sizeOfBuffer], size_t count, const char* fmt, ... )
{
    if( !fmt )
    {
        return -1;
    }
	int retval;
	va_list ap;
    
    size_t maxLength;

	if( count >= sizeOfBuffer )
	{
		maxLength = sizeOfBuffer;
	}
    else
    {
        maxLength = count + 1;
    }

	va_start( ap, fmt );
	retval = vsnprintf( buffer, maxLength, fmt, ap );
	va_end( ap );

    if( retval >= 0 )
    {
        if( size_t( retval ) > sizeOfBuffer )
        {
            retval = -1;
            if( count != _TRUNCATE )
            {
                buffer[0] = 0;
            }
        }
        else if( size_t( retval ) >= count )
        {
            retval = -1;
        }
	}

	return retval;
}


CARBON_CORE_API int strncpy_s( char* dst, size_t dstBufferSize, const char* src, size_t count );

template< size_t dstBufferSize >
inline int strncpy_s( char (&dst)[dstBufferSize], const char* src, size_t count )
{
	return strncpy_s( dst, dstBufferSize, src, count );
}

CARBON_CORE_API int strcpy_s( char* dst, size_t dstBufferSize, const char* src );

template< size_t dstBufferSize >
int strcpy_s( char (&dst)[dstBufferSize], const char* src )
{
	return strcpy_s( dst, dstBufferSize, src );
}

template< size_t sizeOfBuffer >
int sprintf_s( char (&buffer)[sizeOfBuffer], const char* fmt, ... )
{
    if( !fmt )
    {
        return -1;
    }
	int retval;
	va_list ap;

	va_start( ap, fmt );
	retval = vsnprintf( buffer, sizeOfBuffer, fmt, ap );
	va_end( ap );

	if( (0 <= retval) && (sizeOfBuffer <= (size_t) retval) )
	{
        buffer[0] = 0;
		retval = -1;
	}

	return retval;
}

inline int sprintf_s( char* buffer, size_t sizeOfBuffer, const char* fmt, ... )
{
    if( !fmt )
    {
        return -1;
    }
	int retval;
	va_list ap;

	va_start( ap, fmt );
	retval = vsnprintf( buffer, sizeOfBuffer, fmt, ap );
	va_end( ap );

	if( (0 <= retval) && (sizeOfBuffer <= (size_t) retval) )
	{
        buffer[0] = 0;
		retval = -1;
	}

	return retval;
}

CARBON_CORE_API int wcscpy_s( wchar_t* dst, size_t dstBufferSize, const wchar_t* src );
int wcsncpy_s( wchar_t* dst, size_t dstBufferSize, const wchar_t* src, size_t count );

template< size_t bufferSize >
int wcsncpy_s( wchar_t (&buffer)[bufferSize], const wchar_t* src, size_t count )
{
	return wcsncpy_s( buffer, bufferSize, src, count );
}

CARBON_CORE_API int vsprintf_s( char* buffer, size_t bufferSize, const char* fmt, va_list args );

template< size_t bufferSize >
int vsprintf_s( char (&buffer)[bufferSize], const char* fmt, va_list args )
{
	return vsprintf_s( buffer, bufferSize, fmt, args );
}

CARBON_CORE_API int vsnprintf_s( char* buffer, size_t bufferSize, size_t count, const char* fmt, va_list args );

template< size_t bufferSize >
int vsnprintf_s( char (&buffer)[bufferSize], size_t count, const char* fmt, va_list args )
{
	return vsnprintf_s( buffer, bufferSize, count, fmt, args );
}

CARBON_CORE_API int _vscprintf( const char* format, va_list args );

CARBON_CORE_API int fopen_s( FILE** pFile, const char *filename, const char *mode );

CARBON_CORE_API int gmtime_s( struct tm* _tm, const time_t* time );

CARBON_CORE_API int sscanf_s( const char* buffer, const char* format, ... );

inline int swprintf_s( wchar_t *wcs, size_t maxlen, const wchar_t *format, ... )
{
    if( !format )
    {
        return -1;
    }
    va_list ap;
    va_start( ap, format );
    int result = vswprintf( wcs, maxlen, format, ap );
    va_end( ap );
    return result;
}

template< size_t maxlen >
int swprintf_s( wchar_t (&wcs)[maxlen], const wchar_t *format, ... )
{
    if( !format )
    {
        return -1;
    }
    va_list ap;
    va_start( ap, format );
    int result = vswprintf( wcs, maxlen, format, ap );
    va_end( ap );
    return result;
}

#define _strnicmp strncasecmp

#endif // _MSC_VER
#endif // CcpSecureCrt_h
