// Copyright © 2013 CCP ehf.

#include "include/CcpTime.h"
#include "include/CCPAssert.h"
#include <cfloat>
#include <cmath>

#ifdef _WIN32

#include <windows.h>
uint64_t CcpGetTimestamp()
{
	static LARGE_INTEGER last = {0};

	LARGE_INTEGER val;
	if( !QueryPerformanceCounter( &val ) ) 
	{
		return last.QuadPart;
	}

	// Safequard against time running backwards. Has been known to happen with
	// some processors, probably when speed changes.
	if( val.QuadPart > last.QuadPart )
	{
		last = val;
	}
	else
	{
		val = last;
	}
	return val.QuadPart;
}

uint64_t CcpGetTimestampFrequency()
{
	LARGE_INTEGER val;
	if( !QueryPerformanceFrequency( &val ) )
	{
		return 0;
	}

	return val.QuadPart;
}

uint64_t CcpGetTickCount()
{
    return GetTickCount64();
}

bool TimeAsDateTime( CcpDateTime& dateTime, CcpTime time )
{
	return FileTimeToSystemTime( reinterpret_cast<FILETIME*>( &time ), reinterpret_cast<SYSTEMTIME*>( &dateTime ) ) != 0;
}

bool TimeFromDateTime( CcpTime& timeStamp, const CcpDateTime& dateTime )
{
	return SystemTimeToFileTime( reinterpret_cast<const SYSTEMTIME*>( &dateTime ), reinterpret_cast<FILETIME*>( &timeStamp ) ) != 0;
}

CcpTime TimeNow()
{
	CcpTime time = 0;
	GetSystemTimeAsFileTime( (FILETIME*)&time );
	return time;
}


#elif defined(__APPLE__)

#include <mach/mach_time.h>
#include <time.h>
#include <sys/time.h>

uint64_t CcpGetTimestamp()
{
    return mach_absolute_time();
}

uint64_t CcpGetTimestampFrequency()
{
    static mach_timebase_info_data_t info = { 0 };
    if( info.denom == 0 )
    {
        mach_timebase_info( &info );
    }
    return 1000000000UL * info.denom / info.numer;
}

uint64_t CcpGetTickCount()
{
#if 1
	timespec ts = {};
	if( clock_gettime( CLOCK_MONOTONIC, &ts ) )
	{
		return 0;
	}
	return uint64_t( ts.tv_sec ) * uint64_t( 1000 ) + uint64_t( ts.tv_nsec / 1000000 );
#else
	if( host_get_clock_service( mach_host_self(), SYSTEM_CLOCK, &clk ) != KERN_SUCCESS )
	{
		return 0;
	}
	if( clock_get_time( clk, &tm ) != KERN_SUCCESS )
	{
		return 0;
	}
	mach_port_deallocate( mach_task_self(), cclock );

	return uint64_t( tp.tv_sec ) * uint64_t( 1000 ) + uint64_t( tp.tv_nsec / 1000000 );
#endif
}

#else

#include <time.h>

uint64_t CcpGetTimestamp()
{
	timespec tp;
	if( clock_gettime( CLOCK_MONOTONIC, &tp ) != 0 )
	{
		return 0;
	}

	return uint64_t( tp.tv_sec ) * uint64_t( 1000000000 ) + uint64_t( tp.tv_nsec );
}

uint64_t CcpGetTimestampFrequency()
{
	// clock_gettime returns nanoseconds
	return 1000000000;
}

uint64_t CcpGetTickCount()
{
    return clock() / ( CLOCKS_PER_SEC / 1000 );
}

#endif

#if !_WIN32

namespace
{
	const CcpTime MS_TO_UNIX_EPOCH = 11644473600LL * 1000L;
	const CcpTime TICKS_PER_MS = 10000;
}

bool TimeAsDateTime( CcpDateTime& dateTime, CcpTime time )
{
	time_t t = time_t( ( time / TICKS_PER_MS - MS_TO_UNIX_EPOCH ) / 1000 );
	auto timeInfo = *gmtime( &t );
	dateTime.year = 1900 + timeInfo.tm_year;
	dateTime.month = timeInfo.tm_mon + 1;
	dateTime.dayOfWeek = timeInfo.tm_wday;
	dateTime.day = timeInfo.tm_mday;
	dateTime.hour = timeInfo.tm_hour;
	dateTime.minute = timeInfo.tm_min;
	dateTime.second = timeInfo.tm_sec;
    dateTime.milliseconds = uint16_t( ( time / TICKS_PER_MS ) % 1000 );
	return true;
}

bool TimeFromDateTime( CcpTime& timeStamp, const CcpDateTime& dateTime )
{
	struct tm timetuple = {};
    timetuple.tm_year = dateTime.year - 1900;
	timetuple.tm_mon = dateTime.month - 1;
	timetuple.tm_mday = dateTime.day;
	timetuple.tm_hour = dateTime.hour;
	timetuple.tm_min = dateTime.minute;
	timetuple.tm_sec = dateTime.second;
	timetuple.tm_isdst = -1;

	auto timestamp = timegm( &timetuple );
	if( timestamp < 0 )
	{
		return false;
	}

	timeStamp = CcpTime( timestamp ) * 1000 + dateTime.milliseconds;
	timeStamp += MS_TO_UNIX_EPOCH;
	timeStamp *= TICKS_PER_MS;
	return true;
}

CcpTime TimeNow()
{
	const uint64_t HNSEC_SINCE_WINDOWS_EPOCH = 11644473600 * 10000000;

	struct timeval tv;
	if( gettimeofday( &tv, nullptr ) )
	{
		return 0;
	}
	CcpTime time = tv.tv_sec;
	time *= 1000000;
	time += tv.tv_usec;
	time *= 10;
	time += HNSEC_SINCE_WINDOWS_EPOCH;

	return time;
}

#endif



long TimeInMs( CcpTime time )
{
	time /= 10000;

	CCP_ASSERT( time <= LONG_MAX );

	return (long)time;
}


double TimeAsDouble( CcpTime time )
{
	double sec;

	// can either be UTC time or stopwatch time
	if( TimeIsUTC( time ) )
	{
#ifdef _MSC_VER
		_fpreset();
#endif
		CcpTime seconds = time / 10000000;
		sec = (double)seconds;

		time = ( time - ( seconds * 10000000 ) );
		sec += ( time / 10000 ) / 1000.0;
	}
	else
	{
#ifdef _MSC_VER
		_fpreset();
#endif
		CcpTime seconds = time / 10000000;
		sec = (double)seconds;

		time = ( time - ( seconds * 10000000 ) );
		sec += time / 10000000.0;
	}

	return sec;
}

float TimeAsFloat( CcpTime time )
{
	return (float)TimeAsDouble( time );
}

CcpTime TimeFromDouble( double time )
{
	double intpart, fracpart;
	fracpart = modf( time, &intpart );

	CcpTime t = ( CcpTime )( fracpart * 10000000.0 );
	t += ( ( CcpTime )intpart ) * 10000000;

	return t;
}
CcpTime TimeFromMS( long time )
{
	CcpTime t = ( CcpTime )( time * 10000 );
	return t;
}

bool TimeIsUTC( CcpTime time )
{
	// if time is later than the 18th century, let's
	// rule it UTC

	// UTC time starts at year 1600
	// so it's seconds * min * hours * days * years * 200
	const CcpTime EIGHTEENTHCENTURY =
		//10000000 * 60 * 60 * 24 * 365 * 200
		63072000000000000;

	if( time > EIGHTEENTHCENTURY )
		return true;
	else
		return false;
}
