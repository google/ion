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

#ifndef ION_PORT_NULLPTR_H_
#define ION_PORT_NULLPTR_H_

#include <cstddef>

// QNX (gcc 4.4.2) and NaCl x86 gcc (4.4.3) do not define nullptr or nullptr_t,
// We define nullptr via the build system so nullptr can be used without this
// compatibility header.  For nullptr_t or kNullFunction, however, you need to
// include this header.
#if defined(ION_PLATFORM_QNX) ||   \
    (defined(ION_PLATFORM_NACL) && \
     !(defined(ION_ARCH_ARM) || defined(__clang__)))
// If the build system didn't define nullptr, define it now.
#  ifndef nullptr
#    define nullptr NULL
#  endif
namespace std {
  typedef void* nullptr_t;
}  // namespace std
#endif
#define kNullFunction nullptr

#endif  // ION_PORT_NULLPTR_H_
