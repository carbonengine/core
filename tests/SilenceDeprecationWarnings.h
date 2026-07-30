// Copyright © 2026 CCP ehf.

// Helper utility for tests that need to cover deprecated functionality
#pragma once
#ifndef SilenceDeprecationWarnings_H
#define SilenceDeprecationWarnings_H

#if defined(__clang__)
	#define CCP_DISABLE_DEPRECATED_BEGIN \
		_Pragma("clang diagnostic push") \
		_Pragma("clang diagnostic ignored \"-Wdeprecated-declarations\"")
	#define CCP_DISABLE_DEPRECATED_END \
		_Pragma("clang diagnostic pop")
#elif defined(__GNUC__)
	#define CCP_DISABLE_DEPRECATED_BEGIN \
		_Pragma("GCC diagnostic push") \
		_Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
	#define CCP_DISABLE_DEPRECATED_END \
		_Pragma("GCC diagnostic pop")
#elif defined(_MSC_VER)
	#define CCP_DISABLE_DEPRECATED_BEGIN \
		__pragma(warning(push)) \
		__pragma(warning(disable: 4996))
	#define CCP_DISABLE_DEPRECATED_END \
		__pragma(warning(pop))
#else
	#define CCP_DISABLE_DEPRECATED_BEGIN
	#define CCP_DISABLE_DEPRECATED_END
#endif

#endif
