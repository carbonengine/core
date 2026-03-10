// Copyright © 2013 CCP ehf.
//
// Wrapper for std::atomic. Since at the time of writing not all platforms we'd
// like to support provide std::atomic we 

#pragma once
#ifndef CcpAtomic_h
#define CcpAtomic_h

#ifdef _MSC_VER
	#if _MSC_VER >= 1700
		#define CCP_HAS_ATOMIC 1
	#endif
#endif

#ifdef __GNUG__
	#if __GNUC__ == 4 && __GNUC_MINOR__ >= 7
		#define CCP_HAS_ATOMIC 1
	#endif
#endif

#ifdef __clang__
#define CCP_HAS_ATOMIC 1
#endif

#if CCP_HAS_ATOMIC

#include <atomic>
#define CcpAtomic std::atomic

#endif

#endif // CcpAtomic_h