// Copyright © 2013 CCP ehf.

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <optional>
#include <queue>

#include "include/CcpAssert.h"
#include "include/CcpMutex.h"
#include "include/CcpLog.h"
#include "include/CcpTelemetry.h"
#include "include/CcpTime.h"

static CcpLogChannel_t s_ch = CCP_LOG_DEFINE_CHANNEL( "Telemetry" );

#if CCP_TELEMETRY_ENABLED

#pragma warning(push)
#pragma warning(disable : 4996)
#include <tracy/Tracy.hpp>
#pragma warning(pop)
#include <tracy/TracyC.h>

/*!
	\def TMCM_GENERAL
	\brief Legacy index into the registered profiler category array, denoting the default "general" category.

	This used to be a bitmask value back when categories were selected via a bitmask rather than an index into
	`CcpTelemetryGetRegisteredCategories()`. It is kept around for source compatibility with older call sites
	that still pass it to the deprecated `TelemetryZone( uint32_t, ... )` constructor.

	\see TMCM_CPP
	\see CcpTelemetryCategoryRegister
*/

/*!
	\def TMCM_CPP
	\brief Legacy index into the registered profiler category array, denoting the built-in "cpp" category.

	\see TMCM_GENERAL
*/

typedef std::set<std::string> FiberNameStore;

struct TelemetryZone::Private
{
	std::optional<TracyCZoneCtx> telemetryContext; //!< If active, this contains the context object required for the Tracy integration.
	FiberNameStore::const_iterator fiber; //!< The fiber the zone belongs to.
};

/*!
	\struct CcpTelemetryCategory
	\brief Opaque, registered category that zones can be tagged with.

	A `CcpTelemetryCategory` groups related `TelemetryZone` instances (e.g. "rendering", "physics", "scripting")
	so that they can be shown in a distinct color and selectively captured at runtime via
	`CcpTelemetrySetActiveCategories()`. Instances are only ever handed out by reference from
	`CcpTelemetryCategoryRegister()` and `CcpTelemetryGetRegisteredCategories()` - the type itself is opaque to
	consumers of the header, which only ever see a forward declaration.

	\par Example
	\code
	auto [category, ok] = CcpTelemetryCategoryRegister( "physics", CcpColor::Orange );
	if( ok )
	{
		TelemetryZone zone( category, "StepSimulation", __FILE__, __LINE__ );
		// ... simulate ...
	}
	\endcode

	\see CcpTelemetryCategoryRegister
	\see CcpTelemetrySetActiveCategories
*/
struct CcpTelemetryCategory
{
	std::string name;
	CcpColor color{CcpColor::White};
	uint64_t captureBit{0};
};

enum ProfilerState {
	Stopped,
	StartRequested,
	Started,
	StopRequested,
};

std::chrono::steady_clock::time_point s_profilerStartTime;
std::atomic<ProfilerState> s_profilerState{ProfilerState::Stopped};

FiberNameStore s_fiberNameStore; // Persisted fiber name string store, including the empty "root" fiber name

thread_local FiberNameStore::const_iterator t_activeFiber{ s_fiberNameStore.end() }; // default to having no fiber

template<>
struct std::less<FiberNameStore::const_iterator>
{
	bool operator()(const FiberNameStore::const_iterator& lhs, const FiberNameStore::const_iterator& rhs) const
	{
		return lhs->c_str() < rhs->c_str();
	}
};

typedef std::map<FiberNameStore::const_iterator, std::stack<TelemetryZone>> TaskletZoneStore;
thread_local TaskletZoneStore t_taskletZoneStore; // Per-thread record of zones instrumented from python
thread_local TaskletZoneStore::iterator t_activeTaskletZoneStore{ t_taskletZoneStore.end() };
thread_local std::set<void*> t_manuallyTrackedZones; // Keep track of zones created through `CcpTelemetryEnterZone` to ensure that we only pop off the zone store's stack when leaving a manually created zone

constexpr std::chrono::milliseconds s_cleanupDelay{5000};
std::map<FiberNameStore::const_iterator, std::chrono::steady_clock::time_point> s_fiberEraseMap; // Map of fibers scheduled for erasure

typedef TrackableStdMap<CcpMutex*, std::pair<const char*,const char*>> MutexNameMap_t;
typedef TrackableStdMap<CcpThreadId_t , const char*> ThreadNameMap_t;
typedef TrackableStdVector<std::pair<CcpOnTelemetryEventHandler, void*>> EventHandlerVector_t;

namespace
{
	uint32_t s_telemetryTick = 0;

	CcpTelemetryConfig s_config;

	MutexNameMap_t& GetMutexNameMap()
	{
		static MutexNameMap_t s_mutexNames( "CcpTelemetry/s_mutexNames" );
		return s_mutexNames;
	}

	ThreadNameMap_t& GetThreadNameMap()
	{
		static ThreadNameMap_t s_threadNames( "CcpTelemetry/s_threadNames" );
		return s_threadNames;
	}

	EventHandlerVector_t& GetEventHandlers()
	{
		static EventHandlerVector_t s_eventHandlers( "CcpTelemetry/s_eventHandlers" );
		return s_eventHandlers;
	}

	// -------------------------------
	// ProfilerCategory specifics:
	// -------------------------------
	constexpr size_t CCP_TELEMETRY_CATEGORIES_MAX{64};

	CcpMutex s_profilerCategoryRegistryLock( "CcpTelemetry", "ProfilerCategoryRegistry" );

	// Fixed size array of registered ProfilerCategories.
	std::array<std::optional<CcpTelemetryCategory>, CCP_TELEMETRY_CATEGORIES_MAX> s_registeredProfilerCategories{
		CcpTelemetryCategory{ "general", CcpColor::SteelBlue, TMCM_GENERAL }, // legacy definition from TMCM_GENERAL, used to be a bitmask, but can now be treated as index into this array
		CcpTelemetryCategory{ "cpp", CcpColor::Yellow, TMCM_CPP }, // legacy value from TMCM_CPP, used to be a bitmask, but can now be treated as index into this array
		CcpTelemetryCategory{ "core", CcpColor::LightGreen, 1<<2 }
	};

