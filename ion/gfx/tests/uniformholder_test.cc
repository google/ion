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

#include "ion/gfx/uniformholder.h"

#include "ion/base/invalid.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/math/vector.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfx {

namespace {

template<typename T>
static bool Equal(const Uniform& a, const Uniform& b) {
  return &a.GetRegistry() == &b.GetRegistry() &&
      a.GetIndexInRegistry() == b.GetIndexInRegistry() &&
      a.GetType() == b.GetType() &&
      a.GetValue<T>() == b.GetValue<T>();
}

template<typename T>
static bool VectorEqual(const Uniform& a, const Uniform& b) {
  return &a.GetRegistry() == &b.GetRegistry() &&
      a.GetIndexInRegistry() == b.GetIndexInRegistry() &&
      a.GetType() == b.GetType() &&
      T::AreValuesEqual(a.GetValue<T>(), b.GetValue<T>());
}

template<typename T>
static bool ArrayElementEqual(const Uniform& a,
                              const Uniform& b,
                              size_t index) {
  return &a.GetRegistry() == &b.GetRegistry() &&
      a.GetIndexInRegistry() == b.GetIndexInRegistry() &&
      a.GetType() == b.GetType() &&
      T::AreValuesEqual(a.GetValueAt<T>(index), b.GetValueAt<T>(index));
}

// Accessible class derived from UniformHolder.
class MyUniformHolder : public base::Allocatable, public UniformHolder {
 public:
  MyUniformHolder() : UniformHolder(GetAllocator()) {}
  ~MyUniformHolder() override {}
};

}  // anonymous namespace

TEST(UniformHolderTest, EnableDisable) {
  MyUniformHolder holder;

  // Check that the holder is enabled by default.
  EXPECT_TRUE(holder.IsEnabled());
  holder.Enable(true);
  EXPECT_TRUE(holder.IsEnabled());
  holder.Enable(false);
  EXPECT_FALSE(holder.IsEnabled());
  holder.Enable(true);
  EXPECT_TRUE(holder.IsEnabled());
}

