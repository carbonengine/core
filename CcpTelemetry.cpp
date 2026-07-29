// Copyright © 2013 CCP ehf.

#include <array>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <optional>
#include <queue>

#include "include/CCPAssert.h"
#include "include/CcpMutex.h"
#include "include/CcpTelemetry.h"
#include "include/CcpTime.h"

static CcpLogChannel_t s_ch = CCP_LOG_DEFINE_CHANNEL( "Telemetry" );

#if CCP_TELEMETRY_ENABLED

#pragma warning(push)
#pragma warning(disable : 4996)
#include <tracy/Tracy.hpp>
#pragma warning(pop)
#include <tracy/TracyC.h>

typedef std::set<std::string> FiberNameStore;

struct TelemetryZone::Private
{
	std::optional<TracyCZoneCtx> telemetryContext;
	FiberNameStore::const_iterator fiber;
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
	constexpr size_t PROFILER_CATEGORIES_MAX{64};

	CcpMutex s_profilerCategoryRegistryLock( "CcpTelemetry", "ProfilerCategoryRegistry" );

	// Fixed size array of registered ProfilerCategories.
	std::array<std::optional<CcpProfilerCategory>, PROFILER_CATEGORIES_MAX> s_registeredProfilerCategories{
			CcpProfilerCategory{ "core", CcpColor::LightGreen }, // Pre-allocated Profiler Category for core, should core ever need it. This also solves the problem that legacy `TMCM_GENERAL` and `TMCM_CPP` otherwise cause an off-by-one error.
			CcpProfilerCategory{ "general", CcpColor::SteelBlue }, // legacy definition from TMCM_GENERAL, used to be a bitmask, but can now be treated as index into this array
			CcpProfilerCategory{ "cpp", CcpColor::Yellow }, // legacy value from TMCM_CPP, used to be a bitmask, but can now be treated as index into this array
	};

	uint64_t s_activeProfilerCategoryBits{0};


	// List of ProfilerCategory names that have yet to be registered.
	// From a CcpSetActiveProfilerCategories( vector<string> ) call.
	std::vector<std::string> s_pendingProfilerCategoryNames;

	// Get the registered ProfilerCategory color for a given ProfilerCategory
	CcpColor GetProfilerCategoryColor( CcpProfilerCategoryHandle handle )
	{
		return s_registeredProfilerCategories[handle]->color;
	}

	bool IsProfilerCategoryActive( CcpProfilerCategoryHandle handle )
	{
		return ( s_activeProfilerCategoryBits & ( 1ULL<<handle ) ) != 0;
	}
}

bool operator==( const CcpProfilerCategory& lhs, const CcpProfilerCategory& rhs )
{
	// Profiler Categories need to be unique by name
	return lhs.name == rhs.name;
}

CcpProfilerCategoryHandle CcpRegisterProfilerCategory( const CcpProfilerCategory& category )
{
		CcpAutoMutex lock( s_profilerCategoryRegistryLock );

		if( category.name.empty() )
		{
			CCP_LOGERR_CH( s_ch, "Cannot register a Profiler Category without a name" );
			return CCP_PROFILER_CATEGORY_HANDLE_INVALID;
		}

		CcpProfilerCategoryHandle handle{0};
		for( auto& entry : s_registeredProfilerCategories )
		{
			if (!entry)
			{
				entry = category;
				CCP_LOG_CH( s_ch, "Registered a new Profiler Category for '%s' -> %u with color %s", entry->name.c_str(), handle, CcpColorToString( entry->color ).data() );
				break;
			}

			if ( entry->name == category.name )
			{
				CCP_LOGERR_CH( s_ch, "A Profiler Category with the name %s already exists.", entry->name.c_str() );
				return CCP_PROFILER_CATEGORY_HANDLE_INVALID;
			}

			++handle;
		}

		if ( handle > PROFILER_CATEGORIES_MAX )
		{
			return CCP_PROFILER_CATEGORY_HANDLE_INVALID;
		}

		// Make sure previously "pending active" ProfilerCategory is added to the active ProfilerCategories
		auto pendingIt = std::find( s_pendingProfilerCategoryNames.begin(), s_pendingProfilerCategoryNames.end(), category.name );
		if( pendingIt != s_pendingProfilerCategoryNames.end() )
		{
			s_pendingProfilerCategoryNames.erase( pendingIt );
			s_activeProfilerCategoryBits |= ( 1ULL << handle );
			CCP_LOG_CH( s_ch, "Previously pending ProfilerCategory '%s' added to active ProfilerCategories", category.name.c_str() );
		}

		return handle;
}

