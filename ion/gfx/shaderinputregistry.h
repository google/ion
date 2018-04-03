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

#ifndef ION_GFX_SHADERINPUTREGISTRY_H_
#define ION_GFX_SHADERINPUTREGISTRY_H_

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "ion/base/allocator.h"
#include "ion/base/logging.h"
#include "ion/base/referent.h"
#include "ion/base/stlalloc/allocdeque.h"
#include "ion/base/stlalloc/allocmap.h"
#include "ion/base/stlalloc/allocvector.h"
#include "ion/base/stringutils.h"
#include "ion/base/varianttyperesolver.h"
#include "ion/gfx/attribute.h"
#include "ion/gfx/resourceholder.h"
#include "ion/gfx/uniform.h"

namespace ion {
namespace gfx {

class ShaderInputRegistry;

// Convenience typedef for shared pointer to a ShaderInputRegistry.
using ShaderInputRegistryPtr = base::SharedPtr<ShaderInputRegistry>;

// A ShaderInputRegistry acts as a namespace for shader inputs (uniforms and
// attributes). It isolates users from dealing with input locations. Shaders
// using the same ShaderInputRegistry are expected to have matching types and
// meanings for inputs that have identical names. A registry must be specified
// for each ShaderProgram instance that is created.
//
// The registry provides Create<Attribute>() and Create<Uniform>() functions,
// which are the only way to construct valid Attribute and Uniform instances.
// By design, there is no way to delete inputs from a registry. The registry
// must be kept alive as long as any inputs created from it might be used.
//
// A registry must be specified for each ShaderProgram instance that is
// created.  There is also a ShaderInputRegistry instance representing some
// predefined attributes and uniforms, returned by GetGlobalRegistry(). It
// contains specs for the following attributes as buffer object elements:
//
//   vec3 aVertex:
//           The vertex position.
//
//   vec3 aColor:
//           The vertex color.
//
//   vec3 aNormal:
//           The normal direction.
//
//   vec2 aTexCoords:
//           Texture coordinates.
//
// and specs for the following uniforms:
//
//   ivec2 uViewportSize:
//           Replaces the active viewport size (in pixels).
//           Default Value: (0,0)
//
//   mat4 uProjectionMatrix:
//           Replaces the current projection matrix.
//           Default Value: Identity
//
//   mat4 uModelviewMatrix:
//           Accumulates into the current modelview matrix.
//           Default Value: Identity
//
//   vec4 uBaseColor:
//           Replaces the base color for shapes.
//           Default Value:  (1,1,1,1)
//
//
class ION_API ShaderInputRegistry : public ResourceHolder {
 public:
  // Changes that affect this resource.
  enum {
    kUniformAdded = kNumBaseChanges,
    kNumChanges
  };

  // This type defines a function that is used to combine values for two
  // instances of a registered shader input. During traversal, the ShaderState
  // class manages a stack of values for each shader input. By default, each new
  // value for a shader input just replaces the previous value. This function
  // can be defined to change that behavior. For example, it can be used to
  // maintain a cumulative transformation matrix in a Uniform variable. The
  // function is set in the Spec for a shader input and is NULL by default,
  // meaning that standard value replacement should be used.
  //
  // The callback is passed a reference to the two shader inputs to combine
  // (old_value and new_value) and should return the resulting combined shader
  // input.
  //
  // Using a struct since we can't use alias templates (C++11).
  template <typename T>
  struct CombineFunction {
    typedef std::function<const T(const T& old_value, const T& new_value)> Type;
  };

  // A GenerateFunction is similar to a CombineFunction, above, but generates
  // additional inputs based on the value of a single input. Note that a
  // GenerateFunction is applied _after_ a CombineFunction. A GenerateFunction
  // can be used, for example, to generate a rotation matrix (3x3) out of a
  // general transformation matrix (4x4), or to extract angles out of a matrix
  // (e.g., as a Vector3). Note that Specs for all generated inputs must already
  // be Add()ed to the same registry as the passed input.
  //
  // Using a struct since we can't use alias templates (C++11).
  template <typename T>
  struct GenerateFunction {
    typedef std::function<std::vector<T>(const T& current_value)> Type;
  };

  // This struct is stored for each registered ShaderInput.
  template <typename T>
  struct Spec {
    explicit Spec(
        const std::string& name_in = std::string(),
        typename T::ValueType value_type_in =
            static_cast<typename T::ValueType>(0),
        const std::string& doc_string_in = std::string(),
        typename CombineFunction<T>::Type combine_function_in = nullptr,
        typename GenerateFunction<T>::Type generate_function_in = nullptr)
        : name(name_in),
          value_type(value_type_in),
          doc_string(doc_string_in),
          index(0),
          registry_id(0),
          registry(nullptr),
          combine_function(combine_function_in),
          generate_function(generate_function_in) {}

