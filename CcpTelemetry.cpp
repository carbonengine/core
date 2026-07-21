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
std::queue<std::pair<FiberNameStore::const_iterator, std::chrono::steady_clock::time_point>> s_fiberEraseMap; // Map of fibers scheduled for erasure

typedef TrackableStdMap<CcpMutex*, std::pair<const char*,const char*>> MutexNameMap_t;
typedef TrackableStdMap<CcpThreadId_t , const char*> ThreadNameMap_t;
typedef TrackableStdVector<std::pair<CcpOnTelemetryEventHandler, void*>> EventHandlerVector_t;

namespace
{
	std::string ToLower( const std::string& s )
	{
		std::string out( s );
		std::transform( out.begin(), out.end(), out.begin(), []( unsigned char c ) { return static_cast<char>( std::tolower( c ) ); } );
		return out;
	}

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
	// CaptureMask specifics:
	// -------------------------------
	constexpr size_t CAPTURE_MASKS_MAX{64};

	CcpMutex s_captureMaskMutex( "CcpTelemetry", "CaptureMaskMutex" );

	// Fixed size array for registered CaptureMasks.
	// Allows for O(1) lookup based on single-bit CaptureMask value.
	std::array<CcpCaptureMaskInfo, CAPTURE_MASKS_MAX> s_registeredCaptureMasks{
			CcpCaptureMaskInfo{ "general", TMCM_GENERAL, CcpColor::SteelBlue },
			CcpCaptureMaskInfo{ "cpp", TMCM_CPP,     CcpColor::Yellow	},
	};

	// Currently-active CaptureMask. Defaults to "all" until
	// narrowed in a later call to CcpSetActiveCaptureMask()
	uint64_t s_activeCaptureMaskBits = UINT64_MAX;


	// List of CaptureMask names that have yet to be registered.
	// From a CcpSetActiveCaptureMask( vector<string> ) call.
	std::vector<std::string> s_pendingActiveCaptureMaskNames;

	// Returns the index [0..63] of the lowest set bit.
	// Uses a de Bruijn sequence, thanks Claude Code.
	// TODO: Can be replaced with std::countr_zero() once we upgrade to C++20.
	constexpr int CountTrailingZeros64( uint64_t x ) noexcept
	{
		constexpr uint64_t deBruijn = 0x03f79d71b4cb0a89ULL;
		constexpr uint8_t lookup[64] = {
			 0,  1, 48,  2, 57, 49, 28,  3,
			61, 58, 50, 42, 38, 29, 17,  4,
			62, 55, 59, 36, 53, 51, 43, 22,
			45, 39, 33, 30, 24, 18, 12,  5,
			63, 47, 56, 27, 60, 41, 37, 16,
			54, 35, 52, 21, 44, 32, 23, 11,
			46, 26, 40, 15, 34, 20, 31, 10,
			25, 14, 19,  9, 13,  8,  7,  6,
		};
		return lookup[( ( x & ( 0ULL - x ) ) * deBruijn ) >> 58];
	}

	// Store a registered CaptureMask into its array slot
	void StoreRegisteredCaptureMask( uint64_t bit, const std::string& name, CcpColor color )
	{
		auto& entry = s_registeredCaptureMasks[CountTrailingZeros64( bit )];
		entry.name    = name;
		entry.maskBit = bit;
		entry.color   = color;
	}

	uint64_t GetRegisteredCaptureMaskBits()
	{
		uint64_t registeredBits = 0;
		for( const auto& entry : s_registeredCaptureMasks )
		{
			registeredBits |= entry.maskBit;
		}
		return registeredBits;
	}

	// Get the registered CaptureMask color for a given CaptureMask
	// Default to CcpColor::White if not found
	CcpColor GetCaptureMaskColor( uint64_t captureMaskBit )
	{
		if( captureMaskBit == 0 )
		{
			return CcpColor::White;
		}
		const auto& entry = s_registeredCaptureMasks[CountTrailingZeros64( captureMaskBit )];
		return entry.maskBit == captureMaskBit ? entry.color : CcpColor::White;
	}

	bool IsCaptureMaskActive( uint64_t captureMaskBit )
	{
		return ( s_activeCaptureMaskBits & captureMaskBit ) != 0;
	}
}

