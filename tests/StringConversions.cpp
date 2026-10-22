// Copyright © 2025 CCP ehf.

#include "gtest/gtest.h"
#include "StringConversions.h"
#include "blns.h"

class StringConversionTest : public testing::TestWithParam<std::string_view>{};

TEST_P(StringConversionTest, ConvertingStringsTwiceLeavesThemUnchanged)
{
	const auto string = GetParam();

	const auto wideConverted = UTF8ToWide(string.data());
	const auto utf8Converted = WideToUTF8(wideConverted);

	EXPECT_EQ(string.data(), utf8Converted);
}

INSTANTIATE_TEST_SUITE_P(StringConversions, StringConversionTest, testing::ValuesIn(s_BigListOfNaughtyStrings));
