// Copyright © 2020 CCP ehf.


#include "CcpDefines.h"
#include "CcpMacros.h"

const char* CcpGetPlatformToolset()
{
	return CCP_STRINGIZE( PLATFORM_TOOLSET );
}

unsigned CcpGetProcessBitCount()
{
	return sizeof( size_t ) * 8;
}