	uint64_t s_profilerCategoryCaptureMask{0};

	bool IsProfilerCategoryActive( uint64_t captureBit )
	{
		return ( s_profilerCategoryCaptureMask & captureBit ) != 0;
	}
}

/*!
	\brief Compares two `CcpTelemetryCategory` instances for equality.

	Categories are considered equal purely based on their registered name; the color and internal capture bit
	are not taken into account.

	\param lhs First category to compare.
	\param rhs Second category to compare.
	\return `true` if both categories share the same name, `false` otherwise.
*/
bool operator==( const CcpTelemetryCategory& lhs, const CcpTelemetryCategory& rhs )
{
	// Profiler Categories need to be unique by name only
	return lhs.name == rhs.name;
}

/*!
	\brief Returns the display name a `CcpTelemetryCategory` was registered with.

	\param category Category to query, as obtained from `CcpTelemetryCategoryRegister()` or
	                 `CcpTelemetryGetRegisteredCategories()`.
	\return The category's name.

	\par Example
	\code
	auto categories = CcpTelemetryGetRegisteredCategories();
	for( const CcpTelemetryCategory& category : categories )
	{
		printf( "%s\n", CcpTelemetryCategoryGetName( category ).c_str() );
	}
	\endcode
*/
const std::string& CcpTelemetryCategoryGetName( const CcpTelemetryCategory& category )
{
	return category.name;
}

/*!
	\brief Returns the color a `CcpTelemetryCategory` should be rendered with in the profiler UI.

	\param category Category to query, as obtained from `CcpTelemetryCategoryRegister()` or
	                 `CcpTelemetryGetRegisteredCategories()`.
	\return The category's display color.
*/
CcpColor CcpTelemetryCategoryGetColor( const CcpTelemetryCategory& category )
{
	return category.color;
}

/*!
	\brief Registers a new telemetry category, or returns the existing one if `name` is already registered.

	Categories are used to group `TelemetryZone` instances (e.g. by subsystem) and to let consumers selectively
	enable capture for only a subset of zones via `CcpTelemetrySetActiveCategories()`. Up to
	`PROFILER_CATEGORIES_MAX` (64) categories can be registered for the lifetime of the process; three of them
	("general", "cpp", "core") are pre-registered by default.

	\param name  Unique, human-readable name for the category. Must not be empty.
	\param color Color the category's zones should be rendered with. Defaults to `CcpColor::SteelBlue`.
	\return A pair of a reference to the (newly or previously) registered category, and a `bool` that is `true`
	        on success. On failure - an empty `name`, or all 64 slots already taken - the returned reference
	        refers to a static, empty placeholder category and the `bool` is `false`.

	\par Example
	\code
	auto [category, ok] = CcpTelemetryCategoryRegister( "rendering", CcpColor::Green );
	if( ok )
	{
		TelemetryZone zone( category, "DrawFrame", __FILE__, __LINE__ );
	}

	// Registering the same name again simply returns the existing category.
	auto [same, stillOk] = CcpTelemetryCategoryRegister( "rendering" );
	\endcode

	\see CcpTelemetryGetRegisteredCategories
	\see CcpTelemetrySetActiveCategories
*/
std::pair<const CcpTelemetryCategory&, bool> CcpTelemetryCategoryRegister( const std::string& name, CcpColor color )
{
	static const CcpTelemetryCategory empty;
	CcpAutoMutex lock( s_profilerCategoryRegistryLock );

	if( name.empty() )
	{
		CCP_LOGERR_CH( s_ch, "Cannot register a Profiler Category without a name" );
		return { empty, false };
	}

	for( uint64_t i = 0; i < s_registeredProfilerCategories.size(); ++i )
	{
		auto& entry = s_registeredProfilerCategories[i];

		if (!entry)
		{
			entry = { name, color, 1ULL << i };
			CCP_LOG_CH( s_ch, "Registered a new Profiler Category for '%s'", entry->name.c_str() );
			return { *entry, true };
		}

		if ( entry->name == name )
		{
			CCP_LOGERR_CH( s_ch, "A Profiler Category with the name %s already exists, returning existing entry.", entry->name.c_str() );
			return { *entry, true };
		}
	}

	return { empty, false };
}

/*!
	\brief Returns all telemetry categories registered so far, in registration order.

	The result always includes the built-in "general", "cpp" and "core" categories, followed by any categories
	registered via `CcpTelemetryCategoryRegister()`.

	\return A vector of references to the registered categories. The references remain valid for the lifetime
	        of the process, since categories are never unregistered.

	\par Example
	\code
	for( const CcpTelemetryCategory& category : CcpTelemetryGetRegisteredCategories() )
	{
		printf( "Registered category: %s\n", CcpTelemetryCategoryGetName( category ).c_str() );
	}
	\endcode
*/
CcpTelemetryCategories CcpTelemetryGetRegisteredCategories()
{
	CcpAutoMutex lock( s_profilerCategoryRegistryLock );

	CcpTelemetryCategories result;
	result.reserve( s_registeredProfilerCategories.size() );
	for( const auto& registeredEntry : s_registeredProfilerCategories )
	{
		if( ! registeredEntry )
		{
			break;
		}

		result.emplace_back( *registeredEntry );
	}
	return result;
}

