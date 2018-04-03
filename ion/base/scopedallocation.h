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

#ifndef ION_BASE_SCOPEDALLOCATION_H_
#define ION_BASE_SCOPEDALLOCATION_H_

#include <functional>

#include "base/macros.h"
#include "ion/base/allocationmanager.h"
#include "ion/base/allocator.h"
#include "ion/base/datacontainer.h"

namespace ion {
namespace base {

// This template class can be used in situations where you want to allocate an
// object that is not necessarily derived from Allocatable, but you still want
// to use an Allocator for the memory. Note that T must be
// default-constructable.
//
// Examples:
//   // Allocating a local variable:
//   ScopedAllocation<Thing> s1(allocator);  // Creates a Thing instance.
//   DoSomethingWithAThing(s1.Get());        // Accesses the instance.
//   // When s1 goes out of scope, the Thing is deleted using the Allocator.
//
//   // Allocating a local POD array:
//   ScopedAllocation<char> s2(allocator, 9);  // Allocates an array of size 9.
//   printf(s2.Get());                         // Accesses the array.
//   // When s2 goes out of scope, the array is deleted using the Allocator.
//
//   // Transferring the contents of a ScopedAllocation to a DataContainer that
//   // uses the same Allocator. (This can be useful for allocating an array of
//   // objects with an Allocator for use in a DataContainer.):
//   ScopedAllocation<int> s3(allocator, 9);
//   ...  // Fill in the data inside the array.
//   DataContainerPtr dc = s3.TransferToDataContainer(true);
//   // s3 is now empty; dc contains the data.
//
// Note that a lifetime can be used instead of an Allocator pointer, in which
// case the default allocator for that lifetime is used.
template <typename T> class ION_API ScopedAllocation {
 public:
  // This constructor allocates a single T instance using allocator, or the
  // default allocator if a NULL pointer is passed.
  explicit ScopedAllocation(const AllocatorPtr& allocator) {
    Init(allocator, 1U);
  }

  // This constructor allocates a single T instance using the default allocator
  // for lifetime.
  explicit ScopedAllocation(AllocationLifetime lifetime) {
    Init(AllocationManager::GetDefaultAllocatorForLifetime(lifetime), 1U);
  }

  // This constructor allocates count T instances using allocator, or
  // the default allocator if a NULL pointer is passed.
  ScopedAllocation(const AllocatorPtr& allocator, size_t count) {
    Init(allocator, count);
  }

  // This constructor allocates count T instances the default allocator
  // for lifetime.
  ScopedAllocation(AllocationLifetime lifetime, size_t count) {
    Init(AllocationManager::GetDefaultAllocatorForLifetime(lifetime), count);
  }

  // The destructor deletes the instance(s) using the allocator.
  ~ScopedAllocation() {
    DeleteData(instance_ptr_, count_, allocator_, memory_ptr_);
  }

  // Returns a pointer to the allocated T instance(s). This returns NULL if a
  // count of 0 was passed to the constructor.
  T* Get() const { return instance_ptr_; }

  // Creates a DataContainer of the same type and transfers the data from the
  // ScopedAllocation to it. After this call, the ScopedAllocation will be
  // empty, meaning that Get() will return a NULL pointer.
  DataContainerPtr TransferToDataContainer(bool is_wipeable) {
    return DataContainer::Create<T>(
        Release(),
        std::bind(&ScopedAllocation<T>::Deleter, std::placeholders::_1, count_,
                  allocator_, memory_ptr_),
        is_wipeable, allocator_);
  }

 private:
  // Initializes a ScopedAllocation.
  void Init(const AllocatorPtr& allocator, size_t count) {
    allocator_ = AllocationManager::GetNonNullAllocator(allocator);
    count_ = count;
    if (count_) {
      // Allocate the memory for the instance(s). Always use the new[]
      // operator, even if count is 1. Add space for the count of instances.
      memory_ptr_ =
          allocator_->AllocateMemory(sizeof(size_t) + count_ * sizeof(T));
      instance_ptr_ = new(memory_ptr_) T[count_];
    } else {
      memory_ptr_ = nullptr;
      instance_ptr_ = nullptr;
    }
  }

  // Tells the ScopedAllocation that it no longer owns the memory for its
  // data. This is used when transferring the allocated memory to another
  // object, such as a DataContainer, which then becomes responsible for
  // deleting the data.
  T* Release() {
    T* ptr = instance_ptr_;
    instance_ptr_ = nullptr;
    // Leave memory_ptr_ and count alone so that DeleteData() works properly.
    return ptr;
  }

  // Deleter function passed to a DataContainer by TransferToDataContainer().
  static void Deleter(void* data, size_t count, const AllocatorPtr& allocator,
                      void* memory_ptr) {
    DeleteData(reinterpret_cast<T*>(data), count, allocator, memory_ptr);
  }

  // Deletes data created by the ScopedAllocation, calling destructors properly
  // first.
  static void DeleteData(T* data, size_t count, const AllocatorPtr& allocator,
                         void* memory_ptr) {
    if (data) {
      // Call the destructor for each instance.
      for (size_t i = 0; i < count; ++i)
        data[i].~T();
      // Free up the memory.
      if (allocator.Get())
        allocator->DeallocateMemory(memory_ptr);
    }
  }

  // Allocator used to create the instance(s).
  AllocatorPtr allocator_;
  // Pointer to the allocated memory.
  void* memory_ptr_;
  // Pointer to the instance or array of instances, which is different than the
  // memory pointer on some platforms.
  T* instance_ptr_;
  // The number of instances allocated.
  size_t count_;

  DISALLOW_COPY_AND_ASSIGN(ScopedAllocation);
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_SCOPEDALLOCATION_H_
