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

#ifndef ION_GFX_ATTRIBUTE_H_
#define ION_GFX_ATTRIBUTE_H_

#include "ion/gfx/shaderinput.h"

#include "base/integral_types.h"
#include "ion/base/variant.h"
#include "ion/gfx/bufferobject.h"
#include "ion/math/matrix.h"
#include "ion/math/vector.h"

namespace ion {
namespace gfx {

// The AttributeType enum defines all supported attribute shader argument types.
enum AttributeType {
  // Scalar types.
  kFloatAttribute,

  // Vector types.
  kFloatVector2Attribute,
  kFloatVector3Attribute,
  kFloatVector4Attribute,

  // Matrix types.
  kFloatMatrix2x2Attribute,
  kFloatMatrix3x3Attribute,
  kFloatMatrix4x4Attribute,

  // Vertex buffer element.
  kBufferObjectElementAttribute
};

typedef base::Variant<float, math::VectorBase2f, math::VectorBase3f,
                      math::VectorBase4f, math::Matrix2f, math::Matrix3f,
                      math::Matrix4f, BufferObjectElement> AttributeValueType;

class ION_API Attribute :
    public ShaderInput<AttributeValueType, AttributeType> {
 public:
  // The default constructor creates an invalid Attribute instance, which
  // should never be used as is. IsValid() will return false for such an
  // instance.
  Attribute()
      : ShaderInput<AttributeValueType, AttributeType>(),
        normalize_(false),
        divisor_(0) {}
  ~Attribute() {}

  // Returns a string containing "attribute".
  static const char* GetShaderInputTypeName();

  // Returns a string representing a attribute type.
  static const char* GetValueTypeName(const ValueType type);

  // Returns the type for a templated value type. This is instantiated
  // for all supported types
  template <typename T> static ValueType GetTypeByValue();

  // Returns the tag for this input type.
  static Tag GetTag() { return kAttribute; }

  // Checks and sets whether integer values should be mapped to the range [-1,1]
  // (for signed values) or [0,1] (for unsigned values) when they are accessed
  // and converted to floating point. If this is false, values will be converted
  // to floats directly without any normalization. Note that the normalization
  // is performed by the graphics hardware on the fly. By default data is not
  // normalized.
  bool IsFixedPointNormalized() const { return normalize_; }
  void SetFixedPointNormalized(bool normalize) { normalize_ = normalize; }

  bool operator==(const Attribute& other) const;
  // Needed for Field::Set().
  bool operator!=(const Attribute& other) const {
    return !(*this == other);
  }

  // Sets/Gets the attribute divisor.
  unsigned int GetDivisor() const { return divisor_; }
  void SetDivisor(unsigned int divisor) { divisor_ = divisor; }

 private:
  // Whether the attribute should be normalized when sent to the GL.
  bool normalize_;
  // The rate at which new values of the instanced attribute are presented to
  // the shader during instanced rendering.
  unsigned int divisor_;
};

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_ATTRIBUTE_H_