std::vector<CcpProfilerCategory> CcpGetRegisteredProfilerCategories()
{
	CcpAutoMutex lock( s_profilerCategoryRegistryLock );

	std::vector<CcpProfilerCategory> result;
	result.reserve( PROFILER_CATEGORIES_MAX );
	for( const auto& registeredEntry : s_registeredProfilerCategories )
	{
		if( ! registeredEntry )
		{
			break;
		}

		result.push_back( *registeredEntry );
	}
	return result;
}

bool CcpSetActiveProfilerCategories( const std::vector<std::string>& maskNames )
{
	// Guard access to all ProfilerCategories members
	CcpAutoMutex lock( s_profilerCategoryRegistryLock );

	if ( maskNames.size() > PROFILER_CATEGORIES_MAX )
	{
		CCP_LOGERR_CH( s_ch, "Failed setting active Profiler Category because more %lu maskNames were passed in, but only %lu are allowed", maskNames.size(), PROFILER_CATEGORIES_MAX );
		return false;
	}

	s_pendingProfilerCategoryNames.clear();
	uint64_t newActiveProfilerCategory = 0;
	for( const auto& rawName : maskNames )
	{
		if( rawName.empty() )
		{
			continue;
		}

		bool alreadyRegistered = false;
		for ( size_t index = 0; index < s_registeredProfilerCategories.size(); ++index )
		{
			const auto& registeredEntry = s_registeredProfilerCategories[index];
			if ( !registeredEntry )
			{
				break;
			}
			if( registeredEntry->name == rawName )
			{
				newActiveProfilerCategory |= (1ULL << index);
				alreadyRegistered = true;
				break;
			}
		}

		// This ProfilerCategory hasn't been registered (yet) so add it to the pending list
		if( !alreadyRegistered )
		{
			if( std::find( s_pendingProfilerCategoryNames.begin(), s_pendingProfilerCategoryNames.end(), rawName ) == s_pendingProfilerCategoryNames.end() )
			{
				s_pendingProfilerCategoryNames.emplace_back( rawName );
			}
		}
	}

	s_activeProfilerCategoryBits = newActiveProfilerCategory;
	CCP_LOG_CH( s_ch, "Active ProfilerCategory set to %llu (%zu pending unresolved name(s))", static_cast<unsigned long long>( newActiveProfilerCategory ), s_pendingProfilerCategoryNames.size() );

	return true;
}

std::vector<CcpProfilerCategory> CcpGetActiveProfilerCategories()
{
	std::vector<CcpProfilerCategory> result;
	size_t index{0};
	for ( const auto& registeredEntry : s_registeredProfilerCategories )
	{
		uint64_t currentMaskBit = 1ULL << index;

		if ( registeredEntry && ( s_activeProfilerCategoryBits & currentMaskBit ) != 0 )
		{
			result.push_back( *registeredEntry );
		}

		++index;
	}
	return result;
}

bool CcpTelemetryIsConnected()
{
	return TracyIsStarted && TracyIsConnected && s_profilerState.load( std::memory_order_acquire ) == ProfilerState::Started;
}

bool CcpTelemetryIsConnectionRequested()
{
	return TracyIsStarted && !TracyIsConnected && s_profilerState.load( std::memory_order_acquire ) == ProfilerState::StartRequested;
}

bool CcpTelemetryIsStarted()
{
	return s_profilerState.load( std::memory_order_acquire ) == ProfilerState::Started;
}

bool CcpTelemetryIsStopped()
{
	return s_profilerState.load( std::memory_order_acquire ) == ProfilerState::Stopped;
}

bool CcpTelemetryMemoryTrackingIsEnabled()
{
	return s_config.trackMemoryAllocations;
}

bool CcpTelemetryLockTrackingIsEnabled()
{
    return s_config.trackLocks;
}

void CcpRegisterMutex( class CcpMutex& m, const char* owner, const char* name )
{
	MutexNameMap_t& mutexNames = GetMutexNameMap();
	mutexNames[&m] = std::make_pair( owner, name );
}

void CcpRegisterThread( CcpThreadId_t threadId, const char* name )
{
	ThreadNameMap_t& threadNames = GetThreadNameMap();
	threadNames[threadId] = name;
}

bool CcpStartTelemetry( const char* serverOrDumpPath, int connectionType, uint32_t maxThreadCount )
{
	return CcpStartTelemetry( { serverOrDumpPath } );
}

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

void CcpStopTelemetry()
{
	if( s_profilerState.load( std::memory_order_acquire ) == ProfilerState::Stopped || s_profilerState.load( std::memory_order_acquire ) == ProfilerState::StopRequested )
	{
		return;
	}

	CCP_LOG_CH( s_ch, "Profiler stop requested" );
	s_profilerState.store( ProfilerState::StopRequested, std::memory_order_release );
}

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

