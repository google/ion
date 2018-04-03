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
#include <cstring>  // For strcmp().
#include <limits>
#include <memory>
#include <vector>

#include "ion/base/allocatable.h"
#include "ion/base/invalid.h"
#include "ion/base/logchecker.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/math/vector.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfx {

namespace {

// Helper function to add a uniform to a registry.
static bool AddUniform(const ShaderInputRegistryPtr& reg,
                       const std::string& name,
                       const Uniform::ValueType type,
                       const std::string& doc) {
  return reg->Add(ShaderInputRegistry::UniformSpec(name, type, doc));
}

// Returns an array Uniform of the passed name created using the passed registry
// and values to initialize it.
template <typename T>
static Uniform CreateArrayUniform(const ShaderInputRegistryPtr& reg,
                                  const std::string& name,
                                  const std::vector<T>& values) {
  return reg->CreateArrayUniform(name, &values[0], values.size(),
                                 base::AllocatorPtr());
}

}  // anonymous namespace

TEST(UniformTest, CreateUniform) {
  // Prebuilt math::Vector2f for convenience.
  static const math::Vector2f kVec2(1.0f, 2.0f);

  base::LogChecker log_checker;

  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  EXPECT_TRUE(AddUniform(reg, "myInt", kIntUniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myFloat", kFloatUniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myUint", kUnsignedIntUniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myCubeMapTex", kCubeMapTextureUniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myTex", kTextureUniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec2f", kFloatVector2Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec3f", kFloatVector3Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec4f", kFloatVector4Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec2i", kIntVector2Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec3i", kIntVector3Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec4i", kIntVector4Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec2ui", kUnsignedIntVector2Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec3ui", kUnsignedIntVector3Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec4ui", kUnsignedIntVector4Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myMat2f", kMatrix2x2Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myMat3f", kMatrix3x3Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myMat4f", kMatrix4x4Uniform, ""));

  Uniform u;
  EXPECT_FALSE(u.IsValid());
  EXPECT_TRUE(ShaderInputRegistry::GetSpec(u) == nullptr);
  EXPECT_EQ(0U, u.GetStamp());

  // Create.
  u = reg->Create<Uniform>("myFloat", 17.2f);
  EXPECT_TRUE(u.IsValid());
  EXPECT_EQ(reg.Get(), &u.GetRegistry());
  EXPECT_EQ(1U, u.GetIndexInRegistry());
  EXPECT_EQ(kFloatUniform, u.GetType());
  ASSERT_FALSE(base::IsInvalidReference(u.GetValue<float>()));
  EXPECT_EQ(17.2f, u.GetValue<float>());
  EXPECT_TRUE(base::IsInvalidReference(u.GetValue<int>()));
  EXPECT_TRUE(ShaderInputRegistry::GetSpec(u) != nullptr);
  EXPECT_LT(0U, u.GetStamp());
  uint64 initial_stamp = u.GetStamp();

  // Copy should be fine.
  Uniform u2 = u;
  EXPECT_TRUE(u2.IsValid());
  EXPECT_EQ(reg.Get(), &u2.GetRegistry());
  EXPECT_EQ(1U, u2.GetIndexInRegistry());
  EXPECT_EQ(kFloatUniform, u2.GetType());
  ASSERT_FALSE(base::IsInvalidReference(u.GetValue<float>()));
  EXPECT_EQ(17.2f, u2.GetValue<float>());
  EXPECT_TRUE(base::IsInvalidReference(u.GetValue<int>()));
  EXPECT_TRUE(ShaderInputRegistry::GetSpec(u2) != nullptr);
  EXPECT_EQ(initial_stamp, u.GetStamp());
  EXPECT_EQ(initial_stamp, u2.GetStamp());

  // Test == and !=.
  EXPECT_TRUE(u == u2);
  EXPECT_TRUE(u2 == u);
  EXPECT_TRUE(u == u);
  EXPECT_TRUE(u2 == u2);
  EXPECT_FALSE(u != u2);
  EXPECT_FALSE(u2 != u);
  EXPECT_FALSE(u != u);
  EXPECT_FALSE(u2 != u2);

  // Check some more types for coverage.
  static const int kNumUniformTypes =
      static_cast<int>(kMatrix4x4Uniform) + 1;
  std::unique_ptr<Uniform[]> uniforms(new Uniform[2 * kNumUniformTypes]);
  std::unique_ptr<Uniform[]> uniforms2(new Uniform[2 * kNumUniformTypes]);
  uniforms[kIntUniform] = reg->Create<Uniform>("myInt", 1);
  uniforms[kFloatUniform] = reg->Create<Uniform>("myFloat", 1.f);
  uniforms[kUnsignedIntUniform] = reg->Create<Uniform>("myUint", 2U);
  uniforms[kTextureUniform] =
      reg->Create<Uniform>("myTex", TexturePtr(new Texture));
  uniforms[kCubeMapTextureUniform] = reg->Create<Uniform>(
      "myCubeMapTex", CubeMapTexturePtr(new CubeMapTexture));
  uniforms[kFloatVector2Uniform] =
      reg->Create<Uniform>("myVec2f", math::Vector2f(1.f, 2.f));
  uniforms[kFloatVector3Uniform] =
      reg->Create<Uniform>("myVec3f", math::Vector3f(1.f, 2.f, 3.f));
  uniforms[kFloatVector4Uniform] =
      reg->Create<Uniform>("myVec4f", math::Vector4f(1.f, 2.f, 3.f, 4.f));
  uniforms[kIntVector2Uniform] =
      reg->Create<Uniform>("myVec2i", math::Vector2i(1, 2));
  uniforms[kIntVector3Uniform] =
      reg->Create<Uniform>("myVec3i", math::Vector3i(1, 2, 3));
  uniforms[kIntVector4Uniform] =
      reg->Create<Uniform>("myVec4i", math::Vector4i(1, 2, 3, 4));
  uniforms[kUnsignedIntVector2Uniform] =
      reg->Create<Uniform>("myVec2ui", math::Vector2ui(1, 2));
  uniforms[kUnsignedIntVector3Uniform] =
      reg->Create<Uniform>("myVec3ui", math::Vector3ui(1, 2, 3));
  uniforms[kUnsignedIntVector4Uniform] =
      reg->Create<Uniform>("myVec4ui", math::Vector4ui(1, 2, 3, 4));
  uniforms[kMatrix2x2Uniform] =
      reg->Create<Uniform>("myMat2f", math::Matrix2f::Identity());
  uniforms[kMatrix3x3Uniform] =
      reg->Create<Uniform>("myMat3f", math::Matrix3f::Identity());
  uniforms[kMatrix4x4Uniform] =
      reg->Create<Uniform>("myMat4f", math::Matrix4f::Identity());

  uniforms2[kIntUniform] = reg->Create<Uniform>("myInt", 2);
  uniforms2[kFloatUniform] = reg->Create<Uniform>("myFloat", 2.f);
  uniforms2[kUnsignedIntUniform] = reg->Create<Uniform>("myUint", 3U);
  uniforms2[kTextureUniform] =
      reg->Create<Uniform>("myTex", TexturePtr(new Texture));
  uniforms2[kCubeMapTextureUniform] = reg->Create<Uniform>(
      "myCubeMapTex", CubeMapTexturePtr(new CubeMapTexture));
  uniforms2[kFloatVector2Uniform] =
      reg->Create<Uniform>("myVec2f", math::Vector2f(2.f, 1.f));
  uniforms2[kFloatVector3Uniform] =
      reg->Create<Uniform>("myVec3f", math::Vector3f(3.f, 2.f, 1.f));
  uniforms2[kFloatVector4Uniform] =
      reg->Create<Uniform>("myVec4f", math::Vector4f(4.f, 3.f, 2.f, 1.f));
  uniforms2[kIntVector2Uniform] =
      reg->Create<Uniform>("myVec2i", math::Vector2i(2, 1));
  uniforms2[kIntVector3Uniform] =
      reg->Create<Uniform>("myVec3i", math::Vector3i(3, 2, 1));
  uniforms2[kIntVector4Uniform] =
      reg->Create<Uniform>("myVec4i", math::Vector4i(4, 3, 2, 1));
  uniforms2[kUnsignedIntVector2Uniform] =
      reg->Create<Uniform>("myVec2ui", math::Vector2i(2, 1));
  uniforms2[kUnsignedIntVector3Uniform] =
      reg->Create<Uniform>("myVec3ui", math::Vector3i(3, 2, 1));
  uniforms2[kUnsignedIntVector4Uniform] =
      reg->Create<Uniform>("myVec4ui", math::Vector4i(4, 3, 2, 1));
  uniforms2[kMatrix2x2Uniform] =
      reg->Create<Uniform>("myMat2f", math::Matrix2f::Identity() * 2.f);
  uniforms2[kMatrix3x3Uniform] =
      reg->Create<Uniform>("myMat3f", math::Matrix3f::Identity() * 2.f);
  uniforms2[kMatrix4x4Uniform] =
      reg->Create<Uniform>("myMat4f", math::Matrix4f::Identity() * 2.f);

  std::vector<int> ints;
  ints.push_back(1);
  ints.push_back(2);
  std::vector<float> floats;
  floats.push_back(1.f);
  floats.push_back(2.f);
  std::vector<uint32> uints;
  uints.push_back(1U);
  uints.push_back(2U);
  std::vector<TexturePtr> textures;
  textures.push_back(TexturePtr(new Texture));
  textures.push_back(TexturePtr(new Texture));
  std::vector<CubeMapTexturePtr> cubemaps;
  cubemaps.push_back(CubeMapTexturePtr(new CubeMapTexture));
  cubemaps.push_back(CubeMapTexturePtr(new CubeMapTexture));
  std::vector<math::Vector2i> vector2is;
  vector2is.push_back(math::Vector2i(1, 2));
  vector2is.push_back(math::Vector2i(3, 4));
  std::vector<math::Vector3i> vector3is;
  vector3is.push_back(math::Vector3i(1, 2, 3));
  vector3is.push_back(math::Vector3i(4, 5, 6));
  std::vector<math::Vector4i> vector4is;
  vector4is.push_back(math::Vector4i(1, 2, 3, 4));
  vector4is.push_back(math::Vector4i(5, 6, 7, 8));
  std::vector<math::Vector2ui> vector2uis;
  vector2uis.push_back(math::Vector2ui(1U, 2U));
  vector2uis.push_back(math::Vector2ui(3U, 4U));
  std::vector<math::Vector3ui> vector3uis;
  vector3uis.push_back(math::Vector3ui(1U, 2U, 3U));
  vector3uis.push_back(math::Vector3ui(4U, 5U, 6U));
  std::vector<math::Vector4ui> vector4uis;
  vector4uis.push_back(math::Vector4ui(1U, 2U, 3U, 4U));
  vector4uis.push_back(math::Vector4ui(5U, 6U, 7U, 8U));
  std::vector<math::Vector2f> vector2fs;
  vector2fs.push_back(math::Vector2f(1.f, 2.f));
  vector2fs.push_back(math::Vector2f(3.f, 4.f));
  std::vector<math::Vector3f> vector3fs;
  vector3fs.push_back(math::Vector3f(1.f, 2.f, 3.f));
  vector3fs.push_back(math::Vector3f(4.f, 5.f, 6.f));
  std::vector<math::Vector4f> vector4fs;
  vector4fs.push_back(math::Vector4f(1.f, 2.f, 3.f, 4.f));
  vector4fs.push_back(math::Vector4f(5.f, 6.f, 7.f, 8.f));
  std::vector<math::Matrix2f> matrix2fs;
  matrix2fs.push_back(math::Matrix2f::Identity());
  matrix2fs.push_back(math::Matrix2f::Identity() * 2.f);
  std::vector<math::Matrix3f> matrix3fs;
  matrix3fs.push_back(math::Matrix3f::Identity());
  matrix3fs.push_back(math::Matrix3f::Identity() * 2.f);
  std::vector<math::Matrix4f> matrix4fs;
  matrix4fs.push_back(math::Matrix4f::Identity());
  matrix4fs.push_back(math::Matrix4f::Identity() * 2.f);

  uniforms[kNumUniformTypes + kIntUniform] =
      CreateArrayUniform(reg, "myIntArray", ints);
  uniforms[kNumUniformTypes + kFloatUniform] =
      CreateArrayUniform(reg, "myFloatArray", floats);
  uniforms[kNumUniformTypes + kUnsignedIntUniform] =
      CreateArrayUniform(reg, "myUintArray", ints);
  uniforms[kNumUniformTypes + kTextureUniform] =
      CreateArrayUniform(reg, "myTexArray", textures);
  uniforms[kNumUniformTypes + kCubeMapTextureUniform] =
      CreateArrayUniform(reg, "myCubeMapTexArray", cubemaps);
  uniforms[kNumUniformTypes + kFloatVector2Uniform] =
      CreateArrayUniform(reg, "myVec2fArray", vector2fs);
  uniforms[kNumUniformTypes + kFloatVector3Uniform] =
      CreateArrayUniform(reg, "myVec3fArray", vector3fs);
  uniforms[kNumUniformTypes + kFloatVector4Uniform] =
      CreateArrayUniform(reg, "myVec4fArray", vector4fs);
  uniforms[kNumUniformTypes + kIntVector2Uniform] =
      CreateArrayUniform(reg, "myVec2iArray", vector2is);
  uniforms[kNumUniformTypes + kIntVector3Uniform] =
      CreateArrayUniform(reg, "myVec3iArray", vector3is);
  uniforms[kNumUniformTypes + kIntVector4Uniform] =
      CreateArrayUniform(reg, "myVec4iArray", vector4is);
  uniforms[kNumUniformTypes + kUnsignedIntVector2Uniform] =
      CreateArrayUniform(reg, "myVec2uiArray", vector2uis);
  uniforms[kNumUniformTypes + kUnsignedIntVector3Uniform] =
      CreateArrayUniform(reg, "myVec3uiArray", vector3uis);
  uniforms[kNumUniformTypes + kUnsignedIntVector4Uniform] =
      CreateArrayUniform(reg, "myVec4uiArray", vector4uis);
  uniforms[kNumUniformTypes + kMatrix2x2Uniform] =
      CreateArrayUniform(reg, "myMat2fArray", matrix2fs);
  uniforms[kNumUniformTypes + kMatrix3x3Uniform] =
      CreateArrayUniform(reg, "myMat3fArray", matrix3fs);
  uniforms[kNumUniformTypes + kMatrix4x4Uniform] =
      CreateArrayUniform(reg, "myMat4fArray", matrix4fs);

  std::reverse(ints.begin(), ints.end());
  std::reverse(floats.begin(), floats.end());
  std::reverse(uints.begin(), uints.end());
  std::reverse(textures.begin(), textures.end());
  std::reverse(cubemaps.begin(), cubemaps.end());
  std::reverse(vector2fs.begin(), vector2fs.end());
  std::reverse(vector3fs.begin(), vector3fs.end());
  std::reverse(vector4fs.begin(), vector4fs.end());
  std::reverse(vector2is.begin(), vector2is.end());
  std::reverse(vector3is.begin(), vector3is.end());
  std::reverse(vector4is.begin(), vector4is.end());
  std::reverse(vector2uis.begin(), vector2uis.end());
  std::reverse(vector3uis.begin(), vector3uis.end());
  std::reverse(vector4uis.begin(), vector4uis.end());
  std::reverse(matrix2fs.begin(), matrix2fs.end());
  std::reverse(matrix3fs.begin(), matrix3fs.end());
  std::reverse(matrix4fs.begin(), matrix4fs.end());
  uniforms2[kNumUniformTypes + kIntUniform] =
      CreateArrayUniform(reg, "myIntArray", ints);
  uniforms2[kNumUniformTypes + kFloatUniform] =
      CreateArrayUniform(reg, "myFloatArray", floats);
  uniforms2[kNumUniformTypes + kUnsignedIntUniform] =
      CreateArrayUniform(reg, "myUintArray", uints);
  uniforms2[kNumUniformTypes + kTextureUniform] =
      CreateArrayUniform(reg, "myTexArray", textures);
  uniforms2[kNumUniformTypes + kCubeMapTextureUniform] =
      CreateArrayUniform(reg, "myCubeMapTexArray", cubemaps);
  uniforms2[kNumUniformTypes + kFloatVector2Uniform] =
      CreateArrayUniform(reg, "myVec2fArray", vector2fs);
  uniforms2[kNumUniformTypes + kFloatVector3Uniform] =
      CreateArrayUniform(reg, "myVec3fArray", vector3fs);
  uniforms2[kNumUniformTypes + kFloatVector4Uniform] =
      CreateArrayUniform(reg, "myVec4fArray", vector4fs);
  uniforms2[kNumUniformTypes + kIntVector2Uniform] =
      CreateArrayUniform(reg, "myVec2iArray", vector2is);
  uniforms2[kNumUniformTypes + kIntVector3Uniform] =
      CreateArrayUniform(reg, "myVec3iArray", vector3is);
  uniforms2[kNumUniformTypes + kIntVector4Uniform] =
      CreateArrayUniform(reg, "myVec4iArray", vector4is);
  uniforms2[kNumUniformTypes + kUnsignedIntVector2Uniform] =
      CreateArrayUniform(reg, "myVec2uiArray", vector2uis);
  uniforms2[kNumUniformTypes + kUnsignedIntVector3Uniform] =
      CreateArrayUniform(reg, "myVec3uiArray", vector3uis);
  uniforms2[kNumUniformTypes + kUnsignedIntVector4Uniform] =
      CreateArrayUniform(reg, "myVec4uiArray", vector4uis);
  uniforms2[kNumUniformTypes + kMatrix2x2Uniform] =
      CreateArrayUniform(reg, "myMat2fArray", matrix2fs);
  uniforms2[kNumUniformTypes + kMatrix3x3Uniform] =
      CreateArrayUniform(reg, "myMat3fArray", matrix3fs);
  uniforms2[kNumUniformTypes + kMatrix4x4Uniform] =
      CreateArrayUniform(reg, "myMat4fArray", matrix4fs);

  for (int i = 0; i < 2 * kNumUniformTypes; ++i) {
    for (int j = 0; j < 2 * kNumUniformTypes; ++j) {
      SCOPED_TRACE(::testing::Message()
                   << "Testing if type " << i << " == " << j << ": "
                   << (i >= kNumUniformTypes ? "array " : "") << "type "
                   << Uniform::GetValueTypeName(
                          static_cast<UniformType>(i % kNumUniformTypes))
                   << " == " << (j >= kNumUniformTypes ? "array " : "")
                   << "type "
                   << Uniform::GetValueTypeName(
                          static_cast<UniformType>(j % kNumUniformTypes)));
      if (i == j)
        EXPECT_EQ(uniforms[i], uniforms[j]);
      else
        EXPECT_NE(uniforms[i], uniforms[j]);
      EXPECT_NE(uniforms[i], uniforms2[j]);
    }
  }

  // Change to correct value type.
  EXPECT_TRUE(u.SetValue(48.1f));
  EXPECT_TRUE(u.IsValid());
  EXPECT_EQ(reg.Get(), &u.GetRegistry());
  EXPECT_EQ(1U, u.GetIndexInRegistry());
  EXPECT_EQ(kFloatUniform, u.GetType());
  ASSERT_FALSE(base::IsInvalidReference(u.GetValue<float>()));
  EXPECT_EQ(48.1f, u.GetValue<float>());
  EXPECT_TRUE(base::IsInvalidReference(u.GetValue<int>()));
  EXPECT_TRUE(ShaderInputRegistry::GetSpec(u) != nullptr);

  // Change to bad type; leaves Uniform untouched.
  initial_stamp = u.GetStamp();
  EXPECT_FALSE(u.SetValue(kVec2));
  EXPECT_TRUE(u.IsValid());
  EXPECT_EQ(initial_stamp, u.GetStamp());
  EXPECT_EQ(reg.Get(), &u.GetRegistry());
  EXPECT_EQ(1U, u.GetIndexInRegistry());
  EXPECT_EQ(kFloatUniform, u.GetType());
  ASSERT_FALSE(base::IsInvalidReference(u.GetValue<float>()));
  EXPECT_EQ(48.1f, u.GetValue<float>());
  EXPECT_TRUE(base::IsInvalidReference(u.GetValue<int>()));
  EXPECT_TRUE(ShaderInputRegistry::GetSpec(u) != nullptr);

  // Create with bad value type.
  u = reg->Create<Uniform>("myFloat", kVec2);
  EXPECT_FALSE(u.IsValid());
  EXPECT_TRUE(ShaderInputRegistry::GetSpec(u) == nullptr);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "wrong value_type"));

  // Create with an unknown name.
  u = reg->Create<Uniform>("badName", 52);
  EXPECT_TRUE(u.IsValid());
  EXPECT_EQ(52, u.GetValue<int>());
  EXPECT_TRUE(ShaderInputRegistry::GetSpec(u) != nullptr);

  u = Uniform();
  // Copy of an invalid Uniform should also be invalid.
  u2 = u;
  EXPECT_FALSE(u2.IsValid());
  EXPECT_TRUE(ShaderInputRegistry::GetSpec(u2) == nullptr);

  EXPECT_FALSE(log_checker.HasAnyMessages());

  // Ensure that copy of Uniform doesn't end up with same stamp but different
  // values.
  u = reg->Create<Uniform>("myFloat", 3.14f);
  Uniform copy(u);
  EXPECT_EQ(u.GetStamp(), copy.GetStamp());
  u.SetValue(8.2f);
  EXPECT_NE(u.GetStamp(), copy.GetStamp());
  copy.SetValue(9.1f);
  EXPECT_NE(u.GetStamp(), copy.GetStamp());
}

