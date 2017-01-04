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

#ifndef ION_BASE_CIRCULARBUFFER_H_
#define ION_BASE_CIRCULARBUFFER_H_

#include "ion/base/stlalloc/allocunorderedmap.h"
#include "ion/base/stlalloc/allocvector.h"

namespace ion {
namespace base {

// Simple circular buffer class that has fixed capacity and does not grow
// automatically.
template<typename T>
class CircularBuffer : public ion::base::Allocatable {
 public:
  // Create CircularBuffer with maximum size |capacity| allocated from
  // |alloc|. If |do_reserve| is true, the full buffer capacity is allocated at
  // the time of instantiation.
  CircularBuffer(
      size_t capacity, const ion::base::AllocatorPtr& alloc, bool do_reserve);

  // Add an item to the buffer. If the buffer is at full capacity, this
  // overwrites the oldest element in the buffer.
  void AddItem(const T& item) {
    // TODO(user): check to avoid reserving too much space in underlying
    // vector.
    if (buffer_.size() < capacity_) {
      buffer_.push_back(item);
    } else {
      buffer_[next_pos_] = item;
    }
    next_pos_ = (next_pos_ + 1) % capacity_;
  }

  // Return the item at position i. A position of 0 refers to the oldest item
  // recorded in the buffer, while the position (GetSize() - 1) is the newest.
  const T& GetItem(size_t i) const {
    DCHECK(i < buffer_.size());
    DCHECK(i < capacity_);
    if (buffer_.size() < capacity_) {
      return buffer_[i];
    } else {
      size_t wrap_threshold = capacity_ - next_pos_;
      if (i < wrap_threshold) {
        return buffer_[i + next_pos_];
      } else {
        return buffer_[i - wrap_threshold];
      }
    }
  }

  // Get the current number of items in the buffer.
  size_t GetSize() const { return std::min(buffer_.size(), capacity_); }

  // Get the total capacity of the buffer.
  size_t GetCapacity() const { return capacity_; }

  // Clear the buffer.
  void Clear() {
    buffer_.clear();
    next_pos_ = 0;
  }

 private:
  // Maximum buffer capacity.
  const size_t capacity_;

  // The next "free" position in the buffer to write to.
  size_t next_pos_;

  // Underlying vector.
  ion::base::AllocVector<T> buffer_;
};

template<typename T>
CircularBuffer<T>::CircularBuffer(
    size_t capacity, const ion::base::AllocatorPtr& alloc, bool do_reserve)
    : capacity_(capacity), next_pos_(0), buffer_(alloc) {
  if (do_reserve) {
    // Pre-allocate the buffer to avoid lengthy re-allocation.
    buffer_.reserve(capacity_);
  }
}

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_CIRCULARBUFFER_H_