/*!
	\brief Replaces the set of telemetry categories that are actively captured.

	Only zones tagged with an active category are actually recorded by the connected profiler; zones tagged
	with an inactive (or unregistered) category are cheap no-ops. Pass an empty vector to stop capturing any
	category.

	\param categories Names of the categories to activate, as previously passed to `CcpTelemetryCategoryRegister()`.
	                  Unknown names are silently ignored. Must not contain more than `PROFILER_CATEGORIES_MAX`
	                  (64) entries.
	\return `true` on success, `false` if more than 64 names were passed in - in which case the active category
	        set is left unchanged.

	\par Example
	\code
	CcpTelemetrySetActiveCategories( { "cpp", "rendering" } );
	// ... zones tagged "cpp" or "rendering" are now captured ...
	CcpTelemetrySetActiveCategories( {} ); // stop capturing any category
	\endcode

	\see CcpTelemetryGetActiveCategories
*/
bool CcpTelemetrySetActiveCategories( const CcpTelemetryCategories& categories )
{
	CcpAutoMutex lock( s_profilerCategoryRegistryLock );

	if ( categories.size() > s_registeredProfilerCategories.size() )
	{
		CCP_LOGERR_CH( s_ch, "Failed setting active Profiler Category because more %lu maskNames were passed in, but only %lu are allowed", categories.size(), s_registeredProfilerCategories.size() );
		return false;
	}

	uint64_t newActiveProfilerCategory = 0;
	for( const auto& category : categories )
	{
		newActiveProfilerCategory |= category.get().captureBit;
	}

	s_profilerCategoryCaptureMask = newActiveProfilerCategory;

	return true;
}

/*!
	\brief Returns the telemetry categories that are currently active for capture.

	\return A vector of references to the categories most recently activated via `CcpTelemetrySetActiveCategories()`.
	        Empty if no category is currently active.

	\see CcpTelemetrySetActiveCategories
*/
CcpTelemetryCategories CcpTelemetryGetActiveCategories()
{
	CcpTelemetryCategories result;
	size_t index{0};
	for ( const auto& registeredEntry : s_registeredProfilerCategories )
	{
		uint64_t currentMaskBit = 1ULL << index;

		if ( registeredEntry && ( s_profilerCategoryCaptureMask & currentMaskBit ) != 0 )
		{
			result.emplace_back( *registeredEntry );
		}

		++index;
	}
	return result;
}

/*!
	\brief Checks whether the telemetry integration is both started and connected to a profiler client.

	This is the state in which zones and locks are actually captured. Compare with `CcpTelemetryIsStarted()`,
	which only reflects the internal state machine and can be `true` while a connection is still pending.

	\return `true` if telemetry is started and a profiler client is connected, `false` otherwise.
*/
bool CcpTelemetryIsConnected()
{
	return TracyIsStarted && TracyIsConnected && s_profilerState.load( std::memory_order_acquire ) == ProfilerState::Started;
}

/*!
	\brief Checks whether telemetry has been requested to start but is still waiting for a profiler client to connect.

	\return `true` if the telemetry server is listening but no client has connected yet, `false` otherwise.
*/
bool CcpTelemetryIsConnectionRequested()
{
	return TracyIsStarted && !TracyIsConnected && s_profilerState.load( std::memory_order_acquire ) == ProfilerState::StartRequested;
}

/*!
	\brief Checks whether the telemetry integration has fully started (server running and profiler client connected).

	\return `true` if telemetry is in the `Started` state, `false` otherwise.

	\see CcpTelemetryIsConnected
	\see CcpTelemetryIsStopped
*/
bool CcpTelemetryIsStarted()
{
	return s_profilerState.load( std::memory_order_acquire ) == ProfilerState::Started;
}

/*!
	\brief Checks whether the telemetry integration is fully stopped.

	\return `true` if telemetry is in the `Stopped` state, `false` otherwise.

	\see CcpTelemetryIsStarted
	\see CcpStopTelemetry
*/
bool CcpTelemetryIsStopped()
{
	return s_profilerState.load( std::memory_order_acquire ) == ProfilerState::Stopped;
}

/*!
	\brief Checks whether the current telemetry session was configured to track memory allocations.

	\return The value of `CcpTelemetryConfig::trackMemoryAllocations` that was passed to `CcpStartTelemetry()`.

	\see CcpTelemetryTrackAllocation
	\see CcpTelemetryTrackDeallocation
*/
bool CcpTelemetryMemoryTrackingIsEnabled()
{
	return s_config.trackMemoryAllocations;
}

/*!
	\brief Checks whether the current telemetry session was configured to track lock contention.

	\return The value of `CcpTelemetryConfig::trackLocks` that was passed to `CcpStartTelemetry()`.
*/
bool CcpTelemetryLockTrackingIsEnabled()
{
    return s_config.trackLocks;
}

/*!
	\brief Announces a `CcpMutex` to the telemetry integration so that lock contention can be tracked under its
	       given owner and name.

	This is called automatically by `CcpMutex`'s constructor - callers do not need to invoke this directly.

	\param m     Mutex being announced.
	\param owner Name of the subsystem or class that owns the mutex.
	\param name  Human-readable name for the mutex.
*/
void CcpRegisterMutex( class CcpMutex& m, const char* owner, const char* name )
{
	MutexNameMap_t& mutexNames = GetMutexNameMap();
	mutexNames[&m] = std::make_pair( owner, name );
}

/*!
	\brief Assigns a human-readable name to a thread, shown by the connected profiler.

	\param threadId Identifier of the thread being named, as returned by the platform thread APIs.
	\param name     Human-readable name to associate with the thread.

	\par Example
	\code
	CcpRegisterThread( CcpThreadGetCurrentId(), "RenderThread" );
	\endcode
*/
void CcpRegisterThread( CcpThreadId_t threadId, const char* name )
{
	ThreadNameMap_t& threadNames = GetThreadNameMap();
	threadNames[threadId] = name;
}

/*!
	\brief Starts the telemetry integration for the given server or dump path.

	\deprecated Use `CcpStartTelemetry( const CcpTelemetryConfig& config )` instead.

	\param serverOrDumpPath Kept for source compatibility; forwarded as the application name of a default-constructed
	                         `CcpTelemetryConfig`.
	\param connectionType   Unused, kept for source compatibility.
	\param maxThreadCount   Unused, kept for source compatibility.
	\return See `CcpStartTelemetry( const CcpTelemetryConfig& )`.
*/
bool CcpStartTelemetry( const char* serverOrDumpPath, int connectionType, uint32_t maxThreadCount )
{
	return CcpStartTelemetry( { serverOrDumpPath } );
}

