// Copyright © 2025 CCP ehf.

#include "gtest/gtest.h"
#include "CcpCore.h"
#include <algorithm>

TEST( CcpStatistics, CanCreateEntry )
{
	const char* name = "entryName";
	bool resetPerFrame = false;
	CcpStatisticsType_t type = CST_COUNTER_LOW;
	const char* description = "entry description";
	CcpStaticStatisticsEntry e( name, resetPerFrame, type, description );

	EXPECT_EQ( name, e.GetName() );
	EXPECT_EQ( resetPerFrame, e.GetResetPerFrame() );
	EXPECT_EQ( type, e.GetType() );
	EXPECT_EQ( description, e.GetDescription() );
}

TEST( CcpStatistics, CanSetEntryName )
{
	CcpStaticStatisticsEntry e( "entryName", false, CST_COUNTER_LOW, "entry description" );

	e.SetName( "aaa" );
	EXPECT_EQ( "aaa", e.GetName() );
}

TEST( CcpStatistics, CanSetEntryResetPerFrameFlag )
{
	CcpStaticStatisticsEntry e( "entryName", false, CST_COUNTER_LOW, "entry description" );

	e.SetResetPerFrame( true );
	EXPECT_EQ( true, e.GetResetPerFrame() );
}

TEST( CcpStatistics, CanSetEntryType )
{
	CcpStaticStatisticsEntry e( "entryName", false, CST_COUNTER_LOW, "entry description" );

	e.SetType( CST_COUNTER_HIGH );
	EXPECT_EQ( CST_COUNTER_HIGH, e.GetType() );
}

TEST( CcpStatistics, CanSetEntryDescription )
{
	CcpStaticStatisticsEntry e( "entryName", false, CST_COUNTER_LOW, "entry description" );

	e.SetDescription( "aaa" );
	EXPECT_EQ( "aaa", e.GetDescription() );
}

TEST( CcpStatistics, EntryInitialValueIsZero )
{
	CcpStaticStatisticsEntry e( "entryName", false, CST_COUNTER_LOW, "entry description" );

	EXPECT_EQ( 0, e.GetValue() );
	EXPECT_EQ( 0, e.GetPeak() );
}

TEST( CcpStatistics, EntryValueDoesNotChangeUntilCaptureIsCalled )
{
	CcpStaticStatisticsEntry e( "entryName", false, CST_COUNTER_LOW, "entry description" );

	e.Inc();
	EXPECT_EQ( 0, e.GetValue() );
}

TEST( CcpStatistics, CanIncreaseEntryValue )
{
	CcpStaticStatisticsEntry e( "entryName", false, CST_COUNTER_LOW, "entry description" );

	e.Inc();
	e.Capture();
	EXPECT_EQ( 1, e.GetValue() );
}

TEST( CcpStatistics, CanDecreaseEntryValue )
{
	CcpStaticStatisticsEntry e( "entryName", false, CST_COUNTER_LOW, "entry description" );

	e.Inc();
	e.Inc();
	e.Inc();
	e.Dec();
	e.Capture();
	EXPECT_EQ( 2, e.GetValue() );
}

TEST( CcpStatistics, IncreasingValueUpdatesItsPeak )
{
	CcpStaticStatisticsEntry e( "entryName", false, CST_COUNTER_LOW, "entry description" );

	e.Inc();
	e.Inc();
	e.Inc();
	e.Dec();
	EXPECT_EQ( 3, e.GetPeak() );
}

TEST( CcpStatistics, CanAddToEntyValue )
{
	CcpStaticStatisticsEntry e( "entryName", false, CST_COUNTER_LOW, "entry description" );

	e.Add( int64_t( 123 ) );
	EXPECT_EQ( 123, e.GetPeak() );
	EXPECT_EQ( 0, e.GetValue() );

	e.Capture();
	EXPECT_EQ( 123, e.GetValue() );
}

