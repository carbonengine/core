// Copyright © 2026 Fenris Creations.

#pragma once
#ifndef CCP_COLOR_CONSTANTS_H
#define CCP_COLOR_CONSTANTS_H

#include <cstdint>
#include <string>
#include <vector>

#include "carbon_core_export.h"

// Useful RGB color constants
// Initially based on the <named-color> type of CSS standard, see https://www.w3.org/TR/css-color-4/#named-colors.
enum class CcpColor : uint32_t
{
	AliceBlue = 0xf0f8ff,
	AntiqueWhite = 0xfaebd7,
	Aqua = 0x00ffff,
	Aquamarine = 0x7fffd4,
	Azure = 0xf0ffff,
	Beige = 0xf5f5dc,
	Bisque = 0xffe4c4,
	Black = 0x000000,
	BlanchedAlmond = 0xffebcd,
	Blue = 0x0000ff,
	BlueViolet = 0x8a2be2,
	Brown = 0xa52a2a,
	BurlyWood = 0xdeb887,
	CadetBlue = 0x5f9ea0,
	Chartreuse = 0x7fff00,
	Chocolate = 0xd2691e,
	Coral = 0xff7f50,
	CornflowerBlue = 0x6495ed,
	Cornsilk = 0xfff8dc,
	Crimson = 0xdc143c,
	Cyan = 0x00ffff,
	DarkBlue = 0x00008b,
	DarkCyan = 0x008b8b,
	DarkGoldenrod = 0xb8860b,
	DarkGray = 0xa9a9a9,
	DarkGreen = 0x006400,
	DarkGrey = 0xa9a9a9,
	DarkKhaki = 0xbdb76b,
	DarkMagenta = 0x8b008b,
	DarkOliveGreen = 0x556b2f,
	DarkOrange = 0xff8c00,
	DarkOrchid = 0x9932cc,
	DarkRed = 0x8b0000,
	DarkSalmon = 0xe9967a,
	DarkSeaGreen = 0x8fbc8f,
	DarkSlateBlue = 0x483d8b,
	DarkSlateGray = 0x2f4f4f,
	DarkSlateGrey = 0x2f4f4f,
	DarkTurquoise = 0x00ced1,
	DarkViolet = 0x9400d3,
	DeepPink = 0xff1493,
	DeepSkyBlue = 0x00bfff,
	DimGray = 0x696969,
	DimGrey = 0x696969,
	DodgerBlue = 0x1e90ff,
	FireBrick = 0xb22222,
	FloralWhite = 0xfffaf0,
	ForestGreen = 0x228b22,
	Fuchsia = 0xff00ff,
	Gainsboro = 0xdcdcdc,
	GhostWhite = 0xf8f8ff,
	Gold = 0xffd700,
	Goldenrod = 0xdaa520,
	Gray = 0x808080,
	Green = 0x008000,
	GreenYellow = 0xadff2f,
	Grey = 0x808080,
	Honeydew = 0xf0fff0,
	HotPink = 0xff69b4,
	IndianRed = 0xcd5c5c,
	Indigo = 0x4b0082,
	Ivory = 0xfffff0,
	Khaki = 0xf0e68c,
	Lavender = 0xe6e6fa,
	LavenderBlush = 0xfff0f5,
	LawnGreen = 0x7cfc00,
	LemonChiffon = 0xfffacd,
	LightBlue = 0xadd8e6,
	LightCoral = 0xf08080,
	LightCyan = 0xe0ffff,
	LightGoldenrodYellow = 0xfafad2,
	LightGray = 0xd3d3d3,
	LightGreen = 0x90ee90,
	LightGrey = 0xd3d3d3,
	LightPink = 0xffb6c1,
	LightSalmon = 0xffa07a,
	LightSeaGreen = 0x20b2aa,
	LightSkyBlue = 0x87cefa,
	LightSlateGray = 0x778899,
	LightSlateGrey = 0x778899,
	LightSteelBlue = 0xb0c4de,
	LightYellow = 0xffffe0,
	Lime = 0x00ff00,
	LimeGreen = 0x32cd32,
	Linen = 0xfaf0e6,
	Magenta = 0xff00ff,
	Maroon = 0x800000,
	MediumAquamarine = 0x66cdaa,
	MediumBlue = 0x0000cd,
	MediumOrchid = 0xba55d3,
	MediumPurple = 0x9370db,
	MediumSeaGreen = 0x3cb371,
	MediumSlateBlue = 0x7b68ee,
	MediumSpringGreen = 0x00fa9a,
	MediumTurquoise = 0x48d1cc,
	MediumVioletRed = 0xc71585,
	MidnightBlue = 0x191970,
	MintCream = 0xf5fffa,
	MistyRose = 0xffe4e1,
	Moccasin = 0xffe4b5,
	NavajoWhite = 0xffdead,
	Navy = 0x000080,
	OldLace = 0xfdf5e6,
	Olive = 0x808000,
	OliveDrab = 0x6b8e23,
	Orange = 0xffa500,
	OrangeRed = 0xff4500,
	Orchid = 0xda70d6,
	PaleGoldenrod = 0xeee8aa,
	PaleGreen = 0x98fb98,
	PaleTurquoise = 0xafeeee,
	PaleVioletRed = 0xdb7093,
	PapayaWhip = 0xffefd5,
	PeachPuff = 0xffdab9,
	Peru = 0xcd853f,
	Pink = 0xffc0cb,
	Plum = 0xdda0dd,
	PowderBlue = 0xb0e0e6,
	Purple = 0x800080,
	RebeccaPurple = 0x663399,
	Red = 0xff0000,
	RosyBrown = 0xbc8f8f,
	RoyalBlue = 0x4169e1,
	SaddleBrown = 0x8b4513,
	Salmon = 0xfa8072,
	SandyBrown = 0xf4a460,
	SeaGreen = 0x2e8b57,
	SeaShell = 0xfff5ee,
	Sienna = 0xa0522d,
	Silver = 0xc0c0c0,
	SkyBlue = 0x87ceeb,
	SlateBlue = 0x6a5acd,
	SlateGray = 0x708090,
	SlateGrey = 0x708090,
	Snow = 0xfffafa,
	SpringGreen = 0x00ff7f,
	SteelBlue = 0x4682b4,
	Tan = 0xd2b48c,
	Teal = 0x008080,
	Thistle = 0xd8bfd8,
	Tomato = 0xff6347,
	Turquoise = 0x40e0d0,
	Violet = 0xee82ee,
	Wheat = 0xf5deb3,
	White = 0xffffff,
	WhiteSmoke = 0xf5f5f5,
	Yellow = 0xffff00,
	YellowGreen = 0x9acd32,
};

