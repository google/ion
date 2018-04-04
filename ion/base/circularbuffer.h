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

#include <iterator>

#include "ion/base/invalid.h"
#include "ion/base/stlalloc/allocunorderedmap.h"
#include "ion/base/stlalloc/allocvector.h"

namespace ion {
namespace base {

// Simple circular buffer class that has fixed capacity and does not grow
// automatically.
template<typename T>
class CircularBuffer : public ion::base::Allocatable {
 public:
  using Buffer = ion::base::AllocVector<T>;
  using difference_type = typename Buffer::difference_type;
  using size_type = typename Buffer::size_type;
  using value_type = typename Buffer::value_type;
  using pointer = typename Buffer::pointer;
  using reference = typename Buffer::reference;
  using const_reference = typename Buffer::const_reference;

  // Create CircularBuffer with maximum size |capacity| allocated from
  // |alloc|. If |do_reserve| is true, the full buffer capacity is allocated at
  // the time of instantiation.
  CircularBuffer(
      size_t capacity, const ion::base::AllocatorPtr& alloc, bool do_reserve);

  // Copy constructor for CircularBuffer with optional parameter to initialise
  // to a new size.
  // @param circular_buffer The source buffer to copy from.
  // @param alloc Allocator for the circular buffer.
  // @param capacity If 0, this buffer will use the same capacity as the source
  //     buffer, otherwise it will use the capacity provided.  If a value
  //     is passed that is non zero and less than the source buffer's
  //     capacity this object will be undefined, and will DCHECK in debug.
  CircularBuffer(const CircularBuffer& source_buffer,
                 const ion::base::AllocatorPtr& alloc, size_t capacity = 0);

  // Add an item to the buffer. If the buffer is at full capacity, this
  // overwrites the oldest element in the buffer.
  void AddItem(const T& item) {
    // 
    // vector.
    if (buffer_.size() < capacity_) {
      buffer_.push_back(item);
    } else {
      if (num_items_ == capacity_) {
        DropOldestItem();
      }
      buffer_[next_pos_] = item;
    }
    next_pos_ = (next_pos_ + 1) % capacity_;
    ++num_items_;
  }

  // Drops the oldest num_item items from the buffer.
  // The size of the buffer must be larger than num_items when calling this.
  // @param num_items_to_drop The number of items to drop.
  void DropOldestItems(size_t num_items_to_drop = 1) {
    DCHECK_GE(num_items_, num_items_to_drop);
    head_pos_ = (head_pos_ + num_items_to_drop) % capacity_;
    num_items_ -= num_items_to_drop;
  }

  // Drops the oldest item from the buffer.
  // The size of the buffer must be non-zero when calling this.
  void DropOldestItem() {
    DropOldestItems(1);
  }

  // Return the item at position i. A position of 0 refers to the oldest item
  // recorded in the buffer, while the position (GetSize() - 1) is the newest.
  const_reference GetItem(size_t i) const {
    DCHECK_LT(i, buffer_.size());
    DCHECK_LT(i, capacity_);
    return buffer_[(head_pos_ + i) % capacity_];
  }

  // Get the current number of items in the buffer.
  size_t GetSize() const { return num_items_; }

  // Get the total capacity of the buffer.
  size_t GetCapacity() const { return capacity_; }

  // Returns true if the number of elements held equals the capacity.
  bool IsFull() const { return num_items_ == capacity_; }

  // Returns true if there are no elements in the buffer.
  bool IsEmpty() const { return num_items_ == 0; }

  // Returns the first (oldest) item in the buffer.
  const T& GetOldest() const { return GetItem(0); }

  // Returns the last (newest) item in the buffer.
  const T& GetNewest() const { return GetItem(num_items_ - 1); }

  // Clear the buffer.
  void Clear() {
    buffer_.clear();
    num_items_ = 0;
    head_pos_ = 0;
    next_pos_ = 0;
  }

