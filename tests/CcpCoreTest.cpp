// Copyright © 2025 CCP ehf.

#include "gtest/gtest.h"

const char* g_moduleName = "CcpCoreTest";

int main( int argc, char **argv )
{
	::testing::InitGoogleTest( &argc, argv );
	return RUN_ALL_TESTS();
}
