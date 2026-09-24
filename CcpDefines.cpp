// Copyright © 2020 CCP ehf.


#include "CcpDefines.h"
#include "CcpMacros.h"

#include <cstddef>

const char* CcpGetPlatformToolset()
{
	return CCP_STRINGIZE( PLATFORM_TOOLSET );
}

unsigned CcpGetProcessBitCount()
{
	return sizeof( std::size_t ) * 8;
}