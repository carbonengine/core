// Copyright © 2013 CCP ehf.

#ifndef _IBLUE_CRASH_INTERFACE_H_
#define _IBLUE_CRASH_INTERFACE_H_
#pragma once

#include "carbon_core_export.h"

extern CARBON_CORE_API struct ICrashReporter* BeCrashes;

struct ICrashReporter
{
	// Sets key/value pairs (usednch in crash report parameters)
	virtual void SetCrashKeyValue( const char* key, const char* val ) = 0;

	// Turn the crash upload on or off
	virtual void EnableCrashReporting( bool enable ) = 0;

	// Is crash report upload currently enabled?
	virtual bool IsCrashReportingEnabled() = 0;

	virtual void ProduceImmediateDump() = 0;
};

#endif