TEST(UniformTest, NonArrayTypes) {
  // Make sure all Uniforms of all types are created properly.
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  EXPECT_TRUE(AddUniform(reg, "myInt", kIntUniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myFloat", kFloatUniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myUint", kUnsignedIntUniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myTexture", kTextureUniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myCubeMapTexture", kCubeMapTextureUniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec2f", kFloatVector2Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec3f", kFloatVector3Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec4f", kFloatVector4Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec2i", kIntVector2Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec3i", kIntVector3Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec4i", kIntVector4Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec2ui", kUnsignedIntVector2Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec3ui", kUnsignedIntVector3Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec4ui", kUnsignedIntVector4Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myMat2", kMatrix2x2Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myMat3", kMatrix3x3Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myMat4", kMatrix4x4Uniform, ""));
  Uniform u;

#define TEST_UNIFORM_TYPE(name, type_name, value_type, uniform_type, value) \
  u = reg->Create<Uniform>(name, value);                                    \
  EXPECT_EQ(0, strcmp(type_name, Uniform::GetValueTypeName(uniform_type))); \
  EXPECT_EQ(uniform_type, u.GetType());                                     \
  ASSERT_TRUE(u.Is<value_type>());                                          \
  ASSERT_FALSE(base::IsInvalidReference(u.GetValue<value_type>()));         \
  EXPECT_EQ(value, u.GetValue<value_type>())

#define TEST_VEC_UNIFORM_TYPE(name, type_name, value_type, uniform_type,    \
                              value)                                        \
  u = reg->Create<Uniform>(name, value);                                    \
  EXPECT_EQ(0, strcmp(type_name, Uniform::GetValueTypeName(uniform_type))); \
  EXPECT_EQ(uniform_type, u.GetType());                                     \
  ASSERT_TRUE(u.Is<value_type>());                                          \
  ASSERT_FALSE(base::IsInvalidReference(u.GetValue<value_type>()));         \
  EXPECT_TRUE(value_type::AreValuesEqual(value, u.GetValue<value_type>()))

  TexturePtr texture(new Texture);
  CubeMapTexturePtr cubemap(new CubeMapTexture);
  TEST_UNIFORM_TYPE("myInt", "Int", int, kIntUniform, 3);
  TEST_UNIFORM_TYPE("myFloat", "Float", float, kFloatUniform, 32.5f);
  TEST_UNIFORM_TYPE("myUint", "UnsignedInt", uint32, kUnsignedIntUniform, 3U);
  TEST_UNIFORM_TYPE(
      "myTexture", "Texture", TexturePtr, kTextureUniform, texture);
  TEST_UNIFORM_TYPE(
      "myCubeMapTexture", "CubeMapTexture", CubeMapTexturePtr,
      kCubeMapTextureUniform, cubemap);
  TEST_VEC_UNIFORM_TYPE("myVec2f", "FloatVector2", math::VectorBase2f,
                        kFloatVector2Uniform, math::Vector2f(1.0f, 2.0f));
  TEST_VEC_UNIFORM_TYPE("myVec3f", "FloatVector3", math::VectorBase3f,
                        kFloatVector3Uniform, math::Vector3f(1.0f, 2.0f, 3.0f));
  TEST_VEC_UNIFORM_TYPE("myVec4f", "FloatVector4", math::VectorBase4f,
                        kFloatVector4Uniform,
                        math::Vector4f(1.0f, 2.0f, 3.0f, 4.0f));
  TEST_VEC_UNIFORM_TYPE("myVec2i", "IntVector2", math::VectorBase2i,
                        kIntVector2Uniform, math::Vector2i(1, 2));
  TEST_VEC_UNIFORM_TYPE("myVec3i", "IntVector3", math::VectorBase3i,
                        kIntVector3Uniform, math::Vector3i(1, 2, 3));
  TEST_VEC_UNIFORM_TYPE("myVec4i", "IntVector4", math::VectorBase4i,
                        kIntVector4Uniform, math::Vector4i(1, 2, 3, 4));
  TEST_VEC_UNIFORM_TYPE("myVec2ui", "UnsignedIntVector2", math::VectorBase2ui,
                        kUnsignedIntVector2Uniform, math::Vector2ui(1U, 2U));
  TEST_VEC_UNIFORM_TYPE("myVec3ui", "UnsignedIntVector3", math::VectorBase3ui,
                        kUnsignedIntVector3Uniform,
                        math::Vector3ui(1U, 2U, 3U));
  TEST_VEC_UNIFORM_TYPE("myVec4ui", "UnsignedIntVector4", math::VectorBase4ui,
                        kUnsignedIntVector4Uniform,
                        math::Vector4ui(1U, 2U, 3U, 4U));
  TEST_UNIFORM_TYPE("myMat2", "Matrix2x2", math::Matrix2f, kMatrix2x2Uniform,
                    math::Matrix2f::Identity());
  TEST_UNIFORM_TYPE("myMat3", "Matrix3x3", math::Matrix3f, kMatrix3x3Uniform,
                    math::Matrix3f::Identity());
  TEST_UNIFORM_TYPE("myMat4", "Matrix4x4", math::Matrix4f, kMatrix4x4Uniform,
                    math::Matrix4f::Identity());

  EXPECT_EQ(0, strcmp("<UNKNOWN>", Uniform::GetValueTypeName(
      base::InvalidEnumValue<Uniform::ValueType>())));
  int bad_type = 128;
  EXPECT_EQ(0, strcmp("<UNKNOWN>", Uniform::GetValueTypeName(
      static_cast<Uniform::ValueType>(bad_type))));

