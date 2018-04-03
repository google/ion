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

#include "ion/gfx/attribute.h"

#include "ion/base/logging.h"

namespace ion {
namespace gfx {

const char* Attribute::GetShaderInputTypeName() {
  return "attribute";
}

const char* Attribute::GetValueTypeName(Attribute::ValueType type) {
  switch (type) {
    case kFloatAttribute: return "Float";
    case kFloatVector2Attribute: return "FloatVector2";
    case kFloatVector3Attribute: return "FloatVector3";
    case kFloatVector4Attribute: return "FloatVector4";
    case kFloatMatrix2x2Attribute: return "FloatMatrix2x2";
    case kFloatMatrix3x3Attribute: return "FloatMatrix3x3";
    case kFloatMatrix4x4Attribute: return "FloatMatrix4x4";
    case kBufferObjectElementAttribute: return "BufferObjectElement";
    default: return "<UNKNOWN>";
  }
}

template <typename T>
Attribute::ValueType Attribute::GetTypeByValue() {
  // The unspecialized version should never be called.
  LOG(FATAL) << "Unspecialized attribute GetTypeByValue() called.";
  return kFloatAttribute;
}
// Specialize for each supported type.
template <> ION_API
Attribute::ValueType Attribute::GetTypeByValue<float>() {
  return kFloatAttribute;
}
template <> ION_API
Attribute::ValueType Attribute::GetTypeByValue<math::VectorBase2f>() {
  return kFloatVector2Attribute;
}
template <> ION_API
Attribute::ValueType Attribute::GetTypeByValue<math::VectorBase3f>() {
  return kFloatVector3Attribute;
}
template <> ION_API
Attribute::ValueType Attribute::GetTypeByValue<math::VectorBase4f>() {
  return kFloatVector4Attribute;
}
template <> ION_API
Attribute::ValueType Attribute::GetTypeByValue<math::Matrix2f>() {
  return kFloatMatrix2x2Attribute;
}
template <> ION_API
Attribute::ValueType Attribute::GetTypeByValue<math::Matrix3f>() {
  return kFloatMatrix3x3Attribute;
}
template <> ION_API
Attribute::ValueType Attribute::GetTypeByValue<math::Matrix4f>() {
  return kFloatMatrix4x4Attribute;
}
template <> ION_API
Attribute::ValueType Attribute::GetTypeByValue<BufferObjectElement>() {
  return kBufferObjectElementAttribute;
}

bool Attribute::operator==(const Attribute& other) const {
  if (&GetRegistry() == &other.GetRegistry() &&
      GetIndexInRegistry() == other.GetIndexInRegistry() &&
      GetType() == other.GetType() &&
      normalize_ == other.normalize_) {
    // Check the value.
    bool equal = true;
    switch (GetType()) {
      case kFloatAttribute:
        equal = GetValue<float>() == other.GetValue<float>();
        break;
      case kFloatVector2Attribute:
        equal = math::VectorBase2f::AreValuesEqual(
            GetValue<math::VectorBase2f>(),
            other.GetValue<math::VectorBase2f>());
        break;
      case kFloatVector3Attribute:
        equal = math::VectorBase3f::AreValuesEqual(
            GetValue<math::VectorBase3f>(),
            other.GetValue<math::VectorBase3f>());
        break;
      case kFloatVector4Attribute:
        equal = math::VectorBase4f::AreValuesEqual(
            GetValue<math::VectorBase4f>(),
            other.GetValue<math::VectorBase4f>());
        break;
      case kFloatMatrix2x2Attribute:
        equal = GetValue<math::Matrix2f>() == other.GetValue<math::Matrix2f>();
        break;
      case kFloatMatrix3x3Attribute:
        equal = GetValue<math::Matrix3f>() == other.GetValue<math::Matrix3f>();
        break;
      case kFloatMatrix4x4Attribute:
        equal = GetValue<math::Matrix4f>() == other.GetValue<math::Matrix4f>();
        break;
      default:  // kBufferObjectElementAttribute.
        equal = GetValue<BufferObjectElement>().buffer_object.Get() ==
            other.GetValue<BufferObjectElement>().buffer_object.Get() &&
            GetValue<BufferObjectElement>().spec_index ==
            other.GetValue<BufferObjectElement>().spec_index;
        break;
    }
    return equal;
  } else {
    return false;
  }
}

}  // namespace gfx
}  // namespace ion
