// Copyright © 2020 CCP ehf.


#include "include/CcpDefines.h"
#include "include/CcpMacros.h"

#include <cstddef>

const char* CcpGetPlatformToolset()
{
	return CCP_STRINGIZE( PLATFORM_TOOLSET );
}

unsigned CcpGetProcessBitCount()
{
	return sizeof( size_t ) * 8;
}