/*!
	\struct CcpTelemetryConfig
	\brief Configuration passed to `CcpStartTelemetry( const CcpTelemetryConfig& )`.

	\var CcpTelemetryConfig::applicationName
	Name of the application, shown in the connected profiler.

	\var CcpTelemetryConfig::captureDuration
	If non-zero, telemetry automatically stops itself once this much time has passed since it started (see
	`CcpTelemetryRemainingCaptureDuration()`). A value of zero (the default) means "capture indefinitely, until
	`CcpStopTelemetry()` is called explicitly".

	\var CcpTelemetryConfig::trackMemoryAllocations
	Whether allocations reported via `CcpTelemetryTrackAllocation()` / `CcpTelemetryTrackDeallocation()` should
	actually be forwarded to the profiler (see `CcpTelemetryMemoryTrackingIsEnabled()`).

	\var CcpTelemetryConfig::trackLocks
	Whether `CcpMutex` contention should be reported to the profiler (see `CcpTelemetryLockTrackingIsEnabled()`).

	\par Example
	\code
	CcpTelemetryConfig config;
	config.applicationName = "MyGame";
	config.captureDuration = std::chrono::seconds( 30 ); // stop automatically after 30 seconds
	config.trackMemoryAllocations = true;
	config.trackLocks = true;
	CcpStartTelemetry( config );
	\endcode

	\see CcpStartTelemetry
*/

/*!
	\brief Requests that the telemetry integration start listening for a profiler connection.

	Starting is asynchronous: this function only records the request and applies `config`. The actual
	transition to `CcpTelemetryIsStarted()` happens on a subsequent call to `CcpTelemetryTick()`, once the
	telemetry server is listening and a profiler client has connected.

	\param config Application name, optional timed-capture duration, and memory/lock tracking flags to use for
	              this session. See `CcpTelemetryConfig`.
	\return `true` if the request was accepted, `false` if telemetry is already started or a start is already
	        in progress.

	\par Example
	\code
	CcpTelemetryConfig config;
	config.applicationName = "MyGame";
	config.trackMemoryAllocations = true;
	config.trackLocks = true;
	CcpStartTelemetry( config );

	// Somewhere in the main loop:
	while( isRunning )
	{
		CcpTelemetryTick();
	}
	\endcode

	\see CcpTelemetryConfig
	\see CcpStopTelemetry
	\see CcpTelemetryTick
*/
bool CcpStartTelemetry( const CcpTelemetryConfig& config )
{
	if( s_profilerState.load( std::memory_order_acquire ) == ProfilerState::Started || s_profilerState.load( std::memory_order_acquire ) == ProfilerState::StartRequested )
	{
		CCP_LOGERR_CH( s_ch, "Cannot start profiler - already started" );
		return false;
	}

	s_config = config;
	s_telemetryTick = 1;
	CcpTelemetrySetActiveFiber( "" ); // to ensure that all our look-ups are correctly initialized
//	CCP_LOG_CH( s_ch, "Starting profiler - %s - Root fiber is [Fiber %p]", s_config.applicationName.c_str(), t_activeFiber->c_str() );
	s_profilerState.store( ProfilerState::StartRequested, std::memory_order_release );
	return true;
}

/*!
	\brief Requests that the telemetry integration stop and disconnect from the profiler client.

	Stopping is asynchronous, just like starting: this only records the request. The actual transition to
	`CcpTelemetryIsStopped()` happens on a subsequent call to `CcpTelemetryTick()`.

	\see CcpStartTelemetry
	\see CcpTelemetryIsStopped
*/
void CcpStopTelemetry()
{
	if( s_profilerState.load( std::memory_order_acquire ) == ProfilerState::Stopped || s_profilerState.load( std::memory_order_acquire ) == ProfilerState::StopRequested )
	{
		return;
	}

	CCP_LOG_CH( s_ch, "Profiler stop requested" );
	s_profilerState.store( ProfilerState::StopRequested, std::memory_order_release );
}

