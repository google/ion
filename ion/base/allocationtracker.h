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

#ifndef ION_BASE_ALLOCATIONTRACKER_H_
#define ION_BASE_ALLOCATIONTRACKER_H_

#include "ion/base/shareable.h"
#include "ion/base/sharedptr.h"

namespace ion {
namespace base {

class Allocator;
class AllocationSizeTracker;
typedef SharedPtr<AllocationSizeTracker> AllocationSizeTrackerPtr;

// AllocationTracker is an abstract base class for objects that track memory.
// An Allocator instance may contain a pointer to an AllocationTracker that is
// invoked whenever a chunk of memory is allocated or deallocated.
class AllocationTracker : public Shareable {
 public:
  // TrackAllocation() is called immediately after an Allocator allocates
  // memory. It receives a reference to the Allocator that allocated the memory,
  // the requested size, and a pointer to the allocated memory.
  virtual void TrackAllocation(const Allocator& allocator,
                               size_t requested_size, const void* memory) = 0;

  // TrackDeallocation() is called immediately before an Allocator deallocates
  // memory. It receives a reference to the Allocator that is deallocating the
  // memory and a pointer to the deallocated memory.
  virtual void TrackDeallocation(const Allocator& allocator,
                                 const void* memory) = 0;

  // Returns the total number of tracked allocations or deallocations.
  virtual size_t GetAllocationCount() = 0;
  virtual size_t GetDeallocationCount() = 0;

  // Returns the total number of memory ever allocated or deallocated, in
  // bytes. These are not guaranteed to be supported by all derived classes;
  // they may return 0 if not.
  virtual size_t GetAllocatedBytesCount() = 0;
  virtual size_t GetDeallocatedBytesCount() = 0;

  // Returns the number of active allocations or the amount of memory in bytes
  // used by active allocations.  These are not guaranteed to be supported by
  // all derived classes; they may return 0 if not.
  virtual size_t GetActiveAllocationCount() = 0;
  virtual size_t GetActiveAllocationBytesCount() = 0;

  // Sets/returns an AllocationSizeTracker instance used to track GPU memory
  // allocations.
  virtual void SetGpuTracker(const AllocationSizeTrackerPtr& gpu_tracker) = 0;
  virtual AllocationSizeTrackerPtr GetGpuTracker() = 0;
};

// Convenience typedef for shared pointer to an AllocationTracker.
typedef SharedPtr<AllocationTracker> AllocationTrackerPtr;

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_ALLOCATIONTRACKER_H_
