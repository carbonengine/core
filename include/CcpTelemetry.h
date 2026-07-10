// Copyright © 2013 CCP ehf.

#pragma once
#ifndef CcpTelemetry_h
#define CcpTelemetry_h

#include "CcpColorConstants.h"
#include "CcpThread.h"
#include "carbon_core_export.h"

// CCP_TELEMETRY_ENABLED is on by default - to disable Telemetry
// define CCP_TELEMETRY_ENABLED as 0
#ifndef CCP_TELEMETRY_ENABLED
    #if _MSC_VER
		#define CCP_TELEMETRY_ENABLED 1
    #else
        #define CCP_TELEMETRY_ENABLED 0
	#endif
#endif

#if CCP_TELEMETRY_ENABLED
	#include "TrackableContainer.h"

	#define TMCM_GENERAL 1
	#define TMCM_CPP 2

	CARBON_CORE_API void CcpRegisterMutex( class CcpMutex& m, const char* owner, const char* name );

#else

	#define CcpRegisterMutex( m, owner, name )
#endif // CCP_TELEMETRY_ENABLED

CARBON_CORE_API void CcpRegisterThread( CcpThreadId_t threadId, const char* name );

// ---------------------------------------------------------------------------
// CaptureMasks:
// - CaptureMask(s) are used to determine if a given Zone should be emitted to
//   Telemetry tracking or not based on its origin, i.e. carbon component.
// - Each carbon component, core/blue/scheduler/etc..., will register for a
//   CaptureMask with their chosen "component display name" and be assigned an
//   available CaptureMask bit from a 64bit integer mask.
// - A Telemetry display color is also associated with the chosen CaptureMask bit,
//   either manually or automatically allocated based on "best available fit".
// ---------------------------------------------------------------------------
CARBON_CORE_API uint64_t CcpRegisterCaptureMask( const std::string& name );
CARBON_CORE_API uint64_t CcpRegisterCaptureMask( const std::string& name, CcpColor color );

struct CcpTelemetryConfig
{
	std::string applicationName;
	std::chrono::milliseconds captureDuration{};
	bool trackMemoryAllocations{false};
	bool trackLocks{false};
};

[[deprecated( "Use `CcpStartTelemetry( const CcpTelemetryConfig& config ) instead" )]] CARBON_CORE_API bool CcpStartTelemetry( const char* server, int connectionType, uint32_t maxThreadCount );
CARBON_CORE_API bool CcpStartTelemetry( const CcpTelemetryConfig& config );
CARBON_CORE_API void CcpStopTelemetry();
CARBON_CORE_API void CcpTelemetryTick();
CARBON_CORE_API uint32_t CcpTelemetryGetTickCount();

enum CcpTelemetryEvent
{
	CCP_TELEMETRY_STARTED,
	CCP_TELEMETRY_STOPPED,
};

typedef void ( *CcpOnTelemetryEventHandler )( CcpTelemetryEvent, void* userData );

CARBON_CORE_API void CcpRegisterTelemetryEventHandler( CcpOnTelemetryEventHandler, void* userData );
CARBON_CORE_API void CcpUnregisterTelemetryEventHandler( CcpOnTelemetryEventHandler, void* userData );

CARBON_CORE_API bool CcpTelemetryIsConnectionRequested();
CARBON_CORE_API bool CcpTelemetryIsConnected();
CARBON_CORE_API bool CcpTelemetryIsStarted();
CARBON_CORE_API bool CcpTelemetryIsStopped();
CARBON_CORE_API std::chrono::milliseconds CcpTelemetryRemainingCaptureDuration();
CARBON_CORE_API bool CcpTelemetryMemoryTrackingIsEnabled();
CARBON_CORE_API bool CcpTelemetryLockTrackingIsEnabled();

CARBON_CORE_API void CcpTelemetrySetActiveFiber( const std::string& name );
CARBON_CORE_API const std::string& CcpTelemetryGetActiveFiber();
CARBON_CORE_API void CcpTelemetryRemoveFiber( const std::string& name );

class TelemetryZone
{
public:
	TelemetryZone() = delete;
	CARBON_CORE_API TelemetryZone( uint32_t ctx, const char* name, const char* filename, uint32_t lineno, CcpColor color = CcpColor::SteelBlue );
	CARBON_CORE_API ~TelemetryZone();
	TelemetryZone( TelemetryZone&& other ) noexcept;
	TelemetryZone( const TelemetryZone& ) = delete;
	TelemetryZone& operator=( TelemetryZone&& ) = delete;
	TelemetryZone& operator=( const TelemetryZone& ) = delete;

	CARBON_CORE_API void text( const char* text ) const;

private:
	struct Private;
	std::unique_ptr<Private> m_impl;
};

CARBON_CORE_API void CcpTelemetryEnterZone( void* key, const char* name, const char* filename, uint32_t lineno );
CARBON_CORE_API void CcpTelemetryLeaveZone( void* key );
CARBON_CORE_API void CcpTelemetryZoneAddText( void* key, const char* text );

void CcpTelemetryTrackAllocation( void*, size_t );
void CcpTelemetryTrackDeallocation( void* );
#endif