/*!
	\brief Advances the telemetry integration's internal state machine by one tick.

	This must be called regularly (typically once per frame, or in a dedicated polling loop) for
	`CcpStartTelemetry()` and `CcpStopTelemetry()` requests to actually take effect, for the connected profiler
	to receive a frame mark, for pending fiber-name cleanups to run, and for a configured
	`CcpTelemetryConfig::captureDuration` to be enforced.

	\par Example
	\code
	CcpStartTelemetry( config );
	while( isRunning )
	{
		CcpTelemetryTick();
		// ... rest of the frame ...
	}
	\endcode

	\see CcpStartTelemetry
	\see CcpStopTelemetry
	\see CcpTelemetryGetTickCount
*/
void CcpTelemetryTick()
{
	switch ( s_profilerState.load(std::memory_order_acquire) )
	{
	case ProfilerState::StartRequested:
	{
		if (TracyIsStarted)
		{
//			CCP_LOG_CH( s_ch, "Telemetry server started, waiting for connection..." );
			if (TracyIsConnected)
			{
				CCP_LOG_CH( s_ch, "Telemetry server connected to Profiler" );
				TracySetProgramName( s_config.applicationName.c_str() );
				s_profilerState.store( ProfilerState::Started, std::memory_order_release );
				s_profilerStartTime = std::chrono::steady_clock::now();

				auto handlers = GetEventHandlers(); // take a copy of the event handlers in case a callback removes an entry
				for(auto & handler : handlers)
				{
					( *handler.first )( CCP_TELEMETRY_STARTED, handler.second );
				}
			}
		}
		else
		{
			CCP_LOG_CH( s_ch, "Starting Telemetry Server" );
#ifdef TRACY_MANUAL_LIFETIME
			tracy::StartupProfiler();
#endif // TRACY_MANUAL_LIFETIME
		}
		break;
	}
	case ProfilerState::Started:
	{
		if (TracyIsConnected)
		{
			FrameMark;
			++s_telemetryTick;

			auto now = std::chrono::steady_clock::now();

			// Check if there are any pending fiber name erases
			for (auto it = s_fiberEraseMap.begin(); it != s_fiberEraseMap.end(); )
			{
				if ( now >= it->second )
				{
					s_fiberNameStore.erase( it->first );
					it = s_fiberEraseMap.erase( it );
				} else
				{
					++it;
				}
			}

			if( s_config.captureDuration != std::chrono::milliseconds::zero() ) // Check if we have passed our timed sample time
			{
				auto timeSinceStart = now - s_profilerStartTime;
				if( timeSinceStart >= s_config.captureDuration )
				{
					CCP_LOG_CH( s_ch, "Finalizing timed profiler run" );
					CcpStopTelemetry();
				}
			}
		}
		else
		{
			CCP_LOG_CH( s_ch, "Disconnected from profiler" );
			CcpStopTelemetry();
		}
		break;
	}
	case ProfilerState::StopRequested:
	{
		CCP_LOG_CH( s_ch, "Stopping Telemetry Server" );
		FrameMark;
		++s_telemetryTick;
		s_profilerState.store( ProfilerState::Stopped, std::memory_order_release );
		auto handlers = GetEventHandlers(); // use a copy of the event handlers in case a callback removes an entry
		for(auto & handler : handlers)
		{
			( *handler.first )( CCP_TELEMETRY_STOPPED, handler.second );
		}
		break;
	}
	case ProfilerState::Stopped:
		// Nothing to do
		break;
	default:
		CCP_LOGERR_CH( s_ch, "Unhandled profiler state %d", s_profilerState.load(std::memory_order_acquire));
		break;
	}
}

/*!
	\brief Returns how much longer the current timed capture will run.

	Only meaningful when `CcpTelemetryConfig::captureDuration` was set to a non-zero value in the config passed
	to `CcpStartTelemetry()`; otherwise always returns zero.

	\return The remaining capture duration, clamped to zero once it has elapsed.

	\see CcpTelemetryConfig
*/
std::chrono::milliseconds CcpTelemetryRemainingCaptureDuration()
{
	return std::max( std::chrono::milliseconds( 0 ), s_config.captureDuration - std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::steady_clock::now() - s_profilerStartTime ) );
}

/*!
	\brief Reports a memory allocation to the connected profiler.

	Intended to be called from custom allocators (see `CCPMemory.cpp`) rather than directly by application
	code. A no-op unless both `CcpTelemetryMemoryTrackingIsEnabled()` and `CcpTelemetryIsConnected()` are `true`.

	\param p    Address of the allocated memory block.
	\param size Size, in bytes, of the allocated memory block.

	\see CcpTelemetryTrackDeallocation
	\see CcpTelemetryMemoryTrackingIsEnabled
*/
void CcpTelemetryTrackAllocation( void* p, size_t size )
{
	if ( CcpTelemetryMemoryTrackingIsEnabled() && CcpTelemetryIsConnected() ) {
		TracySecureAlloc( p, size );
	}
}

/*!
	\brief Reports a memory deallocation to the connected profiler.

	Intended to be called from custom allocators (see `CCPMemory.cpp`) rather than directly by application
	code. A no-op if `p` is `nullptr`, or unless both `CcpTelemetryMemoryTrackingIsEnabled()` and
	`CcpTelemetryIsConnected()` are `true`.

	\param p Address of the memory block being freed.

	\see CcpTelemetryTrackAllocation
*/
void CcpTelemetryTrackDeallocation( void* p )
{
	if ( p && CcpTelemetryMemoryTrackingIsEnabled() && CcpTelemetryIsConnected() )
	{
		TracySecureFree( p );
	}
}

/*!
	\brief Returns a monotonically increasing counter of `CcpTelemetryTick()` calls since the last
	       `CcpStartTelemetry()`.

	\return The current tick count. Reset to `1` every time `CcpStartTelemetry()` succeeds.

	\see CcpTelemetryTick
*/
uint32_t CcpTelemetryGetTickCount()
{
	return s_telemetryTick;
}

/*!
	\enum CcpTelemetryEvent
	\brief Events reported to handlers registered via `CcpRegisterTelemetryEventHandler()`.

	\var CCP_TELEMETRY_STARTED
	Telemetry has started and connected to a profiler client.

	\var CCP_TELEMETRY_STOPPED
	Telemetry has stopped and disconnected from the profiler client.

	\see CcpRegisterTelemetryEventHandler
*/

/*!
	\typedef CcpOnTelemetryEventHandler
	\brief Callback signature for handlers registered via `CcpRegisterTelemetryEventHandler()`.

	\see CcpRegisterTelemetryEventHandler
	\see CcpTelemetryEvent
*/

/*!
	\brief Registers a callback to be invoked whenever telemetry starts or stops.

	If telemetry is already connected at the time of registration, `handler` is invoked immediately with
	`CCP_TELEMETRY_STARTED`.

	\param handler  Callback to invoke on telemetry start/stop events.
	\param userData Opaque pointer passed back to `handler` unchanged; use it to disambiguate multiple
	                 registrations of the same function pointer.

	\par Example
	\code
	void OnTelemetryEvent( CcpTelemetryEvent event, void* userData )
	{
		if( event == CCP_TELEMETRY_STARTED )
		{
			printf( "Telemetry connected\n" );
		}
	}

	CcpRegisterTelemetryEventHandler( &OnTelemetryEvent, nullptr );
	\endcode

	\see CcpUnregisterTelemetryEventHandler
	\see CcpTelemetryEvent
*/
void CcpRegisterTelemetryEventHandler( CcpOnTelemetryEventHandler handler, void* userData )
{
	GetEventHandlers().push_back( std::make_pair( handler, userData ) );
	if( CcpTelemetryIsConnected() )
	{
		handler( CCP_TELEMETRY_STARTED, userData );
	}
}

