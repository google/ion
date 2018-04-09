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

#ifndef ION_BASE_STLALLOC_ALLOCUNORDEREDSET_H_
#define ION_BASE_STLALLOC_ALLOCUNORDEREDSET_H_

#include <unordered_set>

#include "ion/base/allocatable.h"
#include "ion/base/allocationmanager.h"
#include "ion/base/stlalloc/stlallocator.h"

namespace ion {
namespace base {

// This class can be used in place of std::unordered_set to allow an Ion
// Allocator to be used for memory allocation. There is a constructor that
// takes the Allocator to be used as an argument. For example:
//
//   AllocUnorderedSet<int> local_set(allocator);
//
// AllocUnorderedSet also provides a convenience constructor to declare class
// members of a derived Allocatable class. It uses the same Allocator that was
// used for the Allocatable. For example:
//
//   class MyClass : public Allocatable {
//    public:
//     // The set will use the same Allocator as the MyClass instance.
//     MyClass() : member_set_(*this) {}
//     ...
//    private:
//     AllocUnorderedSet<int> member_set_;
//   };

template <typename Value,
          typename Hash = ::std::hash<Value>,
          typename Pred = ::std::equal_to<Value> >
class AllocUnorderedSet
    : public std::unordered_set<Value, Hash, Pred, StlAllocator<Value> > {
 public:
  typedef StlAllocator<Value> AllocType;
  typedef std::unordered_set<Value, Hash, Pred, AllocType> SetType;

  explicit AllocUnorderedSet(const AllocatorPtr& alloc)
      : SetType(kBucketCountHint, Hash(), Pred(),
                AllocType(AllocationManager::GetNonNullAllocator(alloc))) {}
  explicit AllocUnorderedSet(const Allocatable& owner)
      : SetType(kBucketCountHint, Hash(), Pred(),
                AllocType(owner.GetNonNullAllocator())) {}

  // These constructors can be used to copy from "STL iterables": anything that
  // provides a pair of compatible iterators via begin() and end().
  template <typename ContainerT>
  AllocUnorderedSet(const AllocatorPtr& alloc, const ContainerT& from)
      : SetType(from.begin(), from.end(), kBucketCountHint, Hash(), Pred(),
                AllocType(AllocationManager::GetNonNullAllocator(alloc))) {}
  template <typename ContainerT>
  AllocUnorderedSet(const Allocatable& owner, const ContainerT& from)
      : SetType(from.begin(), from.end(), kBucketCountHint, Hash(), Pred(),
                AllocType(owner.GetNonNullAllocator())) {}

 private:
  // C++11 leaves the default bucket count (hint) as an implementation-defined
  // value.  For concreteness we specify libstdc++'s value here.
  // (see b/18172453 for what various platforms defaulted to as of 2014Q3)
  enum { kBucketCountHint = 10 };
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_STLALLOC_ALLOCUNORDEREDSET_H_