uint64_t CcpRegisterCaptureMask( const std::string& name, CcpColor color )
{
		// Guard access to registered CaptureMasks while we add/update a new entry
		CcpAutoMutex lock( s_captureMaskMutex );

		if( name.empty() )
		{
			CCP_LOGERR_CH( s_ch, "Cannot register a CaptureMask without a name" );
			return 0;
		}

		// Explicitly allow change of color on an existing registered
		// CaptureMask entry in case of re-register on same name.
		for( auto& registeredEntry : s_registeredCaptureMasks )
		{
			if( registeredEntry.maskBit != 0 && registeredEntry.name == name )
			{
				registeredEntry.color = color;
				return registeredEntry.maskBit;
			}
		}

		const uint64_t alreadyRegisteredBits = GetRegisteredCaptureMaskBits();
		if( alreadyRegisteredBits == UINT64_MAX )
		{
			CCP_LOGERR_CH( s_ch, "Cannot register CaptureMask '%s' - all 64 bits are already in use", name.c_str() );
			return 0;
		}

		// Allocate the lowest available free bit for the new CaptureMask
		const uint64_t newMaskBit = ~alreadyRegisteredBits & ( alreadyRegisteredBits + 1 );

		StoreRegisteredCaptureMask( newMaskBit, name, color );
		CCP_LOGWARN_CH( s_ch, "Registered a new CaptureMask for '%s' -> 0x%llx with color %s", name.c_str(), static_cast<unsigned long long>( newMaskBit ), CcpColorToString( color ).data() );

		// Make sure previously "pending active" CaptureMask is included
		auto pendingIt = std::find( s_pendingActiveCaptureMaskNames.begin(), s_pendingActiveCaptureMaskNames.end(), name );
		if( pendingIt != s_pendingActiveCaptureMaskNames.end() )
		{
			s_pendingActiveCaptureMaskNames.erase( pendingIt );
			s_activeCaptureMaskBits |= newMaskBit;
			CCP_LOGWARN_CH( s_ch, "Previously pending CaptureMask '%s' added to activeCaptureMask  -> 0x%llx", name.c_str(), static_cast<unsigned long long>( newMaskBit ) );
		}

		return newMaskBit;
}

std::vector<CcpCaptureMaskInfo> CcpGetRegisteredCaptureMasks()
{
	// Guard access to registered CaptureMasks while return list is populated.
	CcpAutoMutex lock( s_captureMaskMutex );

	std::vector<CcpCaptureMaskInfo> result;
	result.reserve( CAPTURE_MASKS_MAX );
	for( const auto& registeredEntry : s_registeredCaptureMasks )
	{
		if( registeredEntry.maskBit != 0 )
		{
			result.push_back( registeredEntry );
		}
	}
	return result;
}

bool CcpSetActiveCaptureMask( const std::vector<std::string>& maskNames )
{
	// Guard access to all CaptureMasks members
	CcpAutoMutex lock( s_captureMaskMutex );

	if ( maskNames.size() > CAPTURE_MASKS_MAX )
	{
		CCP_LOGERR_CH( s_ch, "Failed setting active capture mask because more %lu maskNames were passed in, but only %lu are allowed", maskNames.size(), CAPTURE_MASKS_MAX );
		return false;
	}

	s_pendingActiveCaptureMaskNames.clear();
	uint64_t newActiveCaptureMask = 0;
	for( const auto& rawName : maskNames )
	{
		if( rawName.empty() )
		{
			continue;
		}

		bool alreadyRegistered = false;
		for( const auto& registeredEntry : s_registeredCaptureMasks )
		{
			if( registeredEntry.maskBit != 0 && registeredEntry.name == rawName )
			{
				newActiveCaptureMask |= registeredEntry.maskBit;
				alreadyRegistered = true;
				break;
			}
		}

		// This CaptureMask hasn't been registered (yet) so add it to the pending list
		if( !alreadyRegistered )
		{
			if( std::find( s_pendingActiveCaptureMaskNames.begin(), s_pendingActiveCaptureMaskNames.end(), rawName ) == s_pendingActiveCaptureMaskNames.end() )
			{
				s_pendingActiveCaptureMaskNames.emplace_back( rawName );
			}
		}
	}

	s_activeCaptureMaskBits = newActiveCaptureMask;
	CCP_LOG_CH( s_ch, "Active CaptureMask set to 0x%llx (%zu pending unresolved name(s))", static_cast<unsigned long long>( newActiveCaptureMask ), s_pendingActiveCaptureMaskNames.size() );

	return true;
}

