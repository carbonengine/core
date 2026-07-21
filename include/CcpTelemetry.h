// Copyright © 2013 CCP ehf.

#pragma once
#ifndef CcpTelemetry_h
#define CcpTelemetry_h

#include <chrono>
#include <memory>
#include <string>
#include <vector>

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
// - An active CaptureMask defaults to "all" but can be set/narrowed before or
//   during a Telemetry session is started using either:
//   - a numerical bit mask value of the active CaptureMask
//   - list of "component display names" ("all" is allowed)
// - Setting active CaptureMask is available for both:
//   - already registered components
//   - "pending" (yet to be registered) components
// ---------------------------------------------------------------------------
struct CcpCaptureMaskInfo
{
	std::string name;    // The lower-case display name of the CaptureMask (carbon-component)
	uint64_t maskBit{0}; // The single bit assigned to this CaptureMask during registration
	CcpColor color{CcpColor::White}; // The chosen/allocated color for the CaptureMask
};

CARBON_CORE_API uint64_t CcpRegisterCaptureMask( const std::string& name, CcpColor color = CcpColor::Fuchsia );
CARBON_CORE_API std::vector<CcpCaptureMaskInfo> CcpGetRegisteredCaptureMasks();

CARBON_CORE_API bool CcpSetActiveCaptureMask( const std::vector<std::string>& maskNames );
CARBON_CORE_API uint64_t CcpGetActiveCaptureMask();


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

// The CaptureMaskBitTag is a work-around to solve disambiguity between the two
// TelemetryZone constructors (uint32_t ctx vs uint64_t captureMaskBit) and
// still preserve ABI compatibility.
// The ambiguity would otherwise be triggered if the default CcpColor parameter
// is omitted in the original (now deprecated) uint32_t ctx constructor.
struct CaptureMaskBitTag { explicit CaptureMaskBitTag() = default; };
inline constexpr CaptureMaskBitTag CaptureMaskBit{};

class TelemetryZone
{
public:
	TelemetryZone() = delete;

	[[deprecated( "Use `TelemetryZone( CaptureMaskBitTag, uint64_t captureMaskBit, ... )` instead" )]]
	CARBON_CORE_API TelemetryZone( uint32_t ctx, const char* name, const char* filename, uint32_t lineno, CcpColor color = CcpColor::SteelBlue );

	CARBON_CORE_API TelemetryZone( CaptureMaskBitTag, uint64_t captureMaskBit, const char* name, const char* filename, uint32_t lineno );
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

[[deprecated( "Use `CcpTelemetryEnterZone( void* key, uint64_t captureMaskBit, ... )` instead" )]]
CARBON_CORE_API void CcpTelemetryEnterZone( void* key, const char* name, const char* filename, uint32_t lineno );

CARBON_CORE_API void CcpTelemetryEnterZone( void* key, uint64_t captureMaskBit, const char* name, const char* filename, uint32_t lineno );
CARBON_CORE_API void CcpTelemetryLeaveZone( void* key );
CARBON_CORE_API void CcpTelemetryZoneAddText( void* key, const char* text );

void CcpTelemetryTrackAllocation( void*, size_t );
void CcpTelemetryTrackDeallocation( void* );
#endif