TEST( CcpStatistics, CanSubtractFromEntyValue )
{
	CcpStaticStatisticsEntry e( "entryName", false, CST_COUNTER_LOW, "entry description" );

	e.Add( int64_t( 123 ) );
	e.Add( int64_t( -31 ) );
	EXPECT_EQ( 123, e.GetPeak() );
	EXPECT_EQ( 0, e.GetValue() );

	e.Capture();
	EXPECT_EQ( 123 - 31, e.GetValue() );
}

TEST( CcpStatistics, CanSetEntyValue )
{
	CcpStaticStatisticsEntry e( "entryName", false, CST_COUNTER_LOW, "entry description" );

	e.Set( uint64_t( 123 ) );
	EXPECT_EQ( 123, e.GetPeak() );
	EXPECT_EQ( 0, e.GetValue() );

	e.Capture();
	EXPECT_EQ( 123, e.GetValue() );
}

TEST( CcpStatistics, CanResetEntryPeak )
{
	CcpStaticStatisticsEntry e( "entryName", false, CST_COUNTER_LOW, "entry description" );

	e.Set( uint64_t( 123 ) );
	e.ResetPeak();
	EXPECT_EQ( 0, e.GetPeak() );
}

TEST( CcpStatistics, CanAddDoubleToEntryValue )
{
	CcpStaticStatisticsEntry e( "entryName", false, CST_COUNTER_LOW, "entry description" );

	e.Add( 123.1 );
	e.Capture();

	EXPECT_EQ( 123, e.GetPeak() );
	EXPECT_EQ( 123, e.GetValue() );
}

TEST( CcpStatistics, CanSetDoubleAsEntryValue )
{
	CcpStaticStatisticsEntry e( "entryName", false, CST_COUNTER_LOW, "entry description" );

	e.Set( 123.1 );
	e.Capture();

	EXPECT_EQ( 123, e.GetPeak() );
	EXPECT_EQ( 123, e.GetValue() );
}

TEST( CcpStatistics, CanAddDoubleToTimeEntryValue )
{
	CcpStaticStatisticsEntry e( "entryName", false, CST_TIME, "entry description" );

	e.Add( 100.5 );
	e.Add( 123.1 );
	e.Capture();

	EXPECT_EQ( 223.6, e.GetPeak() );
	EXPECT_EQ( 223.6, e.GetValue() );
}

TEST( CcpStatistics, CanSetDoubleAsTimeEntryValue )
{
	CcpStaticStatisticsEntry e( "entryName", false, CST_TIME, "entry description" );

	e.Set( 123.1 );
	e.Capture();

	EXPECT_EQ( 123.1, e.GetPeak() );
	EXPECT_EQ( 123.1, e.GetValue() );
}

TEST( CcpStatistics, NonAutoResetEntyValueIsNotResetOnCapture )
{
	CcpStaticStatisticsEntry e( "entryName", false, CST_COUNTER_LOW, "entry description" );

	e.Set( uint64_t( 123 ) );
	e.Capture();
	EXPECT_EQ( 123, e.GetValue() );
	e.Capture();
	EXPECT_EQ( 123, e.GetValue() );
}

TEST( CcpStatistics, AutoResetEntyValueIsResetOnCapture )
{
	CcpStaticStatisticsEntry e( "entryName", true, CST_COUNTER_LOW, "entry description" );

	e.Set( uint64_t( 123 ) );
	e.Capture();
	EXPECT_EQ( 123, e.GetValue() );
	e.Capture();
	EXPECT_EQ( 0, e.GetValue() );
}

TEST( CcpStatistics, UnattachedMeanEntryValueIsZero )
{
	CcpMeanStatisticsEntry mean;

	mean.CalcDerivedStats();
	mean.Capture();
	EXPECT_EQ( 0, mean.GetValue() );
}

TEST( CcpStatistics, CanGetMeanEntrySource )
{
	CcpStaticStatisticsEntry e( "entryName", true, CST_COUNTER_LOW, "entry description" );
	CcpMeanStatisticsEntry mean;
	mean.SetSource( &e );

	EXPECT_EQ( &e, mean.GetSource() );
}

