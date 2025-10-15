// Copyright © 2013 CCP ehf.

#if defined(_MSC_VER) && (_MSC_VER <= 1400)

typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef char int8_t;
typedef unsigned char uint8_t;

#define nullptr NULL

#else

#include <stdint.h>

#endif