  // Base class for iterator types.  Currently the only iterator supported is
  // ConstIterator, but there may be a non-const iterator in the future if
  // CircularBuffer supports a non-const GetItem.
  // This class uses the CRTP style, where the derived type is known to this
  // base class.  This enables the implementation of common operators in the
  // base class, where those operators return values and references to the
  // derived type.
  // In addition, TContainer is given as a template parameter, as it may be
  // a pointer to a const or non-const CircularBuffer.
  template <typename TDerived, typename TContainer>
  class IteratorBase
      : public std::iterator<std::random_access_iterator_tag, T> {
   public:
    IteratorBase() : owner_(nullptr), index_(kInvalidIndex) {}

    IteratorBase(TContainer* owner, size_t index)
        : owner_(owner), index_(index) {}

    friend TDerived& operator-=(TDerived& iter, difference_type diff) {
      DCHECK_NE(nullptr, iter.owner_);
      DCHECK_GE(iter.owner_->GetSize(), iter.index_ - diff);
      iter.index_ -= diff;
      return iter;
    }

    friend TDerived& operator+=(TDerived& iter, difference_type diff) {
      DCHECK_NE(nullptr, iter.owner_);
      DCHECK_GE(iter.owner_->GetSize(), iter.index_ + diff);
      iter.index_ += diff;
      return iter;
    }

    friend difference_type operator-(const TDerived& iter1,
                                     const TDerived& iter2) {
      DCHECK_NE(nullptr, iter1.owner_);
      DCHECK_NE(nullptr, iter2.owner_);
      DCHECK_EQ(iter1.owner_, iter2.owner_);
      DCHECK_GE(iter1.owner_->GetSize(), iter1.index_ - iter2.index_);
      return iter1.index_ - iter2.index_;
    }

    friend TDerived operator-(const TDerived& iter, difference_type diff) {
      DCHECK_NE(nullptr, iter.owner_);
      DCHECK_GE(iter.owner_->GetSize(), iter.index_ - diff);
      return TDerived(iter.owner_, iter.index_ - diff);
    }

    friend TDerived operator+(const TDerived& iter, difference_type diff) {
      DCHECK_NE(nullptr, iter.owner_);
      DCHECK_GE(iter.owner_->GetSize(), iter.index_ + diff);
      return TDerived(iter.owner_, iter.index_ + diff);
    }

    friend TDerived operator+(difference_type diff, const TDerived& iter) {
      DCHECK_NE(nullptr, iter.owner_);
      DCHECK_GE(iter.owner_->GetSize(), iter.index_ + diff);
      return TDerived(iter.owner_, iter.index_ + diff);
    }

    const TDerived& operator++() {
      DCHECK_NE(nullptr, owner_);
      DCHECK_GT(owner_->GetSize(), index_);
      index_++;
      return static_cast<const TDerived&>(*this);
    }

    TDerived operator++(int) {
      auto result = static_cast<TDerived&>(*this);
      ++(*this);
      return result;
    }

    TDerived& operator--() {
      DCHECK_NE(nullptr, owner_);
      DCHECK_LT(0, index_);
      DCHECK_GE(owner_->GetSize(), index_);
      index_--;
      return static_cast<TDerived&>(*this);
    }

    TDerived operator--(int) {
      auto result = static_cast<TDerived&>(*this);
      --(*this);
      return result;
    }

    friend bool operator==(const TDerived& lhs, const TDerived& rhs) {
      return lhs.owner_ == rhs.owner_ && lhs.index_ == rhs.index_;
    }

    friend bool operator!=(const TDerived& lhs, const TDerived& rhs) {
      return !(rhs == lhs);
    }

   protected:
    TContainer* owner_;
    size_t index_;
  };

  // ConstIterator is the constant iterator class.  It is mostly implemented
  // within IteratorBase.
  class ConstIterator
      : public IteratorBase<ConstIterator, const CircularBuffer> {
   public:
    typedef const T& reference;
    typedef const T* pointer;

    ConstIterator(const CircularBuffer* owner, size_t index)
        : IteratorBase<ConstIterator, const CircularBuffer>(owner, index) {}

    reference operator*() const {
      DCHECK_NE(nullptr, this->owner_);
      DCHECK_GT(this->owner_->GetSize(), this->index_);
      return this->owner_->GetItem(this->index_);
    }

    pointer operator->() const {
      DCHECK_NE(nullptr, this->owner_);
      DCHECK_GT(this->owner_->GetSize(), this->index_);
      return &this->owner_->GetItem(this->index_);
    }

    reference operator[](size_t offset) const {
      DCHECK_NE(nullptr, this->owner_);
      DCHECK_GT(this->owner_->GetSize(), this->index_ + offset);
      return this->owner_->GetItem(this->index_ + offset);
    }
  };

  // Support C++ standard typedefs for iterators.
  using const_iterator = ConstIterator;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  // Retrieves the begin const iterator for the CircularBuffer.
  const_iterator cbegin() const { return ConstIterator(this, 0); }

  // Retrieves the end const iterator for the CircularBuffer.
  const_iterator cend() const { return ConstIterator(this, GetSize()); }

  // Retrieves the begin const iterator for the CircularBuffer.
  const_iterator begin() const { return cbegin(); }

  // Retrieves the end const iterator for the CircularBuffer.
  const_iterator end() const { return cend(); }

  // Retrieves the begin const reverse iterator for the CircularBuffer.
  const_reverse_iterator crbegin() const {
    return std::reverse_iterator<const_iterator>(cend());
  }

  // Retrieves the end const reverse iterator for the CircularBuffer.
  const_reverse_iterator crend() const {
    return std::reverse_iterator<const_iterator>(cbegin());
  }

  // Retrieves the begin const reverse iterator for the CircularBuffer.
  const_reverse_iterator rbegin() const { return crbegin(); }

  // Retrieves the end const reverse iterator for the CircularBuffer.
  const_reverse_iterator rend() const { return crend(); }

 private:
  // Maximum buffer capacity.
  const size_t capacity_;

  // The number of items stored in the buffer.
  size_t num_items_;

  // The "head" of the buffer (the oldest element).
  size_t head_pos_;

  // The next "free" position in the buffer to write to.
  size_t next_pos_;

  // Underlying vector.
  Buffer buffer_;
};

template <typename T>
CircularBuffer<T>::CircularBuffer(size_t capacity,
                                  const ion::base::AllocatorPtr& alloc,
                                  bool do_reserve)
    : capacity_(capacity),
      num_items_(0),
      head_pos_(0),
      next_pos_(0),
      buffer_(alloc) {
  if (do_reserve) {
    // Pre-allocate the buffer to avoid lengthy re-allocation.
    buffer_.reserve(capacity_);
  }
}

template <typename T>
CircularBuffer<T>::CircularBuffer(const CircularBuffer<T>& source_buffer,
                                  const ion::base::AllocatorPtr& alloc,
                                  size_t capacity)
    : capacity_([&]() {
        if (capacity != 0) {
          DCHECK(capacity >= source_buffer.GetCapacity())
              << "CircularBuffer copy constructor invoked with invalid "
                 "capacity.";
          return capacity;
        } else {
          return source_buffer.GetCapacity();
        }
      }()),
      num_items_(0),
      head_pos_(0),
      next_pos_(0),
      buffer_(alloc) {
  for (const T& source_item : source_buffer) {
    AddItem(source_item);
  }
}

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_CIRCULARBUFFER_H_
