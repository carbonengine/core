// Copyright © 2025 CCP ehf.

#include "gtest/gtest.h"
#include "StringConversions.h"
#include "blns.h"

class StringConversionTest : public testing::TestWithParam<std::string_view>{};

TEST_P(StringConversionTest, ConvertingStringsTwiceLeavesThemUnchanged)
{
	const auto input = GetParam();
	auto expected = input;
#if __APPLE__
	// macOS conversion routines do not preserve byte order marks, so adjust our expectations
	if (input.size() >= 3 && input[0] == '\xef' && input[1] == '\xbb' && input[2] == '\xbf')
	{
		expected.remove_prefix(3);
	}
#endif

	const auto wideConverted = UTF8ToWide(input.data());
	const auto utf8Converted = WideToUTF8(wideConverted);

	EXPECT_EQ(expected.data(), utf8Converted);
}

INSTANTIATE_TEST_SUITE_P(StringConversions, StringConversionTest, testing::ValuesIn(s_BigListOfNaughtyStrings));
