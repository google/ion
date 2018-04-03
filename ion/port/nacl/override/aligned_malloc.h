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

#ifndef ION_PORT_NACL_OVERRIDE_ALIGNED_MALLOC_H_
#define ION_PORT_NACL_OVERRIDE_ALIGNED_MALLOC_H_

#if defined(ION_PLATFORM_NACL)

#include <malloc.h>

// (p)NaCl doesn't defined aligned_malloc/free, so we define them here
// and force include this file.
inline void* aligned_malloc(size_t size, int minimum_alignment) {
  return memalign(minimum_alignment, size);
}

inline void aligned_free(void *aligned_memory) {
  free(aligned_memory);
}

#endif  // ION_PLATFORM_NACL
#endif  // ION_PORT_NACL_OVERRIDE_ALIGNED_MALLOC_H_
