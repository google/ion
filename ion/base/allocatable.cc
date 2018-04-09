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

#include "ion/base/allocatable.h"

#include <vector>

#include "base/integral_types.h"
#include "ion/base/invalid.h"
#include "ion/base/lockguards.h"
#include "ion/base/staticsafedeclare.h"
#include "ion/base/threadlocalobject.h"

namespace ion {
namespace base {

//-----------------------------------------------------------------------------
//
// Allocatable::Helper class definition.
//
// This is effectively a per-thread singleton that is used to work around holes
// in C++'s allocation implementation. The two main problems are:
//
//  1) During allocation, the new function calls operator new first, then calls
//     the constructor(s) for the created object(s). We want those objects to
//     know which Allocator was used in the call to operator new, but C++ does
//     not provide this information or any way to access it.
//
//  2) An object's destructor is called before the call to its operator delete.
//     If any instance-specific information (such as the Allocator used to
//     create the object) is required by delete, C++ does not provide any way
//     to get it.
//
// Therefore, this static instance stores such information in the interval
// between operator new and the constructor(s), and between the destructor(s)
// and operator delete. An AllocationData instance is used for the former, and
// a DeallocationData instance is used for the latter. Because the compiler is
// free to interleave calls to the operators and constructors/destructors,
// there may be multiple active allocations or deallocations, so a vector of
// each type of data is maintained.
//
// When Allocatable::New() is called, it adds an AllocationData instance to the
// allocations_ vector. The constructor for the new Allocatable object accesses
// that instance and uses its Allocator to set up the object, then removes the
// instance from the vector.
//
// The deallocations_ vector is used similarly. When an Allocatable destructor
// is invoked, it adds a DeallocationData instance to the vector. When Delete()
// is called afterwards, it finds the correct DeallocationData instance in the
// vector and uses its Allocator.
//
// Unfortunately, there is no general way to make this work for array
// allocations. At the time new[] is called, only the start address and size of
// the allocated memory chunk are known. These could be saved in an
// AllocationData, and it is fairly easy to detect when a constructor within
// that chunk is called. However, there is no good way to determine when the
// constructors for all of the instances in the array have been called, meaning
// that the AllocationData can be removed. Different compilers store the array
// count in different places, and those places are not generally accessible in
// the Allocatable constructor, especially when multiple inheritance and
// nonstandard pointer alignment are taken into consideration.
//
// This class is NOT thread-safe. It is stored by the Allocatable class in a
// singleton that wraps the thread-local storage for each instance to avoid
// contention between threads.
//
//-----------------------------------------------------------------------------

class Allocatable::Helper {
 public:
  // Only one instance of this class should be created, using this constructor.
  // The Helper's std::vectors keep references to their Allocators, but it's
  // possible that their allocators have to be destroyed before the Helper goes
  // away at program shutdown. Using the MallocAllocator disallows this
  // possibility, since it is the default allocator and can be used until the
  // end of the program.
  Helper()
      : placement_allocator_(nullptr) {}

  // Adds an AllocationData instance to the allocations_ vector.
  void AddAllocationData(const void* memory_ptr, size_t size,
                         Allocator* allocator) {
    const void* end_ptr = reinterpret_cast<const uint8*>(memory_ptr) + size;
    allocations_.push_back(AllocationData(MemoryRange(memory_ptr, end_ptr),
                                          allocator, AllocationData::kNew));
  }

  // Adds an AllocationData instance to the allocations_ vector.
  void AddPlacementAllocationData(const void* memory_ptr, size_t size,
                                  Allocator* allocator) {
    const void* end_ptr = reinterpret_cast<const uint8*>(memory_ptr) + size;
    allocations_.push_back(AllocationData(MemoryRange(memory_ptr, end_ptr),
                                          allocator,
                                          AllocationData::kPlacement));
  }

  // Adds a DeallocationData instance to the deallocations_ vector.
  void AddDeallocationData(const void *memory_ptr, Allocator* allocator) {
    deallocations_.push_back(DeallocationData(memory_ptr, allocator));
  }

