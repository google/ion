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

#include <algorithm>
#include <map>
#include <mutex>  // NOLINT(build/c++11)
#include <vector>

#include "ion/base/allocatable.h"
#include "ion/base/allocationmanager.h"
#include "ion/base/invalid.h"
#include "ion/base/logging.h"
#include "ion/base/stlalloc/allocmap.h"
#include "ion/base/stlalloc/allocvector.h"

namespace ion {
namespace base {

//-----------------------------------------------------------------------------
//
// FullAllocationTracker::Helper class.
//
//-----------------------------------------------------------------------------

class FullAllocationTracker::Helper : public Allocatable {
 public:
  Helper()
      : allocations_(*this),
        active_map_(*this),
        deallocation_count_(0U),
        allocated_bytes_count_(0U),
        deallocated_bytes_count_(0U),
        active_memory_bytes_count_(0U) {}

  ~Helper() override;

  // Adds an allocation to the vector and the active allocation map. Returns
  // the index into the vector.
  size_t AddAllocation(const void* memory, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    const size_t index = allocations_.size();
    allocations_.push_back(Allocation(memory, size));
    DCHECK(active_map_.find(memory) == active_map_.end());
    active_map_[memory] = index;
    allocated_bytes_count_ += size;
    active_memory_bytes_count_ += size;
    return index;
  }

  // Removes an allocation as active. Returns its index in the vector or
  // kInvalidIndex if it is not found.
  size_t RemoveAllocation(const void* memory) {
    std::lock_guard<std::mutex> lock(mutex_);
    ActiveMap::iterator it = active_map_.find(memory);
    if (it == active_map_.end()) {
      return kInvalidIndex;
    } else {
      const size_t index = it->second;
      DCHECK_LT(index, allocations_.size());
      ++deallocation_count_;
      const size_t size = allocations_[index].size;
      deallocated_bytes_count_ += size;
      DCHECK_LE(size, active_memory_bytes_count_);
      active_memory_bytes_count_ -= size;
      active_map_.erase(it);
      return index;
    }
  }

  // Returns the total number of allocations ever made with this allocator.
  size_t GetAllocationCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return allocations_.size();
  }

  // Returns the total number of deallocations ever made with this allocator.
  size_t GetDeallocationCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return deallocation_count_;
  }

  // Returns the total number of bytes ever allocated.
  size_t GetAllocatedBytesCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return allocated_bytes_count_;
  }

  size_t GetDeallocatedBytesCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return deallocated_bytes_count_;
  }

  // Returns the number of active allocations.
  size_t GetActiveAllocationCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_map_.size();
  }

  // Returns the total amount of memory in bytes used by active allocations.
  size_t GetActiveAllocationBytesCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_memory_bytes_count_;
  }

  // Returns the number of bytes in the indexed allocation.
  size_t GetAllocationBytesCount(size_t index) const {
    std::lock_guard<std::mutex> lock(mutex_);
    DCHECK_LT(index, allocations_.size());
    return allocations_[index].size;
  }

 private:
  // Data stored for each allocation.
  struct Allocation {
    Allocation(const void* memory_in, size_t size_in)
        : memory(memory_in), size(size_in) {}
    const void* memory;  // Pointer to allocated memory chunk.
    size_t size;         // Number of bytes allocated.
  };

  // Returns a vector containing all active allocations, sorted by pointer.
  const AllocVector<Allocation> GetActiveAllocations() const {
    std::lock_guard<std::mutex> lock(mutex_);
    // Copy all current allocations into a vector. We have to loop over the map
    // because the allocations_ vector typically contains inactive allocations
    // in addition to the active ones.
    AllocVector<Allocation> vec(*this);
    vec.reserve(active_map_.size());
    for (ActiveMap::const_iterator it = active_map_.begin();
         it != active_map_.end(); ++it) {
      DCHECK_LT(it->second, allocations_.size());
      vec.push_back(allocations_[it->second]);
    }
    // Sort by memory pointer.
    std::sort(vec.begin(), vec.end(), CompareAllocations);
    return vec;
  }

  // This is used for sorting the vector of AllocationData instances returned
  // by GetActiveAllocations.
  static bool CompareAllocations(const Allocation& a0, const Allocation& a1) {
    return a0.memory < a1.memory;
  }

  // Vector keeping track of every single allocation made with this instance.
  AllocVector<Allocation> allocations_;

  // Map of all active allocations (allocated but not deallocated yet). The key
  // is the pointer and the value is the index into the allocations_ vector.
  typedef AllocMap<const void*, size_t> ActiveMap;
  ActiveMap active_map_;

  // Total number of deallocations made so far.
  size_t deallocation_count_;

  // Total number of bytes allocated and deallocated.
  size_t allocated_bytes_count_;
  size_t deallocated_bytes_count_;

  // Count of total active allocated memory (in bytes).
  size_t active_memory_bytes_count_;

  // Mutex protecting the vector and map;
  mutable std::mutex mutex_;
};