    std::string name;                  // Name of the shader input argument.
    typename T::ValueType value_type;  // Type of the value of the shader input.
    std::string doc_string;            // String describing its use.
    size_t index;                      // Unique index within registry.
    size_t registry_id;                // Id of the owning registry.
    // The registry that created this spec. This is a raw pointer since when the
    // registry is destroyed, the spec will be as well.
    ShaderInputRegistry* registry;

    // Function used to combine values.
    typename CombineFunction<T>::Type combine_function;
    // Function used to generate values.
    typename GenerateFunction<T>::Type generate_function;
  };

  typedef Spec<Attribute> AttributeSpec;
  typedef Spec<Uniform> UniformSpec;

  ShaderInputRegistry();

  // Each ShaderInputRegistry instance is assigned a unique integer ID. This
  // returns that ID.
  size_t GetId() const { return id_; }

  // Includes another ShaderInputRegistry in this one. This is similar to a
  // "using namespace" declaration in C++. Any shader that uses the current
  // registry may also Create() inputs from an included registry. If there is a
  // name conflict (the passed registry defines an input also defined by this
  // registry or its includes), an error is printed and the registry is not
  // included. Returns whether the registry was included.
  bool Include(const ShaderInputRegistryPtr& reg);

  // Includes the global registry in this registry. See the above comment for
  // Include() for failure conditions. Returns whether the global registry was
  // included.
  bool IncludeGlobalRegistry();

  // Returns the vector of included registries.
  const base::AllocVector<ShaderInputRegistryPtr>& GetIncludes() const {
    return includes_;
  }

  // Returns whether the inputs in this registry and its includes are unique.
  // Any duplicate inputs are logged as warnings.
  bool CheckInputsAreUnique() const;

  // Adds a type specification to the registry. This returns false if one
  // already exists with the same name in this or any included registry.
  template <typename T> bool Add(const Spec<T>& spec) {
    size_t array_index = 0;
    std::string name;
    if (Contains(spec.name)) {
      LOG(WARNING) << "Can't add " << T::GetShaderInputTypeName() << " spec"
                   << " for '" << spec.name << "': already present in registry"
                   << " or its includes";
      return false;
    } else if (!ParseShaderInputName(spec.name, &name, &array_index)) {
      LOG(WARNING) << "Can't add " << T::GetShaderInputTypeName() << " spec"
                   << " for '" << spec.name << "': invalid input name.";
      return false;
    } else {
      // Store in vector.
      base::AllocDeque<Spec<T> >& specs = *GetMutableSpecs<T>();
      const size_t index = specs.size();
      specs.push_back(spec);
      specs.back().index = index;
      specs.back().registry_id = id_;
      specs.back().registry = this;
      UpdateLargestRegistrySize(specs.size());
      // Store in map.
      spec_map_[spec.name] = SpecMapEntry(T::GetTag(), index, id_);
      return true;
    }
  }

  // Returns if a Spec for an input of the passed name exists in the registry or
  // its includes.
  bool Contains(const std::string& name) const;

  // Returns the Spec for an input, or NULL if there isn't yet one. The returned
  // pointer is not guaranteed to be persistent if Add() is called again.
  template <typename T> const Spec<T>* Find(const std::string& name) const {
    const size_t num_includes = includes_.size();
    for (size_t i = 0; i < num_includes; ++i) {
      const Spec<T>* spec = includes_[i]->Find<T>(name);
      if (spec)
        return spec;
    }
    const SpecMapType::const_iterator it = spec_map_.find(name);
    if (it == spec_map_.end()) {
      // Neither this regisry nor its includes contains a spec with the right
      // name.
      return nullptr;
    } else if (it->second.tag != T::GetTag()) {
      // A spec with the same name but different type already exists in this
      // registry.
      return nullptr;
    } else {
      // This registry has a spec of the right type. Return it.
      return &GetSpecs<T>()[it->second.index];
    }
  }

  // Returns a vector of all Specs of type T added to this registry.
  template <typename T> const base::AllocDeque<Spec<T> >& GetSpecs() const;

