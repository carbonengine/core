// Copyright © 2026 Fenris Creations.


#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <vector>

#include <CcpColorConstants.h>


TEST( CcpColorConstantsTest, PickedColorIsUnused )
{
	std::vector<Color> existing{ Color::SteelBlue, Color::Yellow };
	for( int i = 0; i < 62; ++i )
	{
		const Color picked = CcpColor::PickDistinctColor( existing );
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
	const std::vector<Color> existingRed{ Color::Red };
	const Color pickedVsRed = CcpColor::PickDistinctColor( existingRed );
	auto distPickedVsRed = CcpColor::GetSquaredDistance( pickedVsRed, Color::Red );

	std::fprintf( stdout,
		"[0x%06x] '%s' vs Picked color: [0x%06x] '%s' - distance = %lld \n",
			  Color::Red, CcpColorToString(Color::Red).c_str(),
			  pickedVsRed, CcpColorToString( pickedVsRed ).c_str(),
			  distPickedVsRed );
	std::fflush( stdout );
	EXPECT_GT( distPickedVsRed, 128 * 128 );

	const std::vector<Color> existingBlue{ Color::Blue };
	const Color pickedVsBlue = CcpColor::PickDistinctColor( existingBlue );
	auto distPickedVsBlue = CcpColor::GetSquaredDistance( pickedVsBlue, Color::Blue );

	std::fprintf( stdout,
			  "[0x%06x] '%s' vs Picked color: [0x%06x] '%s' - distance = %lld \n",
			  Color::Blue, CcpColorToString(Color::Blue).c_str(),
			  pickedVsBlue, CcpColorToString( pickedVsBlue ).c_str(),
			  distPickedVsBlue );
	std::fflush( stdout );
	EXPECT_GT( distPickedVsBlue, 128 * 128 );
}