TEST( CcpStatistics, CanResetMeanEntrySource )
{
	CcpStaticStatisticsEntry e( "entryName", true, CST_COUNTER_LOW, "entry description" );
	CcpMeanStatisticsEntry mean;
	mean.SetSource( &e );
	mean.SetSource( nullptr );

	EXPECT_EQ( nullptr, mean.GetSource() );
}

TEST( CcpStatistics, MeanEntryValueIsMeanOfBaseEntryValues )
{
	CcpStaticStatisticsEntry e( "entryName", true, CST_COUNTER_LOW, "entry description" );
	CcpMeanStatisticsEntry mean;
	mean.SetSource( &e );

	e.Set( uint64_t( 1 ) );
	e.Capture();
	mean.CalcDerivedStats();
	mean.Capture();
	EXPECT_EQ( 1, mean.GetValue() );
	e.Set( uint64_t( 2 ) );
	e.Capture();
	mean.CalcDerivedStats();
	mean.Capture();
	EXPECT_EQ( 1, mean.GetValue() );
	e.Set( uint64_t( 3 ) );
	e.Capture();
	mean.CalcDerivedStats();
	mean.Capture();
	EXPECT_EQ( 2, mean.GetValue() );
}

TEST( CcpStatistics, CanResetMeanEntry )
{
	CcpStaticStatisticsEntry e( "entryName", true, CST_COUNTER_LOW, "entry description" );
	CcpMeanStatisticsEntry mean;
	mean.SetSource( &e );

	e.Set( uint64_t( 1 ) );
	e.Capture();
	mean.CalcDerivedStats();
	mean.Capture();
	
	e.Set( uint64_t( 3 ) );
	e.Capture();
	mean.CalcDerivedStats();
	mean.Capture();
	
	mean.Reset();
	mean.CalcDerivedStats();
	mean.Capture();
	EXPECT_EQ( 3, mean.GetValue() );
}

TEST( CcpStatistics, UnattachedStdDevEntryValueIsZero )
{
	CcpStdDevStatisticsEntry stdDev;

	stdDev.CalcDerivedStats();
	stdDev.Capture();
	EXPECT_EQ( 0, stdDev.GetValue() );
}

TEST( CcpStatistics, CanGetStdDevEntrySource )
{
	CcpStaticStatisticsEntry e( "entryName", true, CST_COUNTER_LOW, "entry description" );
	CcpStdDevStatisticsEntry stdDev;
	stdDev.SetSource( &e );

	EXPECT_EQ( &e, stdDev.GetSource() );
}

TEST( CcpStatistics, CanResetStdDevEntrySource )
{
	CcpStaticStatisticsEntry e( "entryName", true, CST_COUNTER_LOW, "entry description" );
	CcpStdDevStatisticsEntry stdDev;
	stdDev.SetSource( &e );
	stdDev.SetSource( nullptr );

	EXPECT_EQ( nullptr, stdDev.GetSource() );
}

TEST( CcpStatistics, StdDevEntryValueIsStdDevOfBaseEntryValues )
{
	CcpStaticStatisticsEntry e( "entryName", true, CST_COUNTER_LOW, "entry description" );
	CcpStdDevStatisticsEntry stdDev;
	stdDev.SetSource( &e );

	e.Set( uint64_t( 1 ) );
	e.Capture();
	stdDev.CalcDerivedStats();
	stdDev.Capture();
	EXPECT_EQ( 0, stdDev.GetValue() );
	e.Set( uint64_t( 5 ) );
	e.Capture();
	stdDev.CalcDerivedStats();
	stdDev.Capture();
	EXPECT_EQ( 2, stdDev.GetValue() );
}