  // Constructs a ShaderInput with the given name and value. If the name is not
  // found in the registry or its includes, the input is added to this registry.
  // If the name is found and the value is of the same type, a copy of the
  // existing instance is returned. If the name is found and the type is
  // inconsistent, logs an error and returns an invalid ShaderInput instance
  // (i.e., calling IsValid() on it will return false).
  template <typename ShaderInputType, typename T>
  const ShaderInputType Create(const std::string& name_in, const T& value) {
    // Determine the correct type that will allow a T to be stored.
    typedef typename base::VariantTypeResolver<
      typename ShaderInputType::HolderType, T>::Type StoredType;
    typename ShaderInputType::ValueType value_type =
        ShaderInputType::template GetTypeByValue<StoredType>();
    std::string name;
    ShaderInputType input;
    size_t index = 0, registry_id = 0, array_index = 0;
    ShaderInputRegistry* registry = nullptr;
    if (ParseShaderInputName(name_in, &name, &array_index)) {
      if (!Find<ShaderInputType>(name))
        Add<ShaderInputType>(Spec<ShaderInputType>(name, value_type));
      if (ValidateNameAndType<ShaderInputType>(name, value_type, 0U, &registry,
                                               &registry_id, &index))
        input.template Init<T>(*registry, registry_id, index, array_index,
                               value_type, value);
    }
    return input;
  }
  // Constructs a ShaderInput with the given name and value. The input must
  // already exist in the registry or one of its includes, otherwise an invalid
  // instance is returned.
  template <typename ShaderInputType, typename T>
  const ShaderInputType Create(const std::string& name_in,
                               const T& value) const {
    // Determine the correct type that will allow a T to be stored.
    typedef typename base::VariantTypeResolver<
      typename ShaderInputType::HolderType, T>::Type StoredType;
    typename ShaderInputType::ValueType value_type =
        ShaderInputType::template GetTypeByValue<StoredType>();
    std::string name;
    ShaderInputType input;
    size_t index = 0, registry_id = 0, array_index = 0;
    ShaderInputRegistry* registry = nullptr;
    if (ParseShaderInputName(name_in, &name, &array_index) &&
        ValidateNameAndType<ShaderInputType>(name, value_type, 0U, &registry,
                                             &registry_id, &index))
        input.template Init<T>(*registry, registry_id, index, array_index,
                               value_type, value);
    return input;
  }

  // Constructs an array Uniform with the given name and values, using the
  // passed allocator to construct the values in the returned Uniform. If values
  // is NULL then the array is still created but values are left unset as 0s. If
  // the name is not found in the registry or its includes, the input is added
  // to this registry, but if the value type is inconsistent with the registered
  // type, this logs an error and returns an invalid ShaderInput instance (i.e.,
  // calling IsValid() on it will return false).
  template <typename T>
  const Uniform CreateArrayUniform(const std::string& name_in, const T* values,
                                   size_t count,
                                   const base::AllocatorPtr& allocator) {
    // Determine the correct type that will allow a T to be stored.
    typedef typename base::VariantTypeResolver<UniformValueType, T>::Type
        StoredType;
    Uniform input;
    size_t index = 0, registry_id = 0, array_index = 0;
    ShaderInputRegistry* registry = nullptr;
    UniformType value_type = Uniform::GetTypeByValue<StoredType>();
    std::string name;
    if (ParseShaderInputName(name_in, &name, &array_index)) {
      if (!Find<Uniform>(name)) Add<Uniform>(Spec<Uniform>(name, value_type));
      ValidateNameAndType<Uniform>(name, value_type, count, &registry,
                                   &registry_id, &index);
      input.template InitArray<T>(*registry, registry_id, index, array_index,
                                  value_type, values, count, allocator);
    }
    return input;
  }
  template <typename T>
  const Uniform CreateArrayUniform(const std::string& name_in, const T* values,
                                   size_t count,
                                   const base::AllocatorPtr& allocator) const {
    // Determine the correct type that will allow a T to be stored.
    typedef typename base::VariantTypeResolver<UniformValueType, T>::Type
        StoredType;
    Uniform input;
    size_t index = 0, registry_id = 0, array_index = 0;
    ShaderInputRegistry* registry = nullptr;
    UniformType value_type = Uniform::GetTypeByValue<StoredType>();
    std::string name;
    if (ParseShaderInputName(name_in, &name, &array_index) &&
        ValidateNameAndType<Uniform>(name, value_type, count, &registry,
                                     &registry_id, &index))
      input.template InitArray<T>(*registry, registry_id, index, array_index,
                                  value_type, values, count, allocator);
    return input;
  }

  // Convenience function that returns a pointer to the Spec associated with an
  // Attribute or Uniform instance. These return NULL if the Attribute or
  // Uniform is not valid.
  template <typename T>
  static const Spec<T>* GetSpec(const T& input) {
    return !input.IsValid() ? nullptr :
        &input.GetRegistry().template GetSpecs<T>()[input.GetIndexInRegistry()];
  }

