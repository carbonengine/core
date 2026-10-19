// Copyright © 2013 CCP ehf.

#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <stddef.h>
#include <cstdint>
#include <climits>
#include <cmath>
#include <atomic>
#include <ctime>

#ifdef _WIN32
	#include <windows.h>
#else
	#if (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__))) // UNIX
		#include <unistd.h>
	#endif
#endif

#ifdef _MSC_VER
	#include <atlbase.h>
#endif

#include <vector>
#include <algorithm>

#include <CcpLog.h>
