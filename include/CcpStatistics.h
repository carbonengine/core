// Copyright © 2025 CCP ehf.

#ifndef CcpStatistics_H
#define CcpStatistics_H

// See http://carbon/wiki/CcpStatistics

#include "TrackableContainer.h"
#include "CcpTime.h"
#include "CcpAtomic.h"
#include <string>
#include "carbon_core_export.h"

#ifndef CCP_STATS_ENABLED
	// Statistics are enabled by default
	#define CCP_STATS_ENABLED 1
#endif

////////////////////////////////////////////////////////////////////////////
// CcpStatistics
#ifdef _MSC_VER
#pragma warning(disable: 4251)
#endif

// The type of the statistic dictates what group it goes into when graphed
enum CcpStatisticsType_t
{
	CST_COUNTER_HIGH,
	CST_COUNTER_LOW,
	CST_MEMORY,
	CST_TIME,

	CST_TYPE_COUNT
};

// The base statistics entry
class CARBON_CORE_API CcpStaticStatisticsEntry
{
public:
	CcpStaticStatisticsEntry( const char* name, bool b, CcpStatisticsType_t t, const char* desc );
	virtual ~CcpStaticStatisticsEntry();

	void Inc();
	void Dec();
	void Add( int64_t d );
	void Add( double d );
	void Set( uint64_t d );
	void Set( double d );
	void Capture();
	void ResetPeak();
	double GetValue();
	double GetPeak();

	const std::string& GetDescription() const;
	void SetDescription( const std::string& val );
	const std::string& GetName() const;
	void SetName( const std::string& val );
	void SetType( CcpStatisticsType_t type );
	CcpStatisticsType_t GetType();
	bool GetResetPerFrame() const { return m_resetPerFrame; }
	void SetResetPerFrame( bool val );

protected:
	friend class CcpStatistics;

	void UpdatePeak();

	// If true, the value is reset to 0 on every capture
	bool m_resetPerFrame;

	CcpStatisticsType_t m_type;
	std::string m_name;
	std::string m_description;
	
	// The captured value
	CcpAtomic<uint64_t> m_value;

	// The raw value, may change multiple times per frame
	CcpAtomic<uint64_t> m_raw;

	// The peak captured value
	CcpAtomic<uint64_t> m_peak;
};

// Derived statistics entry.
class CARBON_CORE_API CcpDerivedStatisticsEntry : public CcpStaticStatisticsEntry
{
public:
	CcpDerivedStatisticsEntry();
	void SetSource( CcpStaticStatisticsEntry* stat );
	CcpStaticStatisticsEntry* GetSource();

	virtual void CalcDerivedStats() = 0;
	virtual void Reset() = 0;

protected:
	CcpStaticStatisticsEntry* m_sourceStatistic;
};

// Note that on ORBIS we get a link error in Trinity for typeinfo for CcpMeanStatisticsEntry
// if any virtual functions do not have inlined implementations. This is in all likelihood
// a compiler/linker bug - until it's fixed we get around it by inlining all
// the virtual functions.

// Accumulated mean of a given statistic
class CARBON_CORE_API CcpMeanStatisticsEntry : public CcpDerivedStatisticsEntry
{
public:
	CcpMeanStatisticsEntry();

	virtual void CalcDerivedStats() { CalcDerivedStatsImpl(); }
	virtual void Reset() { ResetImpl(); }

protected:
	void CalcDerivedStatsImpl();
	void ResetImpl();

	// Helpers for calculating mean and standard deviation
	unsigned int m_numSamples;

	double m_oldM;
	double m_newM;
};

// Accumulated standard deviation of a given statistic
class CARBON_CORE_API CcpStdDevStatisticsEntry : public CcpDerivedStatisticsEntry
{
public:
	CcpStdDevStatisticsEntry();

	virtual void CalcDerivedStats() { CalcDerivedStatsImpl(); }
	virtual void Reset() { ResetImpl(); }

protected:
	void CalcDerivedStatsImpl();
	void ResetImpl();

	// Helpers for calculating standard deviation
	unsigned int m_numSamples;

	double m_oldM;
	double m_oldS;
	double m_newM;
	double m_newS;
};

class CARBON_CORE_API CcpStatistics
{
public:
	// Usage through C++ is all through static functions
	static void Register( CcpStaticStatisticsEntry* entry );
	static void Unregister( CcpStaticStatisticsEntry* entry );

	static void RegisterDerived( CcpDerivedStatisticsEntry* entry );
	static void UnregisterDerived( CcpDerivedStatisticsEntry* entry );

	static void Init();
	static void Shutdown();

	static void Update();

	typedef TrackableStdVector<CcpStaticStatisticsEntry*> EntryArray;
	static EntryArray& GetEntryArray();

	typedef TrackableStdVector<CcpDerivedStatisticsEntry*> DerivedEntryArray;
	static DerivedEntryArray& GetDerivedEntryArray();
};

#if CCP_STATS_ENABLED

// Helper class for measuring time within a scope
class CARBON_CORE_API CcpStatisticsStopwatch
{
public:
	CcpStatisticsStopwatch( CcpStaticStatisticsEntry& s ) : m_stat( s )
	{
		m_timestamp = CcpGetTimestamp();
	}

	~CcpStatisticsStopwatch()
	{
		uint64_t now = CcpGetTimestamp();
		uint64_t delta = now - m_timestamp;
		double deltaInSeconds = (double)delta / (double)CcpGetTimestampFrequency();
		m_stat.Add( deltaInSeconds );
	}

private:
	CcpStaticStatisticsEntry& m_stat;
	uint64_t m_timestamp;
};

#define CCP_STATS_DECLARE( identifier, name, reset, type, desc ) CcpStaticStatisticsEntry g_ccpStatistics_##identifier( name, reset, type, desc )
#define CCP_STATS_DECLARED_ELSEWHERE( identifier ) extern CcpStaticStatisticsEntry g_ccpStatistics_##identifier

#define CCP_STATS_INC( identifier ) g_ccpStatistics_##identifier.Inc()
#define CCP_STATS_DEC( identifier ) g_ccpStatistics_##identifier.Dec()
#define CCP_STATS_ADD( identifier, x ) g_ccpStatistics_##identifier.Add( (int64_t)(x) )
#define CCP_STATS_SET( identifier, x ) g_ccpStatistics_##identifier.Set( (uint64_t)(x) )
#define CCP_STATS_GET( identifier ) g_ccpStatistics_##identifier.GetValue()
#define CCP_STATS_RESET( identifier ) g_ccpStatistics_##identifier.ResetPeak()

#if CCP_TELEMETRY_ENABLED
	#define CCP_TELEMETRY_ZONE( name ) \
		tmZone zone_##_COUNTER_( TMCM_CPP, name )
#else
	#define CCP_TELEMETRY_ZONE( name )
#endif

#else // CCP_STATS_ENABLED

#define STATS_DECLARE( name, reset, m_type, desc )
#define STATS_DECLARED_ELSEWHERE( name )

#define STATS_UPDATE( time )
#define STATS_INC( name )
#define STATS_DEC( name )
#define STATS_ADD( name, x )
#define STATS_SET( name, x )
#define STATS_GET( name ) ( 0.0 )
#define STATS_RESET( name )
#define STATS_INIT( name )
#define STATS_SCOPED_TIME( name )

#endif

#ifdef _MSC_VER
#pragma warning( default : 4251)
#endif

#endif // CcpStatistics_H
