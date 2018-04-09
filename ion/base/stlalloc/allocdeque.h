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

#ifndef ION_BASE_STLALLOC_ALLOCDEQUE_H_
#define ION_BASE_STLALLOC_ALLOCDEQUE_H_

#include <deque>

#include "ion/base/allocatable.h"
#include "ion/base/allocationmanager.h"
#include "ion/base/stlalloc/stlallocator.h"

namespace ion {
namespace base {

// This class can be used in place of std::deque to allow an Ion Allocator to
// be used for memory allocation. There is a constructor that takes the
// Allocator to be used as an argument. For example:
//
//   AllocDeque<int> deq(allocator);
//
// AllocDeque also provides a convenience constructor to declare class members
// of a derived Allocatable class. It uses the same Allocator that was used for
// the Allocatable. For example:
//
//   class MyClass : public Allocatable {
//    public:
//     // The deque will use the same Allocator as the MyClass instance.
//     MyClass() : member_deq_(*this) {}
//     ...
//    private:
//     AllocDeque<int> member_deq_;
//   };

template <typename T>
class AllocDeque : public std::deque<T, StlAllocator<T> > {
 public:
  typedef StlAllocator<T> AllocType;
  typedef std::deque<T, AllocType> DequeType;
  explicit AllocDeque(const AllocatorPtr& alloc)
      : DequeType(AllocType(AllocationManager::GetNonNullAllocator(alloc))) {}
  explicit AllocDeque(const Allocatable& owner)
      : DequeType(AllocType(owner.GetNonNullAllocator())) {}

  // These constructors can be used to copy from "STL iterables": anything that
  // provides a pair of compatible iterators via begin() and end().
  template <typename ContainerT>
  AllocDeque(const AllocatorPtr& alloc, const ContainerT& from)
      : DequeType(from.begin(), from.end(),
                  AllocType(AllocationManager::GetNonNullAllocator(alloc))) {}
  template <typename ContainerT>
  AllocDeque(const Allocatable& owner, const ContainerT& from)
      : DequeType(from.begin(), from.end(),
                  AllocType(owner.GetNonNullAllocator())) {}
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_STLALLOC_ALLOCDEQUE_H_
