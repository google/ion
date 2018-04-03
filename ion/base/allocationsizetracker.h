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

#ifndef ION_BASE_ALLOCATIONSIZETRACKER_H_
#define ION_BASE_ALLOCATIONSIZETRACKER_H_

#include "ion/base/allocationtracker.h"
#include "ion/base/shareable.h"
#include "ion/base/sharedptr.h"

namespace ion {
namespace base {

// AllocationSizeTracker is an abstract base class for objects that track
// memory.
class AllocationSizeTracker : public AllocationTracker {
 public:
  // TrackAllocationSize() increments the currently tracked allocation size and
  // the number of allocations.
  virtual void TrackAllocationSize(size_t allocation_size) = 0;

  // TrackDeallocationSize() decrements the current tracked allocation size and
  // increments the number of deallocations.
  virtual void TrackDeallocationSize(size_t deallocation_size) = 0;
};

// Convenience typedef for shared pointer to a AllocationSizeTracker.
typedef SharedPtr<AllocationSizeTracker> AllocationSizeTrackerPtr;

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_ALLOCATIONSIZETRACKER_H_
