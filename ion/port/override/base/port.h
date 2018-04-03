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

#ifndef ION_PORT_OVERRIDE_BASE_PORT_H_
#define ION_PORT_OVERRIDE_BASE_PORT_H_

// Adapts Google's standard base/port.h for Ion.

#if defined(ION_PLATFORM_ANDROID) || defined(ION_GOOGLE_INTERNAL)
#include <cerrno>  // For ENOMEM.
#endif

// Map required STL types and functions into the global namespace.
#include <algorithm>
#include <bitset>
#include <cmath>
#include <iostream>  // NOLINT
#include <set>
#include <string>
#include <vector>
using std::bitset;
using std::endl;
using std::max;
using std::min;
using std::set;
#if !defined(ION_GOOGLE_INTERNAL)
using std::string;
#endif
#if defined(ION_PLATFORM_ANDROID) || defined(ION_GOOGLE_INTERNAL)
#define IS_LITTLE_ENDIAN
// The Android definition of isinf changes not by compiler, but based
// on the level of the ABI you're targeting.  ABI9 provides no definition
// of isinf in math.h.  ABI20 (as used in the 64-bit targets) provides one
// in math.h (int isinf(double)) that conflicts with the one in cmath
// (int  (isinf)(double) __NDK_FPABI_MATH__ __pure2;) so we don't shove
// shove cmath's isinf into std for the newer ABIs.
#if __ANDROID_API__ <= 9
using std::isinf;
#endif
#elif defined(ION_PLATFORM_NACL)
using std::isinf;
using std::isnan;
#endif
using std::swap;
using std::vector;

#include "ion/port/override/absl/base/port.h"
#include "util/port.h"

// It seems that clang does not define nullptr_t where we expects it.
#if defined(LANG_CXX11) && defined(__clang__)
namespace std {
  typedef decltype(nullptr) nullptr_t;
}  // namespace std
#endif

#if defined(ION_PLATFORM_WINDOWS)
// Google's base/port.h redefines vsnprintf and snprintf to specific Google
// versions that we do not want to use, so we define them back here.
#undef snprintf
#undef vsnprintf
#define snprintf _snprintf
#define vsnprintf _vsnprintf

// Windows headers define lots of macros for compatibility with old code and for
// abstracting away "ANSI" and "wide" versions of APIs with strings.  Some of
// these macros conflict with our method and variable names.  In general, we
// don't need the macros, so we'll undef the most problematic ones here.
#undef CreateDirectory
#undef FAR
#undef far
#undef GetCurrentDirectory
#undef GetCurrentTime
#undef GetMessage
#undef max
#undef min
#undef NEAR
#undef near
#undef OPTIONAL
#undef RemoveDirectory
#undef RGB
#undef SendMessage
#endif  // ION_PLATFORM_WINDOWS

#endif  // ION_PORT_OVERRIDE_BASE_PORT_H_
