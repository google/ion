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

#ifndef ION_GFX_UNIFORMHOLDER_H_
#define ION_GFX_UNIFORMHOLDER_H_

#include <string>

#include "ion/base/allocatable.h"
#include "ion/base/invalid.h"
#include "ion/base/stlalloc/allocvector.h"
#include "ion/gfx/uniform.h"

namespace ion {
namespace gfx {

// A UniformHolder is a base class for an object that holds Uniform values. The
// most important object derived from UniformHolder is Node.
//
// The ShaderInputRegistry of any Uniform added to a UniformHolder must have a
// longer lifetime than the holder. Otherwise, an invalid memory access will
// result.
class ION_API UniformHolder {
 public:
  // Adds a uniform to this and returns an index that can be used to refer
  // to the uniform. Note that this index has nothing to do with the GL concept
  // of uniform location, it is invalid if ClearUniforms() is ever used, and may
  // refer to a different uniform if the uniform is ever replaced with
  // ReplaceUniform(). Returns base::kInvalidIndex if an attempt is made to add
  // an invalid uniform.
  size_t AddUniform(const Uniform& uniform) {
    if (uniform.IsValid()) {
      uniforms_.push_back(uniform);
      return uniforms_.size() - 1U;
    } else {
      return base::kInvalidIndex;
    }
  }

  // Replaces the uniform at an index with the passed value, if the index is
  // valid. Returns if the replacement is successful.
  bool ReplaceUniform(size_t index, const Uniform& uniform) {
    if (uniform.IsValid() && index < uniforms_.size()) {
      uniforms_[index] = uniform;
      return true;
    } else {
      return false;
    }
  }

  // Removes the uniform with the passed name if it exists. Returns true iff the
  // uniform existed and hence got removed. Note that this will change the
  // indices of other uniforms within the holder.
  bool RemoveUniformByName(const std::string& name) {
    const size_t index = GetUniformIndex(name);
    if (index == base::kInvalidIndex)
      return false;
    uniforms_.erase(uniforms_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
  }

  // Clears the vector of uniforms in this.
  void ClearUniforms() { uniforms_.clear(); }

  // Gets the vector of uniforms.
  const base::AllocVector<Uniform>& GetUniforms() const { return uniforms_; }

  // Sets the value of the uniform at an index if the index is valid. Returns
  // true if the index is valid and the set was successful (i.e., T is a valid
  // type for the selected uniform), and false otherwise.
  template <typename T> bool SetUniformValue(size_t index, const T& value) {
    if (index < uniforms_.size())
      return uniforms_[index].SetValue(value);
    return false;
  }

  // Sets the value of the array uniform at an index if the index is valid.
  // Returns true if the index is valid and the set was successful
  // (i.e., T is a valid type for the selected uniform), and false otherwise.
  template <typename T> bool SetUniformValueAt(size_t index,
                                               size_t array_index,
                                               const T& value) {
    if (index < uniforms_.size())
      return uniforms_[index].SetValueAt(array_index, value);
    return false;
  }

  // Returns the index of the uniform this with the given name, if it exists.
  // The Uniform must have been added with AddUniform() or ReplaceUniform(). If
  // no uniform with the name exists in this then returns
  // ion::base::kInvalidIndex. Note that this is a relatively slow operation
  // and should be used sparingly.
  size_t GetUniformIndex(const std::string& name) const;

  // Convenience function to set the value of a uniform specified by name. This
  // returns false if there is no uniform with that name or the value type does
  // not match.
  template <typename T> bool SetUniformByName(const std::string& name,
                                              const T& value) {
    const size_t index = GetUniformIndex(name);
    return index == base::kInvalidIndex ? false :
        SetUniformValue<T>(index, value);
  }

  // Convenience function to set the value of an element of an array uniform
  // designated by |name|.  This returns false if:
  //   - There is no uniform matching |name|.
  //   - Value type 'T' does not match the uniform array type.
  //   - |array_index| exceeds the size of the uniform array.
  template <typename T> bool SetUniformByNameAt(const std::string& name,
                                                size_t array_index,
                                                const T& value) {
    const size_t index = GetUniformIndex(name);
    return (index == base::kInvalidIndex || index >= uniforms_.size()) ?
        false :
        uniforms_[index].SetValueAt(array_index, value);
  }

  // Enables or disables the UniformHolder. Disabled holders are skipped over
  // during rendering; their values are not sent to OpenGL. UniformHolders are
  // enabled by default.
  void Enable(bool enable) { is_enabled_ = enable; }
  bool IsEnabled() const { return is_enabled_; }

 protected:
  // The constructor is protected because this is a base class. It requires an
  // allocator to use for its vector of uniforms.
  explicit UniformHolder(const base::AllocatorPtr& alloc);

  // The destructor is protected because all base::Referent classes must have
  // protected or private destructors.
  virtual ~UniformHolder();

 private:
  bool is_enabled_;
  base::AllocVector<Uniform> uniforms_;
};

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_UNIFORMHOLDER_H_
