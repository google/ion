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

#include "ion/gfx/shaderinputregistry.h"

#include <stdint.h>

#include <algorithm>

#include "ion/base/staticsafedeclare.h"
#include "ion/math/matrix.h"
#include "ion/port/atomic.h"

namespace ion {
namespace gfx {

namespace {

//-----------------------------------------------------------------------------
//
// Helper functions.
//
//-----------------------------------------------------------------------------

// This function is used to combine matrices to maintain cumulative
// transformations in uniforms.
static const Uniform CombineMatrices(const Uniform& old_value,
                                     const Uniform& new_value) {
  DCHECK_EQ(kMatrix4x4Uniform, old_value.GetType());
  DCHECK_EQ(kMatrix4x4Uniform, new_value.GetType());

  const math::Matrix4f& m0 = old_value.GetValue<math::Matrix4f>();
  const math::Matrix4f& m1 = new_value.GetValue<math::Matrix4f>();

  Uniform result = old_value;
  result.SetValue(m0 * m1);
  return result;
}

}  // anonymous namespace

//-----------------------------------------------------------------------------
//
// ShaderInputRegistry::StaticData class definition.
//
//-----------------------------------------------------------------------------

class ShaderInputRegistry::StaticData {
 public:
  // Only one instance of this class should be created using this constructor.
  StaticData()
      : registry_count_(0), largest_registry_size_(0) {}

  // Gets a unique identifier for a registry.
  int32_t GetUniqueId() const {
    return ++registry_count_;
  }

  // Sets the size of the largest registry if size is larger than the currently
  // held size.
  void UpdateLargestRegistrySize(int32_t size) const {
    int32_t largest_size = largest_registry_size_.load();
    bool succeeded = false;
    // largest_size is updated on each iteration if the exchange is not
    // successful. Therefore, keep trying until either the exchange succeeds,
    // or we load a largest_size bigger than size.
    while (size > largest_size && !succeeded) {
      succeeded = largest_registry_size_.compare_exchange_strong(
          largest_size, size);
      // largest_size is updated on compare failure
    }
  }

 private:
  // The number of registries that have been created.
  mutable std::atomic<int32_t> registry_count_;

  // The number of entries in the largest registry.
  mutable std::atomic<int32_t> largest_registry_size_;
};

//-----------------------------------------------------------------------------
//
// ShaderInputRegistry::StaticGlobalRegistryData class definition.
//
//-----------------------------------------------------------------------------

class ShaderInputRegistry::StaticGlobalRegistryData {
 public:
  // Only one instance of this class should be created using this constructor.
  StaticGlobalRegistryData()
      : global_registry_(new(base::kLongTerm) ShaderInputRegistry) {
    global_registry_->Add(AttributeSpec(
        "aVertex", kBufferObjectElementAttribute, "Vertex position."));
    global_registry_->Add(AttributeSpec(
        "aColor", kBufferObjectElementAttribute, "Vertex color."));
    global_registry_->Add(AttributeSpec(
        "aNormal", kBufferObjectElementAttribute, "Normal direction."));
    global_registry_->Add(AttributeSpec(
        "aTexCoords", kBufferObjectElementAttribute, "Texture coordinates."));

    global_registry_->Add(UniformSpec(
        "uViewportSize", kIntVector2Uniform, "Viewport Size."));
    global_registry_->Add(UniformSpec(
        "uProjectionMatrix", kMatrix4x4Uniform, "Projection matrix."));
    global_registry_->Add(UniformSpec("uModelviewMatrix",
                                      kMatrix4x4Uniform,
                                      "Cumulative modelview matrix.",
                                      CombineMatrices));
    global_registry_->Add(UniformSpec(
        "uBaseColor", kFloatVector4Uniform, "Base color for shapes."));
  }

  // Gets the global registry.
  const ShaderInputRegistryPtr& GetGlobalRegistry() const {
    return global_registry_;
  }

