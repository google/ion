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

#ifndef ION_GFXUTILS_BUFFERTOATTRIBUTEBINDER_H_
#define ION_GFXUTILS_BUFFERTOATTRIBUTEBINDER_H_

#include <string>

#include "base/integral_types.h"
#include "ion/base/invalid.h"
#include "ion/base/logging.h"
#include "ion/base/static_assert.h"
#include "ion/base/type_structs.h"
#include "ion/base/varianttyperesolver.h"
#include "ion/gfx/attribute.h"
#include "ion/gfx/attributearray.h"
#include "ion/gfx/bufferobject.h"
#include "ion/gfx/shaderinputregistry.h"

namespace ion {
namespace gfxutils {

// These functions must be specialized for every type used in a vertex. Ion
// has implementations for the following types:
//   int8, int16, int32
//   uint8, uint16, uint32
//   float
//   All VectorBase types defined in ion/math/vector.h
template <typename T> ION_API
gfx::BufferObject::ComponentType GetComponentType();
template <typename T> ION_API size_t GetComponentCount();

// BufferToAttributeBinder is a simple interface to insert a set of Attributes
// containing BufferObjectElements into a AttributeArray, and also create the
// corresponding Elements in the BufferObject. Use BindAndNormalize() to bind a
// fixed-point Attribute that should have its data normalized, and Bind()
// otherwise.
//
// Example Usage:
//
// struct Vertex {  // Vertex structure.
//   Vector3f position;
//   Vector3ui8 normal;
//   int id;
//   float temperature;
// };
// Vertex v;
// BufferToAttributeBinder<Vertex> binder = BufferToAttributeBinder<Vertex>(v)
//       .Bind(v.position, "aPosition")
//       .BindAndNormalize(v.normal, "aNormal")
//       .Bind(v.id, "aId")
//       .Bind(v.temperature, "aTemperature");
// ...
// ShaderInputRegistryPtr reg(new ShaderInputRegistry);
// AttributeArrayPtr vertex_array(new AttributeArray);
// BufferObjectPtr buffer_object(new BufferObject);
// ... (initialization of objects)
// binder.Apply(reg, vertex_array, buffer_object);
//
// The same Binder can be used with other objects that need to share the same
// structure, as long as the registries are compatible (they must contain the
// attribute names passed to the binder through Bind()):
// ShaderInputRegistry reg2;
// AttributeArrayPtr vertex_array2(new AttributeArray);
// binder.Apply(reg2, vertex_array2, buffer_object);
//
// A different BufferObject may also be used:
// BufferObjectPtr buffer_object2(new BufferObject);
// binder.Apply(reg2, vertex_array2, buffer_object2);
//
// A simpler shorthand is possible if the binder is only needed for a single
// AttributeArray and BufferObject pair:
// ...
// ShaderInputRegistryPtr reg2(new ShaderInputRegistry);
// AttributeArrayPtr vertex_array(new AttributeArray);
// BufferObjectPtr buffer_object(new BufferObject);
// ... (initialization of objects)
// Vertex v;
// BufferToAttributeBinder<Vertex>(v)
//       .Bind(v.position, "aPosition")
//       .BindAndNormalize(v.normal, "aNormal")
//       .Bind(v.id, "aId")
//       .Bind(v.temperature, "aTemperature")
//       .Apply(reg2, vertex_array, buffer_object);

template <typename T>
class BufferToAttributeBinder {
 public:
  explicit BufferToAttributeBinder(const T& base_struct)
      : base_address_(reinterpret_cast<const char*>(&base_struct)) {
    DCHECK(base_address_);
  }

  ~BufferToAttributeBinder() {}

  void Apply(const gfx::ShaderInputRegistryPtr& reg,
             const gfx::AttributeArrayPtr& aa,
             const gfx::BufferObjectPtr& bo) const {
    const size_t num_bindings = bindings_.size();
    for (size_t i = 0; i < num_bindings; ++i) {
      const size_t element_index = bo->AddSpec(
          bindings_[i].type, bindings_[i].count, bindings_[i].offset);
      gfx::Attribute a = reg->Create<gfx::Attribute>(
          bindings_[i].name, gfx::BufferObjectElement(bo, element_index));
      DCHECK(a.IsValid());
      a.SetFixedPointNormalized(bindings_[i].normalize);
      a.SetDivisor(bindings_[i].divisor);
      aa->AddAttribute(a);
    }
  }

  // Validates that the bindings within the binder are consistent with a packed
  // struct, logging warning messages if they are not. The passed registry is
  // required to determine if the bindings are valid. Returns whether the struct
  // is tightly packed. Note that this will log warnings and return false if
  // called for a struct that contains fields not bound by this binder.
  bool AreBindingsPacked(const gfx::ShaderInputRegistry& reg) {
    // Track the overall struct size based on the sizes of the individual
    // bindings.
    size_t struct_size = 0;
    bool struct_is_packed = true;
    const size_t num_bindings = bindings_.size();
    for (size_t i = 0; i < num_bindings; ++i) {
      if (reg.Contains(bindings_[i].name)) {
        struct_size += bindings_[i].size;
        // If the binding's offset is 0 then it is by definition packed.
        if (bindings_[i].offset) {
          size_t closest = i;
          if (!IsBindingPacked(i, &closest)) {
            LOG(WARNING)
                << "Attribute '" << bindings_[i].name << "' is not"
                << " tightly packed, performance may suffer.  The closest"
                << " binding before it is '" << bindings_[closest].name
                << ",' but it ends at offset "
                << (bindings_[closest].offset + bindings_[closest].size)
                << ", while '" << bindings_[i].name << "' starts at offset "
                << bindings_[i].offset;
            struct_is_packed = false;
          }
        }
      }
    }
    if (struct_size != sizeof(T)) {
      LOG(WARNING) << "Vertex struct is not tightly packed ("
                   << (sizeof(T) - struct_size)
                   << " byte(s) are wasted), performance may suffer.";
      struct_is_packed = false;
    }
    return struct_is_packed;
  }