FullAllocationTracker::Helper::~Helper() {
  // There should be no active allocations left.
  const AllocVector<Allocation> allocations = GetActiveAllocations();
  if (!allocations.empty()) {
    LOG(ERROR) << "FullAllocationTracker " << this << " destroyed with "
               << allocations.size() << " active allocations:";
    for (size_t i = 0; i < allocations.size(); ++i) {
      const Allocation& a = allocations[i];
      LOG(ERROR) << "  [" << i << "] " << a.size << " bytes at " << a.memory;
    }
  }
}

//-----------------------------------------------------------------------------
//
// FullAllocationTracker functions.
//
//-----------------------------------------------------------------------------

FullAllocationTracker::FullAllocationTracker()
    : helper_(new(AllocationManager::GetMallocAllocator()) Helper),
      tracing_ostream_(nullptr) {}

FullAllocationTracker::~FullAllocationTracker() {
  // Destroying the helper should check for remaining active allocations.
  helper_.reset(nullptr);
}

void FullAllocationTracker::TrackAllocation(
    const Allocator& allocator, size_t requested_size, const void* memory) {
  const size_t index = helper_->AddAllocation(memory, requested_size);
  if (tracing_ostream_) {
    (*tracing_ostream_) << "FullAllocationTracker " << this << " [" << index
                        << "] Allocated   " << requested_size << " bytes @ "
                        << memory << " with allocator " << &allocator << "\n";
  }
}

void FullAllocationTracker::TrackDeallocation(
    const Allocator& allocator, const void* memory) {
  const size_t index = helper_->RemoveAllocation(memory);
  if (index == kInvalidIndex) {
    LOG(ERROR) << "FullAllocationTracker " << this << ": pointer " << memory
               << " does not correspond to an active allocation";
  } else {
    if (tracing_ostream_) {
      const size_t size = helper_->GetAllocationBytesCount(index);
      (*tracing_ostream_) << "FullAllocationTracker " << this << " [" << index
                          << "] Deallocated " << size << " bytes @ " << memory
                          << " with allocator " << &allocator << "\n";
    }
  }
}

size_t FullAllocationTracker::GetAllocationCount() {
  return helper_->GetAllocationCount();
}

size_t FullAllocationTracker::GetDeallocationCount() {
  return helper_->GetDeallocationCount();
}

size_t FullAllocationTracker::GetAllocatedBytesCount() {
  return helper_->GetAllocatedBytesCount();
}

size_t FullAllocationTracker::GetDeallocatedBytesCount() {
  return helper_->GetDeallocatedBytesCount();
}

size_t FullAllocationTracker::GetActiveAllocationCount() {
  return helper_->GetActiveAllocationCount();
}

size_t FullAllocationTracker::GetActiveAllocationBytesCount() {
  return helper_->GetActiveAllocationBytesCount();
}

void FullAllocationTracker::SetGpuTracker(
    const AllocationSizeTrackerPtr& gpu_tracker) {}
AllocationSizeTrackerPtr FullAllocationTracker::GetGpuTracker() {
  return AllocationSizeTrackerPtr();
}

}  // namespace base
}  // namespace ion
