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

#ifndef ION_GFX_SHADERINPUT_H_
#define ION_GFX_SHADERINPUT_H_

#include <cstring>  // For size_t.

#include "base/integral_types.h"
#include "ion/base/allocator.h"

namespace ion {
namespace gfx {

class ShaderInputRegistry;

class ShaderInputBase {
 public:
  // This is only used to determine the type of a ShaderInputRegistry::Spec
  // since Attributes and Uniforms share the space of names.
  enum Tag {
    kUniform,
    kAttribute
  };

  ShaderInputBase() {}
  ~ShaderInputBase() {}

 protected:
  // Returns atomically post-incremented stamp.
  static uint64 GetNewStamp();
};

// A ShaderInput instance represents a general shader input.
template <typename ValueHolderType, typename ValueEnumType>
class ShaderInput : public ShaderInputBase {
 public:
  typedef ValueHolderType HolderType;
  typedef ValueEnumType ValueType;

  ~ShaderInput() {}

  // Returns true if this is a valid instance created by a ShaderInputRegistry.
  // If this returns false, most of the other methods should not be called.
  bool IsValid() const { return registry_ != nullptr; }

  // Returns the ShaderInputRegistry the shader input is defined in.
  // This will crash if called on an invalid instance.
  const ShaderInputRegistry& GetRegistry() const { return *registry_; }

  // Returns the index of the shader input within the registry. This should not
  // be called on an invalid instance.
  size_t GetIndexInRegistry() const { return index_in_registry_; }

  // Returns the id of the owning registry. This value will not be useful if
  // called on an invalid instance.
  size_t GetRegistryId() const { return registry_id_; }

  // Returns the array index of this input; by default this is 0. An array
  // index is specified in the input name with square brackets, e.g.,
  // uMyArray[2] has an index of 2.
  size_t GetArrayIndex() const { return array_index_; }

  // Returns the type of the shader input. This should not be called on an
  // invalid instance.
  ValueType GetType() const { return type_; }

  // If this instance contains a value of type T, this returns a const
  // reference to it.  Otherwise, it returns an InvalidReference. This should
  // not be called on an invalid instance.
  template <typename T> const T& GetValue() const {
    return value_.template Get<T>();
  }

  // If this instance contains an array of values of type T with a length
  // smaller than the passed index, this returns a const reference to the
  // element at i. Otherwise, it returns an InvalidReference. This should not be
  // called on an invalid instance.
  template <typename T> const T& GetValueAt(size_t i) const {
    return value_.template GetValueAt<T>(i);
  }

  // Returns the number of elements in the held type. This is 0 if this holds
  // only a scalar value.
  size_t GetCount() const {
    return value_.GetCount();
  }

  // If this instance contains a value of type T, this returns true, otherwise
  // it returns false.
  template <typename T> bool Is() const {
    return value_.template Is<T>();
  }

  // If this instance contains an array of values of type T, this returns true,
  // otherwise it returns false.
  template <typename T> bool IsArrayOf() const {
    return value_.template IsArrayOf<T>();
  }

  // If this instance contains a value of type T, this changes it to the new
  // value. Otherwise, it does nothing but return false. This should not be
  // called on an invalid instance.
  template <typename T> bool SetValue(const T& value) {
    if (value_.template IsAssignableTo<T>()) {
      SetNewStamp();
      value_.Set(value);
      return true;
    } else {
      return false;
    }
  }

  // If this instance contains a array of values of type T with a length larger
  // than i, this changes the element at i to the new value. Otherwise, it does
  // nothing but return false. This should not be called on an invalid instance.
  template <typename T> bool SetValueAt(size_t i, const T& value) {
    if (value_.template ElementsAssignableTo<T>()) {
      SetNewStamp();
      value_.SetValueAt(i, value);
      return true;
    } else {
      return false;
    }
  }

  // Returns the stamp of the input. The stamp is a global counter which is
  // advanced any time a ShaderInput is modified. Two Uniforms with the same
  // stamp are guaranteed to have the same values. Two Uniforms with different
  // stamps may or may not have the same value.
  uint64 GetStamp() const { return stamp_; }

 protected:
  // The default constructor creates an invalid ShaderInput instance, which
  // should never be used as is. IsValid() will return false for such an
  // instance.
  ShaderInput()
      : ShaderInputBase(),
        registry_(nullptr),
        index_in_registry_(0),
        registry_id_(0),
        type_(static_cast<ValueType>(0)),
        stamp_(0U),
        array_index_(0) {}

  // Assigns a new stamp to this Input.
  void SetNewStamp() { stamp_ = GetNewStamp(); }

  // Initializes the ShaderInput to a valid state. This is passed the
  // ShaderInputRegistry containing the shader input definition, its index
  // within that registry, the type, and the initial value. The value is
  // assumed to be consistent with the registered type. This function is private
  // because only the ShaderInputRegistry class can create valid instances,
  // enforcing type consistency.
  template <typename T>
  void Init(const ShaderInputRegistry& registry, size_t registry_id,
            size_t index_in_registry, size_t array_index, ValueType type,
            const T& value) {
    registry_ = &registry;
    registry_id_ = registry_id;
    index_in_registry_ = index_in_registry;
    array_index_ = array_index;
    type_ = type;
    value_.Set(value);
    SetNewStamp();
  }

  // Initializes the ShaderInput to a valid state. This is passed the
  // ShaderInputRegistry containing the shader input definition, its index
  // within that registry, the type, and the initial set of values (or NULL to
  // not set values). The value is assumed to be consistent with the registered
  // type. This function is private because only the ShaderInputRegistry class
  // can create valid instances, enforcing type consistency.
  template <typename T>
  void InitArray(const ShaderInputRegistry& registry, size_t registry_id,
                 size_t index_in_registry, size_t array_index, ValueType type,
                 const T* values, size_t count,
                 const base::AllocatorPtr& allocator) {
    registry_ = &registry;
    registry_id_ = registry_id;
    index_in_registry_ = index_in_registry;
    array_index_ = array_index;
    type_ = type;
    value_.template InitArray<T>(allocator, count);
    if (values) {
      for (size_t i = 0; i < count; ++i)
        value_.SetValueAt(i, values[i]);
    }
    SetNewStamp();
  }

  // Returns the allocator used to make array allocations.
  const base::AllocatorPtr& GetArrayAllocator() const {
    return value_.GetArrayAllocator();
  }

 private:
  // The registry containing the shader input definition and its index.
  const ShaderInputRegistry* registry_;
  size_t index_in_registry_;
  size_t registry_id_;
  ValueType type_;
  ValueHolderType value_;
  // A global stamp which is changed every time the ShaderInput is modified.
  uint64 stamp_;
  // The starting array index of the input.
  size_t array_index_;

  friend class ShaderInputRegistry;
};

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_SHADERINPUT_H_