std::chrono::milliseconds CcpTelemetryRemainingCaptureDuration()
{
	return std::max( std::chrono::milliseconds( 0 ), s_config.captureDuration - std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::steady_clock::now() - s_profilerStartTime ) );
}

void CcpTelemetryTrackAllocation( void* p, size_t size )
{
	if ( CcpTelemetryMemoryTrackingIsEnabled() && CcpTelemetryIsConnected() ) {
		TracySecureAlloc( p, size );
	}
}

void CcpTelemetryTrackDeallocation( void* p )
{
	if ( p && CcpTelemetryMemoryTrackingIsEnabled() && CcpTelemetryIsConnected() )
	{
		TracySecureFree( p );
	}
}

uint32_t CcpTelemetryGetTickCount()
{
	return s_telemetryTick;
}

void CcpRegisterTelemetryEventHandler( CcpOnTelemetryEventHandler handler, void* userData )
{
	GetEventHandlers().push_back( std::make_pair( handler, userData ) );
	if( CcpTelemetryIsConnected() )
	{
		handler( CCP_TELEMETRY_STARTED, userData );
	}
}

void CcpUnregisterTelemetryEventHandler( CcpOnTelemetryEventHandler handler, void* userData )
{
	auto& handlers = GetEventHandlers();
	auto it = std::find( handlers.begin(), handlers.end(), std::make_pair( handler, userData ) );
	if( it != handlers.end() )
	{
		handlers.erase( it );
	}
}

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

const std::string& CcpTelemetryGetActiveFiber()
{
	return *t_activeFiber;
}

TelemetryZone::TelemetryZone( CcpProfilerCategoryHandle handle, const char* name, const char* filename, uint32_t lineno, CcpColor obsolete ) : m_impl(std::make_unique<Private>())
{
	if( s_profilerState.load( std::memory_order_acquire ) != ProfilerState::Started )
	{
		return;
	}

	if ( handle >= PROFILER_CATEGORIES_MAX )
	{
		CCP_LOGERR_CH( s_ch, "Invalid Profiler Category handle %d - skipping creation of zone named %s", handle, name );
		return;
	}

	CCP_ASSERT( filename != nullptr );
	CCP_ASSERT( name != nullptr );

	auto color = GetProfilerCategoryColor( handle );
	const int active = IsProfilerCategoryActive( handle );
	auto data = ___tracy_alloc_srcloc( lineno, filename, strlen( filename ), name, strlen( name ), static_cast<uint32_t>( color ) );
//	CCP_LOG_CH( s_ch, "[Fiber %p] Creating zone %s (%p)", t_activeFiber->c_str(), ret.first->c_str(), this );
	m_impl->fiber = t_activeFiber;
	m_impl->telemetryContext.emplace( ___tracy_emit_zone_begin_alloc( data, active ) );
}

TelemetryZone::TelemetryZone( TelemetryZone&& other ) noexcept : m_impl( std::make_unique<Private>() )
{
	m_impl->fiber = other.m_impl->fiber;
	m_impl->telemetryContext = other.m_impl->telemetryContext;
	// mark this instance's zone as inactive in case the destructor runs
	other.m_impl->telemetryContext.reset();
//	CCP_LOG_CH( s_ch, "[Fiber %p] Moving zone %p (fiber=%s) to new zone %p (fiber=%s)", t_activeFiber->c_str(), &other, other.m_impl->fiber->c_str(), this, m_impl->fiber->c_str() );
}

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

void TelemetryZone::text( const char* text ) const
{
	if( s_profilerState.load( std::memory_order_acquire ) == ProfilerState::Started && m_impl->telemetryContext )
	{
		CCP_ASSERT( text != nullptr );
		TracyCZoneText( m_impl->telemetryContext.value(), text, strlen( text ) );
	}
}

void CcpTelemetryEnterZone( void* key, const char* name, const char* filename, uint32_t lineno )
{
	if( s_profilerState.load( std::memory_order_acquire ) == ProfilerState::Started )
	{
		t_manuallyTrackedZones.emplace( key );
		t_activeTaskletZoneStore->second.emplace( TMCM_CPP, name, filename, lineno );
	}
}

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

CcpProfilerCategoryHandle CcpRegisterProfilerCategory( const std::string&, CcpColor )
{
	return 0;
}

std::vector<CcpProfilerCategory> CcpGetRegisteredProfilerCategories()
{
	return {};
}

bool CcpSetActiveProfilerCategory( const std::vector<std::string>& )
{
}

std::vector<std::string> CcpGetActiveProfilerCategory()
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
	return "";
}

void CcpTelemetryRemoveFiber( const std::string& )
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

#endif // CCP_TELEMETRY_ENABLED
