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

#ifndef ION_GFX_UNIFORM_H_
#define ION_GFX_UNIFORM_H_

#include <limits>

#include "ion/gfx/shaderinput.h"

#include "base/integral_types.h"

#include "ion/base/variant.h"
#include "ion/gfx/cubemaptexture.h"
#include "ion/gfx/texture.h"
#include "ion/math/matrix.h"
#include "ion/math/vector.h"

namespace ion {
namespace gfx {

// The UniformType enum defines all supported uniform shader argument types.
enum UniformType {
  // Scalar types.
  kFloatUniform,
  kIntUniform,
  kUnsignedIntUniform,

  // Texture types.
  kCubeMapTextureUniform,
  kTextureUniform,

  // Vector types.
  kFloatVector2Uniform,
  kFloatVector3Uniform,
  kFloatVector4Uniform,
  kIntVector2Uniform,
  kIntVector3Uniform,
  kIntVector4Uniform,
  kUnsignedIntVector2Uniform,
  kUnsignedIntVector3Uniform,
  kUnsignedIntVector4Uniform,

  // Matrix types.
  kMatrix2x2Uniform,
  kMatrix3x3Uniform,
  kMatrix4x4Uniform,
};

typedef base::Variant<
    float, int, uint32,
    math::VectorBase2f, math::VectorBase3f, math::VectorBase4f,
    math::VectorBase2i, math::VectorBase3i, math::VectorBase4i,
    math::VectorBase2ui, math::VectorBase3ui, math::VectorBase4ui,
    math::Matrix2f, math::Matrix3f, math::Matrix4f,
    CubeMapTexturePtr, TexturePtr> UniformValueType;

// A Uniform instance represents a uniform shader argument. A Variant is used to
// store the actual type-specific value, and the interface is based on that.
// The Uniform class is designed to be lightweight enough that instances can be
// copied quickly, so they can be stored in vectors, used in stacks, and so on.
class ION_API Uniform : public ShaderInput<UniformValueType, UniformType> {
 public:
  // The default constructor creates an invalid Uniform instance, which
  // should never be used as is. IsValid() will return false for such an
  // instance.
  Uniform() {}
  ~Uniform() {}

  // Returns a string containing "uniform".
  static const char* GetShaderInputTypeName();

  // Returns a string representing a uniform type.
  static const char* GetValueTypeName(const ValueType type);

  // Returns the type for a templated value type. This is instantiated
  // for all supported types
  template <typename T> static ValueType GetTypeByValue();

  // Returns the tag for this input type.
  static Tag GetTag() { return kUniform; }

  // Merges the value of this with replacement if both have the same type.
  // This is useful for merging partial array uniforms. replacement will replace
  // values in this if the array ranges overlap.
  void MergeValuesFrom(const Uniform& other);

  bool operator==(const Uniform& other) const;
  bool operator!=(const Uniform& other) const {
    return !(*this == other);
  }

  // Merges replacement and base into merged. Returns true if a merge was
  // needed, false otherwise in which case replacement completely replaces base.
  static bool GetMerged(
      const Uniform& base, const Uniform& replacement, Uniform* merged);

 private:
  template <typename T>
  void MergeValuesInternal(const Uniform& replacement);
};

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_UNIFORM_H_
