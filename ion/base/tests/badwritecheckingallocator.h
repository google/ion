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

#ifndef ION_BASE_TESTS_BADWRITECHECKINGALLOCATOR_H_
#define ION_BASE_TESTS_BADWRITECHECKINGALLOCATOR_H_

#include <unordered_map>

#include "ion/base/allocationmanager.h"
#include "ion/base/allocator.h"

namespace ion {
namespace base {
namespace testing {

// Simple derived Allocator that enables testing for bad writes. All
// "deallocated" memory is overwritten with a special signature that is checked
// when the BadWriteCheckingAllocator is destroyed. Note that this can only
// for overruns that are within the maximum size of the instance. All
// allocations are padded to be at least 16 bytes and 16-byte aligned.
class BadWriteCheckingAllocator : public Allocator {
 public:
  static const uint8 kSignature = 0xfe;

  // Constructing a BadWriteCheckingAllocator requires a maximum size in bytes
  // and a helper Allocator to use for the actual allocations. Note that since
  // this is a fairly simple Allocator, max_size is the maximum _total_ size
  // of all allocations; deallocations do _not_ free any actual memory.
  BadWriteCheckingAllocator(size_t max_size, const AllocatorPtr& alloc) :
      max_size_(max_size),
      bytes_used_(0),
      allocation_count_(0),
      allocator_(AllocationManager::GetNonNullAllocator(alloc)),
      memory_(static_cast<uint8*>(allocator_->AllocateMemory(max_size))) {
    // Write the freed signature.
    memset(memory_, kSignature, max_size_);
    // Ensure that the memory starts at a 16-byte boundary.
    size_t memory_start = reinterpret_cast<size_t>(memory_);
    bytes_used_ = (0x10 - (memory_start & 0xf)) & 0xf;
    memory_start = reinterpret_cast<size_t>(&memory_[bytes_used_]);
    CHECK_EQ(0U, memory_start & 0xf);
  }

  ~BadWriteCheckingAllocator() override {
    if (allocation_count_)
      LOG(ERROR) << "BadWriteCheckingAllocator [" << this << "] destroyed with "
                 << allocation_count_ << "active allocations!";

    // Ensure that all memory was freed properly and contains the correct
    // signature, and that no additional memory was overwritten.
    for (size_t i = 0; i < max_size_; ++i)
      if (memory_[i] != kSignature)
        LOG(ERROR) << "Memory at offset " << i << " of " << max_size_
                   << " was overwritten!";

    // Actually free the allocated memory.
    allocator_->DeallocateMemory(memory_);
  }

 private:
  void* Allocate(size_t size) override {
    // Ensure 16-byte alignment.
    size += (0x10 - (size & 0xf)) & 0xf;
    CHECK_EQ(0U, size & 0xf);
    CHECK(bytes_used_ + size <= max_size_)
        << "BadWriteCheckingAllocator supports up to " << max_size_
        << " bytes, attempted to allocate " << (bytes_used_ + size)
        << " bytes!";
    ++allocation_count_;
    void* ptr = &memory_[bytes_used_];
    memset(ptr, 0, size);
    // Track the allocation.
    allocations_[ptr] = size;
    bytes_used_ += size;
    return ptr;
  }

  void Deallocate(void* p) override {
    if (p) {
      AllocationMap::iterator it = allocations_.find(p);
      if (it != allocations_.end()) {
        --allocation_count_;
        const size_t size = it->second;
        memset(p, kSignature, size);
      } else {
        LOG(ERROR) << "Pointer " << p
                   << " was not allocated by this BadWriteCheckingAllocator!";
      }
    }
  }

  // The maximum size of all allocations, in bytes.
  size_t max_size_;
  // The number of bytes used.
  size_t bytes_used_;
  // The total number of allocations made.
  size_t allocation_count_;
  // The Allocator used to make the actual memory allocations.
  base::AllocatorPtr allocator_;

  // The allocations made by this.
  typedef std::unordered_map<void*, size_t> AllocationMap;
  AllocationMap allocations_;

  // The actual allocated memory chunk.
  uint8* memory_;

  DISALLOW_COPY_AND_ASSIGN(BadWriteCheckingAllocator);
};

}  // namespace testing
}  // namespace base
}  // namespace ion

#endif  // ION_BASE_TESTS_BADWRITECHECKINGALLOCATOR_H_
