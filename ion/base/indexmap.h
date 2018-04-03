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

#ifndef ION_BASE_INDEXMAP_H_
#define ION_BASE_INDEXMAP_H_

#include "ion/base/logging.h"

namespace ion {
namespace base {

// This template class can be used to map between two kinds of indices when
// the following assumptions apply:
//  - There are N potential values for both types of index.
//  - The N values of OrderedIndexType range from 0 to N-1 and are presented in
//    order.
//  - The N values of UnorderedIndexType are in an unspecified range and may or
//    may not be presented in order.
//
// Mapping from an OrderedIndexType to an UnorderedIndexType is constant-time,
// while mapping the other way around is linear.
//
// This class is very light-weight and is intended to be constructed as a
// temporary variable when needed.
template <typename OrderedIndexType, typename UnorderedIndexType>
class IndexMap {
 public:
  // The map is initialized with an array of N UnorderedIndexType values that
  // directly correspond to a 0-based array of OrderedIndexType, in the same
  // order. It is assumed that the lifetime of the passed array is at least as
  // long as the IndexMap instance's, since the IndexMap does not copy the
  // array values.
  IndexMap(const UnorderedIndexType unordered_indices[], size_t count)
      : unordered_indices_(unordered_indices),
        count_(count) {}

  // Returns the count passed to the constructor.
  size_t GetCount() const { return count_; }

  // Returns the UnorderedIndexType corresponding to the given OrderedIndexType
  // value. This is a constant-time operation.
  UnorderedIndexType GetUnorderedIndex(OrderedIndexType ordered_index) const {
    DCHECK_LE(0, static_cast<int>(ordered_index));
    DCHECK_GT(count_, static_cast<size_t>(ordered_index));
    return unordered_indices_[ordered_index];
  }

  // Returns the OrderedIndexType corresponding to the given UnorderedIndexType
  // value. This is a linear-time operation. This will DCHECK if the index is
  // not found.
  OrderedIndexType GetOrderedIndex(UnorderedIndexType unordered_index) const {
    int ordered_index = -1;
    for (size_t i = 0; i < count_; ++i) {
      if (unordered_indices_[i] == unordered_index) {
        ordered_index = static_cast<int>(i);
        break;
      }
    }
    DCHECK_GE(ordered_index, 0) << "IndexMap: Invalid unordered index "
                                << unordered_index << " does not match any "
                                << "ordered index.";
    return static_cast<OrderedIndexType>(ordered_index);
  }

 private:
  const UnorderedIndexType* unordered_indices_;
  const size_t count_;
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_INDEXMAP_H_