#undef TEST_UNIFORM_TYPE
#undef TEST_VEC_UNIFORM_TYPE
}

TEST(UniformTest, ArrayTypes) {
  std::vector<int> ints;
  ints.push_back(1);
  ints.push_back(2);
  std::vector<float> floats;
  floats.push_back(1.f);
  floats.push_back(2.f);
  std::vector<TexturePtr> textures;
  textures.push_back(TexturePtr());
  textures.push_back(TexturePtr());
  std::vector<CubeMapTexturePtr> cubemaps;
  cubemaps.push_back(CubeMapTexturePtr());
  cubemaps.push_back(CubeMapTexturePtr());
  std::vector<math::Vector2i> vector2is;
  vector2is.push_back(math::Vector2i(1, 2));
  vector2is.push_back(math::Vector2i(3, 4));
  std::vector<math::Vector3i> vector3is;
  vector3is.push_back(math::Vector3i(1, 2, 3));
  vector3is.push_back(math::Vector3i(4, 5, 6));
  std::vector<math::Vector4i> vector4is;
  vector4is.push_back(math::Vector4i(1, 2, 3, 4));
  vector4is.push_back(math::Vector4i(5, 6, 7, 8));
  std::vector<math::Vector2f> vector2fs;
  vector2fs.push_back(math::Vector2f(1.f, 2.f));
  vector2fs.push_back(math::Vector2f(3.f, 4.f));
  std::vector<math::Vector3f> vector3fs;
  vector3fs.push_back(math::Vector3f(1.f, 2.f, 3.f));
  vector3fs.push_back(math::Vector3f(4.f, 5.f, 6.f));
  std::vector<math::Vector4f> vector4fs;
  vector4fs.push_back(math::Vector4f(1.f, 2.f, 3.f, 4.f));
  vector4fs.push_back(math::Vector4f(5.f, 6.f, 7.f, 8.f));
  std::vector<math::Matrix2f> matrix2fs;
  matrix2fs.push_back(math::Matrix2f::Identity());
  matrix2fs.push_back(math::Matrix2f::Identity());
  std::vector<math::Matrix3f> matrix3fs;
  matrix3fs.push_back(math::Matrix3f::Identity());
  matrix3fs.push_back(math::Matrix3f::Identity());
  std::vector<math::Matrix4f> matrix4fs;
  matrix4fs.push_back(math::Matrix4f::Identity());
  matrix4fs.push_back(math::Matrix4f::Identity());

  // Make sure all Uniforms of all types are created properly.
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  EXPECT_TRUE(AddUniform(reg, "myIntArray", kIntUniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myFloatArray", kFloatUniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myTextureArray", kTextureUniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myCubeMapTextureArray",
                         kCubeMapTextureUniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec2fArray", kFloatVector2Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec3fArray", kFloatVector3Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec4fArray", kFloatVector4Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec2iArray", kIntVector2Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec3iArray", kIntVector3Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec4iArray", kIntVector4Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myMat2Array", kMatrix2x2Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myMat3Array", kMatrix3x3Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myMat4Array", kMatrix4x4Uniform, ""));
  Uniform u;

#define TEST_UNIFORM_ARRAY_TYPE(name, type_name, value_type, uniform_type,  \
                                values)                                     \
  u = CreateArrayUniform(reg, name, values);                                \
  EXPECT_EQ(0, strcmp(type_name, Uniform::GetValueTypeName(uniform_type))); \
  EXPECT_EQ(uniform_type, u.GetType());                                     \
  ASSERT_TRUE(base::IsInvalidReference(u.GetValue<value_type>()));          \
  EXPECT_EQ(2U, u.GetCount());                                              \
  ASSERT_TRUE(u.IsArrayOf<value_type>());                                   \
  for (size_t i = 0; i < values.size(); ++i)                                \
  EXPECT_EQ(values[i], u.GetValueAt<value_type>(i))

#define TEST_VEC_ARRAY_UNIFORM_TYPE(name, type_name, value_type, uniform_type, \
                                    values)                                    \
  u = CreateArrayUniform(reg, name, values);                                   \
  EXPECT_EQ(0, strcmp(type_name, Uniform::GetValueTypeName(uniform_type)));    \
  EXPECT_EQ(uniform_type, u.GetType());                                        \
  ASSERT_TRUE(base::IsInvalidReference(u.GetValue<value_type>()));             \
  EXPECT_EQ(2U, u.GetCount());                                                 \
  ASSERT_TRUE(u.IsArrayOf<value_type>());                                      \
  for (size_t i = 0; i < values.size(); ++i)                                   \
  EXPECT_TRUE(                                                                 \
      value_type::AreValuesEqual(values[i], u.GetValueAt<value_type>(i)))

  TEST_UNIFORM_ARRAY_TYPE("myIntArray", "Int", int, kIntUniform, ints);
  TEST_UNIFORM_ARRAY_TYPE("myFloatArray", "Float", float, kFloatUniform,
                          floats);
  TEST_UNIFORM_ARRAY_TYPE("myTextureArray", "Texture", TexturePtr,
                          kTextureUniform, textures);
  TEST_UNIFORM_ARRAY_TYPE("myCubeMapTextureArray", "CubeMapTexture",
                          CubeMapTexturePtr, kCubeMapTextureUniform, cubemaps);
  TEST_VEC_ARRAY_UNIFORM_TYPE("myVec2fArray", "FloatVector2",
                              math::VectorBase2f, kFloatVector2Uniform,
                              vector2fs);
  TEST_VEC_ARRAY_UNIFORM_TYPE("myVec3fArray", "FloatVector3",
                              math::VectorBase3f, kFloatVector3Uniform,
                              vector3fs);
  TEST_VEC_ARRAY_UNIFORM_TYPE("myVec4fArray", "FloatVector4",
                              math::VectorBase4f, kFloatVector4Uniform,
                              vector4fs);
  TEST_VEC_ARRAY_UNIFORM_TYPE("myVec2iArray", "IntVector2", math::VectorBase2i,
                              kIntVector2Uniform, vector2is);
  TEST_VEC_ARRAY_UNIFORM_TYPE("myVec3iArray", "IntVector3", math::VectorBase3i,
                              kIntVector3Uniform, vector3is);
  TEST_VEC_ARRAY_UNIFORM_TYPE("myVec4iArray", "IntVector4", math::VectorBase4i,
                              kIntVector4Uniform, vector4is);
  TEST_UNIFORM_ARRAY_TYPE("myMat2Array", "Matrix2x2", math::Matrix2f,
                          kMatrix2x2Uniform, matrix2fs);
  TEST_UNIFORM_ARRAY_TYPE("myMat3Array", "Matrix3x3", math::Matrix3f,
                          kMatrix3x3Uniform, matrix3fs);
  TEST_UNIFORM_ARRAY_TYPE("myMat4Array", "Matrix4x4", math::Matrix4f,
                          kMatrix4x4Uniform, matrix4fs);

#undef TEST_UNIFORM_ARRAY_TYPE
#undef TEST_VEC_ARRAY_UNIFORM_TYPE
}

