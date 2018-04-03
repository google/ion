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

#ifndef ION_PORT_ATOMIC_H_
#define ION_PORT_ATOMIC_H_

// QNX (gcc 4.4.2) and NaCl x86 gcc (4.4.3) do not include the C++ <atomic>
// header, but do provide the older <cstdatomic>
#if defined(ION_PLATFORM_QNX) || \
    (defined(ION_PLATFORM_NACL) && \
     !(defined(ION_ARCH_ARM) || defined(__clang__)))
#  include <cstdatomic>

// For some reason, cstdatomic declares but does not define atomic<T*>::store().
// Either omitting the declaration or providing the definition would allow it
// to work properly. We provide it here. If a future toolchain upgrade causes
// this to break, further condition it or remove it at that time.
namespace std {
template<typename _Tp>
void atomic<_Tp*>::store(_Tp* __v, memory_order __m) volatile {
  atomic_address::store(__v, __m);
}
}  // namespace std

#else
#  include <atomic>
#endif

#endif  // ION_PORT_ATOMIC_H_
