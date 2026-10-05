// Copyright © 2026 CCP ehf.

#include <CcpCrash.h>

class CcpCrashDeathTest : public ::testing::Test {};

TEST_F( CcpCrashDeathTest, CrashOnPurpose )
{
	EXPECT_DEATH( CcpCrashOnPurpose(), "" );
}
