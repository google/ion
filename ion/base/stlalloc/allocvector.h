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

#ifndef ION_BASE_STLALLOC_ALLOCVECTOR_H_
#define ION_BASE_STLALLOC_ALLOCVECTOR_H_

#include <vector>

#include "ion/base/allocatable.h"
#include "ion/base/allocationmanager.h"
#include "ion/base/stlalloc/stlallocator.h"

namespace ion {
namespace base {

// This class can be used in place of std::vector to allow an Ion Allocator to
// be used for memory allocation. There is a constructor that takes the
// Allocator to be used as an argument. For example:
//
//   AllocVector<int> vec(allocator);
//
// AllocVector also provides a convenience constructor to declare class members
// of a derived Allocatable class. It uses the same Allocator that was used for
// the Allocatable. For example:
//
//   class MyClass : public Allocatable {
//    public:
//     // The vector will use the same Allocator as the MyClass instance.
//     MyClass() : member_vec_(*this) {}
//     ...
//    private:
//     AllocVector<int> member_vec_;
//   };

template <typename T>
class AllocVector : public std::vector<T, StlAllocator<T> > {
 public:
  typedef StlAllocator<T> AllocType;
  typedef std::vector<T, AllocType> VectorType;
  explicit AllocVector(const AllocatorPtr& alloc)
      : VectorType(AllocType(AllocationManager::GetNonNullAllocator(alloc))) {}
  explicit AllocVector(const Allocatable& owner)
      : VectorType(AllocType(owner.GetNonNullAllocator())) {}

  AllocVector(const AllocatorPtr& alloc, size_t n, const T& val)
      : VectorType(n, val,
                   AllocType(AllocationManager::GetNonNullAllocator(alloc))) {}
  AllocVector(const Allocatable& owner, size_t n, const T& val)
      : VectorType(n, val, AllocType(owner.GetNonNullAllocator())) {}

  template <class IteratorT>
  AllocVector(const AllocatorPtr& alloc, IteratorT first, IteratorT last)
      : VectorType(first, last,
                   AllocType(AllocationManager::GetNonNullAllocator(alloc))) {}
  template <class IteratorT>
  AllocVector(const Allocatable& owner, IteratorT first, IteratorT last)
      : VectorType(first, last,
                   AllocType(owner.GetNonNullAllocator())) {}

  // These constructors can be used to copy from "STL iterables": anything that
  // provides a pair of compatible iterators via begin() and end().
  template <typename ContainerT>
  AllocVector(const AllocatorPtr& alloc, const ContainerT& from)
      : VectorType(from.begin(), from.end(),
                   AllocType(AllocationManager::GetNonNullAllocator(alloc))) {}
  template <typename ContainerT>
  AllocVector(const Allocatable& owner, const ContainerT& from)
      : VectorType(from.begin(), from.end(),
                   AllocType(owner.GetNonNullAllocator())) {}

  // These constructors allow AllocVector to be initialized using c++11's
  // list initialization:
  // AllocVector<int> vector(allocator, {1, 2, 3, 4});
  AllocVector(const AllocatorPtr& alloc, std::initializer_list<T> init)
      : VectorType(init,
                   AllocType(AllocationManager::GetNonNullAllocator(alloc))) {}
  AllocVector(const Allocatable& owner, std::initializer_list<T> init)
      : VectorType(init,
                   AllocType(owner.GetNonNullAllocator())) {}
};

// InlinedAllocVectoris a similar to AllocVector, but uses inlined storage for
// its first N elements, then uses an Ion Allocator if that size is exceeded.
template <typename T, int N>
class InlinedAllocVector : public std::vector<T, StlInlinedAllocator<T, N>> {
 public:
  typedef StlInlinedAllocator<T, N> AllocType;
  typedef std::vector<T, AllocType> VectorType;
  typedef std::vector<T, StlInlinedAllocator<T, N>> BaseType;
  explicit InlinedAllocVector(const AllocatorPtr& alloc)
      : VectorType(AllocType(AllocationManager::GetNonNullAllocator(alloc))) {}
  explicit InlinedAllocVector(const Allocatable& owner)
      : VectorType(AllocType(owner.GetNonNullAllocator())) {}

  InlinedAllocVector(const AllocatorPtr& alloc, size_t n, const T& val = T())
      : VectorType(n, val,
                   AllocType(AllocationManager::GetNonNullAllocator(alloc))) {}
  InlinedAllocVector(const Allocatable& owner, size_t n, const T& val = T())
      : VectorType(n, val, AllocType(owner.GetNonNullAllocator())) {}

  template <class IteratorT>
  InlinedAllocVector(const AllocatorPtr& alloc, IteratorT first, IteratorT last)
      : VectorType(first, last,
                   AllocType(AllocationManager::GetNonNullAllocator(alloc))) {}
  template <class IteratorT>
  InlinedAllocVector(const Allocatable& owner, IteratorT first, IteratorT last)
      : VectorType(first, last,
                   AllocType(owner.GetNonNullAllocator())) {}

  // These constructors can be used to copy from "STL iterables": anything that
  // provides a pair of compatible iterators via begin() and end().
  template <typename ContainerT>
  InlinedAllocVector(const AllocatorPtr& alloc, const ContainerT& from)
      : VectorType(from.begin(), from.end(),
                   AllocType(AllocationManager::GetNonNullAllocator(alloc))) {}
  template <typename ContainerT>
  InlinedAllocVector(const Allocatable& owner, const ContainerT& from)
      : VectorType(from.begin(), from.end(),
                   AllocType(owner.GetNonNullAllocator())) {}

  // Allow copying.
  InlinedAllocVector(const InlinedAllocVector& other)
      : BaseType(other) {}
  InlinedAllocVector& operator=(const InlinedAllocVector& other) {
    *(static_cast<BaseType*>(this)) = static_cast<const BaseType&>(other);
    return *this;
  }

  // Do not allow moving. The inlined allocator's memory is inherently
  // non-movable.
  InlinedAllocVector(InlinedAllocVector&&) = delete;
  InlinedAllocVector& operator=(InlinedAllocVector&&) = delete;
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_STLALLOC_ALLOCVECTOR_H_
