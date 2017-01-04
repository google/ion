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

#include "ion/base/allocator.h"

#include "ion/base/allocationmanager.h"

namespace ion {
namespace base {

Allocator::~Allocator() {}

const AllocatorPtr& Allocator::GetAllocatorForLifetime(
    AllocationLifetime lifetime) const {
  return AllocationManager::GetDefaultAllocatorForLifetime(lifetime);
}

void* Allocator::AllocateMemory(size_t size) {
  void* ptr = Allocate(size);
  if (AllocationTracker* tracker = tracker_.Get())
    tracker->TrackAllocation(*this, size, ptr);
  return ptr;
}

void Allocator::DeallocateMemory(void* p) {
  if (AllocationTracker* tracker = tracker_.Get())
    tracker->TrackDeallocation(*this, p);
  Deallocate(p);
}

}  // namespace base
}  // namespace ion
