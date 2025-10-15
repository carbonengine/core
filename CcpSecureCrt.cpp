// Copyright © 2013 CCP ehf.

#ifndef _MSC_VER

#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <cerrno>
#include <cwchar>
#include "include/CcpSecureCrt.h"

int strncpy_s( char* dst, size_t dstBufferSize, const char* src, size_t count )
{
    if( dstBufferSize == 0 || !dst )
    {
        return EINVAL;
    }
    if( !src )
    {
        dst[0] = 0;
        return EINVAL;
    }
    bool truncate = false;
	if( count > dstBufferSize )
	{
        if( count == _TRUNCATE )
        {
            truncate = true;
        }
		count = dstBufferSize;
	}
    char* c = dst;
    for( size_t i = 0; i < count; ++i )
    {
        *c++ = *src;
        if( *src++ == 0 )
        {
            return 0;
        }
    }
    if( count < dstBufferSize )
    {
        dst[count] = 0;
    }
    else if( truncate )
    {
        dst[dstBufferSize - 1] = 0;
        return STRUNCATE;
    }
    else
    {
        dst[0] = 0;
        return ERANGE;
    }
	return 0;
}

int strcpy_s( char* dst, size_t dstBufferSize, const char* src )
{
    if( !src )
    {
        dst[0] = 0;
        return EINVAL;
    }
	return strncpy_s( dst, dstBufferSize, src, strlen( src ) );
}

int wcscpy_s( wchar_t* dst, size_t dstBufferSize, const wchar_t* src )
{
    if( !dst )
    {
        return EINVAL;
    }
    if( !src )
    {
        dst[0] = 0;
        return EINVAL;
    }
    wchar_t* c = dst;
    for( size_t i = 0; i < dstBufferSize; ++i )
    {
        *c++ = *src;
        if( *src++ == 0 )
        {
            return 0;
        }
    }
	dst[0] = 0;
	return ERANGE;
}

int wcsncpy_s( wchar_t* dst, size_t dstBufferSize, const wchar_t* src, size_t count )
{
    if( dstBufferSize == 0 || !dst )
    {
        return EINVAL;
    }
    if( !src )
    {
        dst[0] = 0;
        return EINVAL;
    }
    bool truncate = false;
	if( count > dstBufferSize )
	{
        if( count == _TRUNCATE )
        {
            truncate = true;
        }
		count = dstBufferSize;
	}
    wchar_t* c = dst;
    for( size_t i = 0; i < count; ++i )
    {
        *c++ = *src;
        if( *src++ == 0 )
        {
            return 0;
        }
    }
    if( count < dstBufferSize )
    {
        dst[count] = 0;
    }
    else if( truncate )
    {
        dst[dstBufferSize - 1] = 0;
        return STRUNCATE;
    }
    else
    {
        dst[0] = 0;
        return ERANGE;
    }
	return 0;
}

int vsprintf_s( char* buffer, size_t bufferSize, const char* fmt, va_list args )
{
    if( !buffer || !fmt )
    {
        return -1;
    }
	int result = vsnprintf( buffer, bufferSize, fmt, args );
    if( result < 0 )
    {
        return -1;
    }
    if( result >= bufferSize )
    {
        buffer[0] = 0;
        return -1;
    }
    return result;
}

int vsnprintf_s( char* buffer, size_t bufferSize, size_t count, const char* fmt, va_list args )
{
    if( !buffer || bufferSize == 0 || !fmt )
    {
        return -1;
    }
    bool truncate = count == _TRUNCATE;
	if( (count > bufferSize) || (count == _TRUNCATE) )
	{
		count = bufferSize - 1;
    }
    int result = vsnprintf( buffer, count + 1, fmt, args );
    if( result < 0 )
    {
        buffer[0] = 0;
        return -1;
    }
    if( result > bufferSize )
    {
        if( !truncate )
        {
            buffer[0] = 0;
        }
        return -1;
    }
    if( result > count && !truncate )
    {
        return -1;
    }
	return result;
}

int _vscprintf( const char* format, va_list args )
{
    if( !format )
    {
        return -1;
    }
	return vsnprintf( nullptr, 0, format, args );
}

int fopen_s( FILE** pFile, const char *filename, const char *mode )
{
	*pFile = fopen( filename, mode );
	if( !*pFile )
	{
		return errno;
	}

	return 0;
}

int gmtime_s( struct tm* _tm, const time_t* time )
{
    if( _tm == nullptr )
    {
        return EINVAL;
    }
    else if( time == nullptr )
    {
        _tm->tm_sec = -1;
        _tm->tm_min = -1;
        _tm->tm_hour = -1;
        _tm->tm_mday = -1;
        _tm->tm_mon = -1;
        _tm->tm_year = -1;
        _tm->tm_wday = -1;
        _tm->tm_yday = -1;
        _tm->tm_isdst = -1;
        _tm->tm_gmtoff = -1;
        _tm->tm_zone = nullptr;

        return EINVAL;
    }

    gmtime_r( time, _tm );

    return 0;
}

int sscanf_s( const char* buffer, const char* format, ... )
{
	va_list ap;
	va_start( ap, format );
	int retval = vsscanf( buffer, format, ap );
	va_end( ap );
    return retval;
}

#endif
