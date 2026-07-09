// Copyright © 2026 Fenris Creations.

#include "include/CcpColorConstants.h"

std::string CcpColorToString( Color color )
{
	// For colors sharing the same RGB value (Aqua/Cyan, Fuchsia/Magenta, and the
	// various *Gray/*Grey pairs), the first name declared in the enum wins.
	switch( color )
	{
		case Color::AliceBlue: return "AliceBlue";
		case Color::AntiqueWhite: return "AntiqueWhite";
		case Color::Aqua: return "Aqua"; // == Cyan
		case Color::Aquamarine: return "Aquamarine";
		case Color::Azure: return "Azure";
		case Color::Beige: return "Beige";
		case Color::Bisque: return "Bisque";
		case Color::Black: return "Black";
		case Color::BlanchedAlmond: return "BlanchedAlmond";
		case Color::Blue: return "Blue";
		case Color::BlueViolet: return "BlueViolet";
		case Color::Brown: return "Brown";
		case Color::BurlyWood: return "BurlyWood";
		case Color::CadetBlue: return "CadetBlue";
		case Color::Chartreuse: return "Chartreuse";
		case Color::Chocolate: return "Chocolate";
		case Color::Coral: return "Coral";
		case Color::CornflowerBlue: return "CornflowerBlue";
		case Color::Cornsilk: return "Cornsilk";
		case Color::Crimson: return "Crimson";
		case Color::DarkBlue: return "DarkBlue";
		case Color::DarkCyan: return "DarkCyan";
		case Color::DarkGoldenrod: return "DarkGoldenrod";
		case Color::DarkGray: return "DarkGray"; // == DarkGrey
		case Color::DarkGreen: return "DarkGreen";
		case Color::DarkKhaki: return "DarkKhaki";
		case Color::DarkMagenta: return "DarkMagenta";
		case Color::DarkOliveGreen: return "DarkOliveGreen";
		case Color::DarkOrange: return "DarkOrange";
		case Color::DarkOrchid: return "DarkOrchid";
		case Color::DarkRed: return "DarkRed";
		case Color::DarkSalmon: return "DarkSalmon";
		case Color::DarkSeaGreen: return "DarkSeaGreen";
		case Color::DarkSlateBlue: return "DarkSlateBlue";
		case Color::DarkSlateGray: return "DarkSlateGray"; // == DarkSlateGrey
		case Color::DarkTurquoise: return "DarkTurquoise";
		case Color::DarkViolet: return "DarkViolet";
		case Color::DeepPink: return "DeepPink";
		case Color::DeepSkyBlue: return "DeepSkyBlue";
		case Color::DimGray: return "DimGray"; // == DimGrey
		case Color::DodgerBlue: return "DodgerBlue";
		case Color::FireBrick: return "FireBrick";
		case Color::FloralWhite: return "FloralWhite";
		case Color::ForestGreen: return "ForestGreen";
		case Color::Fuchsia: return "Fuchsia"; // == Magenta
		case Color::Gainsboro: return "Gainsboro";
		case Color::GhostWhite: return "GhostWhite";
		case Color::Gold: return "Gold";
		case Color::Goldenrod: return "Goldenrod";
		case Color::Gray: return "Gray"; // == Grey
		case Color::Green: return "Green";
		case Color::GreenYellow: return "GreenYellow";
		case Color::Honeydew: return "Honeydew";
		case Color::HotPink: return "HotPink";
		case Color::IndianRed: return "IndianRed";
		case Color::Indigo: return "Indigo";
		case Color::Ivory: return "Ivory";
		case Color::Khaki: return "Khaki";
		case Color::Lavender: return "Lavender";
		case Color::LavenderBlush: return "LavenderBlush";
		case Color::LawnGreen: return "LawnGreen";
		case Color::LemonChiffon: return "LemonChiffon";
		case Color::LightBlue: return "LightBlue";
		case Color::LightCoral: return "LightCoral";
		case Color::LightCyan: return "LightCyan";
		case Color::LightGoldenrodYellow: return "LightGoldenrodYellow";
		case Color::LightGray: return "LightGray"; // == LightGrey
		case Color::LightGreen: return "LightGreen";
		case Color::LightPink: return "LightPink";
		case Color::LightSalmon: return "LightSalmon";
		case Color::LightSeaGreen: return "LightSeaGreen";
		case Color::LightSkyBlue: return "LightSkyBlue";
		case Color::LightSlateGray: return "LightSlateGray"; // == LightSlateGrey
		case Color::LightSteelBlue: return "LightSteelBlue";
		case Color::LightYellow: return "LightYellow";
		case Color::Lime: return "Lime";
		case Color::LimeGreen: return "LimeGreen";
		case Color::Linen: return "Linen";
		case Color::Maroon: return "Maroon";
		case Color::MediumAquamarine: return "MediumAquamarine";
		case Color::MediumBlue: return "MediumBlue";
		case Color::MediumOrchid: return "MediumOrchid";
		case Color::MediumPurple: return "MediumPurple";
		case Color::MediumSeaGreen: return "MediumSeaGreen";
		case Color::MediumSlateBlue: return "MediumSlateBlue";
		case Color::MediumSpringGreen: return "MediumSpringGreen";
		case Color::MediumTurquoise: return "MediumTurquoise";
		case Color::MediumVioletRed: return "MediumVioletRed";
		case Color::MidnightBlue: return "MidnightBlue";
		case Color::MintCream: return "MintCream";
		case Color::MistyRose: return "MistyRose";
		case Color::Moccasin: return "Moccasin";
		case Color::NavajoWhite: return "NavajoWhite";
		case Color::Navy: return "Navy";
		case Color::OldLace: return "OldLace";
		case Color::Olive: return "Olive";
		case Color::OliveDrab: return "OliveDrab";
		case Color::Orange: return "Orange";
		case Color::OrangeRed: return "OrangeRed";
		case Color::Orchid: return "Orchid";
		case Color::PaleGoldenrod: return "PaleGoldenrod";
		case Color::PaleGreen: return "PaleGreen";
		case Color::PaleTurquoise: return "PaleTurquoise";
		case Color::PaleVioletRed: return "PaleVioletRed";
		case Color::PapayaWhip: return "PapayaWhip";
		case Color::PeachPuff: return "PeachPuff";
		case Color::Peru: return "Peru";
		case Color::Pink: return "Pink";
		case Color::Plum: return "Plum";
		case Color::PowderBlue: return "PowderBlue";
		case Color::Purple: return "Purple";
		case Color::RebeccaPurple: return "RebeccaPurple";
		case Color::Red: return "Red";
		case Color::RosyBrown: return "RosyBrown";
		case Color::RoyalBlue: return "RoyalBlue";
		case Color::SaddleBrown: return "SaddleBrown";
		case Color::Salmon: return "Salmon";
		case Color::SandyBrown: return "SandyBrown";
		case Color::SeaGreen: return "SeaGreen";
		case Color::SeaShell: return "SeaShell";
		case Color::Sienna: return "Sienna";
		case Color::Silver: return "Silver";
		case Color::SkyBlue: return "SkyBlue";
		case Color::SlateBlue: return "SlateBlue";
		case Color::SlateGray: return "SlateGray"; // == SlateGrey
		case Color::Snow: return "Snow";
		case Color::SpringGreen: return "SpringGreen";
		case Color::SteelBlue: return "SteelBlue";
		case Color::Tan: return "Tan";
		case Color::Teal: return "Teal";
		case Color::Thistle: return "Thistle";
		case Color::Tomato: return "Tomato";
		case Color::Turquoise: return "Turquoise";
		case Color::Violet: return "Violet";
		case Color::Wheat: return "Wheat";
		case Color::White: return "White";
		case Color::WhiteSmoke: return "WhiteSmoke";
		case Color::Yellow: return "Yellow";
		case Color::YellowGreen: return "YellowGreen";
		default: return "ColorNotFound";
	}
}
