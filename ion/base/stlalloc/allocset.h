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

#ifndef ION_BASE_STLALLOC_ALLOCSET_H_
#define ION_BASE_STLALLOC_ALLOCSET_H_

#include <set>

#include "ion/base/allocatable.h"
#include "ion/base/allocationmanager.h"
#include "ion/base/stlalloc/stlallocator.h"

namespace ion {
namespace base {

// This class can be used in place of std::set to allow an Ion Allocator to be
// used for memory allocation. There is a constructor that takes the Allocator
// to be used as an argument. For example:
//
//   AllocSet<int> local_set(allocator);
//
// AllocSet also provides a convenience constructor to declare class members
// of a derived Allocatable class. It uses the same Allocator that was used for
// the Allocatable. For example:
//
//   class MyClass : public Allocatable {
//    public:
//     // The set will use the same Allocator as the MyClass instance.
//     MyClass() : member_set_(*this) {}
//     ...
//    private:
//     AllocSet<int> member_set_;
//   };

template <typename T, typename Compare = std::less<T> >
class AllocSet : public std::set<T, Compare, StlAllocator<T> > {
 public:
  typedef StlAllocator<T> AllocType;
  typedef std::set<T, Compare, AllocType> SetType;
  explicit AllocSet(const AllocatorPtr& alloc)
      : SetType(Compare(),
                AllocType(AllocationManager::GetNonNullAllocator(alloc))) {}
  explicit AllocSet(const Allocatable& owner)
      : SetType(Compare(), AllocType(owner.GetNonNullAllocator())) {}

  // These constructors can be used to copy from "STL iterables": anything that
  // provides a pair of compatible iterators via begin() and end().
  template <typename ContainerT>
  AllocSet(const AllocatorPtr& alloc, const ContainerT& from)
      : SetType(from.begin(), from.end(), Compare(),
                AllocType(AllocationManager::GetNonNullAllocator(alloc))) {}
  template <typename ContainerT>
  AllocSet(const Allocatable& owner, const ContainerT& from)
      : SetType(from.begin(), from.end(), Compare(),
                AllocType(owner.GetNonNullAllocator())) {}

  // These constructors allow AllocSet to be initialized using c++11's
  // list initialization:
  // AllocSet<int> set(allocator, {1, 2, 3, 4});
  AllocSet(const AllocatorPtr& alloc, std::initializer_list<T> init)
      : SetType(init, Compare(),
                AllocType(AllocationManager::GetNonNullAllocator(alloc))) {}
  AllocSet(const Allocatable& owner, std::initializer_list<T> init)
      : SetType(init, Compare(),
                AllocType(owner.GetNonNullAllocator())) {}
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_STLALLOC_ALLOCSET_H_
