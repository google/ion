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

#ifndef ION_PORT_MACROS_H_
#define ION_PORT_MACROS_H_

// ION_DISALLOW_ASSIGN_ONLY is specifically for working around a bug where a
// copy only type cannot be used in std::vector due to gcc bug 49836.
// It should be the last thing in a class declaration.
// The defined operator must not actually be called.
// 
// libstdc++ 4.6 or earlier.
#if (defined(ION_PLATFORM_NACL) && !defined(ION_PLATFORM_PNACL)) || \
    (defined(ION_PLATFORM_LINUX) && !defined(ION_GOOGLE_INTERNAL)) || \
    defined(ION_PLATFORM_QNX)
#define ION_DISALLOW_ASSIGN_ONLY(TypeName) \
 public: \
  TypeName& operator=(const TypeName&) { return *this; } \
 private:  // semicolon ends up here
#elif LANG_CXX11
#define ION_DISALLOW_ASSIGN_ONLY(TypeName) \
  void operator=(const TypeName&) = delete
#else
#define ION_DISALLOW_ASSIGN_ONLY(TypeName) \
  void operator=(const TypeName&)
#endif

// Make ION_PRETTY_FUNCTION available on all platforms.
#if defined(_MSC_VER)
#define ION_PRETTY_FUNCTION __FUNCSIG__
#else
#define ION_PRETTY_FUNCTION __PRETTY_FUNCTION__
#endif

#endif  // ION_PORT_MACROS_H_