TEST(UniformHolderTest, AddReplaceSetClearUniforms) {
  MyUniformHolder holder;
  ShaderInputRegistryPtr reg(new ShaderInputRegistry());
  reg->Add(ShaderInputRegistry::UniformSpec("myFloat", kFloatUniform, ""));
  reg->Add(ShaderInputRegistry::UniformSpec("myVec2f",
                                            kFloatVector2Uniform,
                                            ""));
  Uniform a1 = reg->Create<Uniform>("myFloat", 17.2f);
  Uniform a2 = reg->Create<Uniform>("myVec2f", math::Vector2f(0.f, 1.f));
  std::vector<math::Vector3f> vec3fs;
  vec3fs.push_back(math::Vector3f(1, 0, 0));
  vec3fs.push_back(math::Vector3f(1, 1, 0));
  Uniform a3 = reg->CreateArrayUniform("myVec3fs",
                                       &vec3fs[0],
                                       vec3fs.size(),
                                       base::AllocatorPtr());

  // Check that there are no uniforms added.
  EXPECT_EQ(0U, holder.GetUniforms().size());

  // Check that it is possible to add Uniforms.
  EXPECT_EQ(0U, holder.AddUniform(a1));
  EXPECT_EQ(1U, holder.GetUniforms().size());
  EXPECT_EQ(1U, holder.AddUniform(a2));
  EXPECT_EQ(2U, holder.GetUniforms().size());
  EXPECT_EQ(2U, holder.AddUniform(a3));
  EXPECT_EQ(3U, holder.GetUniforms().size());
  EXPECT_TRUE(Equal<float>(a1, holder.GetUniforms()[0]));
  EXPECT_TRUE(VectorEqual<math::VectorBase2f>(a2, holder.GetUniforms()[1]));
  EXPECT_TRUE(
      ArrayElementEqual<math::VectorBase3f>(a3, holder.GetUniforms()[2], 0));
  EXPECT_TRUE(
      ArrayElementEqual<math::VectorBase3f>(a3, holder.GetUniforms()[2], 1));

  // Check that we can recover the uniform indices.
  EXPECT_EQ(0U, holder.GetUniformIndex("myFloat"));
  EXPECT_EQ(1U, holder.GetUniformIndex("myVec2f"));
  EXPECT_EQ(2U, holder.GetUniformIndex("myVec3fs"));

  // Check that an invalid index is returned.
  EXPECT_EQ(base::kInvalidIndex, holder.GetUniformIndex("does not exist"));

  // Check that we can change the value of a uniform.
  math::Vector2f vec(1.1f, 2.2f);
  EXPECT_TRUE(holder.SetUniformValue(0U, 12.5f));

  // The local variable version has not changed.
  EXPECT_FALSE(Equal<float>(a1, holder.GetUniforms()[0]));
  EXPECT_EQ(12.5f, holder.GetUniforms()[0].GetValue<float>());
  EXPECT_TRUE(VectorEqual<math::VectorBase2f>(a2, holder.GetUniforms()[1]));
  EXPECT_TRUE(
      ArrayElementEqual<math::VectorBase3f>(a3, holder.GetUniforms()[2], 0));
  EXPECT_TRUE(
      ArrayElementEqual<math::VectorBase3f>(a3, holder.GetUniforms()[2], 1));

  // Check that we can set the second uniform.
  EXPECT_TRUE(holder.SetUniformValue(1U, vec));
  EXPECT_EQ(12.5f, holder.GetUniforms()[0].GetValue<float>());
  EXPECT_TRUE(math::VectorBase2f::AreValuesEqual(
      vec, holder.GetUniforms()[1].GetValue<math::VectorBase2f>()));

  // Check that we can set the (last) array uniform.
  math::Vector3f newVec3f(0.25f, 0.5f, 0.75f);
  EXPECT_FALSE(holder.SetUniformValueAt(3U, 1, newVec3f));
  EXPECT_TRUE(holder.SetUniformValueAt(2U, 1, newVec3f));
  EXPECT_EQ(12.5f, holder.GetUniforms()[0].GetValue<float>());
  EXPECT_TRUE(math::VectorBase3f::AreValuesEqual(
      vec3fs[0], holder.GetUniforms()[2].GetValueAt<math::VectorBase3f>(0)));
  EXPECT_TRUE(math::VectorBase3f::AreValuesEqual(
      newVec3f, holder.GetUniforms()[2].GetValueAt<math::VectorBase3f>(1)));

  // Check that nothing happens if we try to change the value at an index that
  // does not exist.
  EXPECT_FALSE(holder.SetUniformValue(3U, 3.14f));
  EXPECT_EQ(12.5f, holder.GetUniforms()[0].GetValue<float>());
  EXPECT_TRUE(math::VectorBase2f::AreValuesEqual(
      vec, holder.GetUniforms()[1].GetValue<math::VectorBase2f>()));
  EXPECT_TRUE(math::VectorBase3f::AreValuesEqual(
      vec3fs[0], holder.GetUniforms()[2].GetValueAt<math::VectorBase3f>(0)));
  EXPECT_TRUE(math::VectorBase3f::AreValuesEqual(
      newVec3f, holder.GetUniforms()[2].GetValueAt<math::VectorBase3f>(1)));

  // Check that setting an invalid value type also fails.
  EXPECT_FALSE(holder.SetUniformValue(0U, vec));
  EXPECT_EQ(12.5f, holder.GetUniforms()[0].GetValue<float>());
  EXPECT_TRUE(math::VectorBase2f::AreValuesEqual(
      vec, holder.GetUniforms()[1].GetValue<math::VectorBase2f>()));
  EXPECT_TRUE(math::VectorBase3f::AreValuesEqual(
      vec3fs[0], holder.GetUniforms()[2].GetValueAt<math::VectorBase3f>(0)));
  EXPECT_TRUE(math::VectorBase3f::AreValuesEqual(
      newVec3f, holder.GetUniforms()[2].GetValueAt<math::VectorBase3f>(1)));

  // Test SetUniformByName() convenience function.
  EXPECT_FALSE(holder.SetUniformByName("no_such_name", 1.5f));
  EXPECT_FALSE(holder.SetUniformByName("myFloat", vec));  // Wrong type.
  EXPECT_TRUE(holder.SetUniformByName<float>("myFloat", 6.0));

  // Test SetUniformByNameAt() convenience function.
  math::Vector3f vec3f(0.1f, 0.2f, 0.4f);
  EXPECT_FALSE(holder.SetUniformByNameAt("no_such_name", 0, vec3f));
  EXPECT_FALSE(holder.SetUniformByNameAt("myVec3fs", 0, vec));  // Wrong type.
  const math::Vector3f vec3fs0 = vec3fs[0];
  EXPECT_TRUE(holder.SetUniformByNameAt("myVec3fs", 1, vec3f));
  EXPECT_TRUE(math::VectorBase3f::AreValuesEqual(
      vec3f, holder.GetUniforms()[2].GetValueAt<math::VectorBase3f>(1)));
  EXPECT_TRUE(math::VectorBase3f::AreValuesEqual(
      vec3fs0, holder.GetUniforms()[2].GetValueAt<math::VectorBase3f>(0)));

  // Check that we can remove individual uniforms by name.
  EXPECT_EQ(3U, holder.GetUniforms().size());
  EXPECT_TRUE(holder.RemoveUniformByName("myFloat"));
  EXPECT_EQ(2U, holder.GetUniforms().size());
  EXPECT_FALSE(holder.RemoveUniformByName("myFloat"));
  EXPECT_EQ(0U, holder.GetUniformIndex("myVec2f"));
  EXPECT_EQ(1U, holder.GetUniformIndex("myVec3fs"));

  // Check that we can clear the list.
  holder.ClearUniforms();
  EXPECT_EQ(0U, holder.GetUniforms().size());

  // Check that we can replace an Uniform.
  EXPECT_EQ(0U, holder.AddUniform(a1));
  EXPECT_EQ(1U, holder.GetUniforms().size());
  EXPECT_TRUE(Equal<float>(a1, holder.GetUniforms()[0]));
  EXPECT_TRUE(holder.ReplaceUniform(0U, a2));
  EXPECT_EQ(1U, holder.GetUniforms().size());
  EXPECT_FALSE(Equal<float>(a1, holder.GetUniforms()[0]));
  EXPECT_TRUE(VectorEqual<math::VectorBase2f>(a2, holder.GetUniforms()[0]));

  // Check that trying to replace a Uniform with an invalid index fails.
  EXPECT_FALSE(holder.ReplaceUniform(3U, a2));
  EXPECT_FALSE(holder.ReplaceUniform(10U, a2));

  // Check that trying to add or replace Uniforms with invalid Uniforms fails.
  Uniform invalid;
  EXPECT_EQ(base::kInvalidIndex, holder.AddUniform(invalid));
  EXPECT_FALSE(holder.ReplaceUniform(0, invalid));
  EXPECT_EQ(1U, holder.GetUniforms().size());
}

}  // namespace gfx
}  // namespace ion
