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

#ifndef ION_BASE_VECTORDATACONTAINER_H_
#define ION_BASE_VECTORDATACONTAINER_H_

#include "base/macros.h"
#include "ion/base/datacontainer.h"
#include "ion/base/stlalloc/allocvector.h"
#include "ion/port/nullptr.h"  // For kNullFunction.

namespace ion {
namespace base {

// VectorDataContainer is a special kind of DataContainer that is backed by an
// AllocVector. Accessing its underlying AllocVector provides a mechanism for a
// DataContainer with resizeable storage. Note that unlike a regular
// DataContainer, a VectorDataContainer can be created directly, but is
// templated on the type of the data.
template <typename T>
class ION_API VectorDataContainer : public DataContainer {
 public:
  explicit VectorDataContainer(bool is_wipeable)
      : DataContainer(kNullFunction, is_wipeable), vector_(*this) {}

  // Returns a const reference to the vector backing this instance.
  const AllocVector<T>& GetVector() const { return vector_; }

  // Returns a pointer to the vector backing this instance.
  AllocVector<T>* GetMutableVector() { return &vector_; }

 protected:
  // The destructor is protected because all base::Referent classes must have
  // protected or private destructors.
  ~VectorDataContainer() override {}

 private:
  // Actually delete data.
  // Note: vector::clear() is not required to reduce capacity, just size. On
  // C++-11, the proper sequence is clear() followed by
  // shrink_to_fit(). Pre-C++-11, over-writing the vector with an empty one is
  // the proper way to reduce capacity.
  void InternalWipeData() override { vector_ = AllocVector<T>(GetAllocator()); }

  // Returns the data pointer of this instance, which is only valid if the
  // internal vector has data.
  void* GetDataPtr() const override {
    return vector_.empty() ? nullptr : &vector_[0];
  }

  // The actual data.
  mutable AllocVector<T> vector_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VectorDataContainer);
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_VECTORDATACONTAINER_H_