 private:
  // Global registry.
  ShaderInputRegistryPtr global_registry_;
};

//-----------------------------------------------------------------------------
//
// ShaderInputRegistry class definition.
//
//-----------------------------------------------------------------------------

ShaderInputRegistry::ShaderInputRegistry()
    : uniform_specs_(kUniformAdded,
                     base::AllocDeque<UniformSpec>(*this),
                     this),
      attribute_specs_(*this),
      includes_(*this),
      spec_map_(*this) {
  id_ = GetStaticData()->GetUniqueId();
}

ShaderInputRegistry::~ShaderInputRegistry() {
}

bool ShaderInputRegistry::Contains(const std::string& name) const {
  const SpecMapType::const_iterator it = spec_map_.find(name);
  if (it == spec_map_.end()) {
    // A spec with the name is not in this registry; search includes.
    const size_t num_includes = includes_.size();
    for (size_t i = 0; i < num_includes; ++i)
      if (includes_[i]->Contains(name))
        return true;
    return false;
  } else {
    // A spec with the same name but different type already exists, or this
    // registry has a spec of the right type.
    return true;
  }
}

bool ShaderInputRegistry::Include(const ShaderInputRegistryPtr& reg) {
  if (!reg.Get())
    return false;

  // Registries cannot include themselves.
  if (reg.Get() == this) {
    LOG(ERROR) << "Can't include registry " << reg->GetId() << " in registry "
               << GetId() << " because a registry cannot include itself";
    return false;
  }

  // Check that reg does not contain any inputs that this registry or any of
  // its existing inputs already contain.
  const SpecMapType specs = GetAllSpecEntries();
  for (SpecMapType::const_iterator it = specs.begin(); it != specs.end();
       ++it) {
    if (reg->Contains(it->first)) {
      LOG(ERROR) << "Can't include registry " << reg->GetId() << " in registry "
                 << GetId() << " because they or their includes both define the"
                 << " shader input '" << it->first << "'";
      return false;
    }
  }

  includes_.push_back(reg);
  return true;
}

bool ShaderInputRegistry::IncludeGlobalRegistry() {
  return Include(GetStaticGlobalRegistryData()->GetGlobalRegistry());
}

bool ShaderInputRegistry::CheckInputsAreUnique() const {
  bool duplicates_found = false;
  SpecMapType specs = spec_map_;
  const size_t num_includes = includes_.size();
  for (size_t i = 0; i < num_includes; ++i) {
    const SpecMapType& included_specs = includes_[i]->GetAllSpecEntries();
    // Check if anything from included_specs is in spec_map.
    for (SpecMapType::const_iterator it = included_specs.begin();
         it != included_specs.end(); ++it) {
      if (specs.count(it->first)) {
        LOG(WARNING) << "Registry " << specs[it->first].registry_id
                     << " defines duplicate input '" << it->first << "' which"
                     << " is also defined in registry "
                     << it->second.registry_id;
        duplicates_found = true;
      }
    }
    specs.insert(included_specs.begin(), included_specs.end());
  }
  return !duplicates_found;
}

template <> ION_API base::AllocDeque<ShaderInputRegistry::AttributeSpec>*
ShaderInputRegistry::GetMutableSpecs() {
  return &attribute_specs_;
}

template <> ION_API base::AllocDeque<ShaderInputRegistry::UniformSpec>*
ShaderInputRegistry::GetMutableSpecs() {
  return uniform_specs_.GetMutable();
}

const ShaderInputRegistry::SpecMapType
ShaderInputRegistry::GetAllSpecEntries() const {
  SpecMapType specs = spec_map_;
  const size_t num_includes = includes_.size();
  for (size_t i = 0; i < num_includes; ++i) {
    const SpecMapType& included_specs = includes_[i]->GetAllSpecEntries();
    specs.insert(included_specs.begin(), included_specs.end());
  }
  return specs;
}

template <>
ION_API const base::AllocDeque<ShaderInputRegistry::AttributeSpec>&
ShaderInputRegistry::GetSpecs() const {
  return attribute_specs_;
}

template <>
ION_API const base::AllocDeque<ShaderInputRegistry::UniformSpec>&
ShaderInputRegistry::GetSpecs() const {
  return uniform_specs_.Get();
}

const ShaderInputRegistryPtr& ShaderInputRegistry::GetGlobalRegistry() {
  return GetStaticGlobalRegistryData()->GetGlobalRegistry();
}

void ShaderInputRegistry::UpdateLargestRegistrySize(size_t size) {
  GetStaticData()->UpdateLargestRegistrySize(static_cast<int32_t>(size));
}

ShaderInputRegistry::StaticData* ShaderInputRegistry::GetStaticData() {
  ION_DECLARE_SAFE_STATIC_POINTER(StaticData, s_static_data);
  return s_static_data;
}

ShaderInputRegistry::StaticGlobalRegistryData*
ShaderInputRegistry::GetStaticGlobalRegistryData() {
  ION_DECLARE_SAFE_STATIC_POINTER(StaticGlobalRegistryData,
                                  s_static_registry_data);
  return s_static_registry_data;
}

}  // namespace gfx
}  // namespace ion