  template <typename FieldType>
  BufferToAttributeBinder& Bind(const FieldType& field,
                                const std::string& attribute_name) {
    return BindInternal(field, attribute_name, false, 0);
  }

  template <typename FieldType>
  BufferToAttributeBinder& BindAndNormalize(const FieldType& field,
                                            const std::string& attribute_name) {
    return BindInternal(field, attribute_name, true, 0);
  }

  template <typename FieldType>
  BufferToAttributeBinder& Bind(const FieldType& field,
                                const std::string& attribute_name,
                                unsigned int divisor) {
    return BindInternal(field, attribute_name, false, divisor);
  }

  template <typename FieldType>
  BufferToAttributeBinder& BindAndNormalize(const FieldType& field,
                                            const std::string& attribute_name,
                                            unsigned int divisor) {
    return BindInternal(field, attribute_name, true, divisor);
  }

 private:
  // Variant containing all possible bound attribute types.
  typedef base::Variant<int8, uint8, int16, uint16, int32, uint32,
                        float,
                        math::VectorBase1i8, math::VectorBase1ui8,
                        math::VectorBase1i16, math::VectorBase1ui16,
                        math::VectorBase1i, math::VectorBase1ui,
                        math::VectorBase1f,
                        math::VectorBase2i8, math::VectorBase2ui8,
                        math::VectorBase2i16, math::VectorBase2ui16,
                        math::VectorBase2i, math::VectorBase2ui,
                        math::VectorBase2f,
                        math::VectorBase3i8, math::VectorBase3ui8,
                        math::VectorBase3i16, math::VectorBase3ui16,
                        math::VectorBase3i, math::VectorBase3ui,
                        math::VectorBase3f,
                        math::VectorBase4i8, math::VectorBase4ui8,
                        math::VectorBase4i16, math::VectorBase4ui16,
                        math::VectorBase4i, math::VectorBase4ui,
                        math::VectorBase4f, math::Matrix2f, math::Matrix3f,
                        math::Matrix4f> AttributeType;

  struct Binding {
    Binding(size_t offset_in, size_t count_in, size_t size_in,
            const std::string& name_in,
            gfx::BufferObject::ComponentType type_in, bool normalize_in,
            unsigned int divisor_in)
        : offset(offset_in),
          count(count_in),
          size(size_in),
          name(name_in),
          type(type_in),
          normalize(normalize_in),
          divisor(divisor_in) {}
    size_t offset;
    size_t count;
    size_t size;
    std::string name;
    gfx::BufferObject::ComponentType type;
    bool normalize;
    unsigned int divisor;
  };

  // This is private to prevent default initialization.
  BufferToAttributeBinder()
      :  base_address_(nullptr) {}

  template <typename FieldType>
  BufferToAttributeBinder& BindInternal(const FieldType& field,
                                        const std::string& attribute_name,
                                        bool normalize, unsigned int divisor) {
    typedef typename base::VariantTypeResolver<
      AttributeType, FieldType>::Type ResolvedFieldType;
    ION_STATIC_ASSERT((!base::IsSameType<void, ResolvedFieldType>::value),
                      "Cannot resolve attribute type to bind");
    const char* field_address = reinterpret_cast<const char*>(&field);
    const size_t offset = field_address - base_address_;
    bindings_.push_back(Binding(offset, GetComponentCount<ResolvedFieldType>(),
                                sizeof(FieldType), attribute_name,
                                GetComponentType<ResolvedFieldType>(),
                                normalize, divisor));
    return *this;
  }

  // Returns whether the binding at i is tightly packed and sets closest to the
  // index of the closest binding before i.
  bool IsBindingPacked(size_t i, size_t* closest) {
    bool packed = false;
    // Track the index of the closest binding before this one, and the
    // distance in bytes from its end to the start of this binding.
    size_t closest_index = i;
    size_t closest_distance = 0;
    const size_t num_bindings = bindings_.size();
    for (size_t j = 0; j < num_bindings; ++j) {
      // A binding is tightly packed if there is another binding whose
      // offset + size is the current binding's offset.
      const size_t end_of_binding = bindings_[j].offset + bindings_[j].size;
      if (end_of_binding == bindings_[i].offset) {
        // The binding at j ends at this binding, so this binding is
        // packed.
        packed = true;
        break;
      } else if (end_of_binding < bindings_[i].offset &&
                 end_of_binding > closest_distance) {
        // Save the closest binding before this one for more helpful
        // logging.
        closest_index = j;
        closest_distance = end_of_binding;
      }
    }
    if (closest)
      *closest = closest_index;
    return packed;
  }

  const char* base_address_;
  std::vector<Binding> bindings_;
};

}  // namespace gfxutils
}  // namespace ion

#endif  // ION_GFXUTILS_BUFFERTOATTRIBUTEBINDER_H_
