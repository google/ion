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

#include "ion/base/fullallocationtracker.h"

#include <sstream>
#include <string>
#include <vector>

#include "ion/base/allocationmanager.h"
#include "ion/base/logchecker.h"
#include "ion/base/stringutils.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace base {

namespace {

// A derived Allocator class used only to create dummy instances.
class DummyAllocator : public ion::base::Allocator {
 public:
  // Implementations don't matter - they will never be called.
  void* Allocate(size_t size) override { return nullptr; }
  void Deallocate(void* p) override {}
};

// Handy function to cast a pointer.
static const void* Pointer(size_t address) {
  return reinterpret_cast<const void*>(address);
}

}  // anonymous namespace

TEST(FullAllocationTracker, NullGpuTracker) {
  // FullAllocationTracker does not implement a GPU tracker.
  FullAllocationTrackerPtr fat(new FullAllocationTracker());
  AllocationSizeTrackerPtr ast;
  fat->SetGpuTracker(ast);
  EXPECT_TRUE(fat->GetGpuTracker().Get() == nullptr);
}

TEST(FullAllocationTracker, Counting) {
  LogChecker log_checker;
  FullAllocationTrackerPtr fat(new FullAllocationTracker());
  AllocatorPtr da(new DummyAllocator);

  EXPECT_EQ(0U, fat->GetAllocationCount());
  EXPECT_EQ(0U, fat->GetDeallocationCount());
  EXPECT_EQ(0U, fat->GetAllocatedBytesCount());
  EXPECT_EQ(0U, fat->GetDeallocatedBytesCount());
  EXPECT_EQ(0U, fat->GetActiveAllocationCount());
  EXPECT_EQ(0U, fat->GetActiveAllocationBytesCount());

  fat->TrackAllocation(*da, 100U, Pointer(0xface));
  EXPECT_EQ(1U, fat->GetAllocationCount());
  EXPECT_EQ(0U, fat->GetDeallocationCount());
  EXPECT_EQ(100U, fat->GetAllocatedBytesCount());
  EXPECT_EQ(0U, fat->GetDeallocatedBytesCount());
  EXPECT_EQ(1U, fat->GetActiveAllocationCount());
  EXPECT_EQ(100U, fat->GetActiveAllocationBytesCount());

  fat->TrackAllocation(*da, 200U, Pointer(0xdeed));
  EXPECT_EQ(2U, fat->GetAllocationCount());
  EXPECT_EQ(0U, fat->GetDeallocationCount());
  EXPECT_EQ(300U, fat->GetAllocatedBytesCount());
  EXPECT_EQ(0U, fat->GetDeallocatedBytesCount());
  EXPECT_EQ(2U, fat->GetActiveAllocationCount());
  EXPECT_EQ(300U, fat->GetActiveAllocationBytesCount());

  fat->TrackDeallocation(*da, Pointer(0xdeed));
  EXPECT_EQ(2U, fat->GetAllocationCount());
  EXPECT_EQ(1U, fat->GetDeallocationCount());
  EXPECT_EQ(300U, fat->GetAllocatedBytesCount());
  EXPECT_EQ(200U, fat->GetDeallocatedBytesCount());
  EXPECT_EQ(1U, fat->GetActiveAllocationCount());
  EXPECT_EQ(100U, fat->GetActiveAllocationBytesCount());

  fat->TrackAllocation(*da, 300U, Pointer(0xbead));
  EXPECT_EQ(3U, fat->GetAllocationCount());
  EXPECT_EQ(1U, fat->GetDeallocationCount());
  EXPECT_EQ(600U, fat->GetAllocatedBytesCount());
  EXPECT_EQ(200U, fat->GetDeallocatedBytesCount());
  EXPECT_EQ(2U, fat->GetActiveAllocationCount());
  EXPECT_EQ(400U, fat->GetActiveAllocationBytesCount());

  fat->TrackDeallocation(*da, Pointer(0xface));
  EXPECT_EQ(3U, fat->GetAllocationCount());
  EXPECT_EQ(2U, fat->GetDeallocationCount());
  EXPECT_EQ(600U, fat->GetAllocatedBytesCount());
  EXPECT_EQ(300U, fat->GetDeallocatedBytesCount());
  EXPECT_EQ(1U, fat->GetActiveAllocationCount());
  EXPECT_EQ(300U, fat->GetActiveAllocationBytesCount());

  fat->TrackDeallocation(*da, Pointer(0xbead));
  EXPECT_EQ(3U, fat->GetAllocationCount());
  EXPECT_EQ(3U, fat->GetDeallocationCount());
  EXPECT_EQ(600U, fat->GetAllocatedBytesCount());
  EXPECT_EQ(600U, fat->GetDeallocatedBytesCount());
  EXPECT_EQ(0U, fat->GetActiveAllocationCount());
  EXPECT_EQ(0U, fat->GetActiveAllocationBytesCount());

  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST(FullAllocationTracker, Tracing) {
  LogChecker log_checker;
  FullAllocationTrackerPtr fat(new FullAllocationTracker());
  AllocatorPtr da(new DummyAllocator);

  // Check that default value is correct.
  EXPECT_EQ(nullptr, fat->GetTracingStream());

  // Check that the stream changes appropriately.
  std::ostringstream s;
  fat->SetTracingStream(&s);
  EXPECT_EQ(&s, fat->GetTracingStream());

  // Do a few things and check the resulting strings.
  fat->TrackAllocation(*da, 32U, Pointer(0xface));
  fat->TrackAllocation(*da, 64U, Pointer(0xbabe));
  fat->TrackDeallocation(*da, Pointer(0xbabe));
  fat->TrackAllocation(*da, 16U, Pointer(0xb00b));
  fat->TrackDeallocation(*da, Pointer(0xface));
  fat->TrackDeallocation(*da, Pointer(0xb00b));

  const std::vector<std::string> v = SplitString(s.str(), "\n");
  EXPECT_EQ(6U, v.size());
  EXPECT_NE(std::string::npos, v[0].find("[0] Allocated   32 bytes @ "));
  EXPECT_NE(std::string::npos, v[1].find("[1] Allocated   64 bytes @ "));
  EXPECT_NE(std::string::npos, v[2].find("[1] Deallocated 64 bytes @ "));
  EXPECT_NE(std::string::npos, v[3].find("[2] Allocated   16 bytes @ "));
  EXPECT_NE(std::string::npos, v[4].find("[0] Deallocated 32 bytes @ "));
  EXPECT_NE(std::string::npos, v[5].find("[2] Deallocated 16 bytes @ "));

  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST(FullAllocationTracker, BadDeletion) {
  LogChecker log_checker;
  {
    FullAllocationTrackerPtr fat(new FullAllocationTracker());
    AllocatorPtr da(new DummyAllocator);

    fat->TrackAllocation(*da, 32U, Pointer(0xface));
    fat->TrackDeallocation(*da, Pointer(0xface + 1U));
    EXPECT_TRUE(log_checker.HasMessage(
        "ERROR", "does not correspond to an active allocation"));

    fat->TrackDeallocation(*da, Pointer(0xface));
  }

  // The allocator should have been deleted.
  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST(FullAllocationTracker, DeletedWhileActive) {
  LogChecker log_checker;
  FullAllocationTrackerPtr fat(new FullAllocationTracker());
  AllocatorPtr da(new DummyAllocator);

  {
    fat->TrackAllocation(*da, 14U, Pointer(0xface));
    fat->TrackAllocation(*da, 62U, Pointer(0xbabe));
    fat.Reset(nullptr);  // Deletes the allocator with 2 active allocations.
  }

#if !ION_PRODUCTION
  const std::vector<std::string> m = log_checker.GetAllMessages();
  EXPECT_EQ(3U, m.size());
  EXPECT_NE(std::string::npos,
            m[0].find("destroyed with 2 active allocations"));
  EXPECT_NE(std::string::npos, m[1].find("62 bytes at"));
  EXPECT_NE(std::string::npos, m[2].find("14 bytes at"));
#endif

  EXPECT_FALSE(log_checker.HasAnyMessages());
}

}  // namespace base
}  // namespace ion
