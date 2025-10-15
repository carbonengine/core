// Copyright © 2013 CCP ehf.

#pragma once
#ifndef CcpTime_h
#define CcpTime_h

#include "CcpTypes.h"
#include "carbon_core_export.h"

// Get a timestamp, with the highest precision available on the system. The unit
// is not defined - may vary between systems, but is consistent throughout the
// session.
CARBON_CORE_API uint64_t CcpGetTimestamp();

// Get the timestamp frequency of CcpGetTimestamp, in counts per second. Use
// these two functions together to measure time.
CARBON_CORE_API uint64_t CcpGetTimestampFrequency();

// Low-resolution timer in milliseconds
CARBON_CORE_API uint64_t CcpGetTickCount();


typedef int64_t CcpTime;

struct CcpDateTime
{
    uint16_t year;
    uint16_t month;
    uint16_t dayOfWeek;
    uint16_t day;
    uint16_t hour;
    uint16_t minute;
    uint16_t second;
    uint16_t milliseconds;
};


CARBON_CORE_API long TimeInMs( CcpTime time );
CARBON_CORE_API double TimeAsDouble( CcpTime time );
CARBON_CORE_API float TimeAsFloat( CcpTime time );
CARBON_CORE_API bool TimeIsUTC( CcpTime time );
CARBON_CORE_API CcpTime TimeFromDouble( double time );
CARBON_CORE_API CcpTime TimeFromMS( long time );

CARBON_CORE_API bool TimeAsDateTime( CcpDateTime& dateTime, CcpTime time );
CARBON_CORE_API bool TimeFromDateTime( CcpTime& timeStamp, const CcpDateTime& dateTime );
CARBON_CORE_API CcpTime TimeNow();


#endif // CcpTime_h