  // If instance_ptr (which typically corresponds to an Allocatable instance)
  // is within the memory range of any current AllocationData, this sets
  // allocator_out to its Allocator, sets memory_ptr_out to the address of the
  // memory chunk containing it, and returns true. Otherwise, it just returns
  // false.
  bool FindAllocationData(const void* instance_ptr, Allocator** allocator_out,
                          const void** memory_ptr_out);

  // Finds the current DeallocationData whose memory pointer is equal to
  // memory_ptr, then removes it and returns its Allocator. This should never
  // return NULL.
  const AllocatorPtr FindDeallocationData(const void* memory_ptr);

  // Returns the placement Allocator.
  Allocator* GetPlacementAllocator() {
    return placement_allocator_;
  }

  // Sets the placement Allocator.
  void SetPlacementAllocator(Allocator* allocator) {
    placement_allocator_ = allocator;
  }

 private:
  // A memory address range in an AllocationData instance.
  struct MemoryRange {
    MemoryRange(const void* min_in, const void* max_in)
        : min_address(min_in), max_address(max_in) {}
    const void* min_address;
    const void* max_address;
  };

  // Stores data about an allocation to communicate to Allocatable constructors
  // for all instances within an allocated chunk of memory.
  struct AllocationData {
    enum AllocationType {
      kNew,
      kPlacement
    };
    AllocationData(const MemoryRange& memory_range_in, Allocator* allocator_in,
                   AllocationType allocation_type_in)
        : memory_range(memory_range_in),
          allocator(allocator_in),
          allocation_type(allocation_type_in) {}
    // The range in memory of the allocated data.
    MemoryRange memory_range;
    // The Allocator that was used to allocate the memory.
    Allocator* allocator;
    // Whether this is a placement allocation or not. Placement allocations
    // can store an allocator, but should not be used with Delete, since the
    // memory was allocated by another mechanism.
    AllocationType allocation_type;
  };

  // Stores data about a deallocation to communicate to Delete().
  struct DeallocationData {
    DeallocationData(const void* memory_ptr_in, Allocator* allocator_in)
        : memory_ptr(memory_ptr_in), allocator(allocator_in) {}
    // Pointer to the beginning of the allocated memory.
    const void* memory_ptr;
    // The Allocator that was used to allocate the memory. This is a SharedPtr
    // rather than a raw pointer to ensure that the Allocator instance outlives
    // the Allocatable (just in case the Allocatable holds the last reference).
    AllocatorPtr allocator;
  };

