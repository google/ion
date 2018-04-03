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

#ifndef ION_BASE_ARRAY2_H_
#define ION_BASE_ARRAY2_H_

#include "ion/base/allocatable.h"
#include "ion/base/invalid.h"
#include "ion/base/logging.h"
#include "ion/base/stlalloc/allocvector.h"

namespace ion {
namespace base {

// Simple rectangular 2D array class with range-checked indexing, templatized
// by the element type.
template <typename T> class Array2 : public Allocatable {
 public:
  // The default constructor creates an empty (0x0) array.
  Array2() : width_(0), height_(0), data_(GetAllocator()) {}

  // Constructor that creates an array of specified size with undefined
  // elements.
  Array2(size_t width, size_t height)
      : width_(width), height_(height), data_(GetAllocator()) {
    data_.resize(width * height);
  }

  // Constructor that creates an array of specified size with elements all set
  // to initial_value.
  Array2(size_t width, size_t height, const T& initial_value)
      : width_(width),
        height_(height),
        data_(GetAllocator(), width * height, initial_value) {
  }

  ~Array2() override {}

  size_t GetWidth() const { return width_; }
  size_t GetHeight() const { return height_; }
  size_t GetSize() const { return data_.size(); }

  // Sets one element of the array. Does nothing but log an error message and
  // return false if the indices are not valid.
  bool Set(size_t column, size_t row, const T& val) {
    const size_t index = GetIndex(column, row);
    if (index == base::kInvalidIndex) {
      return false;
    } else {
      data_[index] = val;
      return true;
    }
  }

  // Returns the indexed element of the array. Logs an error message and
  // returns an invalid reference if the indices are not valid.
  const T& Get(size_t column, size_t row) const {
    const size_t index = GetIndex(column, row);
    return index == base::kInvalidIndex ?
        base::InvalidReference<T>() : data_[index];
  }

  // Returns a pointer the indexed element of the array. Logs an error message
  // and returns a NULL pointer if the indices are not valid.
  T* GetMutable(size_t column, size_t row) {
    const size_t index = GetIndex(column, row);
    return index == base::kInvalidIndex ? nullptr : &data_[index];
  }

 private:
  // Converts column and row into a vector index. Logs an error and returns
  // base::kInvalidIndex if the indices are bad.
  size_t GetIndex(size_t column, size_t row) const {
    if (column >= width_ || row >= height_) {
      LOG(ERROR) << "Bad indices (" << column << ", " << row
                 << ") for Array2 of size " << width_ << " x " << height_;
      return base::kInvalidIndex;
    }
    return row * width_ + column;
  }

  size_t width_;
  size_t height_;
  AllocVector<T> data_;
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_ARRAY2_H_
