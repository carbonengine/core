// Copyright © 2025 CCP ehf.

#include "include/CcpStatistics.h"

#include "CcpTelemetry.h"

static double SCALING_FACTOR_FOR_TIME = 1000000.0;

CcpStatistics::EntryArray& CcpStatistics::GetEntryArray()
{
	static EntryArray s_entries( "CcpStatistics/s_entries" );
	return s_entries;
}

CcpStatistics::DerivedEntryArray& CcpStatistics::GetDerivedEntryArray()
{
	static DerivedEntryArray s_derivedEntries( "CcpStatistics/s_derivedEntries" );
	return s_derivedEntries;
}

void CcpStatistics::Init()
{
	EntryArray& a = GetEntryArray();
	for(EntryArray::iterator it = a.begin(); it != a.end(); ++it)
	{
	    (*it)->ResetPeak();
	}
}

void CcpStatistics::Shutdown()
{

}

void CcpStatistics::Register( CcpStaticStatisticsEntry *entry )
{
	GetEntryArray().push_back( entry );
}

void CcpStatistics::Unregister( CcpStaticStatisticsEntry *entry )
{
	EntryArray &a = GetEntryArray();
	EntryArray::iterator findResult;
	findResult = std::find( a.begin(), a.end(), entry );
	if ( findResult != a.end() )
	{
		// We have the resource that we were looking for registered
		CcpStaticStatisticsEntry* backItem = a.back();
		*findResult = backItem;

		a.pop_back();
	}
}

void CcpStatistics::RegisterDerived( CcpDerivedStatisticsEntry *entry )
{
	GetDerivedEntryArray().push_back( entry );
}

void CcpStatistics::UnregisterDerived( CcpDerivedStatisticsEntry *entry )
{
	DerivedEntryArray &a = GetDerivedEntryArray();
	DerivedEntryArray::iterator findResult;
	findResult = std::find( a.begin(), a.end(), entry );
	if ( findResult != a.end() )
	{
		// We have the resource that we were looking for registered
		CcpDerivedStatisticsEntry* backItem = a.back();
		*findResult = backItem;

		a.pop_back();
	}
}

void CcpStatistics::Update()
{
	EntryArray& a = GetEntryArray();
	for(EntryArray::iterator it = a.begin(); it != a.end(); ++it)
	{
		(*it)->Capture();
	}

	DerivedEntryArray& b = GetDerivedEntryArray();
	for( auto it = b.begin(); it != b.end(); ++it )
	{
		(*it)->CalcDerivedStats();
		(*it)->Capture();
	}
}


CcpStaticStatisticsEntry::CcpStaticStatisticsEntry( const char* s, bool b, CcpStatisticsType_t t, const char* desc ) :
	m_name( s ),
	m_description( desc ),
	m_resetPerFrame( b ),
	m_type( t ),
	m_value( 0 ),
	m_raw( 0 ),
	m_peak( 0 )
{
	CcpStatistics::Register( this );
}

CcpStaticStatisticsEntry::~CcpStaticStatisticsEntry()
{
	CcpStatistics::Unregister( this );
}

void CcpStaticStatisticsEntry::Capture()
{
	// Thread safety compromised here
	uint64_t value = m_raw;
	m_value = value;
	if( m_resetPerFrame )
	{
		m_raw = 0;
	}

#if CCP_TELEMETRY_ENABLED
	if( CcpTelemetryIsConnected() )
	{
		static tracy::PlotFormatType plotTypes[4] = { tracy::PlotFormatType::Number, tracy::PlotFormatType::Number, tracy::PlotFormatType::Memory, tracy::PlotFormatType::Number };
		double valueAsDouble = (double)value;
		if( m_type == CST_TIME )
		{
			// Tracy expects time values in milliseconds
			valueAsDouble /= 1000;
		}
		TracyPlotConfig( m_name.c_str(), plotTypes[m_type], false, true, tracy::Color::DodgerBlue4 );
		TracyPlot( m_name.c_str(), valueAsDouble );
	}
#endif
}

void CcpStaticStatisticsEntry::Inc()
{
	++m_raw;
	UpdatePeak();
}

void CcpStaticStatisticsEntry::Dec()
{
	--m_raw;
}

void CcpStaticStatisticsEntry::Add( int64_t d )
{
	m_raw += d;
	UpdatePeak();
}

void CcpStaticStatisticsEntry::Add( double d )
{
	int64_t fixedPointValue;
	if( m_type == CST_TIME )
	{
		fixedPointValue = (int64_t)( d * SCALING_FACTOR_FOR_TIME );
	}
	else
	{
		fixedPointValue = (int64_t)d;
	}
	Add( fixedPointValue );
}

void CcpStaticStatisticsEntry::Set( uint64_t d )
{
	m_raw = d;
	UpdatePeak();
}