  // Allocation and deallocation data vectors. These cannot be AllocVectors
  // since on some platforms instantiating an AllocVector causes an allocation,
  // which results in continuously trying to recreate the Helper. Note that
  // these vectors are nearly always very small, usually only a few elements
  // long.
  std::vector<AllocationData> allocations_;
  std::vector<DeallocationData> deallocations_;
  Allocator* placement_allocator_;
};

bool Allocatable::Helper::FindAllocationData(const void* instance_ptr,
                                             Allocator** allocator_out,
                                             const void** memory_ptr_out) {
  const size_t num_allocations = allocations_.size();
  for (size_t i = 0; i < num_allocations; ++i) {
    AllocationData& ad = allocations_[i];
    const void* memory_start = ad.memory_range.min_address;
    const void* memory_end = ad.memory_range.max_address;
    if (memory_start <= instance_ptr && instance_ptr < memory_end) {
      *allocator_out = ad.allocator;
      // If this is a placement allocation, then don't set the memory pointer.
      // The allocatable must have its memory deleted manually, not by operator
      // delete.
      *memory_ptr_out =
          ad.allocation_type == AllocationData::kNew ? memory_start : nullptr;
      *memory_ptr_out = memory_start;
      // Remove the AllocationData.
      allocations_.erase(allocations_.begin() + i);
      return true;
    }
  }
  // Not found; the Allocatable array must have been declared on the stack or
  // could be an STL placement allocation.
  if (placement_allocator_) {
    *allocator_out = placement_allocator_;
    *memory_ptr_out = nullptr;
    return true;
  }
  // The Allocatable must have been declared on the stack.
  return false;
}

const AllocatorPtr Allocatable::Helper::FindDeallocationData(
    const void* memory_ptr) {
  DCHECK(memory_ptr);
  const size_t num_deallocations = deallocations_.size();
  AllocatorPtr allocator;
  for (size_t i = 0; i < num_deallocations; ++i) {
    DeallocationData& dd = deallocations_[i];
    if (dd.memory_ptr == memory_ptr) {
      allocator = dd.allocator;
      deallocations_.erase(deallocations_.begin() + i);
      break;
    }
  }
  // This should never return NULL, since this function is not called if
  // memory_ptr is NULL (meaning the allocation was on the stack).
  DCHECK(allocator.Get());
  return allocator;
}

//-----------------------------------------------------------------------------
//
// Allocatable class functions.
//
//-----------------------------------------------------------------------------

Allocatable::Allocatable() {
  Construct();
}

Allocatable::Allocatable(const Allocatable&) {
  Construct();
}

void Allocatable::Construct() {
  // Access the Allocator that was stored in the Helper by New(). If it is not
  // found, assume this was allocated on the stack and leave a NULL Allocator.
  Allocator* allocator;
  const void* memory_ptr;
  if (GetHelper()->FindAllocationData(this, &allocator, &memory_ptr)) {
    allocator_.Reset(allocator);
    memory_ptr_ = memory_ptr;
  } else {
    memory_ptr_ = nullptr;
  }
}

Allocatable::Allocatable(const AllocatorPtr& allocator_in)
    : allocator_(allocator_in), memory_ptr_(nullptr) {
  Allocator* allocator;
  const void* memory_ptr;
  // Avoid warnings when the below DCHECK is optimized out.
  (void)allocator;
  (void)memory_ptr;
  // Ensure that this instance was created on the stack.
  DCHECK(!GetHelper()->FindAllocationData(this, &allocator, &memory_ptr))
      << "Allocatable can take an AllocatorPtr in its constructor only when "
         "created on the stack";
}

Allocatable::~Allocatable() {
  // If memory_ptr_ was set by the constructor, add a DeallocationData to the
  // Helper so that Delete() knows which Allocator to use.
  if (memory_ptr_) {
    Helper* helper = GetHelper();
    if (helper) {
      helper->AddDeallocationData(memory_ptr_, allocator_.Get());
    }
  }
}

void* Allocatable::New(size_t size, const AllocatorPtr& allocator) {
  const AllocatorPtr& a = AllocationManager::GetNonNullAllocator(allocator);
  DCHECK(a.Get());
  void* memory_ptr = a->AllocateMemory(size);
  // If memory is returned, store an entry in the Helper so the
  // constructor can get the Allocator pointer.
  if (memory_ptr)
    GetHelper()->AddAllocationData(memory_ptr, size, a.Get());

  return memory_ptr;
}

void Allocatable::Delete(void* memory_ptr) {
  // memory_ptr is non-NULL if this was not allocated on the stack.
  if (memory_ptr) {
    // Find the correct Allocator to deallocate the memory.
    AllocatorPtr allocator = GetHelper()->FindDeallocationData(memory_ptr);
    DCHECK(allocator.Get());
    allocator->DeallocateMemory(memory_ptr);
  }
}

void* Allocatable::PlacementNew(size_t size, const AllocatorPtr& allocator,
                                void* memory_ptr) {
  if (memory_ptr)
    GetHelper()->AddPlacementAllocationData(memory_ptr, size, allocator.Get());
  return memory_ptr;
}

Allocator* Allocatable::GetPlacementAllocator() {
  return GetHelper()->GetPlacementAllocator();
}

void Allocatable::SetPlacementAllocator(Allocator* allocator) {
  GetHelper()->SetPlacementAllocator(allocator);
}

Allocatable::Helper* Allocatable::GetHelper() {
  // Access the Helper instance from thread-local storage so it is unique per
  // thread and does not have to lock for thread-safety.
  ION_DECLARE_SAFE_STATIC_POINTER(ThreadLocalObject<Helper>, s_helper);
  return s_helper->Get();
}

}  // namespace base
}  // namespace ion
