// Copyright © 2013 CCP ehf.
//
// include this file to pull in the full public interface of CcpCore. This is
// ideal for putting in your StdAfx.h file.

#pragma once
#ifndef CcpCore_h
#define CcpCore_h

#include "CcpTypes.h"
#include "CcpMacros.h"
#include "CCPMemory.h"
#include "CCPMemoryTracker.h"
#include "CCPAssert.h"
#include "CcpMutex.h"
#include "CcpSemaphore.h"
#include "CcpTelemetry.h"
#include "CachedAllocator.h"
#include "CCPHash.h"
#include "TrackableContainer.h"
#include "ICrashReporter.h"
#include "CcpSecureCrt.h"
#include "CcpProcess.h"
#include "CcpThread.h"
#include "CcpTime.h"
#include "CcpAtomic.h"
#include "CcpScopeGuard.h"
#include "CcpStatistics.h"
#include "ICcpStream.h"
#include "CcpFileUtils.h"
#include "StringConversions.h"
#include "CcpCrash.h"
#include "CcpDefines.h"

#endif