std::vector<std::string> CcpGetActiveCaptureMask()
{
	std::vector<std::string> result;
	size_t index{0};
	for ( const auto& registeredEntry : s_registeredCaptureMasks )
	{
		uint64_t currentMaskBit = 1ULL << index;

		if ( ( s_activeCaptureMaskBits & currentMaskBit ) != 0 )
		{
			result.push_back( registeredEntry.name );
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

			// Give the profiler a few seconds to receive information from the fiber name store before deallocating
			// the underlying string
			if ( !s_fiberEraseMap.empty() )
			{
				auto now = std::chrono::steady_clock::now();
				auto elem = s_fiberEraseMap.front();
				while ( !s_fiberEraseMap.empty() && elem.second >= now )
				{
					s_fiberNameStore.erase( elem.first );
					s_fiberEraseMap.pop();
					elem = s_fiberEraseMap.front();
				}
			}

			if( s_config.captureDuration != std::chrono::milliseconds::zero() ) // Check if we have passed our timed sample time
			{
				auto timeSinceStart = std::chrono::steady_clock::now() - s_profilerStartTime;
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

// Deprecated version
TelemetryZone::TelemetryZone( uint32_t ctx, const char* name, const char* filename, uint32_t lineno, CcpColor color ) : m_impl(std::make_unique<Private>())
{
	if( s_profilerState.load( std::memory_order_acquire ) != ProfilerState::Started )
	{
		return;
	}

	CCP_ASSERT( filename != nullptr );
	CCP_ASSERT( name != nullptr );

	const int active = ( ( ctx & TMCM_CPP ) != 0 ) && IsCaptureMaskActive( ctx );
	auto data = ___tracy_alloc_srcloc( lineno, filename, strlen( filename ), name, strlen( name ), static_cast<uint32_t>( color ) );
//	CCP_LOG_CH( s_ch, "[Fiber %p] Creating zone %s (%p)", t_activeFiber->c_str(), ret.first->c_str(), this );
	m_impl->fiber = t_activeFiber;
	m_impl->telemetryContext.emplace( ___tracy_emit_zone_begin_alloc( data, active ) );
}

// Preferred version
TelemetryZone::TelemetryZone( CaptureMaskBitTag, uint64_t captureMaskBit, const char* name, const char* filename, uint32_t lineno ) : m_impl( std::make_unique<Private>() )
{
	if( s_profilerState.load( std::memory_order_acquire ) != ProfilerState::Started )
	{
		return;
	}

	CCP_ASSERT( filename != nullptr );
	CCP_ASSERT( name != nullptr );

	const CcpColor color = GetCaptureMaskColor( captureMaskBit );
	const int active = IsCaptureMaskActive( captureMaskBit );
	auto data = ___tracy_alloc_srcloc( lineno, filename, strlen( filename ), name, strlen( name ), static_cast<uint32_t>( color ) );
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

// Deprecated version
void CcpTelemetryEnterZone( void* key, const char* name, const char* filename, uint32_t lineno )
{
	CcpTelemetryEnterZone( key, TMCM_CPP, name, filename, lineno ); // The default assigned color for TMCM_CPP is CcpColor::Yellow
}

// Preferred version
void CcpTelemetryEnterZone( void* key, uint64_t captureMaskBit, const char* name, const char* filename, uint32_t lineno )
{
	if( s_profilerState.load( std::memory_order_acquire ) == ProfilerState::Started )
	{
		t_manuallyTrackedZones.emplace( key );
		t_activeTaskletZoneStore->second.emplace( CaptureMaskBit, captureMaskBit, name, filename, lineno );
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

uint64_t CcpRegisterCaptureMask( const std::string& )
{
	return 0;
}

uint64_t CcpRegisterCaptureMask( const std::string&, CcpColor )
{
	return 0;
}

std::vector<CcpCaptureMaskInfo> CcpGetRegisteredCaptureMasks()
{
	return {};
}

void CcpSetActiveCaptureMask( uint64_t )
{
}

void CcpSetActiveCaptureMask( const std::vector<std::string>& )
{
}

uint64_t CcpGetActiveCaptureMask()
{
	return 0;
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

void CcpTelemetryEnterZone( void* key, uint64_t captureMaskBit, const char* name, const char* filename, uint32_t lineno )
{
}

void CcpTelemetryLeaveZone( void* key )
{
}

void CcpTelemetryZoneAddText( void* key, const char* text )
{
}

#endif // CCP_TELEMETRY_ENABLED
