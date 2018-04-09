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

#include "ion/port/memory.h"
#include "ion/base/logging.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

TEST(Memory, SystemMemory) {
#if defined(ION_PLATFORM_LINUX) || defined(ION_PLATFORM_ANDROID) || \
  defined(ION_PLATFORM_MAC) || defined(ION_PLATFORM_IOS) || \
  defined(ION_PLATFORM_WINDOWS) || defined(ION_GOOGLE_INTERNAL)
  EXPECT_GT(ion::port::GetSystemMemorySize(), 0U);
#else
  EXPECT_EQ(0U, ion::port::GetSystemMemorySize());
#endif
}

TEST(Memory, ProcessMemory) {
  const uint64 process_memory = ion::port::GetProcessResidentMemorySize();
#if defined(ION_PLATFORM_LINUX) || defined(ION_PLATFORM_ANDROID) || \
  defined(ION_PLATFORM_MAC) || defined(ION_PLATFORM_IOS) || \
  defined(ION_GOOGLE_INTERNAL) || \
  (defined(ION_PLATFORM_WINDOWS) && !defined(ION_GOOGLE_INTERNAL))
  EXPECT_GT(process_memory, 0U);
  static const uint64 kAllocationSize = 10000000;
  uint8* const allocated_memory = static_cast<uint8*>(malloc(kAllocationSize));
  memset(allocated_memory, 255, kAllocationSize);
  // Prevent the compiler from optimizing out the above or reducing the
  // allocation size.
  uint64 total = 0U;
  for (uint64 i = 0; i < kAllocationSize; ++i)
    total += allocated_memory[i];
  EXPECT_EQ(255U * kAllocationSize, total);
  const uint64 new_process_memory = ion::port::GetProcessResidentMemorySize();
  EXPECT_GT(new_process_memory, kAllocationSize);
# if defined(ADDRESS_SANITIZER) ||\
     defined(MEMORY_SANITIZER) ||\
     defined(THREAD_SANITIZER)
  // Under ASAN and MSAN, the apparently memory size doesn't change.
  EXPECT_GE(new_process_memory, process_memory);
# else
  EXPECT_GT(new_process_memory, process_memory);
# endif
  free(allocated_memory);
#else
  EXPECT_EQ(0U, process_memory);
#endif
}
