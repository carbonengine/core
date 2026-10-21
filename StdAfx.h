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
#elif __has_include(<unistd.h>)
	#include <unistd.h>
#else
	#error "Unsupported platform"
#endif

#ifdef _MSC_VER
	#include <atlbase.h>
#endif

#include <vector>
#include <algorithm>

#include <CcpLog.h>
