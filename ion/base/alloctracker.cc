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

#include "ion/base/alloctracker.h"

#include <stdlib.h>  // For malloc() and free().

#include "ion/base/allocator.h"
#include "ion/base/stlalloc/allocmap.h"

#if !defined(ION_ALLOC_TRACKER_DEFINED)
# error alloctracker.cc requires the --track-allocations build option.
#endif

//-----------------------------------------------------------------------------
//
// Helper functions.
//
//-----------------------------------------------------------------------------

namespace {

// Returns the singleton AllocTracker instance.
static ion::base::AllocTracker* GetTracker() {
  return ion::base::AllocTracker::GetMutableInstance();
}

// Using the regular (global) new operator to create internal objects needed by
// the AllocTracker causes really bad things to happen. This uses placement new
// to avoid them.
template <typename T>
static T* CreateWithPlacementNew() {
  void* ptr = malloc(sizeof(T));
  return new(ptr) T;
}

}  // anonymous namespace

namespace ion {
namespace base {

//-----------------------------------------------------------------------------
//
// AllocTracker::Helper class.
//
//-----------------------------------------------------------------------------

class AllocTracker::Helper {
 public:
  Helper()
      : internal_allocator_(CreateWithPlacementNew<InternalAllocator>()),
        size_map_(internal_allocator_) {}
  ~Helper() {}

  // These functions manage a map of currently-allocated pointers to their
  // sizes to allow counting allocated bytes.
  void AddSize(void* ptr, size_t size) {
    size_map_[ptr] = size;
  }
  size_t FindAndRemoveSize(void* ptr) {
    size_t bytes = 0;
    SizeMap::iterator it = size_map_.find(ptr);
    if (it != size_map_.end()) {
      bytes = it->second;
      size_map_.erase(it);
    }
    return bytes;
  }

 private:
  // This is an Ion Allocator that uses the AllocTracker.
  class InternalAllocator : public ion::base::Allocator {
   public:
    virtual void* Allocate(size_t size) {
      // There is a bootstrapping problem that occurs if anything is allocated
      // during construction of the AllocTracker singleton instance. If the
      // AllocTracker pointer is not yet set, just use malloc.
      if (AllocTracker* tracker = GetTracker()) {
        return tracker->New(size, kInternalAlloc);
      } else {
        return malloc(size);
      }
    }
    virtual void Deallocate(void* ptr) {
      GetTracker()->Delete(ptr, kInternalAlloc);
    }

   protected:
    virtual ~InternalAllocator() {}
  };
  typedef ion::base::SharedPtr<InternalAllocator> InternalAllocatorPtr;

  // This is used for allocating the SizeMap. Otherwise, it would use global
  // new, which would be bad.
  InternalAllocatorPtr internal_allocator_;

  // Tracks sizes (in bytes) of open allocations, keyed by pointer.
  typedef ion::base::AllocMap<void*, size_t> SizeMap;
  SizeMap size_map_;
};

//-----------------------------------------------------------------------------
//
// AllocTracker functions.
//
//-----------------------------------------------------------------------------

AllocTracker* AllocTracker::GetMutableInstance() {
  // Cannot use a staticsafedeclare macro here, or there will be infinite
  // recursion.  This breaks thread safety.
  static AllocTracker* tracker = CreateWithPlacementNew<AllocTracker>();
  return tracker;
}

const AllocTracker& AllocTracker::GetInstance() {
  return *GetMutableInstance();
}

AllocTracker::AllocTracker() {
  // The Helper contains stuff which may require allocations, so the
  // AllocTracker instance has to be constructed properly before the Helper can
  // be created.
  helper_.reset(CreateWithPlacementNew<Helper>());
}

AllocTracker::~AllocTracker() {
}

void AllocTracker::SetBaseline() {
  baseline_counts_ = all_counts_;
}

void* AllocTracker::New(std::size_t size, AllocType type) {
  void* ptr = malloc(size);

  // Update Counts.
  TypeCounts &all_c = all_counts_.counts[type];
  TypeCounts &open_c = open_counts_.counts[type];
  ++all_c.allocs;
  all_c.bytes += size;
  ++open_c.allocs;
  open_c.bytes += size;

  // Do NOT store internal allocations in the SizeMap. Otherwise, that could
  // cause new internal allocations and recursing until death.
  if (type != kInternalAlloc)
    helper_->AddSize(ptr, size);

  return ptr;
}

void AllocTracker::Delete(void* ptr, AllocType type) {
  size_t bytes = type == kInternalAlloc ? 0 : helper_->FindAndRemoveSize(ptr);

  // Update Counts.
  TypeCounts &open_c = open_counts_.counts[type];
  if (open_c.allocs)
    --open_c.allocs;
  if (open_c.bytes >= bytes)
    open_c.bytes -= bytes;

  free(ptr);
}

}  // namespace base
}  // namespace ion

//-----------------------------------------------------------------------------
//
// Global new and delete operators.
//
//-----------------------------------------------------------------------------

using ion::base::AllocTracker;

void* operator new(std::size_t size) {
  return GetTracker()->New(size, AllocTracker::kNonArrayAlloc);
}

void* operator new[](std::size_t size) {
  return GetTracker()->New(size, AllocTracker::kArrayAlloc);
}

void operator delete(void* ptr) {
  GetTracker()->Delete(ptr, AllocTracker::kNonArrayAlloc);
}

void operator delete[](void* ptr) {
  GetTracker()->Delete(ptr, AllocTracker::kArrayAlloc);
}
