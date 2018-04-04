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

#include "ion/gfx/uniform.h"

#include <algorithm>
#include <limits>

#include "ion/base/logging.h"
#include "ion/math/range.h"

namespace ion {
namespace gfx {

namespace {

// Helper function for comparing Uniform values with VectorBase types.
template <typename VectorType>
static inline bool AreVectorUniformsEqual(const Uniform& u0,
                                          const Uniform& u1) {
  return VectorType::AreValuesEqual(u0.GetValue<VectorType>(),
                                    u1.GetValue<VectorType>());
}

// Helper function for comparing Uniform values with VectorBase types.
template <typename T>
static inline bool AreUniformArraysEqual(const Uniform& u0,
                                         const Uniform& u1) {
  // Note that u0.IsArrayOf<T>() is checked before this function is called.
  bool equal = u0.GetCount() == u1.GetCount() && u1.IsArrayOf<T>();
  if (equal) {
    const size_t count = u0.GetCount();
    for (size_t i = 0; i < count; ++i)
      if (u0.GetValueAt<T>(i) != u1.GetValueAt<T>(i)) {
        equal = false;
        break;
      }
  }
  return equal;
}

// Helper function for comparing Uniform values with VectorBase types.
template <typename T>
static inline bool AreUniformVectorArraysEqual(const Uniform& u0,
                                               const Uniform& u1) {
  // Note that u0.IsArrayOf<T>() is checked before this function is called.
  bool equal = u0.GetCount() == u1.GetCount() && u1.IsArrayOf<T>();
  if (equal) {
    const size_t count = u0.GetCount();
    for (size_t i = 0; i < count; ++i)
      if (!T::AreValuesEqual(u0.GetValueAt<T>(i), u1.GetValueAt<T>(i))) {
        equal = false;
        break;
      }
  }
  return equal;
}
}  // anonymous namespace

const char* Uniform::GetShaderInputTypeName() {
  return "uniform";
}

const char* Uniform::GetValueTypeName(Uniform::ValueType type) {
  switch (type) {
    case kCubeMapTextureUniform: return "CubeMapTexture";
    case kFloatUniform: return "Float";
    case kIntUniform: return "Int";
    case kUnsignedIntUniform: return "UnsignedInt";
    case kTextureUniform: return "Texture";
    case kFloatVector2Uniform: return "FloatVector2";
    case kFloatVector3Uniform: return "FloatVector3";
    case kFloatVector4Uniform: return "FloatVector4";
    case kIntVector2Uniform: return "IntVector2";
    case kIntVector3Uniform: return "IntVector3";
    case kIntVector4Uniform: return "IntVector4";
    case kUnsignedIntVector2Uniform: return "UnsignedIntVector2";
    case kUnsignedIntVector3Uniform: return "UnsignedIntVector3";
    case kUnsignedIntVector4Uniform: return "UnsignedIntVector4";
    case kMatrix2x2Uniform: return "Matrix2x2";
    case kMatrix3x3Uniform: return "Matrix3x3";
    case kMatrix4x4Uniform: return "Matrix4x4";
    default: return "<UNKNOWN>";
  }
}

template <typename T> Uniform::ValueType Uniform::GetTypeByValue() {
  // The unspecialized version should never be called.
  LOG(FATAL) << "Unspecialized uniform Uniform::GetTypeByValue() called.";
  return kIntUniform;
}
// Specialize for each supported type.
template <> ION_API Uniform::ValueType Uniform::GetTypeByValue<float>() {
  return kFloatUniform;
}
template <> ION_API Uniform::ValueType Uniform::GetTypeByValue<int>() {
  return kIntUniform;
}
template <> ION_API Uniform::ValueType Uniform::GetTypeByValue<uint32>() {
  return kUnsignedIntUniform;
}
template <> ION_API
Uniform::ValueType Uniform::GetTypeByValue<CubeMapTexturePtr>() {
  return kCubeMapTextureUniform;
}
template <> ION_API
Uniform::ValueType Uniform::GetTypeByValue<TexturePtr>() {
  return kTextureUniform;
}
template <> ION_API
Uniform::ValueType Uniform::GetTypeByValue<math::VectorBase2f>() {
  return kFloatVector2Uniform;
}
template <> ION_API
Uniform::ValueType Uniform::GetTypeByValue<math::VectorBase3f>() {
  return kFloatVector3Uniform;
}
template <> ION_API
Uniform::ValueType Uniform::GetTypeByValue<math::VectorBase4f>() {
  return kFloatVector4Uniform;
}
template <> ION_API
Uniform::ValueType Uniform::GetTypeByValue<math::VectorBase2i>() {
  return kIntVector2Uniform;
}
template <> ION_API
Uniform::ValueType Uniform::GetTypeByValue<math::VectorBase3i>() {
  return kIntVector3Uniform;
}
template <> ION_API
Uniform::ValueType Uniform::GetTypeByValue<math::VectorBase4i>() {
  return kIntVector4Uniform;
}
template <> ION_API
Uniform::ValueType Uniform::GetTypeByValue<math::VectorBase2ui>() {
  return kUnsignedIntVector2Uniform;
}
template <> ION_API
Uniform::ValueType Uniform::GetTypeByValue<math::VectorBase3ui>() {
  return kUnsignedIntVector3Uniform;
}
template <> ION_API
Uniform::ValueType Uniform::GetTypeByValue<math::VectorBase4ui>() {
  return kUnsignedIntVector4Uniform;
}
template <> ION_API
Uniform::ValueType Uniform::GetTypeByValue<math::Matrix2f>() {
  return kMatrix2x2Uniform;
}
template <> ION_API
Uniform::ValueType Uniform::GetTypeByValue<math::Matrix3f>() {
  return kMatrix3x3Uniform;
}
template <> ION_API
Uniform::ValueType Uniform::GetTypeByValue<math::Matrix4f>() {
  return kMatrix4x4Uniform;
}

template <typename T>
void Uniform::MergeValuesInternal(const Uniform& replacement) {
  typedef math::Range<1, size_t> ArrayRange;
  Uniform u;
  const ArrayRange this_range = ArrayRange::BuildWithSize(
      GetArrayIndex(), GetCount() == 0U ? 0U : GetCount() - 1U);
  const ArrayRange replacement_range = ArrayRange::BuildWithSize(
      replacement.GetArrayIndex(),
      replacement.GetCount() == 0U ? 0U : replacement.GetCount() - 1U);
  // final_range is union of this_range and replacement_range.
  ArrayRange final_range = this_range;
  final_range.ExtendByRange(replacement_range);

  // Make the new value an array.
  u.InitArray<T>(GetRegistry(), GetRegistryId(), GetIndexInRegistry(),
                 final_range.GetMinPoint(), GetType(), nullptr,
                 final_range.GetSize() + 1,
                 GetArrayAllocator());
  const T* values = GetCount() ? &GetValueAt<T>(0) : &GetValue<T>();
  const T* replacement_values = replacement.GetCount() ?
      &replacement.GetValueAt<T>(0) : &replacement.GetValue<T>();
  size_t final_index = 0U;
  for (size_t i = final_range.GetMinPoint();
       i <= final_range.GetMaxPoint(); ++i) {
    if (replacement_range.ContainsPoint(i)) {
      // The replacement's values take precedence over the values in this.
      u.SetValueAt<T>(final_index, *replacement_values++);
      if (this_range.ContainsPoint(i))
        values++;
    } else if (this_range.ContainsPoint(i)) {
      // Any pre-existing values from this need to get added.
      u.SetValueAt<T>(final_index, *values++);
    }
    ++final_index;
  }
  *this = u;
}

void Uniform::MergeValuesFrom(const Uniform& other) {
  // We can only merge values that represent the same uniform, but we don't need
  // to merge a Uniform with itself.
  if (&GetRegistry() == &other.GetRegistry() &&
      GetIndexInRegistry() == other.GetIndexInRegistry() &&
      GetType() == other.GetType() &&
      &other != this) {
    if (!GetMerged(*this, other, this)) {
      // Just assign.
      *this = other;
    }
  }
}

bool Uniform::GetMerged(
    const Uniform& base, const Uniform& replacement, Uniform* merged) {
  // Don't merge same Uniforms or if either is invalid.
  if (&base == &replacement || !base.IsValid())
    return false;
  if (!replacement.IsValid()) {
    *merged = base;
    return true;
  }
  // We can only merge values that represent the same uniform.
  if (!(&base.GetRegistry() == &replacement.GetRegistry() &&
        base.GetIndexInRegistry() == replacement.GetIndexInRegistry() &&
        base.GetType() == replacement.GetType())) {
    return false;
  }
  // No need to merge if replacement covers entire extent of base.
  if (replacement.GetArrayIndex() <= base.GetArrayIndex() &&
      replacement.GetArrayIndex() + replacement.GetCount() >=
      base.GetArrayIndex() + base.GetCount()) {
    return false;
  }
  if (merged != &base)
    *merged = base;
  switch (base.GetType()) {
    case kFloatUniform:
      merged->MergeValuesInternal<float>(replacement);
      break;
    case kIntUniform:
      merged->MergeValuesInternal<int>(replacement);
      break;
    case kUnsignedIntUniform:
      merged->MergeValuesInternal<uint32>(replacement);
      break;
    case kCubeMapTextureUniform:
      merged->MergeValuesInternal<CubeMapTexturePtr>(replacement);
      break;
    case kTextureUniform:
      merged->MergeValuesInternal<TexturePtr>(replacement);
      break;
    case kFloatVector2Uniform:
      merged->MergeValuesInternal<math::VectorBase2f>(replacement);
      break;
    case kFloatVector3Uniform:
      merged->MergeValuesInternal<math::VectorBase3f>(replacement);
      break;
    case kFloatVector4Uniform:
      merged->MergeValuesInternal<math::VectorBase4f>(replacement);
      break;
    case kIntVector2Uniform:
      merged->MergeValuesInternal<math::VectorBase2i>(replacement);
      break;
    case kIntVector3Uniform:
      merged->MergeValuesInternal<math::VectorBase3i>(replacement);
      break;
    case kIntVector4Uniform:
      merged->MergeValuesInternal<math::VectorBase4i>(replacement);
      break;
    case kUnsignedIntVector2Uniform:
      merged->MergeValuesInternal<math::VectorBase2ui>(replacement);
      break;
    case kUnsignedIntVector3Uniform:
      merged->MergeValuesInternal<math::VectorBase3ui>(replacement);
      break;
    case kUnsignedIntVector4Uniform:
      merged->MergeValuesInternal<math::VectorBase4ui>(replacement);
      break;
    case kMatrix2x2Uniform:
      merged->MergeValuesInternal<math::Matrix2f>(replacement);
      break;
    case kMatrix3x3Uniform:
      merged->MergeValuesInternal<math::Matrix3f>(replacement);
      break;
    case kMatrix4x4Uniform:
      merged->MergeValuesInternal<math::Matrix4f>(replacement);
      break;
#if !defined(ION_COVERAGE)  // COV_NF_START
      // A Uniform type is explicitly set to a valid type.
    default:
      break;
#endif  // COV_NF_END
  }
  return true;
}

bool Uniform::operator==(const Uniform& other) const {
#define CHECK_UNIFORMS_EQUAL(type)                     \
  if (IsArrayOf<type>())                               \
    equal = AreUniformArraysEqual<type>(*this, other); \
  else                                                 \
    equal = GetValue<type>() == other.GetValue<type>();

#define CHECK_VECTOR_UNIFORMS_EQUAL(type)                    \
  if (IsArrayOf<type>())                                     \
    equal = AreUniformVectorArraysEqual<type>(*this, other); \
  else                                                       \
    equal = AreVectorUniformsEqual<type>(*this, other);

  if (&GetRegistry() == &other.GetRegistry() &&
      GetIndexInRegistry() == other.GetIndexInRegistry() &&
      GetType() == other.GetType()) {
    // Check the value.
    bool equal = true;
    switch (GetType()) {
      case kFloatUniform:
        CHECK_UNIFORMS_EQUAL(float);
        break;
      case kIntUniform:
        CHECK_UNIFORMS_EQUAL(int);
        break;
      case kUnsignedIntUniform:
        CHECK_UNIFORMS_EQUAL(uint32);
        break;
      case kCubeMapTextureUniform:
        CHECK_UNIFORMS_EQUAL(CubeMapTexturePtr);
        break;
      case kTextureUniform:
        CHECK_UNIFORMS_EQUAL(TexturePtr);
        break;
      case kFloatVector2Uniform:
        CHECK_VECTOR_UNIFORMS_EQUAL(math::VectorBase2f);
        break;
      case kFloatVector3Uniform:
        CHECK_VECTOR_UNIFORMS_EQUAL(math::VectorBase3f);
        break;
      case kFloatVector4Uniform:
        CHECK_VECTOR_UNIFORMS_EQUAL(math::VectorBase4f);
        break;
      case kIntVector2Uniform:
        CHECK_VECTOR_UNIFORMS_EQUAL(math::VectorBase2i);
        break;
      case kIntVector3Uniform:
        CHECK_VECTOR_UNIFORMS_EQUAL(math::VectorBase3i);
        break;
      case kIntVector4Uniform:
        CHECK_VECTOR_UNIFORMS_EQUAL(math::VectorBase4i);
        break;
      case kUnsignedIntVector2Uniform:
        CHECK_VECTOR_UNIFORMS_EQUAL(math::VectorBase2ui);
        break;
      case kUnsignedIntVector3Uniform:
        CHECK_VECTOR_UNIFORMS_EQUAL(math::VectorBase3ui);
        break;
      case kUnsignedIntVector4Uniform:
        CHECK_VECTOR_UNIFORMS_EQUAL(math::VectorBase4ui);
        break;
      case kMatrix2x2Uniform:
        CHECK_UNIFORMS_EQUAL(math::Matrix2f);
        break;
      case kMatrix3x3Uniform:
        CHECK_UNIFORMS_EQUAL(math::Matrix3f);
        break;
      case kMatrix4x4Uniform:
        CHECK_UNIFORMS_EQUAL(math::Matrix4f);
        break;
#if !defined(ION_COVERAGE)  // COV_NF_START
      // A Uniform type is explicitly set to a valid type.
      default:
        break;
#endif  // COV_NF_END
    }
    return equal;
  } else {
    return false;
  }

#undef CHECK_UNIFORMS_EQUAL
#undef CHECK_VECTOR_UNIFORMS_EQUAL
}
}  // namespace gfx
}  // namespace ion
