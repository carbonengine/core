// Copyright © 2026 Fenris Creations.

#include "include/CcpColorConstants.h"

std::string CcpColorToString( CcpColor color )
{
	// For colors sharing the same RGB value (Aqua/Cyan, Fuchsia/Magenta, and the
	// various *Gray/*Grey pairs), the first name declared in the enum wins.
	switch( color )
	{
		case CcpColor::AliceBlue: return "AliceBlue";
		case CcpColor::AntiqueWhite: return "AntiqueWhite";
		case CcpColor::Aqua: return "Aqua"; // == Cyan
		case CcpColor::Aquamarine: return "Aquamarine";
		case CcpColor::Azure: return "Azure";
		case CcpColor::Beige: return "Beige";
		case CcpColor::Bisque: return "Bisque";
		case CcpColor::Black: return "Black";
		case CcpColor::BlanchedAlmond: return "BlanchedAlmond";
		case CcpColor::Blue: return "Blue";
		case CcpColor::BlueViolet: return "BlueViolet";
		case CcpColor::Brown: return "Brown";
		case CcpColor::BurlyWood: return "BurlyWood";
		case CcpColor::CadetBlue: return "CadetBlue";
		case CcpColor::Chartreuse: return "Chartreuse";
		case CcpColor::Chocolate: return "Chocolate";
		case CcpColor::Coral: return "Coral";
		case CcpColor::CornflowerBlue: return "CornflowerBlue";
		case CcpColor::Cornsilk: return "Cornsilk";
		case CcpColor::Crimson: return "Crimson";
		case CcpColor::DarkBlue: return "DarkBlue";
		case CcpColor::DarkCyan: return "DarkCyan";
		case CcpColor::DarkGoldenrod: return "DarkGoldenrod";
		case CcpColor::DarkGray: return "DarkGray"; // == DarkGrey
		case CcpColor::DarkGreen: return "DarkGreen";
		case CcpColor::DarkKhaki: return "DarkKhaki";
		case CcpColor::DarkMagenta: return "DarkMagenta";
		case CcpColor::DarkOliveGreen: return "DarkOliveGreen";
		case CcpColor::DarkOrange: return "DarkOrange";
		case CcpColor::DarkOrchid: return "DarkOrchid";
		case CcpColor::DarkRed: return "DarkRed";
		case CcpColor::DarkSalmon: return "DarkSalmon";
		case CcpColor::DarkSeaGreen: return "DarkSeaGreen";
		case CcpColor::DarkSlateBlue: return "DarkSlateBlue";
		case CcpColor::DarkSlateGray: return "DarkSlateGray"; // == DarkSlateGrey
		case CcpColor::DarkTurquoise: return "DarkTurquoise";
		case CcpColor::DarkViolet: return "DarkViolet";
		case CcpColor::DeepPink: return "DeepPink";
		case CcpColor::DeepSkyBlue: return "DeepSkyBlue";
		case CcpColor::DimGray: return "DimGray"; // == DimGrey
		case CcpColor::DodgerBlue: return "DodgerBlue";
		case CcpColor::FireBrick: return "FireBrick";
		case CcpColor::FloralWhite: return "FloralWhite";
		case CcpColor::ForestGreen: return "ForestGreen";
		case CcpColor::Fuchsia: return "Fuchsia"; // == Magenta
		case CcpColor::Gainsboro: return "Gainsboro";
		case CcpColor::GhostWhite: return "GhostWhite";
		case CcpColor::Gold: return "Gold";
		case CcpColor::Goldenrod: return "Goldenrod";
		case CcpColor::Gray: return "Gray"; // == Grey
		case CcpColor::Green: return "Green";
		case CcpColor::GreenYellow: return "GreenYellow";
		case CcpColor::Honeydew: return "Honeydew";
		case CcpColor::HotPink: return "HotPink";
		case CcpColor::IndianRed: return "IndianRed";
		case CcpColor::Indigo: return "Indigo";
		case CcpColor::Ivory: return "Ivory";
		case CcpColor::Khaki: return "Khaki";
		case CcpColor::Lavender: return "Lavender";
		case CcpColor::LavenderBlush: return "LavenderBlush";
		case CcpColor::LawnGreen: return "LawnGreen";
		case CcpColor::LemonChiffon: return "LemonChiffon";
		case CcpColor::LightBlue: return "LightBlue";
		case CcpColor::LightCoral: return "LightCoral";
		case CcpColor::LightCyan: return "LightCyan";
		case CcpColor::LightGoldenrodYellow: return "LightGoldenrodYellow";
		case CcpColor::LightGray: return "LightGray"; // == LightGrey
		case CcpColor::LightGreen: return "LightGreen";
		case CcpColor::LightPink: return "LightPink";
		case CcpColor::LightSalmon: return "LightSalmon";
		case CcpColor::LightSeaGreen: return "LightSeaGreen";
		case CcpColor::LightSkyBlue: return "LightSkyBlue";
		case CcpColor::LightSlateGray: return "LightSlateGray"; // == LightSlateGrey
		case CcpColor::LightSteelBlue: return "LightSteelBlue";
		case CcpColor::LightYellow: return "LightYellow";
		case CcpColor::Lime: return "Lime";
		case CcpColor::LimeGreen: return "LimeGreen";
		case CcpColor::Linen: return "Linen";
		case CcpColor::Maroon: return "Maroon";
		case CcpColor::MediumAquamarine: return "MediumAquamarine";
		case CcpColor::MediumBlue: return "MediumBlue";
		case CcpColor::MediumOrchid: return "MediumOrchid";
		case CcpColor::MediumPurple: return "MediumPurple";
		case CcpColor::MediumSeaGreen: return "MediumSeaGreen";
		case CcpColor::MediumSlateBlue: return "MediumSlateBlue";
		case CcpColor::MediumSpringGreen: return "MediumSpringGreen";
		case CcpColor::MediumTurquoise: return "MediumTurquoise";
		case CcpColor::MediumVioletRed: return "MediumVioletRed";
		case CcpColor::MidnightBlue: return "MidnightBlue";
		case CcpColor::MintCream: return "MintCream";
		case CcpColor::MistyRose: return "MistyRose";
		case CcpColor::Moccasin: return "Moccasin";
		case CcpColor::NavajoWhite: return "NavajoWhite";
		case CcpColor::Navy: return "Navy";
		case CcpColor::OldLace: return "OldLace";
		case CcpColor::Olive: return "Olive";
		case CcpColor::OliveDrab: return "OliveDrab";
		case CcpColor::Orange: return "Orange";
		case CcpColor::OrangeRed: return "OrangeRed";
		case CcpColor::Orchid: return "Orchid";
		case CcpColor::PaleGoldenrod: return "PaleGoldenrod";
		case CcpColor::PaleGreen: return "PaleGreen";
		case CcpColor::PaleTurquoise: return "PaleTurquoise";
		case CcpColor::PaleVioletRed: return "PaleVioletRed";
		case CcpColor::PapayaWhip: return "PapayaWhip";
		case CcpColor::PeachPuff: return "PeachPuff";
		case CcpColor::Peru: return "Peru";
		case CcpColor::Pink: return "Pink";
		case CcpColor::Plum: return "Plum";
		case CcpColor::PowderBlue: return "PowderBlue";
		case CcpColor::Purple: return "Purple";
		case CcpColor::RebeccaPurple: return "RebeccaPurple";
		case CcpColor::Red: return "Red";
		case CcpColor::RosyBrown: return "RosyBrown";
		case CcpColor::RoyalBlue: return "RoyalBlue";
		case CcpColor::SaddleBrown: return "SaddleBrown";
		case CcpColor::Salmon: return "Salmon";
		case CcpColor::SandyBrown: return "SandyBrown";
		case CcpColor::SeaGreen: return "SeaGreen";
		case CcpColor::SeaShell: return "SeaShell";
		case CcpColor::Sienna: return "Sienna";
		case CcpColor::Silver: return "Silver";
		case CcpColor::SkyBlue: return "SkyBlue";
		case CcpColor::SlateBlue: return "SlateBlue";
		case CcpColor::SlateGray: return "SlateGray"; // == SlateGrey
		case CcpColor::Snow: return "Snow";
		case CcpColor::SpringGreen: return "SpringGreen";
		case CcpColor::SteelBlue: return "SteelBlue";
		case CcpColor::Tan: return "Tan";
		case CcpColor::Teal: return "Teal";
		case CcpColor::Thistle: return "Thistle";
		case CcpColor::Tomato: return "Tomato";
		case CcpColor::Turquoise: return "Turquoise";
		case CcpColor::Violet: return "Violet";
		case CcpColor::Wheat: return "Wheat";
		case CcpColor::White: return "White";
		case CcpColor::WhiteSmoke: return "WhiteSmoke";
		case CcpColor::Yellow: return "Yellow";
		case CcpColor::YellowGreen: return "YellowGreen";
		default: return "ColorNotFound";
	}
}
