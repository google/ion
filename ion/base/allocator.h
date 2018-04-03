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

#ifndef ION_BASE_ALLOCATOR_H_
#define ION_BASE_ALLOCATOR_H_

#include "base/macros.h"
#include "ion/base/allocationtracker.h"
#include "ion/base/logging.h"
#include "ion/base/shareable.h"
#include "ion/base/sharedptr.h"

namespace ion {
namespace base {

// All memory allocated within Ion uses an Allocator chosen based on the
// predicted lifetime of the target object. This enum defines the lifetime
// categories.
enum AllocationLifetime {
  // kShortTerm is used for objects that are very transient in nature, such as
  // scratch memory used to compute a result.
  kShortTerm,

  // kMediumTerm is used for objects that don't fall into the kShortTerm or
  // kLongTerm categories.
  kMediumTerm,

  // kLongTerm is used for objects that have persistent lifetimes, such as
  // managers.
  kLongTerm,
};

// The number of supported lifetimes.
static const int kNumAllocationLifetimes = kLongTerm + 1;

// Convenience typedef for shared pointer to an Allocator.
class Allocator;
typedef SharedPtr<Allocator> AllocatorPtr;

// Allocator is an abstract base class for a memory allocator used for Ion
// objects derived from Allocatable. The lifetime of an Allocator is managed
// through AllocatorPtr instances (SharedPtr to an Allocator). Every
// Allocatable created with an Allocator maintains an AllocatorPtr to that
// Allocator. Therefore, clients should maintain at least one other
// AllocatorPtr to an Allocator to prevent it from being destroyed while it is
// still needed.
class ION_API Allocator : public Shareable {
 public:
  // Allocates memory of the given size.
  void* AllocateMemory(size_t size);

  // Deallocates a previously-allocated memory block.
  void DeallocateMemory(void* p);

  // Returns the correct Allocator to use to allocate memory with a specific
  // lifetime. The base class implements this to return the default Allocator
  // for the lifetime from the AllocationManager. Derived classes may override
  // this to provide a different Allocator to support different allocation
  // schemes.
  virtual const AllocatorPtr& GetAllocatorForLifetime(
      AllocationLifetime lifetime) const;

  // Sets/returns an AllocationTracker instance used to track the workings of
  // this instance. The pointer is NULL by default, resulting in no
  // tracking. Note that it is probably a bad idea to change the tracker
  // instance while any memory allocated by this instance is still active.
  void SetTracker(const AllocationTrackerPtr& tracker) { tracker_ = tracker; }
  const AllocationTrackerPtr& GetTracker() const { return tracker_; }

 protected:
  Allocator() {}

  // The destructor is protected because all instances should be managed
  // through SharedPtr.
  ~Allocator() override;

  // Derived classes must define this to allocate size bytes of memory and
  // return a pointer to it. The returned memory must make the same alignment
  // guarantee that malloc() makes: the memory is suitably aligned for any kind
  // of variable.
  virtual void* Allocate(size_t size) = 0;

  // Derived classes must define this to return memory previously allocated by
  // a call to Allocate().
  virtual void Deallocate(void* p) = 0;

 private:
  AllocationTrackerPtr tracker_;

  DISALLOW_COPY_AND_ASSIGN(Allocator);
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_ALLOCATOR_H_
