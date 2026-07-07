// Copyright © 2013 CCP ehf.
//
// Wrapper for std::atomic. Since at the time of writing not all platforms we'd
// like to support provide std::atomic we 

#pragma once
#ifndef CcpAtomic_h
#define CcpAtomic_h

#include <atomic>
#define CcpAtomic std::atomic

#endif // CcpAtomic_h