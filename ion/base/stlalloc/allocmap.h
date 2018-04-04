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

#ifndef ION_BASE_STLALLOC_ALLOCMAP_H_
#define ION_BASE_STLALLOC_ALLOCMAP_H_

#include <map>

#include "ion/base/allocatable.h"
#include "ion/base/allocationmanager.h"
#include "ion/base/stlalloc/stlallocator.h"

namespace ion {
namespace base {

// This class can be used in place of std::map to allow an Ion Allocator to be
// used for memory allocation. There is a constructor that takes the Allocator
// to be used as an argument. For example:
//
//   AllocMap<int, float> local_map(allocator);
//
// AllocMap also provides a convenience constructor to declare class members
// of a derived Allocatable class. It uses the same Allocator that was used for
// the Allocatable. For example:
//
//   class MyClass : public Allocatable {
//    public:
//     // The map will use the same Allocator as the MyClass instance.
//     MyClass() : member_map_(*this) {}
//     ...
//    private:
//     AllocMap<int, float> member_map_;
//   };

template <typename K, typename V, typename Compare = std::less<K> >
class AllocMap : public std::map<K, V, Compare,
                                 StlAllocator<std::pair<const K, V> > > {
 public:
  typedef std::pair<const K, V> PairType;
  typedef StlAllocator<PairType> AllocType;
  typedef std::map<K, V, Compare, AllocType> MapType;
  explicit AllocMap(const AllocatorPtr& alloc)
      : MapType(Compare(),
                AllocType(AllocationManager::GetNonNullAllocator(alloc))) {}
  explicit AllocMap(const Allocatable& owner)
      : MapType(Compare(), AllocType(owner.GetNonNullAllocator())) {}

  // These constructors can be used to copy from "STL iterables": anything that
  // provides a pair of compatible iterators via begin() and end().
  template <typename ContainerT>
  AllocMap(const AllocatorPtr& alloc, const ContainerT& from)
      : MapType(from.begin(), from.end(), Compare(),
                AllocType(AllocationManager::GetNonNullAllocator(alloc))) {}
  template <typename ContainerT>
  AllocMap(const Allocatable& owner, const ContainerT& from)
      : MapType(from.begin(), from.end(), Compare(),
                AllocType(owner.GetNonNullAllocator())) {}

  // These constructors allow AllocMap to be initialized using c++11's
  // list initialization:
  // AllocMap<int, float> map(allocator, { {1, 10.0f}, {2, 20.0f} });
  AllocMap(const AllocatorPtr& alloc, std::initializer_list<PairType> init)
      : MapType(init, Compare(),
                AllocType(AllocationManager::GetNonNullAllocator(alloc))) {}
  AllocMap(const Allocatable& owner, std::initializer_list<PairType> init)
      : MapType(init, Compare(),
                AllocType(owner.GetNonNullAllocator())) {}
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_STLALLOC_ALLOCMAP_H_
