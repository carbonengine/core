// Copyright © 2026 Fenris Creations.


#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <vector>

#include <CcpColorConstants.h>


TEST( CcpColorConstantsTest, PickedColorIsUnused )
{
	std::vector<CcpColor> existing{ CcpColor::SteelBlue, CcpColor::Yellow };
	for( int i = 0; i < 62; ++i )
	{
		const CcpColor picked = ColorUtil::PickDistinctColor( existing );
		EXPECT_EQ( existing.end(), std::find( existing.begin(), existing.end(), picked ) );
		existing.push_back( picked );

		// TODO: Remove this once visually validated.
		// Print out selected colors for manual checking
		std::fprintf( stdout,
					  "[%2d]: %s  -  (0x%06x)\n",
					  i,
					  CcpColorToString( picked ).c_str(),
					  static_cast<uint32_t>( picked ) );
	}
	std::fflush( stdout );
}

TEST( CcpColorConstantsTest, PickedColorIsDistinctFromExisting )
{
	// With a single existing color, the pick should be far away from it in RGB space,
	// not merely a slightly different shade.
	const std::vector<CcpColor> existingRed{ CcpColor::Red };
	const CcpColor pickedVsRed = ColorUtil::PickDistinctColor( existingRed );
	auto distPickedVsRed = ColorUtil::GetSquaredDistance( pickedVsRed, CcpColor::Red );

	std::fprintf( stdout,
		"[0x%06x] '%s' vs Picked color: [0x%06x] '%s' - distance = %lld \n",
			  CcpColor::Red, CcpColorToString(CcpColor::Red).c_str(),
			  pickedVsRed, CcpColorToString( pickedVsRed ).c_str(),
			  distPickedVsRed );
	std::fflush( stdout );
	EXPECT_GT( distPickedVsRed, 128 * 128 );

	const std::vector<CcpColor> existingBlue{ CcpColor::Blue };
	const CcpColor pickedVsBlue = ColorUtil::PickDistinctColor( existingBlue );
	auto distPickedVsBlue = ColorUtil::GetSquaredDistance( pickedVsBlue, CcpColor::Blue );

	std::fprintf( stdout,
			  "[0x%06x] '%s' vs Picked color: [0x%06x] '%s' - distance = %lld \n",
			  CcpColor::Blue, CcpColorToString(CcpColor::Blue).c_str(),
			  pickedVsBlue, CcpColorToString( pickedVsBlue ).c_str(),
			  distPickedVsBlue );
	std::fflush( stdout );
	EXPECT_GT( distPickedVsBlue, 128 * 128 );
}
