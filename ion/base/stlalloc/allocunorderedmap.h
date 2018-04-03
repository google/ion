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

#ifndef ION_BASE_STLALLOC_ALLOCUNORDEREDMAP_H_
#define ION_BASE_STLALLOC_ALLOCUNORDEREDMAP_H_

#include <unordered_map>

#include "ion/base/allocatable.h"
#include "ion/base/allocationmanager.h"
#include "ion/base/stlalloc/stlallocator.h"

namespace ion {
namespace base {

// This class can be used in place of std::unordered_map to allow an Ion
// Allocator to be used for memory allocation. There is a constructor that
// takes the Allocator to be used as an argument. For example:
//
//   AllocUnorderedMap<int, float> local_map(allocator);
//
// AllocUnorderedMap also provides a convenience constructor to declare class
// members of a derived Allocatable class. It uses the same Allocator that was
// used for the Allocatable. For example:
//
//   class MyClass : public Allocatable {
//    public:
//     // The map will use the same Allocator as the MyClass instance.
//     MyClass() : member_map_(*this) {}
//     ...
//    private:
//     AllocUnorderedMap<int, float> member_map_;
//   };

template <typename Key,
          typename Value,
          typename Hash = ::std::hash<Key>,
          typename Pred = ::std::equal_to<Key> >
class AllocUnorderedMap : public std::unordered_map<
  Key, Value, Hash, Pred, StlAllocator<std::pair<const Key, Value> > > {
 public:
  typedef std::pair<const Key, Value> PairType;
  typedef StlAllocator<PairType> AllocType;
  typedef std::unordered_map<Key, Value, Hash, Pred, AllocType> MapType;

  explicit AllocUnorderedMap(const AllocatorPtr& alloc)
      : MapType(kBucketCountHint, Hash(), Pred(),
                AllocType(AllocationManager::GetNonNullAllocator(alloc))) {}
  explicit AllocUnorderedMap(const Allocatable& owner)
      : MapType(kBucketCountHint, Hash(), Pred(),
                AllocType(owner.GetNonNullAllocator())) {}

  // These constructors can be used to copy from "STL iterables": anything that
  // provides a pair of compatible iterators via begin() and end().
  template <typename ContainerT>
  AllocUnorderedMap(const AllocatorPtr& alloc, const ContainerT& from)
      : MapType(from.begin(), from.end(), kBucketCountHint, Hash(), Pred(),
                AllocType(AllocationManager::GetNonNullAllocator(alloc))) {}
  template <typename ContainerT>
  AllocUnorderedMap(const Allocatable& owner, const ContainerT& from)
      : MapType(from.begin(), from.end(), kBucketCountHint, Hash(), Pred(),
                AllocType(owner.GetNonNullAllocator())) {}

 private:
  // C++11 leaves the default bucket count (hint) as an implementation-defined
  // value.  For concreteness we specify libstdc++'s value here.
  // (see b/18172453 for what various platforms defaulted to as of 2014Q3)
  enum { kBucketCountHint = 10 };
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_STLALLOC_ALLOCUNORDEREDMAP_H_