TEST(UniformTest, GetMerged) {
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  EXPECT_TRUE(AddUniform(reg, "myFloat", kFloatUniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myFloatArray", kFloatUniform, ""));

  // Test invalid.
  Uniform invalidA, invalidB;
  Uniform valid = reg->Create<Uniform>("myFloat", 1.f);
  Uniform merged;
  bool did_merge = Uniform::GetMerged(valid, invalidA, &merged);
  EXPECT_TRUE(did_merge);
  EXPECT_EQ(valid, merged);
  did_merge = Uniform::GetMerged(invalidA, valid, &merged);
  EXPECT_FALSE(did_merge);
  did_merge = Uniform::GetMerged(invalidA, invalidB, &merged);
  EXPECT_FALSE(did_merge);

  // Single float, no need to merge because final result is just the
  // 'replacement' uniform..
  Uniform myFloatA = reg->Create<Uniform>("myFloat", 1.f);
  Uniform myFloatB = reg->Create<Uniform>("myFloat", 2.f);
  did_merge = Uniform::GetMerged(myFloatA, myFloatA, &merged);
  EXPECT_FALSE(did_merge);
  did_merge = Uniform::GetMerged(myFloatA, myFloatB, &merged);
  EXPECT_FALSE(did_merge);
  did_merge = Uniform::GetMerged(myFloatB, myFloatA, &merged);
  EXPECT_FALSE(did_merge);

  const float float_vals[] = { 1.f, 2.f, 3.f, -1.f, -2.f, -3.f };

  Uniform myFloatArrayA = reg->CreateArrayUniform<float>(
      "myFloatArray[1]", float_vals, 2, base::AllocatorPtr());
  // myFloatArrayA = {xx, 1.f, 2.f }
  EXPECT_EQ(1U, myFloatArrayA.GetArrayIndex());
  EXPECT_EQ(2U, myFloatArrayA.GetCount());
  EXPECT_EQ(1.f, myFloatArrayA.GetValueAt<float>(0));
  EXPECT_EQ(2.f, myFloatArrayA.GetValueAt<float>(1));

  // Different types, should not merge.
  Uniform myInt = reg->Create<Uniform>("myInt", 3);
  did_merge = Uniform::GetMerged(myInt, myFloatA, &merged);
  EXPECT_FALSE(did_merge);
  did_merge = Uniform::GetMerged(myFloatA, myInt, &merged);
  EXPECT_FALSE(did_merge);
  did_merge = Uniform::GetMerged(myFloatArrayA, myInt, &merged);
  EXPECT_FALSE(did_merge);
  did_merge = Uniform::GetMerged(myInt, myFloatArrayA, &merged);
  EXPECT_FALSE(did_merge);

  // Arrays.
  // myFloatArrayB completely replaces myFloatArrayA so no need to merge.
  Uniform myFloatArrayB = reg->CreateArrayUniform<float>(
      "myFloatArray[1]", float_vals, 2, base::AllocatorPtr());
  did_merge = Uniform::GetMerged(myFloatArrayA, myFloatArrayB, &merged);
  EXPECT_FALSE(did_merge);
  // myFloatArrayC completely replaces myFloatArrayA so no need to merge.
  Uniform myFloatArrayC = reg->CreateArrayUniform<float>(
      "myFloatArray", float_vals, 4, base::AllocatorPtr());
  did_merge = Uniform::GetMerged(myFloatArrayA, myFloatArrayC, &merged);
  EXPECT_FALSE(did_merge);
  // Needs merge.
  Uniform myFloatArrayD = reg->CreateArrayUniform<float>(
      "myFloatArray[1]", float_vals + 3, 1, base::AllocatorPtr());
  // myFloatArrayD = {xx, -1.f }
  EXPECT_EQ(1U, myFloatArrayD.GetArrayIndex());
  EXPECT_EQ(1U, myFloatArrayD.GetCount());
  did_merge = Uniform::GetMerged(myFloatArrayA, myFloatArrayD, &merged);
  EXPECT_TRUE(did_merge);
  EXPECT_EQ(2U, merged.GetCount());
  EXPECT_EQ(-1.f, merged.GetValueAt<float>(0));
  Uniform myFloatArrayE = reg->CreateArrayUniform<float>(
      "myFloatArray[1]", float_vals + 2, 3, base::AllocatorPtr());
  // myFloatArrayE = {xx, 3.f, -1.f, -2.f }
  did_merge = Uniform::GetMerged(myFloatArrayA, myFloatArrayE, &merged);
  EXPECT_FALSE(did_merge);
  Uniform myFloatArrayF = reg->CreateArrayUniform<float>(
      "myFloatArray", float_vals, 1, base::AllocatorPtr());
  // myFloatArrayF = { 1.f }
  did_merge = Uniform::GetMerged(myFloatArrayA, myFloatArrayF, &merged);
  EXPECT_TRUE(did_merge);
  EXPECT_EQ(3U, merged.GetCount());
  EXPECT_EQ(1.f, merged.GetValueAt<float>(0));
  EXPECT_EQ(1.f, merged.GetValueAt<float>(1));
  EXPECT_EQ(2.f, merged.GetValueAt<float>(2));
  Uniform myFloatArrayG = reg->CreateArrayUniform<float>(
      "myFloatArray[2]", float_vals, 2, base::AllocatorPtr());
  // myFloatArrayG = {xx, xx, 1.f, 2.f }
  did_merge = Uniform::GetMerged(myFloatArrayA, myFloatArrayG, &merged);
  EXPECT_TRUE(did_merge);
  EXPECT_EQ(3U, merged.GetCount());
  EXPECT_EQ(1.f, merged.GetValueAt<float>(0));
  EXPECT_EQ(1.f, merged.GetValueAt<float>(1));
  EXPECT_EQ(2.f, merged.GetValueAt<float>(2));
  Uniform myFloatArrayH = reg->CreateArrayUniform<float>(
      "myFloatArray[3]", float_vals, 1, base::AllocatorPtr());
  // myFloatArrayH = {xx, xx, xx, 1.f }
  did_merge = Uniform::GetMerged(myFloatArrayA, myFloatArrayH, &merged);
  EXPECT_TRUE(did_merge);
  EXPECT_EQ(3U, merged.GetCount());
  EXPECT_EQ(1.f, merged.GetValueAt<float>(0));
  EXPECT_EQ(2.f, merged.GetValueAt<float>(1));
  EXPECT_EQ(1.f, merged.GetValueAt<float>(2));
  Uniform myFloatArrayI = reg->CreateArrayUniform<float>(
      "myFloatArray[0]", float_vals, 1, base::AllocatorPtr());
  // myFloatArrayI = { 1.f }
  did_merge = Uniform::GetMerged(myFloatArrayA, myFloatArrayI, &merged);
  EXPECT_TRUE(did_merge);
  EXPECT_EQ(3U, merged.GetCount());
  EXPECT_EQ(1.f, merged.GetValueAt<float>(0));
  EXPECT_EQ(1.f, merged.GetValueAt<float>(1));
  EXPECT_EQ(2.f, merged.GetValueAt<float>(2));
}

TEST(UniformTest, MergeValuesFrom) {
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  EXPECT_TRUE(AddUniform(reg, "myInt", kIntUniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myFloat", kFloatUniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myFloat2", kFloatUniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec3f", kFloatVector3Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myFloatArray", kFloatUniform, ""));
  // Single float.
  Uniform a = reg->Create<Uniform>("myFloat", 1.f);
  const uint64 astamp = a.GetStamp();

  Uniform b = reg->Create<Uniform>("myFloat", 2.f);
  EXPECT_EQ(astamp + 1U, b.GetStamp());
  // Different names does nothing.
  b.MergeValuesFrom(reg->Create<Uniform>("myFloat2", 1.f));
  EXPECT_EQ(2.f, b.GetValue<float>());
  EXPECT_EQ(1.f, a.GetValue<float>());
  b.MergeValuesFrom(reg->Create<Uniform>("myInt", 1));
  EXPECT_EQ(2.f, b.GetValue<float>());
  EXPECT_EQ(1.f, a.GetValue<float>());
  b.MergeValuesFrom(b);
  EXPECT_EQ(2.f, b.GetValue<float>());
  EXPECT_EQ(1.f, a.GetValue<float>());
  // This should work, b just copies a including a's stamp.
  b.MergeValuesFrom(a);
  EXPECT_EQ(1.f, b.GetValue<float>());
  EXPECT_EQ(1.f, a.GetValue<float>());
  EXPECT_EQ(a.GetStamp(), b.GetStamp());

  // Single vector.
  a = reg->Create<Uniform>("myVec3f", math::Vector3f(1.f, 2.f, 3.f));
  b = reg->Create<Uniform>("myVec3f", math::Vector3f(3.f, 2.f, 1.f));
  b.MergeValuesFrom(a);
  EXPECT_TRUE(math::Vector3f::AreValuesEqual(math::Vector3f(1.f, 2.f, 3.f),
                                             b.GetValue<math::VectorBase3f>()));
  EXPECT_TRUE(math::Vector3f::AreValuesEqual(math::Vector3f(1.f, 2.f, 3.f),
                                             a.GetValue<math::VectorBase3f>()));

  // Array of floats.
  std::vector<float> floats;
  floats.push_back(1.f);
  floats.push_back(2.f);
  a = CreateArrayUniform(reg, "myFloatArray", floats);
  b = reg->Create<Uniform>("myFloatArray", 3.f);
  // Bad type and name does nothing.
  b.MergeValuesFrom(reg->Create<Uniform>("myFloat2", 1.f));
  EXPECT_EQ(0U, b.GetCount());
  EXPECT_EQ(3.f, b.GetValue<float>());
  // This will merge a into b, creating an array of length 2.
  b.MergeValuesFrom(a);
  EXPECT_EQ(2U, b.GetCount());
  EXPECT_EQ(1.f, b.GetValueAt<float>(0U));
  EXPECT_EQ(2.f, b.GetValueAt<float>(1U));

  // This will make an array of length 3 with the first 2 values from a and the
  // last from b.
  a = CreateArrayUniform(reg, "myFloatArray[0]", floats);
  b = CreateArrayUniform(reg, "myFloatArray[1]", floats);
  b.MergeValuesFrom(a);
  EXPECT_EQ(3U, b.GetCount());
  EXPECT_EQ(1.f, b.GetValueAt<float>(0U));
  EXPECT_EQ(2.f, b.GetValueAt<float>(1U));
  EXPECT_EQ(2.f, b.GetValueAt<float>(2U));
  // This will update the stamp.
  uint64 bstamp = b.GetStamp();
  EXPECT_TRUE(b.SetValueAt<float>(1U, 3.f));
  EXPECT_EQ(3.f, b.GetValueAt<float>(1U));
  EXPECT_EQ(bstamp + 1U, b.GetStamp());
  // The should not update the stamp since the type is invalid.
  EXPECT_FALSE(b.SetValueAt<int>(1U, 2));
  EXPECT_EQ(3.f, b.GetValueAt<float>(1U));
  EXPECT_EQ(bstamp + 1U, b.GetStamp());

  // This will make an array of length 3 with the first value from a and the
  // last from b.
  a = reg->Create<Uniform>("myFloatArray[0]", 1.1f);
  b = reg->Create<Uniform>("myFloatArray[2]", 2.2f);
  b.MergeValuesFrom(a);
  EXPECT_EQ(3U, b.GetCount());
  EXPECT_EQ(0U, b.GetArrayIndex());
  EXPECT_EQ(1.1f, b.GetValueAt<float>(0U));
  EXPECT_EQ(0.f, b.GetValueAt<float>(1U));
  EXPECT_EQ(2.2f, b.GetValueAt<float>(2U));

  // This will make an array of length 2 with the first value from a and the
  // last from b, starting from index 1.
  a = reg->Create<Uniform>("myFloatArray[1]", 1.1f);
  b = reg->Create<Uniform>("myFloatArray[2]", 2.2f);
  b.MergeValuesFrom(a);
  EXPECT_EQ(2U, b.GetCount());
  EXPECT_EQ(1U, b.GetArrayIndex());
  EXPECT_EQ(1.1f, b.GetValueAt<float>(0U));
  EXPECT_EQ(2.2f, b.GetValueAt<float>(1U));

  // This will make an array of length 4 with the first element from a and the
  // last 3 from b.
  floats.push_back(3.f);
  a = reg->Create<Uniform>("myFloatArray", 5.f);
  b = CreateArrayUniform(reg, "myFloatArray[1]", floats);
  b.MergeValuesFrom(a);
  EXPECT_EQ(4U, b.GetCount());
  EXPECT_EQ(5.f, b.GetValueAt<float>(0U));
  EXPECT_EQ(1.f, b.GetValueAt<float>(1U));
  EXPECT_EQ(2.f, b.GetValueAt<float>(2U));
  EXPECT_EQ(3.f, b.GetValueAt<float>(3U));

  // This will make an array of length 5 with the first element from a, an unset
  // second element, and the last 3 from b.
  a = reg->Create<Uniform>("myFloatArray", 5.f);
  b = CreateArrayUniform(reg, "myFloatArray[2]", floats);
  EXPECT_EQ(0U, a.GetArrayIndex());
  EXPECT_EQ(2U, b.GetArrayIndex());
  b.MergeValuesFrom(a);
  EXPECT_EQ(5U, b.GetCount());
  EXPECT_EQ(5.f, b.GetValueAt<float>(0U));
  EXPECT_EQ(0.f, b.GetValueAt<float>(1U));
  EXPECT_EQ(1.f, b.GetValueAt<float>(2U));
  EXPECT_EQ(2.f, b.GetValueAt<float>(3U));
  EXPECT_EQ(3.f, b.GetValueAt<float>(4U));

  // This will make an array of length 6, three from each uniform.
  a = CreateArrayUniform(reg, "myFloatArray[0]", floats);
  b = CreateArrayUniform(reg, "myFloatArray[3]", floats);
  EXPECT_EQ(0U, a.GetArrayIndex());
  EXPECT_EQ(3U, b.GetArrayIndex());
  b.MergeValuesFrom(a);
  EXPECT_EQ(6U, b.GetCount());
  EXPECT_EQ(1.f, b.GetValueAt<float>(0U));
  EXPECT_EQ(2.f, b.GetValueAt<float>(1U));
  EXPECT_EQ(3.f, b.GetValueAt<float>(2U));
  EXPECT_EQ(1.f, b.GetValueAt<float>(3U));
  EXPECT_EQ(2.f, b.GetValueAt<float>(4U));
  EXPECT_EQ(3.f, b.GetValueAt<float>(5U));
  EXPECT_EQ(0U, b.GetArrayIndex());

  // This will make an array of length 6, three from each uniform. The initial
  // index of the uniform will be 1.
  a = CreateArrayUniform(reg, "myFloatArray[1]", floats);
  b = CreateArrayUniform(reg, "myFloatArray[4]", floats);
  EXPECT_EQ(1U, a.GetArrayIndex());
  EXPECT_EQ(4U, b.GetArrayIndex());
  bstamp = b.GetStamp();
  b.MergeValuesFrom(a);
  EXPECT_EQ(6U, b.GetCount());
  EXPECT_EQ(1.f, b.GetValueAt<float>(0U));
  EXPECT_EQ(2.f, b.GetValueAt<float>(1U));
  EXPECT_EQ(3.f, b.GetValueAt<float>(2U));
  EXPECT_EQ(1.f, b.GetValueAt<float>(3U));
  EXPECT_EQ(2.f, b.GetValueAt<float>(4U));
  EXPECT_EQ(3.f, b.GetValueAt<float>(5U));
  EXPECT_EQ(1U, b.GetArrayIndex());
  // +1 for temporary merge uniform, +6 for each array value.
  EXPECT_EQ(bstamp + 7U, b.GetStamp());

  // Test all array types for coverage.
  std::vector<int> ints;
  ints.push_back(1);
  ints.push_back(2);
  floats.pop_back();
  std::vector<uint32> uints;
  uints.push_back(1U);
  uints.push_back(2U);
  std::vector<TexturePtr> textures;
  textures.push_back(TexturePtr());
  textures.push_back(TexturePtr());
  std::vector<CubeMapTexturePtr> cubemaps;
  cubemaps.push_back(CubeMapTexturePtr());
  cubemaps.push_back(CubeMapTexturePtr());
  std::vector<math::Vector2i> vector2is;
  vector2is.push_back(math::Vector2i(1, 2));
  vector2is.push_back(math::Vector2i(3, 4));
  std::vector<math::Vector3i> vector3is;
  vector3is.push_back(math::Vector3i(1, 2, 3));
  vector3is.push_back(math::Vector3i(4, 5, 6));
  std::vector<math::Vector4i> vector4is;
  vector4is.push_back(math::Vector4i(1, 2, 3, 4));
  vector4is.push_back(math::Vector4i(5, 6, 7, 8));
  std::vector<math::Vector2ui> vector2uis;
  vector2uis.push_back(math::Vector2ui(1U, 2U));
  vector2uis.push_back(math::Vector2ui(3U, 4U));
  std::vector<math::Vector3ui> vector3uis;
  vector3uis.push_back(math::Vector3ui(1U, 2U, 3U));
  vector3uis.push_back(math::Vector3ui(4U, 5U, 6U));
  std::vector<math::Vector4ui> vector4uis;
  vector4uis.push_back(math::Vector4ui(1U, 2U, 3U, 4U));
  vector4uis.push_back(math::Vector4ui(5U, 6U, 7U, 8U));
  std::vector<math::Vector2f> vector2fs;
  vector2fs.push_back(math::Vector2f(1.f, 2.f));
  vector2fs.push_back(math::Vector2f(3.f, 4.f));
  std::vector<math::Vector3f> vector3fs;
  vector3fs.push_back(math::Vector3f(1.f, 2.f, 3.f));
  vector3fs.push_back(math::Vector3f(4.f, 5.f, 6.f));
  std::vector<math::Vector4f> vector4fs;
  vector4fs.push_back(math::Vector4f(1.f, 2.f, 3.f, 4.f));
  vector4fs.push_back(math::Vector4f(5.f, 6.f, 7.f, 8.f));
  std::vector<math::Matrix2f> matrix2fs;
  matrix2fs.push_back(math::Matrix2f::Identity());
  matrix2fs.push_back(math::Matrix2f::Identity() * 2.f);
  std::vector<math::Matrix3f> matrix3fs;
  matrix3fs.push_back(math::Matrix3f::Identity());
  matrix3fs.push_back(math::Matrix3f::Identity() * 2.f);
  std::vector<math::Matrix4f> matrix4fs;
  matrix4fs.push_back(math::Matrix4f::Identity());
  matrix4fs.push_back(math::Matrix4f::Identity() * 2.f);

  // CReate the remaining types.
  EXPECT_TRUE(AddUniform(reg, "myIntArray", kIntUniform, ""));
  EXPECT_TRUE(AddUniform(reg, "my.UintArray", kUnsignedIntUniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myTextureArray", kTextureUniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myCubeMapTextureArray",
                         kCubeMapTextureUniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec2fArray", kFloatVector2Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec3fArray", kFloatVector3Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec4fArray", kFloatVector4Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec2iArray", kIntVector2Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec3iArray", kIntVector3Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec4iArray", kIntVector4Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec2uiArray", kUnsignedIntVector2Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec3uiArray", kUnsignedIntVector3Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myVec4uiArray", kUnsignedIntVector4Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myMat2Array", kMatrix2x2Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myMat3Array", kMatrix3x3Uniform, ""));
  EXPECT_TRUE(AddUniform(reg, "myMat4Array", kMatrix4x4Uniform, ""));
  Uniform u1, u2;

#define TEST_UNIFORM_ARRAY_TYPE(name, type_name, value_type, uniform_type, \
                                values)                                    \
  u1 = CreateArrayUniform(reg, name "[2]", values);                        \
  u2 = CreateArrayUniform(reg, name, values);                              \
  EXPECT_EQ(2U, u1.GetCount());                                            \
  EXPECT_EQ(2U, u2.GetCount());                                            \
  for (size_t i = 0; i < values.size(); ++i)                               \
    EXPECT_EQ(values[i], u1.GetValueAt<value_type>(i));                    \
  for (size_t i = 0; i < values.size(); ++i)                               \
    EXPECT_EQ(values[i], u2.GetValueAt<value_type>(i));                    \
  u2.MergeValuesFrom(u1);                                                  \
  EXPECT_EQ(4U, u2.GetCount());                                            \
  EXPECT_EQ(values[0], u2.GetValueAt<value_type>(0));                      \
  EXPECT_EQ(values[1], u2.GetValueAt<value_type>(1));                      \
  EXPECT_EQ(values[0], u2.GetValueAt<value_type>(2));                      \
  EXPECT_EQ(values[1], u2.GetValueAt<value_type>(3));

#define TEST_VEC_ARRAY_UNIFORM_TYPE(name, type_name, value_type, uniform_type, \
                                    values)                                    \
  u1 = CreateArrayUniform(reg, name "[2]", values);                            \
  u2 = CreateArrayUniform(reg, name, values);                                  \
  EXPECT_EQ(2U, u1.GetCount());                                                \
  EXPECT_EQ(2U, u2.GetCount());                                                \
  u2.MergeValuesFrom(u1);                                                      \
  EXPECT_EQ(4U, u2.GetCount());                                                \
  EXPECT_TRUE(                                                                 \
      value_type::AreValuesEqual(values[0], u2.GetValueAt<value_type>(0)));    \
  EXPECT_TRUE(                                                                 \
      value_type::AreValuesEqual(values[1], u2.GetValueAt<value_type>(1)));    \
  EXPECT_TRUE(                                                                 \
      value_type::AreValuesEqual(values[0], u2.GetValueAt<value_type>(2)));    \
  EXPECT_TRUE(                                                                 \
      value_type::AreValuesEqual(values[1], u2.GetValueAt<value_type>(3)));

  TEST_UNIFORM_ARRAY_TYPE("myIntArray", "Int", int, kIntUniform, ints);
  TEST_UNIFORM_ARRAY_TYPE("myUintArray", "UnsignedInt", uint32,
                          kUnsignedIntUniform, uints);
  TEST_UNIFORM_ARRAY_TYPE("myFloatArray", "Float", float, kFloatUniform,
                          floats);
  TEST_UNIFORM_ARRAY_TYPE("myTextureArray", "Texture", TexturePtr,
                          kTextureUniform, textures);
  TEST_UNIFORM_ARRAY_TYPE("myCubeMapTextureArray", "CubeMapTexture",
                          CubeMapTexturePtr, kCubeMapTextureUniform, cubemaps);
  TEST_VEC_ARRAY_UNIFORM_TYPE("myVec2fArray", "FloatVector2",
                              math::VectorBase2f, kFloatVector2Uniform,
                              vector2fs);
  TEST_VEC_ARRAY_UNIFORM_TYPE("myVec3fArray", "FloatVector3",
                              math::VectorBase3f, kFloatVector3Uniform,
                              vector3fs);
  TEST_VEC_ARRAY_UNIFORM_TYPE("myVec4fArray", "FloatVector4",
                              math::VectorBase4f, kFloatVector4Uniform,
                              vector4fs);
  TEST_VEC_ARRAY_UNIFORM_TYPE("myVec2iArray", "IntVector2", math::VectorBase2i,
                              kIntVector2Uniform, vector2is);
  TEST_VEC_ARRAY_UNIFORM_TYPE("myVec3iArray", "IntVector3", math::VectorBase3i,
                              kIntVector3Uniform, vector3is);
  TEST_VEC_ARRAY_UNIFORM_TYPE("myVec4iArray", "IntVector4", math::VectorBase4i,
                              kIntVector4Uniform, vector4is);
  TEST_VEC_ARRAY_UNIFORM_TYPE("myVec2uiArray", "UnsignedIntVector2",
                              math::VectorBase2ui, kUnsignedIntVector2Uniform,
                              vector2uis);
  TEST_VEC_ARRAY_UNIFORM_TYPE("myVec3uiArray", "UnsignedIntVector3",
                              math::VectorBase3ui, kUnsignedIntVector3Uniform,
                              vector3uis);
  TEST_VEC_ARRAY_UNIFORM_TYPE("myVec4uiArray", "UnsignedIntVector4",
                              math::VectorBase4ui, kUnsignedIntVector4Uniform,
                              vector4uis);
  TEST_UNIFORM_ARRAY_TYPE("myMat2Array", "Matrix2x2", math::Matrix2f,
                          kMatrix2x2Uniform, matrix2fs);
  TEST_UNIFORM_ARRAY_TYPE("myMat3Array", "Matrix3x3", math::Matrix3f,
                          kMatrix3x3Uniform, matrix3fs);
  TEST_UNIFORM_ARRAY_TYPE("myMat4Array", "Matrix4x4", math::Matrix4f,
                          kMatrix4x4Uniform, matrix4fs);
#undef TEST_UNIFORM_ARRAY_TYPE
#undef TEST_VEC_ARRAY_UNIFORM_TYPE
}

}  // namespace gfx
}  // namespace ion
