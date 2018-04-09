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

#ifndef ION_PORT_ALIGN_H_
#define ION_PORT_ALIGN_H_

//
// The ION_ALIGN macro can be used in situations where a variable needs to be
// aligned to a minimum number of bytes. It can be used within any scope or for
// member variables of a class or struct and may be followed by an initializer.
//
// For example:
//    int ION_ALIGN(16) i;
//    float ION_ALIGN(16) f = 13.2;
//

// The ION_ALIGNMENT_ENABLED flag indicates whether alignment of Ion objects
// should be enabled on the platform.
#if !defined(ION_ALIGNMENT_ENABLED)
#  if defined(ION_PLATFORM_WINDOWS) || defined(ION_PLATFORM_ANDROID) || \
      defined(ION_GOOGLE_INTERNAL)
// Alignment is turned off by default on Windows because the compiler has
// problems with copying data structures containing aligned elements. Many
// Android devices (e.g., 2012 Nexus 7) do not properly support alignment.
#    define ION_ALIGNMENT_ENABLED 0
#  else
#    define ION_ALIGNMENT_ENABLED 1
#  endif
#endif

#if ION_ALIGNMENT_ENABLED
#  if defined(ION_PLATFORM_WINDOWS)
#    define ION_ALIGN(num_bytes) __declspec(align(num_bytes))
#  else
// All gcc-based compilers use this attribute.
#    define ION_ALIGN(num_bytes) __attribute__((aligned(num_bytes)))
#  endif
#else
#  define ION_ALIGN(num_bytes)
#endif

#if defined(ION_PLATFORM_WINDOWS)
#define ION_ALIGNOF(T) __alignof(T)
#else
#define ION_ALIGNOF(T) __alignof__(T)
#endif

#endif  // ION_PORT_ALIGN_H_
