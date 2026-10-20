// Copyright © 2013 CCP ehf.


#ifndef CCPMACROS_H
#define CCPMACROS_H

#pragma once
#if defined(_MSC_VER)
	#ifdef BLUEBUILD_STATIC
		#define BLUEIMPORT
	#else
		#ifdef BLUEBUILD
			#define BLUEIMPORT __declspec(dllexport)
		#else
			#define BLUEIMPORT __declspec(dllimport)
		#endif
	#endif
#elif defined(__APPLE__)
#ifdef BLUEBUILD_STATIC
#define BLUEIMPORT
#else
#ifdef BLUEBUILD
#define BLUEIMPORT __attribute((visibility("default")))
#else
#define BLUEIMPORT
#endif
#endif
#else
	#define BLUEIMPORT
#endif

#define CCP_CONCATENATE_DIRECT(s1, s2) s1##s2
#define CCP_CONCATENATE(s1, s2) CCP_CONCATENATE_DIRECT(s1, s2)

#define CCP_ANONYMOUS_VARIABLE(str) CCP_CONCATENATE(str, __LINE__)

// Return x aligned to y bytes - useful for aligning memory allocations to 16 bytes boundaries, for example
#define CCP_ALIGN( x, y ) (x + (y-1)) & ~(y-1)

//////////////////////////////////////////////////////////////////////////////////////////////////
///
/// Stringize - the helper is needed to expand a macro before the actual stringizing occurs.
///

#define CCP_STRINGIZE( x ) CCP_STRINGIZE_HELPER( x )
#define CCP_STRINGIZE_HELPER( s ) #s

// A handy macro to silence unused variable/parameter warnings
#define CCP_UNUSED( x ) ((void)(x))

// Macro for size_t format for printf function family. It different in standard C++ and in MSVC
#if defined(_MSC_VER)
#define CCP_SIZET_FORMAT "Iu"
#else
#define CCP_SIZET_FORMAT "zu"
#endif

// Macro for int64_t format for printf function family. It different in standard C++ and in MSVC
#if defined(_MSC_VER)
#define CCP_INT64_FORMAT "I64d"
#else
#define CCP_INT64_FORMAT "lld"
#endif


#endif