// Return the name of the color.
// For colors with the same RGB value (e.g. Aqua/Cyan, Gray/Grey),
// the first name declared in Color enum class is returned.
CARBON_CORE_API std::string CcpColorToString( Color color );

namespace CcpColor
{
// Decompose a Color into its 8-bit red, green and blue components.
static inline void ToRgb( Color color, int& r, int& g, int& b )
{
	const uint32_t value = static_cast<uint32_t>( color );
	r = static_cast<int>( ( value >> 16 ) & 0xff );
	g = static_cast<int>( ( value >> 8 ) & 0xff );
	b = static_cast<int>( value & 0xff );
}

// Find the squared Euclidean distance between two colors in RGB space.
static inline int64_t GetSquaredDistance( Color lhs, Color rhs )
{
	int r1, g1, b1;
	int r2, g2, b2;
	ToRgb( lhs, r1, g1, b1 );
	ToRgb( rhs, r2, g2, b2 );
	const int64_t dr = r1 - r2;
	const int64_t dg = g1 - g2;
	const int64_t db = b1 - b2;
	return dr * dr + dg * dg + db * db;
}

// All named colors, used as candidates when picking a display color.
// NOTE:
// - Remove a color from this list if you don't want it to be selected
//   when automatically picking a display color for a new CaptureMask entry.
static constexpr Color s_awailableNamedColors[] = {
	//Color::AliceBlue,
	Color::AntiqueWhite,
	Color::Aqua /* == Cyan */,
	Color::Aquamarine,
	//Color::Azure,
	Color::Beige,
	Color::Bisque,
	//Color::Black,
	Color::BlanchedAlmond,
	Color::Blue,
	Color::BlueViolet,
	Color::Brown,
	Color::BurlyWood,
	Color::CadetBlue,
	Color::Chartreuse,
	Color::Chocolate,
	Color::Coral,
	Color::CornflowerBlue,
	Color::Cornsilk,
	Color::Crimson,
	Color::DarkBlue,
	Color::DarkCyan,
	Color::DarkGoldenrod,
	//Color::DarkGray /* == DarkGrey */,
	Color::DarkGreen,
	Color::DarkKhaki,
	Color::DarkMagenta,
	Color::DarkOliveGreen,
	Color::DarkOrange,
	Color::DarkOrchid,
	Color::DarkRed,
	Color::DarkSalmon,
	Color::DarkSeaGreen,
	Color::DarkSlateBlue,
	Color::DarkSlateGray /* == DarkSlateGrey */,
	Color::DarkTurquoise,
	Color::DarkViolet,
	Color::DeepPink,
	Color::DeepSkyBlue,
	//Color::DimGray /* == DimGrey */,
	Color::DodgerBlue,
	Color::FireBrick,
	//Color::FloralWhite,
	Color::ForestGreen,
	Color::Fuchsia /* == Magenta */,
	Color::Gainsboro,
	//Color::GhostWhite,
	Color::Gold,
	Color::Goldenrod,
	//Color::Gray /* == Grey */,
	Color::Green,
	Color::GreenYellow,
	Color::Honeydew,
	Color::HotPink,
	Color::IndianRed,
	Color::Indigo,
	//Color::Ivory,
	Color::Khaki,
	Color::Lavender,
	Color::LavenderBlush,
	Color::LawnGreen,
	Color::LemonChiffon,
	Color::LightBlue,
	Color::LightCoral,
	Color::LightCyan,
	Color::LightGoldenrodYellow,
	Color::LightGray /* == LightGrey */,
	Color::LightGreen,
	Color::LightPink,
	Color::LightSalmon,
	Color::LightSeaGreen,
	Color::LightSkyBlue,
	Color::LightSlateGray /* == LightSlateGrey */,
	Color::LightSteelBlue,
	Color::LightYellow,
	Color::Lime,
	Color::LimeGreen,
	Color::Linen,
	Color::Maroon,
	Color::MediumAquamarine,
	Color::MediumBlue,
	Color::MediumOrchid,
	Color::MediumPurple,
	Color::MediumSeaGreen,
	Color::MediumSlateBlue,
	Color::MediumSpringGreen,
	Color::MediumTurquoise,
	Color::MediumVioletRed,
	Color::MidnightBlue,
	//Color::MintCream,
	Color::MistyRose,
	Color::Moccasin,
	Color::NavajoWhite,
	Color::Navy,
	Color::OldLace,
	Color::Olive,
	Color::OliveDrab,
	Color::Orange,
	Color::OrangeRed,
	Color::Orchid,
	Color::PaleGoldenrod,
	Color::PaleGreen,
	Color::PaleTurquoise,
	Color::PaleVioletRed,
	Color::PapayaWhip,
	Color::PeachPuff,
	Color::Peru,
	Color::Pink,
	Color::Plum,
	Color::PowderBlue,
	Color::Purple,
	Color::RebeccaPurple,
	Color::Red,
	Color::RosyBrown,
	Color::RoyalBlue,
	Color::SaddleBrown,
	Color::Salmon,
	Color::SandyBrown,
	Color::SeaGreen,
	//Color::SeaShell,
	Color::Sienna,
	Color::Silver,
	Color::SkyBlue,
	Color::SlateBlue,
	Color::SlateGray /* == SlateGrey */,
	//Color::Snow,
	Color::SpringGreen,
	Color::SteelBlue,
	Color::Tan,
	Color::Teal,
	Color::Thistle,
	Color::Tomato,
	Color::Turquoise,
	Color::Violet,
	Color::Wheat,
	//Color::White,
	//Color::WhiteSmoke,
	Color::Yellow,
	Color::YellowGreen,
};

// Pick a display color from the s_awailableNamedColors list above
// that are reasonably distinct from incoming existingColors.
// Candidates are scored by their distance to existingColors and
// the color furthest away wins.
static inline Color PickDistinctColor( const std::vector<Color>& existingColors )
{
	Color bestColor = Color::SteelBlue;
	int64_t bestScore = -1;
	for( const Color candidate : s_awailableNamedColors )
	{
		int64_t minDistance = INT64_MAX;
		for( const Color existing : existingColors )
		{
			const int64_t dist = GetSquaredDistance( candidate, existing );
			if( dist < minDistance )
			{
				minDistance = dist;
			}
		}

		if( minDistance > bestScore )
		{
			bestScore = minDistance;
			bestColor = candidate;
		}
	}
	return bestColor;
}
}

#endif
