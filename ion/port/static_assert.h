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

#ifndef ION_PORT_STATIC_ASSERT_H_
#define ION_PORT_STATIC_ASSERT_H_

// ION_PORT_STATIC_ASSERT() is the platform-dependent implementation of the
// ION_STATIC_ASSERT() macro in ion/base/static_assert.h; see that file for
// usage details.

// -----------------------------------------------------------------------------
// Use native support if possible.
//
// Note: There is a built-in _Static_assert() macro available in gcc starting
// with version 4.6, but it is NOT available in g++.
#if (LANG_CXX11 || _MSC_VER >= 1600) && !defined(ION_PLATFORM_NACL)
#define ION_PORT_STATIC_ASSERT(expr, message) static_assert(expr, message)

// -----------------------------------------------------------------------------
// Otherwise, this macro does a semi-reasonable job.
#else

// Some necessary internal goo to get __LINE__ into the variable name.
#define ION__STATIC_ASSERT_CAT0(a, b) a ## b
#define ION__STATIC_ASSERT_CAT1(a, b) ION__STATIC_ASSERT_CAT0(a, b)

// Public macro that causes a compile-time error if expr is false. If we are
// allowed to use c++11.  G++ >= 4.8, in the absence of
// `-Wno-unused-local-typedefs` (which is negated by -Wall) will fuss about
// the typedefs generated here.  Tag them as unused.

#if __GNUC__
# define ION_ATTRIBUTE_UNUSED __attribute__((unused))
#else
# define ION_ATTRIBUTE_UNUSED
#endif

#define ION_PORT_STATIC_ASSERT(expr, message)                                  \
  typedef int ION__STATIC_ASSERT_CAT1(static_assert_failed_at_line, __LINE__)[ \
      static_cast<bool>(expr) ? 1 : -1] ION_ATTRIBUTE_UNUSED

// -----------------------------------------------------------------------------
#endif

#endif  // ION_PORT_STATIC_ASSERT_H_