/*!
	\brief Removes a callback previously registered with `CcpRegisterTelemetryEventHandler()`.

	Both `handler` and `userData` must match the values passed at registration time for the entry to be found.

	\param handler  Callback that was registered.
	\param userData Opaque pointer that was registered alongside `handler`.
*/
void CcpUnregisterTelemetryEventHandler( CcpOnTelemetryEventHandler handler, void* userData )
{
	auto& handlers = GetEventHandlers();
	auto it = std::find( handlers.begin(), handlers.end(), std::make_pair( handler, userData ) );
	if( it != handlers.end() )
	{
		handlers.erase( it );
	}
}

// Internal helper: switches the active fiber by string-store iterator. Used by the public
// `CcpTelemetrySetActiveFiber( const std::string& )` overload below, once the name has been interned.
void CcpTelemetrySetActiveFiber( FiberNameStore::const_iterator elem )
{
	if ( elem == t_activeFiber )
	{
		return;
	}

	if ( TracyIsStarted )
	{
		if( elem->empty() )
		{
			TracyFiberLeave;
		}
		else
		{
			TracyFiberEnter( elem->c_str() );
		}
	}

	t_activeFiber = elem;

	// Ensure a zone stack exists for the currently active fiber
	auto existing = t_taskletZoneStore.lower_bound( t_activeFiber );
	if ( existing != t_taskletZoneStore.end() && ! ( t_taskletZoneStore.key_comp()( t_activeFiber, existing->first ) ) )
	{
		t_activeTaskletZoneStore = existing;
	}
	else
	{
		t_activeTaskletZoneStore = t_taskletZoneStore.emplace_hint( existing, t_activeFiber, std::stack<TelemetryZone>() );
	}
//	CCP_LOG_CH( s_ch, "[Fiber %p] [Store %p] Setting active tasklet zone store", t_activeFiber, t_activeTaskletZoneStore );
}

/*!
	\brief Marks `name` as the calling thread's active fiber for zone bookkeeping purposes.

	Zones entered while a fiber is active (e.g. via `TelemetryZone` or the deprecated `CcpTelemetryEnterZone()`)
	are tracked per-fiber rather than per-thread, so that fibers which get resumed on different OS threads still
	show a coherent call stack in the profiler. Pass an empty string to indicate "no fiber" (the root context).

	\param name Name of the fiber to activate. An empty string deactivates fiber tracking for the calling thread.

	\par Example
	\code
	CcpTelemetrySetActiveFiber( "WorkerFiber1" );
	// ... work performed on behalf of the fiber ...
	CcpTelemetrySetActiveFiber( "" ); // back to the root context
	\endcode

	\see CcpTelemetryGetActiveFiber
	\see CcpTelemetryRemoveFiber
*/
void CcpTelemetrySetActiveFiber( const std::string& name )
{
	auto elem = s_fiberNameStore.insert( name );
	s_fiberEraseMap.erase( elem.first ); // cancel any pending deletion of this name
//	if ( elem.second )
//	{
//		CCP_LOG_CH( s_ch, "Registered new [Fiber %p]", elem.first->c_str() );
//	}
	CcpTelemetrySetActiveFiber( elem.first );
}

/*!
	\brief Schedules a previously named fiber for removal from the fiber name store.

	If `name` is the calling thread's currently active fiber, the active fiber is first reset to "no fiber"
	(equivalent to calling `CcpTelemetrySetActiveFiber( "" )`). The name itself is only erased from the
	internal store after a short grace period, to avoid invalidating references still in flight.

	\param name Name of the fiber to remove. Empty names are ignored, since the "no fiber" root context is
	            never removed.

	\see CcpTelemetrySetActiveFiber
*/
void CcpTelemetryRemoveFiber( const std::string& name )
{
	// Cannot remove nameless fibers
	if ( name.empty() )
	{
		return;
	}

	auto fiber = s_fiberNameStore.find( name );
	if( fiber != s_fiberNameStore.end() )
	{
//		CCP_LOG_CH( s_ch, "Marking [Fiber %p] for removal", fiber->c_str() );
		t_taskletZoneStore.erase( fiber );
		s_fiberEraseMap.emplace( fiber, std::chrono::steady_clock::now() + s_cleanupDelay );
		if ( t_activeFiber == fiber )
		{
			CcpTelemetrySetActiveFiber( "" );
		}
	}
}

/*!
	\brief Returns the name of the calling thread's currently active fiber.

	\return The name most recently passed to `CcpTelemetrySetActiveFiber()`, or an empty string if no fiber is
	        active.

	\see CcpTelemetrySetActiveFiber
*/
const std::string& CcpTelemetryGetActiveFiber()
{
	return *t_activeFiber;
}

/*!
	\class TelemetryZone
	\brief RAII scope marker that reports a named, timed span of work to the connected profiler.

	Construct a `TelemetryZone` at the start of the scope you want to measure; it automatically ends the zone
	when it goes out of scope. Zones are cheap no-ops while telemetry is not started, and are only actually
	recorded while their `CcpTelemetryCategory` is active (see `CcpTelemetrySetActiveCategories()`).

	`TelemetryZone` is move-only: it cannot be copied, and can only be moved into e.g. an `std::optional` or a
	container, since a zone must have a single, well-defined owner responsible for ending it.

	\par Example
	\code
	auto [category, ok] = CcpTelemetryCategoryRegister( "physics" );

	void StepSimulation()
	{
		TelemetryZone zone( category, "StepSimulation", __FILE__, __LINE__ );
		zone.text( "10 bodies" );
		// ... do work; the zone ends automatically when `zone` goes out of scope ...
	}
	\endcode

	\see CcpTelemetryCategoryRegister
*/

