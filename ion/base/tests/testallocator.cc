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

#include "ion/base/tests/testallocator.h"

#include "ion/base/allocationmanager.h"

namespace ion {
namespace base {
namespace testing {

size_t TestAllocator::num_creations_ = 0;
size_t TestAllocator::num_deletions_ = 0;

TestAllocator::TestAllocator()
    : num_allocated_(0), num_deallocated_(0), bytes_allocated_(0) {
  AllocationTrackerPtr tracker(new FullAllocationTracker);
  SetTracker(tracker);
  ++num_creations_;
}

TestAllocator::~TestAllocator() {
  ++num_deletions_;
}

void* TestAllocator::Allocate(size_t size) {
  ++num_allocated_;
  bytes_allocated_ += size;
  return AllocationManager::GetMallocAllocator()->AllocateMemory(size);
}

void TestAllocator::Deallocate(void* p) {
  ++num_deallocated_;
  return AllocationManager::GetMallocAllocator()->DeallocateMemory(p);
}

const AllocatorPtr& TestAllocator::GetAllocatorForLifetime(
    AllocationLifetime lifetime) const {
  // If the caller installed an allocator, use it. Otherwise, defer to the
  // base class's version of the function.
  return allocators_[lifetime].Get() ? allocators_[lifetime] :
      Allocator::GetAllocatorForLifetime(lifetime);
}

}  // namespace testing
}  // namespace base
}  // namespace ion
