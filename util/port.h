/**
Copyright 2017 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS-IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/

#ifndef UTIL_PORT_H_
#define UTIL_PORT_H_

#include "base/integral_types.h"

// The following guarantees declaration of the byte swap functions.
#if defined(ION_PLATFORM_WINDOWS)
#include <stdlib.h>  // NOLINT(build/include)
#elif defined(ION_PLATFORM_MAC) || defined(ION_PLATFORM_IOS)
// Mac OS X / Darwin features.
#include <libkern/OSByteOrder.h>
#endif

// Some C libraries provide macros for these symbols, e.g. in byteswap.h, and
// the macros interfere with defining the inline function with that name.

#ifdef bswap_16
#undef bswap_16
#endif

#ifdef bswap_32
#undef bswap_32
#endif

#ifdef bswap_64
#undef bswap_64
#endif

#if defined(ION_PLATFORM_WINDOWS)

#define bswap_16(x) _byteswap_ushort(x)
#define bswap_32(x) _byteswap_ulong(x)
#define bswap_64(x) _byteswap_uint64(x)

#elif defined(ION_PLATFORM_MAC) || defined(ION_PLATFORM_IOS)

#define bswap_16(x) OSSwapInt16(x)
#define bswap_32(x) OSSwapInt32(x)
#define bswap_64(x) OSSwapInt64(x)

#else

static inline uint16 ByteSwap16(uint16 x) {
  return static_cast<uint16>(((x & 0xFF) << 8) | ((x & 0xFF00) >> 8));
}
static inline uint32 ByteSwap32(uint32 x) {
  return (((x & 0xFF) << 24) |
          ((x & 0xFF00) << 8) |
          ((x & 0xFF0000) >> 8) |
          ((x & 0xFF000000) >> 24));
}
static inline uint64 ByteSwap64(uint64 x) {
  return (((x & GG_ULONGLONG(0xFF)) << 56) |
          ((x & GG_ULONGLONG(0xFF00)) << 40) |
          ((x & GG_ULONGLONG(0xFF0000)) << 24) |
          ((x & GG_ULONGLONG(0xFF000000)) << 8) |
          ((x & GG_ULONGLONG(0xFF00000000)) >> 8) |
          ((x & GG_ULONGLONG(0xFF0000000000)) >> 24) |
          ((x & GG_ULONGLONG(0xFF000000000000)) >> 40) |
          ((x & GG_ULONGLONG(0xFF00000000000000)) >> 56));
}

#define bswap_16(x) ByteSwap16(x)
#define bswap_32(x) ByteSwap32(x)
#define bswap_64(x) ByteSwap64(x)

#endif

#endif  // UTIL_PORT_H_