  // Returns the ShaderInputRegistry instance representing all supported global
  // uniforms and attributes.
  static const ShaderInputRegistryPtr& GetGlobalRegistry();

 protected:
  // The destructor is protected because all base::Referent classes must have
  // protected or private destructors.
  ~ShaderInputRegistry() override;

 private:
  // StaticData contains some of the static member data for the
  // ShaderInputRegistry class.
  class StaticData;
  // StaticGlobalRegistryData contains a static instance of ShaderInputRegistry.
  // It needs to be in its own class since the ShaderInputRegistry constructor
  // depends on StaticData.
  class StaticGlobalRegistryData;

  // Returns the StaticData instance, creating it first if necessary in a
  // thread-safe way.
  static StaticData* GetStaticData();

  // Returns the StaticGlobalRegistryData instance, creating it first if
  // necessary in a thread-safe way.
  static StaticGlobalRegistryData* GetStaticGlobalRegistryData();

  // Updates the size of the largest registry.
  static void UpdateLargestRegistrySize(size_t size);

  struct SpecMapEntry {
    SpecMapEntry()
        : tag(ShaderInputBase::kUniform),
          index(0),
          registry_id(0) {}

    SpecMapEntry(const ShaderInputBase::Tag& tag_in, size_t index_in,
                 size_t registry_id_in)
        : tag(tag_in),
          index(index_in),
          registry_id(registry_id_in) {}

    ShaderInputBase::Tag tag;
    size_t index;
    size_t registry_id;
  };
  typedef base::AllocMap<const std::string, SpecMapEntry> SpecMapType;

  // Returns a map of all Specs added to the registry and its includes.
  const SpecMapType GetAllSpecEntries() const;

  // Get an editable vector of specs of type T in this registry only.
  template <typename T>
  base::AllocDeque<Spec<T> >* GetMutableSpecs();

  // If the name corresponds to a registered input and the type matches its
  // type, this sets index to the index of the input and returns true.
  // Otherwise, it logs an error and returns false. The passed size specifies
  // the size if the shader input is an array type; passing 0 means that the
  // input is not an array.
  template <typename T>
  bool ValidateNameAndType(const std::string& name,
                           const typename T::ValueType value_type, size_t size,
                           ShaderInputRegistry** registry, size_t* registry_id,
                           size_t* index) const {
    DCHECK(index);
    if (const Spec<T>* spec = Find<T>(name)) {
      if (spec->value_type == value_type) {
        *index = spec->index;
        *registry_id = spec->registry_id;
        *registry = spec->registry;
        return true;
      } else {
        LOG(ERROR) << "Can't create " << T::GetShaderInputTypeName() << " '"
                   << name << "': wrong value_type (got "
                   << T::GetValueTypeName(value_type) << ", expected "
                   << T::GetValueTypeName(spec->value_type) << ")";
        return false;
      }
    } else {
      LOG(ERROR) << "Can't create " << T::GetShaderInputTypeName() << " '"
                 << name << "': no Spec exists for this name, did you forget "
                 << "to Add() it?";
      return false;
    }
  }

  // Parses an input name into a string name and integral index. Returns whether
  // the parsing was successful.
  bool ParseShaderInputName(const std::string& input, std::string* name,
                            size_t* index) const {
    // Try to find an array specification.
    *index = 0;
    name->clear();
    const size_t open_pos = input.find("[");
    const size_t close_pos = input.find("]");
    if (open_pos != std::string::npos && close_pos != std::string::npos &&
        close_pos > open_pos + 1U) {
      const std::vector<std::string> tokens = base::SplitString(input, "[]");
      *name = tokens[0];
      if (tokens.size() > 1 && !tokens[1].empty())
        *index = static_cast<size_t>(base::StringToInt32(tokens[1]));
    } else if (open_pos == std::string::npos &&
               close_pos == std::string::npos) {
      *name = input;
    } else {
      return false;
    }
    return true;
  }

  // Vectors of added Specs.
  Field<base::AllocDeque<UniformSpec> > uniform_specs_;
  base::AllocDeque<AttributeSpec> attribute_specs_;

  // Vector of included registries.
  base::AllocVector<ShaderInputRegistryPtr> includes_;

  // Maps shader input name to the index of the Spec within the vector.
  SpecMapType spec_map_;

  // Unique registry ID.
  size_t id_;

  // Make sure unique ID's remain unique.
  DISALLOW_COPY_AND_ASSIGN(ShaderInputRegistry);
};

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_SHADERINPUTREGISTRY_H_