TEST( CcpStatistics, CanResetStdDevEntry )
{
	CcpStaticStatisticsEntry e( "entryName", true, CST_COUNTER_LOW, "entry description" );
	CcpStdDevStatisticsEntry stdDev;
	stdDev.SetSource( &e );

	e.Set( uint64_t( 1 ) );
	e.Capture();
	stdDev.CalcDerivedStats();
	stdDev.Capture();
	e.Set( uint64_t( 5 ) );
	e.Capture();
	stdDev.CalcDerivedStats();
	stdDev.Capture();
	
	stdDev.Reset();
	stdDev.CalcDerivedStats();
	stdDev.Capture();
	EXPECT_EQ( 0, stdDev.GetValue() );
}

TEST( CcpStatistics, EntryIsAutoRegisteredWithCcpStatistics )
{
	auto& entries = CcpStatistics::GetEntryArray();
	auto prevSize = entries.size();

	CcpStaticStatisticsEntry e( "entryName", true, CST_COUNTER_LOW, "entry description" );

	EXPECT_EQ( prevSize + 1, entries.size() );
	EXPECT_NE( entries.end(), std::find( entries.begin(), entries.end(), &e ) );
}

TEST( CcpStatistics, EntryIsAutoUnregisteredWithCcpStatistics )
{
	auto& entries = CcpStatistics::GetEntryArray();
	auto prevSize = entries.size();
	CcpStaticStatisticsEntry* pointer;
	{
		CcpStaticStatisticsEntry e( "entryName", true, CST_COUNTER_LOW, "entry description" );
		pointer = &e;
	}
	EXPECT_EQ( prevSize, entries.size() );
	EXPECT_EQ( entries.end(), std::find( entries.begin(), entries.end(), pointer ) );
}

TEST( CcpStatistics, CanRegisterDerivedEntryWithCcpStatistics )
{
	auto& entries = CcpStatistics::GetDerivedEntryArray();
	auto prevSize = entries.size();

	CcpMeanStatisticsEntry mean;
	CcpStatistics::RegisterDerived( &mean );

	EXPECT_EQ( prevSize + 1, entries.size() );
	EXPECT_NE( entries.end(), std::find( entries.begin(), entries.end(), &mean ) );

	CcpStatistics::UnregisterDerived( &mean );
}

TEST( CcpStatistics, CanUnregisterDerivedEntryWithCcpStatistics )
{
	auto& entries = CcpStatistics::GetDerivedEntryArray();
	auto prevSize = entries.size();

	CcpMeanStatisticsEntry mean;
	CcpStatistics::RegisterDerived( &mean );
	CcpStatistics::UnregisterDerived( &mean );

	EXPECT_EQ( prevSize, entries.size() );
	EXPECT_EQ( entries.end(), std::find( entries.begin(), entries.end(), &mean ) );
}

TEST( CcpStatistics, UpdateCapturesEntryValue )
{
	CcpStaticStatisticsEntry e( "entryName", false, CST_COUNTER_LOW, "entry description" );
	e.Inc();
	CcpStatistics::Update();

	EXPECT_EQ( 1, e.GetValue() );
}

TEST( CcpStatistics, UpdateCapturesDerivedEntryValue )
{
	CcpStaticStatisticsEntry e( "entryName", false, CST_COUNTER_LOW, "entry description" );
	e.Inc();
	CcpMeanStatisticsEntry mean;
	mean.SetSource( &e );
	CcpStatistics::RegisterDerived( &mean );

	CcpStatistics::Update();

	EXPECT_EQ( 1, mean.GetValue() );
	CcpStatistics::UnregisterDerived( &mean );
}

TEST( CcpStatistics, CanMeasureTimeWithStopwatch )
{
	CcpStaticStatisticsEntry e( "entryName", false, CST_TIME, "entry description" );

	{
		CcpStatisticsStopwatch stopwatch( e );
		CcpThreadSleep( 100 );
	}

	e.Capture();
	EXPECT_LT( 0.05, e.GetValue() );
	EXPECT_GT( 0.25, e.GetValue() );
}