void CcpStaticStatisticsEntry::Set( double d )
{
	uint64_t fixedPointValue;
	if( m_type == CST_TIME )
	{
		fixedPointValue = (uint64_t)( d * SCALING_FACTOR_FOR_TIME );
	}
	else
	{
		fixedPointValue = (uint64_t)d;
	}
	Set( fixedPointValue );
}

double CcpStaticStatisticsEntry::GetPeak()
{
	if( m_type == CST_TIME )
	{
		return m_peak / SCALING_FACTOR_FOR_TIME;
	}
	else
	{
		return double( m_peak );
	}
}

void CcpStaticStatisticsEntry::ResetPeak()
{
	m_peak = 0;
}

const std::string& CcpStaticStatisticsEntry::GetDescription() const
{
	return m_description;
}

void CcpStaticStatisticsEntry::SetDescription( const std::string& val )
{
	m_description = val;
}

const std::string& CcpStaticStatisticsEntry::GetName() const
{
	return m_name;
}

void CcpStaticStatisticsEntry::SetName( const std::string& val )
{
	m_name = val;
}

void CcpStaticStatisticsEntry::SetType( CcpStatisticsType_t type )
{
	m_type = type;
}

void CcpStaticStatisticsEntry::SetResetPerFrame( bool val )
{
	m_resetPerFrame = val;
}

double CcpStaticStatisticsEntry::GetValue()
{
	if( m_type == CST_TIME )
	{
		return m_value / SCALING_FACTOR_FOR_TIME;
	}
	else
	{
		return double( m_value );
	}
}

CcpStatisticsType_t CcpStaticStatisticsEntry::GetType()
{
	return m_type;
}

void CcpStaticStatisticsEntry::UpdatePeak()
{
	// Thread safety breaks down here - need to add to CcpAtomics to do this
	// as an atomic operation.
	// See http://stackoverflow.com/questions/16190078/how-to-atomically-update-a-maximum-value
	if( m_raw > m_peak )
	{
		uint64_t value = m_raw;
		m_peak = value;
	}
}


CcpDerivedStatisticsEntry::CcpDerivedStatisticsEntry() :
	CcpStaticStatisticsEntry( "", false, CST_COUNTER_LOW, "" ),
	m_sourceStatistic( nullptr )
{
}

void CcpDerivedStatisticsEntry::SetSource( CcpStaticStatisticsEntry* stat )
{
	m_sourceStatistic = stat;
	if( stat )
	{
		// 'stat' could be nullptr if it is being decoupled from the source
		m_type = stat->GetType();
	}
}

CcpStaticStatisticsEntry* CcpDerivedStatisticsEntry::GetSource()
{
	return m_sourceStatistic;
}

CcpMeanStatisticsEntry::CcpMeanStatisticsEntry() :
	m_numSamples( 0 ),
	m_oldM( 0 ),
	m_newM( 0 )
{
	m_resetPerFrame = false;
}

void CcpMeanStatisticsEntry::CalcDerivedStatsImpl()
{
	if( !m_sourceStatistic )
	{
		return;
	}

	++m_numSamples;

	double n = (double)m_numSamples;
	double value = m_sourceStatistic->GetValue();

	if( m_numSamples == 1 )
	{
		m_oldM = value;
		m_newM = value;
	}
	else
	{
		m_newM = m_oldM + (value - m_oldM) / n;
		m_oldM = m_newM;
	}

	Set( m_newM );
}

void CcpMeanStatisticsEntry::ResetImpl()
{
	m_numSamples = 0;
	m_oldM = 0;
	m_newM = 0;
}

CcpStdDevStatisticsEntry::CcpStdDevStatisticsEntry() :
	m_numSamples( 0 ),
	m_oldM( 0 ),
	m_oldS( 0 ),
	m_newM( 0 ),
	m_newS( 0 )
{
	m_resetPerFrame = false;
}

void CcpStdDevStatisticsEntry::CalcDerivedStatsImpl()
{
	if( !m_sourceStatistic )
	{
		return;
	}

	// http://www.johndcook.com/standard_deviation.html

	++m_numSamples;

	double n = (double)m_numSamples;
	double value = m_sourceStatistic->GetValue();

	if( m_numSamples == 1 )
	{
		m_oldM = value;
		m_newM = value;
		m_oldS = 0.0;
		m_newS = 0.0;
	}
	else
	{
		m_newM = m_oldM + (value - m_oldM) / n;
		m_newS = m_oldS + (value - m_oldM) * (value - m_newM);

		m_oldM = m_newM;
		m_oldS = m_newS;
	}

	double variance = 0.0;
	if( n > 1.0 )
	{
		variance = m_newS / (n - 1.0);
	}

	double stddev = 0.0;
	if( variance > 0.0 )
	{
		stddev = sqrt(variance);
	}

	Set( stddev );
}

void CcpStdDevStatisticsEntry::ResetImpl()
{
	m_numSamples = 0;
	m_oldM = 0;
	m_newM = 0;
	m_oldS = 0;
	m_newS = 0;
}
