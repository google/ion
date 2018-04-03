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

#ifndef ION_BASE_TESTS_TESTALLOCATOR_H_
#define ION_BASE_TESTS_TESTALLOCATOR_H_

#include "ion/base/allocator.h"
#include "ion/base/fullallocationtracker.h"

namespace ion {
namespace base {
namespace testing {

// Derived Allocator that tracks allocation and deallocation. It delegates
// allocation and deallocation to a MallocAllocator and uses a
// FullAllocationTracker so that mismatched allocation problems are detected.
class TestAllocator : public Allocator {
 public:
  TestAllocator();

  // These are defined to count allocations.
  void* Allocate(size_t size) override;
  void Deallocate(void* p) override;

  // These can be used for testing lifetime-specific functions.
  void SetAllocatorForLifetime(AllocationLifetime lifetime,
                               const AllocatorPtr& allocator) {
    allocators_[lifetime] = allocator;
  }
  const AllocatorPtr& GetAllocatorForLifetime(
      AllocationLifetime lifetime) const override;

  // Returns the total number of objects allocated with this allocator.
  size_t GetNumAllocated() const { return num_allocated_; }

  // Returns the total number of objects deallocated with this allocator.
  size_t GetNumDeallocated() const { return num_deallocated_; }

  // Returns the total number of bytes used for objects allocated with this
  // allocator.
  size_t GetBytesAllocated() const { return bytes_allocated_; }

  // Returns the number of times a TestAllocator instance was created or
  // destroyed.
  static size_t GetNumCreations() { return num_creations_; }
  static size_t GetNumDeletions() { return num_deletions_; }

  // Clears the counts of TestAllocator creations or deletions.
  static void ClearNumCreations() { num_creations_ = 0; }
  static void ClearNumDeletions() { num_deletions_ = 0; }

 protected:
  ~TestAllocator() override;

 private:
  static size_t num_creations_;
  static size_t num_deletions_;

  size_t num_allocated_;
  size_t num_deallocated_;
  size_t bytes_allocated_;
  AllocatorPtr allocators_[ion::base::kNumAllocationLifetimes];
};

typedef SharedPtr<TestAllocator> TestAllocatorPtr;

}  // namespace testing
}  // namespace base
}  // namespace ion

#endif  // ION_BASE_TESTS_TESTALLOCATOR_H_
