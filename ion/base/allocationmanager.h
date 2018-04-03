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

#ifndef ION_BASE_ALLOCATIONMANAGER_H_
#define ION_BASE_ALLOCATIONMANAGER_H_

#include "base/macros.h"
#include "ion/base/allocator.h"

namespace ion {
namespace base {

// AllocationManager is a singleton class that is used to manage Allocators
// used to allocate Ion objects.
class ION_API AllocationManager {
 public:
  ~AllocationManager();

  // Sets/returns the AllocationLifetime that is assumed when a default new()
  // operator is used for an Allocatable. It is MediumTerm by default.
  static void SetDefaultAllocationLifetime(AllocationLifetime lifetime) {
    GetInstance()->default_allocation_lifetime_ = lifetime;
  }
  static AllocationLifetime GetDefaultAllocationLifetime() {
    return GetInstance()->default_allocation_lifetime_;
  }

  // Sets/returns the default Allocator to use for a specific
  // AllocationLifetime. The default Allocator will be used for allocations of
  // Allocatable objects when using the new(AllocationLifetime) operator or
  // when Ion allocates subordinate objects, unless the Allocator for the owner
  // object overrides its GetAllocatorForLifetime() function.
  //
  // By default, the Allocator returned by GetMallocAllocator() is used for all
  // lifetimes. Setting an Allocator to NULL restores GetMallocAllocator() as
  // the default allocator.
  //
  // Note that these functions are not thread-safe, as it does not make sense
  // for multiple threads to change the default allocation settings.
  // Thread-specific allocation strategies can be implemented by overriding the
  // Allocator::GetAllocatorForLifetime() function.
  static void SetDefaultAllocatorForLifetime(AllocationLifetime lifetime,
                                             const AllocatorPtr& allocator) {
    AllocationManager* mgr = GetInstance();
    mgr->default_allocators_[lifetime] =
        allocator.Get() ? allocator : mgr->malloc_allocator_;
  }
  static const AllocatorPtr& GetDefaultAllocatorForLifetime(
      AllocationLifetime lifetime) {
    return GetInstance()->default_allocators_[lifetime];
  }

  // Convenience function that returns the default allocator to use when no
  // lifetime is specified. It returns the default allocator for the lifetime
  // returned by GetDefaultAllocationLifetime().
  static const AllocatorPtr& GetDefaultAllocator() {
    AllocationManager* mgr = GetInstance();
    return mgr->default_allocators_[mgr->default_allocation_lifetime_];
  }

  // Returns an allocator that performs conventional allocation and
  // deallocation with malloc() and free().
  static const AllocatorPtr& GetMallocAllocator() {
    return GetInstance()->malloc_allocator_;
  }

  // This convenience function can be used where a non-NULL Allocator pointer
  // is needed. It returns the passed-in Allocator if it is not NULL; otherwise
  // it returns the Allocator returned by GetDefaultAllocator().
  static const AllocatorPtr& GetNonNullAllocator(
      const AllocatorPtr& allocator) {
    return allocator.Get() ? allocator : GetDefaultAllocator();
  }

 private:
  // MallocAllocator is a derived Allocator class that uses malloc/free for
  // memory management. This is used by default for all lifetimes.
  class MallocAllocator;

  // The constructor is private since this is a singleton class.
  AllocationManager();

  // Returns the singleton instance.
  static AllocationManager* GetInstance();

  // Default AllocationLifetime to assume for standard operator new().
  AllocationLifetime default_allocation_lifetime_;

  // Per-lifetime default Allocators.
  AllocatorPtr default_allocators_[kNumAllocationLifetimes];

  // A safe pointer to an MallocAllocator instance.
  AllocatorPtr malloc_allocator_;

  DISALLOW_COPY_AND_ASSIGN(AllocationManager);
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_ALLOCATIONMANAGER_H_