/*!
	\brief Constructs a zone using a legacy, bitmask-style category handle.

	\deprecated Use the `TelemetryZone( const CcpTelemetryCategory&, const char*, const char*, uint32_t )`
	            constructor instead.

	\param handle   Legacy category handle, e.g. `TMCM_CPP` or a bit index into the registered category array.
	\param name     Name of the zone, as shown in the profiler.
	\param filename Source file the zone originates from; pass `__FILE__`.
	\param lineno   Source line the zone originates from; pass `__LINE__`.
	\param color    Color to render the zone with. Defaults to `CcpColor::SteelBlue`.
*/
TelemetryZone::TelemetryZone( uint32_t handle, const char* name, const char* filename, uint32_t lineno, CcpColor color ) : m_impl(std::make_unique<Private>())
{
	if( s_profilerState.load( std::memory_order_acquire ) != ProfilerState::Started )
	{
		return;
	}

	CCP_ASSERT( filename != nullptr );
	CCP_ASSERT( name != nullptr );

	const int active = IsProfilerCategoryActive( handle );
	auto data = ___tracy_alloc_srcloc( lineno, filename, strlen( filename ), name, strlen( name ), static_cast<uint32_t>( color ) );
//	CCP_LOG_CH( s_ch, "[Fiber %p] Creating zone %s (%p)", t_activeFiber->c_str(), ret.first->c_str(), this );
	m_impl->fiber = t_activeFiber;
	m_impl->telemetryContext.emplace( ___tracy_emit_zone_begin_alloc( data, active ) );
}

/*!
	\brief Constructs a zone tagged with a registered `CcpTelemetryCategory`.

	The zone is only actually recorded by the connected profiler if telemetry is started and `category` is
	currently active (see `CcpTelemetrySetActiveCategories()`); otherwise this constructor is a cheap no-op.

	\param category Category to tag the zone with, as obtained from `CcpTelemetryCategoryRegister()`.
	\param name     Name of the zone, as shown in the profiler.
	\param filename Source file the zone originates from; pass `__FILE__`.
	\param lineno   Source line the zone originates from; pass `__LINE__`.

	\par Example
	\code
	auto [category, ok] = CcpTelemetryCategoryRegister( "rendering" );
	TelemetryZone zone( category, "DrawFrame", __FILE__, __LINE__ );
	\endcode
*/
TelemetryZone::TelemetryZone( const CcpTelemetryCategory& category, const char* name, const char* filename, uint32_t lineno ) : m_impl( std::make_unique<Private>() )
{
	if( s_profilerState.load( std::memory_order_acquire ) != ProfilerState::Started )
	{
		return;
	}

	const int active = IsProfilerCategoryActive( category.captureBit );
	auto data = ___tracy_alloc_srcloc( lineno, filename, strlen( filename ), name, strlen( name ), static_cast<uint32_t>( category.color ) );
	//	CCP_LOG_CH( s_ch, "[Fiber %p] Creating zone %s (%p)", t_activeFiber->c_str(), ret.first->c_str(), this );
	m_impl->fiber = t_activeFiber;
	m_impl->telemetryContext.emplace( ___tracy_emit_zone_begin_alloc( data, active ) );
}

/*!
	\brief Transfers ownership of an in-flight zone from `other` to the newly constructed instance.

	After the move, `other` no longer ends any zone on destruction.

	\param other Zone to move from.
*/
TelemetryZone::TelemetryZone( TelemetryZone&& other ) noexcept : m_impl( std::make_unique<Private>() )
{
	m_impl->fiber = other.m_impl->fiber;
	m_impl->telemetryContext = other.m_impl->telemetryContext;
	// mark this instance's zone as inactive in case the destructor runs
	other.m_impl->telemetryContext.reset();
//	CCP_LOG_CH( s_ch, "[Fiber %p] Moving zone %p (fiber=%s) to new zone %p (fiber=%s)", t_activeFiber->c_str(), &other, other.m_impl->fiber->c_str(), this, m_impl->fiber->c_str() );
}

/*!
	\brief Ends the zone, if one is still active on this instance.

	Ending happens on whichever fiber the zone was originally started on, temporarily switching the active
	fiber back if the calling thread has since moved on to a different one.
*/
TelemetryZone::~TelemetryZone()
{
	// Notify Tracy of all zones ended with a valid context, regardless of profiler state
	if( !m_impl->telemetryContext )
	{
		return;
	}

	// Zones need to end on the same fiber they were started from, so do a little song and dance to ensure that
	auto previous = t_activeFiber;
	CcpTelemetrySetActiveFiber( m_impl->fiber );
//	CCP_LOG_CH( s_ch, "[Fiber %p] Leaving zone %p (fiber=%s)", t_activeFiber->c_str(), this, m_impl->fiber->c_str() );
	TracyCZoneEnd( m_impl->telemetryContext.value() );
	CcpTelemetrySetActiveFiber( previous );
}

/*!
	\brief Attaches a free-form text annotation to the zone, visible alongside it in the profiler.

	A no-op unless the zone is currently active (telemetry started and its category active).

	\param text Text to attach to the zone. Must not be `nullptr`.

	\par Example
	\code
	TelemetryZone zone( category, "LoadAsset", __FILE__, __LINE__ );
	zone.text( assetPath.c_str() );
	\endcode
*/
void TelemetryZone::text( const char* text ) const
{
	if( s_profilerState.load( std::memory_order_acquire ) == ProfilerState::Started && m_impl->telemetryContext )
	{
		CCP_ASSERT( text != nullptr );
		TracyCZoneText( m_impl->telemetryContext.value(), text, strlen( text ) );
	}
}

/*!
	\brief Manually enters a zone identified by an opaque key.

	\deprecated Use a `TelemetryZone` instead, which ends its zone automatically via RAII instead of requiring a
	            matching `CcpTelemetryLeaveZone()` call.

	\param key      Opaque identifier used to match this call with a later `CcpTelemetryLeaveZone( key )`. Zones
	                 sharing the same `key` nest, forming a stack.
	\param name     Name of the zone, as shown in the profiler.
	\param filename Source file the zone originates from; pass `__FILE__`.
	\param lineno   Source line the zone originates from; pass `__LINE__`.

	\see CcpTelemetryLeaveZone
	\see TelemetryZone
*/
void CcpTelemetryEnterZone( void* key, const char* name, const char* filename, uint32_t lineno )
{
	if( s_profilerState.load( std::memory_order_acquire ) == ProfilerState::Started )
	{
		t_manuallyTrackedZones.emplace( key );
		t_activeTaskletZoneStore->second.emplace( TMCM_CPP, name, filename, lineno );
	}
}

/*!
	\brief Manually leaves the zone most recently entered under `key`.

	\deprecated Use a `TelemetryZone` instead.

	\param key Opaque identifier previously passed to `CcpTelemetryEnterZone()`.

	\see CcpTelemetryEnterZone
*/
void CcpTelemetryLeaveZone( void* key )
{
	if ( t_manuallyTrackedZones.find( key ) != t_manuallyTrackedZones.end() )
	{
//		CCP_LOG_CH( s_ch, "[Fiber %p] [Store %p] [Zone %p] Leave", t_activeFiber, t_activeTaskletZoneStore, &t_activeTaskletZoneStore->second.top() );
		if ( !t_activeTaskletZoneStore->second.empty() )
		{
			t_activeTaskletZoneStore->second.pop();
		}
		if ( t_activeTaskletZoneStore->second.empty() ) {
			t_manuallyTrackedZones.erase( key );
		}
	}
}

/*!
	\brief Attaches a free-form text annotation to the zone most recently entered under `key`.

	\deprecated Use `TelemetryZone::text()` instead.

	\param key  Opaque identifier previously passed to `CcpTelemetryEnterZone()`.
	\param text Text to attach to the zone. If `nullptr`, the call is ignored.

	\see CcpTelemetryEnterZone
	\see TelemetryZone::text
*/
void CcpTelemetryZoneAddText( void* key, const char* text )
{
	if ( text != nullptr )
	{
		if ( !t_activeTaskletZoneStore->second.empty() && t_manuallyTrackedZones.find( key ) != t_manuallyTrackedZones.end() )
		{
			t_activeTaskletZoneStore->second.top().text( text );
		}
	}
}

#else

// With telemetry compiled out, every entry point below is a no-op, and every query reports that
// nothing is registered, nothing is active and nothing is being captured. The declarations in
// `CcpTelemetry.h` remain visible in this configuration, so call sites need no guarding.

struct CcpTelemetryCategory
{
	std::string name;
	CcpColor color{CcpColor::White};
};

struct TelemetryZone::Private
{
};

namespace
{
	const CcpTelemetryCategory s_emptyCategory;
}

bool operator==( const CcpTelemetryCategory& lhs, const CcpTelemetryCategory& rhs )
{
	return lhs.name == rhs.name;
}

const std::string& CcpTelemetryCategoryGetName( const CcpTelemetryCategory& category )
{
	return category.name;
}

CcpColor CcpTelemetryCategoryGetColor( const CcpTelemetryCategory& category )
{
	return category.color;
}

bool CcpTelemetryIsConnectionRequested()
{
	return false;
}

bool CcpTelemetryIsConnected()
{
	return false;
}

bool CcpTelemetryIsStarted()
{
	return false;
}

bool CcpTelemetryIsStopped()
{
	return true;
}

std::chrono::milliseconds CcpTelemetryRemainingCaptureDuration()
{
	return std::chrono::milliseconds::zero();
}

bool CcpTelemetryMemoryTrackingIsEnabled()
{
	return false;
}

bool CcpTelemetryLockTrackingIsEnabled()
{
    return false;
}

void CcpRegisterThread( CcpThreadId_t threadId, const char* name )
{
}

std::pair<const CcpTelemetryCategory&, bool> CcpTelemetryCategoryRegister( const std::string&, CcpColor )
{
	return { s_emptyCategory, false };
}

CcpTelemetryCategories CcpTelemetryGetRegisteredCategories()
{
	return {};
}

bool CcpTelemetrySetActiveCategories( const CcpTelemetryCategories& )
{
	return false;
}

CcpTelemetryCategories CcpTelemetryGetActiveCategories()
{
	return {};
}

bool CcpStartTelemetry( const char* server, int connectionType, uint32_t maxThreadCount )
{
	return false;
}

bool CcpStartTelemetry( const CcpTelemetryConfig& config )
{
	return false;
}

void CcpStopTelemetry()
{
}

void CcpTelemetryTick()
{
}

uint32_t CcpTelemetryGetTickCount()
{
	return 0;
}

void CcpRegisterTelemetryEventHandler( CcpOnTelemetryEventHandler handler, void* userData )
{
}

void CcpUnregisterTelemetryEventHandler( CcpOnTelemetryEventHandler handler, void* userData )
{
}

void CcpTelemetrySetActiveFiber( const std::string& )
{
}

const std::string& CcpTelemetryGetActiveFiber()
{
	static const std::string s_noFiber;
	return s_noFiber;
}

void CcpTelemetryRemoveFiber( const std::string& )
{
}

TelemetryZone::TelemetryZone( uint32_t, const char*, const char*, uint32_t, CcpColor )
{
}

TelemetryZone::TelemetryZone( const CcpTelemetryCategory&, const char*, const char*, uint32_t )
{
}

TelemetryZone::TelemetryZone( TelemetryZone&& ) noexcept = default;

TelemetryZone::~TelemetryZone() = default;

void TelemetryZone::text( const char* ) const
{
}

void CcpTelemetryEnterZone( void* key, const char* name, const char* filename, uint32_t lineno )
{
}

void CcpTelemetryLeaveZone( void* key )
{
}

void CcpTelemetryZoneAddText( void* key, const char* text )
{
}

void CcpTelemetryTrackAllocation( void*, size_t )
{
}

void CcpTelemetryTrackDeallocation( void* )
{
}

#endif // CCP_TELEMETRY_ENABLED
