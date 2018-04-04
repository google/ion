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

#include <cstring>  // For strcmp().

#include "ion/base/invalid.h"
#include "ion/base/logchecker.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/math/vector.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfx {

// Helper function to add an attribute to a registry.
static bool AddAttribute(const ShaderInputRegistryPtr& reg,
                         const std::string& name,
                         const Attribute::ValueType type,
                         const std::string& doc) {
  return reg->Add(ShaderInputRegistry::AttributeSpec(name, type, doc));
}

TEST(AttributeTest, CreateAttribute) {
  base::LogChecker log_checker;

  // Prebuilt math::Vector2f for convenience.
  static const math::Vector2f kVec2(1.0f, 2.0f);

  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  EXPECT_TRUE(AddAttribute(
      reg, "myBuffer", kBufferObjectElementAttribute, ""));
  EXPECT_TRUE(AddAttribute(reg, "myFloat", kFloatAttribute, ""));
  EXPECT_TRUE(AddAttribute(reg, "myVec2f", kFloatVector2Attribute, ""));
  EXPECT_TRUE(AddAttribute(reg, "myVec3f", kFloatVector3Attribute, ""));
  EXPECT_TRUE(AddAttribute(reg, "myVec4f", kFloatVector4Attribute, ""));
  EXPECT_TRUE(AddAttribute(reg, "myMat2f", kFloatMatrix2x2Attribute, ""));
  EXPECT_TRUE(AddAttribute(reg, "myMat3f", kFloatMatrix3x3Attribute, ""));
  EXPECT_TRUE(AddAttribute(reg, "myMat4f", kFloatMatrix4x4Attribute, ""));

  Attribute a;
  EXPECT_FALSE(a.IsValid());
  EXPECT_TRUE(ShaderInputRegistry::GetSpec(a) == nullptr);

  // Create.
  a = reg->Create<Attribute>("myFloat", 17.2f);
  EXPECT_TRUE(a.IsValid());
  EXPECT_EQ(reg.Get(), &a.GetRegistry());
  EXPECT_EQ(1U, a.GetIndexInRegistry());
  EXPECT_EQ(kFloatAttribute, a.GetType());
  ASSERT_FALSE(base::IsInvalidReference(a.GetValue<float>()));
  EXPECT_EQ(17.2f, a.GetValue<float>());
  EXPECT_TRUE(base::IsInvalidReference(a.GetValue<math::VectorBase4f>()));
  EXPECT_TRUE(ShaderInputRegistry::GetSpec(a) != nullptr);
  EXPECT_FALSE(a.IsFixedPointNormalized());
  EXPECT_EQ(0U, a.GetDivisor());

  // Copy should be fine.
  Attribute a2 = a;
  EXPECT_TRUE(a2.IsValid());
  EXPECT_EQ(reg.Get(), &a2.GetRegistry());
  EXPECT_EQ(1U, a2.GetIndexInRegistry());
  EXPECT_EQ(kFloatAttribute, a2.GetType());
  ASSERT_FALSE(base::IsInvalidReference(a2.GetValue<float>()));
  EXPECT_EQ(17.2f, a2.GetValue<float>());
  EXPECT_TRUE(base::IsInvalidReference(a2.GetValue<math::VectorBase4f>()));
  EXPECT_TRUE(ShaderInputRegistry::GetSpec(a2) != nullptr);
  EXPECT_FALSE(a2.IsFixedPointNormalized());
  EXPECT_EQ(0U, a.GetDivisor());

  // Test == and !=.
  EXPECT_TRUE(a == a2);
  EXPECT_TRUE(a2 == a);
  EXPECT_TRUE(a == a);
  EXPECT_TRUE(a2 == a2);
  EXPECT_FALSE(a != a2);
  EXPECT_FALSE(a2 != a);
  EXPECT_FALSE(a != a);
  EXPECT_FALSE(a2 != a2);

  Attribute a2f = reg->Create<Attribute>("myVec2f", math::Vector2f(1.f, 2.f));
  Attribute a3f = reg->Create<Attribute>("myVec3f",
                                         math::Vector3f(1.f, 2.f, 3.f));
  Attribute a4f = reg->Create<Attribute>("myVec4f",
                                         math::Vector4f(1.f, 2.f, 3.f, 4.f));
  Attribute am2f = reg->Create<Attribute>("myMat2f", math::Matrix2f(1.f, 2.f,
                                                                    3.f, 4.f));
  Attribute am3f = reg->Create<Attribute>("myMat3f",
                                          math::Matrix3f(1.f, 2.f, 3.f,
                                                         4.f, 5.f, 6.f,
                                                         7.f, 8.f, 9.f));
  Attribute am4f =
      reg->Create<Attribute>("myMat4f", math::Matrix4f(1.f, 2.f, 3.f, 4.f,
                                                       5.f, 6.f, 7.f, 8.f,
                                                       9.f, 10.f, 11.f, 12.f,
                                                       13.f, 14.f, 15.f, 16.f));
  EXPECT_TRUE(a2f == a2f);
  EXPECT_TRUE(a3f == a3f);
  EXPECT_TRUE(a4f == a4f);
  EXPECT_FALSE(a2f == a3f);
  EXPECT_FALSE(a3f == a2f);
  EXPECT_FALSE(a2f == a4f);
  EXPECT_FALSE(a4f == a2f);
  EXPECT_FALSE(a3f == a4f);
  EXPECT_FALSE(a4f == a3f);
  EXPECT_FALSE(a2f != a2f);
  EXPECT_FALSE(a3f != a3f);
  EXPECT_FALSE(a4f != a4f);
  EXPECT_TRUE(a2f != a3f);
  EXPECT_TRUE(a3f != a2f);
  EXPECT_TRUE(a2f != a4f);
  EXPECT_TRUE(a4f != a2f);
  EXPECT_TRUE(a3f != a4f);
  EXPECT_TRUE(a4f != a3f);

  EXPECT_TRUE(am2f == am2f);
  EXPECT_TRUE(am3f == am3f);
  EXPECT_TRUE(am4f == am4f);
  EXPECT_FALSE(am2f == am3f);
  EXPECT_FALSE(am3f == am2f);
  EXPECT_FALSE(am2f == am4f);
  EXPECT_FALSE(am4f == am2f);
  EXPECT_FALSE(am3f == am4f);
  EXPECT_FALSE(am4f == am3f);
  EXPECT_FALSE(am2f != am2f);
  EXPECT_FALSE(am3f != am3f);
  EXPECT_FALSE(am4f != am4f);
  EXPECT_TRUE(am2f != am3f);
  EXPECT_TRUE(am3f != am2f);
  EXPECT_TRUE(am2f != am4f);
  EXPECT_TRUE(am4f != am2f);
  EXPECT_TRUE(am3f != am4f);
  EXPECT_TRUE(am4f != am3f);

  // Change to correct value type.
  EXPECT_TRUE(a.SetValue(48.1f));
  EXPECT_TRUE(a.IsValid());
  EXPECT_EQ(reg.Get(), &a.GetRegistry());
  EXPECT_EQ(1U, a.GetIndexInRegistry());
  EXPECT_EQ(kFloatAttribute, a.GetType());
  ASSERT_FALSE(base::IsInvalidReference(a.GetValue<float>()));
  EXPECT_EQ(48.1f, a.GetValue<float>());
  EXPECT_TRUE(base::IsInvalidReference(a.GetValue<math::VectorBase4f>()));
  EXPECT_NE(nullptr, ShaderInputRegistry::GetSpec(a));
  EXPECT_FALSE(a.IsFixedPointNormalized());

  // Test == and != again.
  EXPECT_FALSE(a == a2);
  EXPECT_FALSE(a2 == a);
  EXPECT_TRUE(a == a);
  EXPECT_TRUE(a2 == a2);
  EXPECT_TRUE(a != a2);
  EXPECT_TRUE(a2 != a);
  EXPECT_FALSE(a != a);
  EXPECT_FALSE(a2 != a2);

  // Change to bad type; leaves Attribute untouched.
  EXPECT_FALSE(a.SetValue(kVec2));
  EXPECT_TRUE(a.IsValid());
  EXPECT_EQ(reg.Get(), &a.GetRegistry());
  EXPECT_EQ(1U, a.GetIndexInRegistry());
  EXPECT_EQ(kFloatAttribute, a.GetType());
  ASSERT_FALSE(base::IsInvalidReference(a.GetValue<float>()));
  EXPECT_EQ(48.1f, a.GetValue<float>());
  EXPECT_TRUE(base::IsInvalidReference(a.GetValue<math::VectorBase4f>()));
  EXPECT_NE(nullptr, ShaderInputRegistry::GetSpec(a));
  EXPECT_FALSE(a.IsFixedPointNormalized());

  // Create with bad value type.
  a = reg->Create<Attribute>("myFloat", kVec2);
  EXPECT_FALSE(a.IsValid());
  EXPECT_TRUE(ShaderInputRegistry::GetSpec(a) == nullptr);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "wrong value_type"));

  // Create with an unknown name; it should be automatically added.
  a = reg->Create<Attribute>("badName", 52.4f);
  EXPECT_TRUE(a.IsValid());
  EXPECT_EQ(kFloatAttribute, a.GetType());
  EXPECT_EQ(52.4f, a.GetValue<float>());
  EXPECT_TRUE(ShaderInputRegistry::GetSpec(a) != nullptr);

  // Copy of an invalid Attribute should also be invalid.
  a = Attribute();
  a2 = a;
  EXPECT_FALSE(a2.IsValid());
  EXPECT_TRUE(ShaderInputRegistry::GetSpec(a2) == nullptr);

  BufferObjectPtr vb(new BufferObject);
  BufferObjectElement element;
  Attribute a_buffer = reg->Create<Attribute>("myBuffer",
                                              BufferObjectElement(vb, 0));
  EXPECT_TRUE(a_buffer.IsValid());
  EXPECT_FALSE(ShaderInputRegistry::GetSpec(a_buffer) == nullptr);
  EXPECT_FALSE(a_buffer.IsFixedPointNormalized());
  // Check that the normalization of an attribute can bet set.
  a_buffer.SetFixedPointNormalized(true);
  EXPECT_TRUE(a_buffer.IsFixedPointNormalized());
  EXPECT_FALSE(a.IsFixedPointNormalized());
  a_buffer.SetFixedPointNormalized(false);
  EXPECT_FALSE(a_buffer.IsFixedPointNormalized());
  EXPECT_FALSE(a.IsFixedPointNormalized());

  // Check that the divisor of an attribute can be set.
  a_buffer.SetDivisor(1);
  EXPECT_EQ(1U, a_buffer.GetDivisor());

  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST(AttributeTest, AllTypes) {
  // Make sure all Attributes of all types are created properly.
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  EXPECT_TRUE(AddAttribute(reg, "myFloat", kFloatAttribute, ""));
  EXPECT_TRUE(AddAttribute(reg, "myVec2f", kFloatVector2Attribute, ""));
  EXPECT_TRUE(AddAttribute(reg, "myVec3f", kFloatVector3Attribute, ""));
  EXPECT_TRUE(AddAttribute(reg, "myVec4f", kFloatVector4Attribute, ""));
  EXPECT_TRUE(AddAttribute(reg, "myMat2f", kFloatMatrix2x2Attribute, ""));
  EXPECT_TRUE(AddAttribute(reg, "myMat3f", kFloatMatrix3x3Attribute, ""));
  EXPECT_TRUE(AddAttribute(reg, "myMat4f", kFloatMatrix4x4Attribute, ""));
  EXPECT_TRUE(AddAttribute(
      reg, "myBuffer", kBufferObjectElementAttribute, ""));

  Attribute a;
  BufferObjectPtr vb(new BufferObject);

#define TEST_ATTRIBUTE_TYPE(name, type_name, value_type, attribute_type, value)\
  a = reg->Create<Attribute>(name, value);                                     \
  EXPECT_EQ(0, strcmp(type_name, Attribute::GetValueTypeName(attribute_type)));\
  EXPECT_EQ(attribute_type, a.GetType());                                      \
  ASSERT_FALSE(base::IsInvalidReference(a.GetValue<value_type>()));            \
  EXPECT_EQ(value, a.GetValue<value_type>())

#define TEST_VEC_ATTRIBUTE_TYPE(name, type_name, value_type,                   \
                                attribute_type, value)                         \
  a = reg->Create<Attribute>(name, value);                                     \
  EXPECT_EQ(0, strcmp(type_name, Attribute::GetValueTypeName(attribute_type)));\
  EXPECT_EQ(attribute_type, a.GetType());                                      \
  ASSERT_FALSE(base::IsInvalidReference(a.GetValue<value_type>()));            \
  EXPECT_TRUE(value_type::AreValuesEqual(value, a.GetValue<value_type>()))

#define TEST_BOEATTRIBUTE_TYPE( \
            name, type_name, value_type, attribute_type, value)                \
  a = reg->Create<Attribute>(name, value);                                     \
  EXPECT_EQ(0, strcmp(type_name, Attribute::GetValueTypeName(attribute_type)));\
  EXPECT_EQ(attribute_type, a.GetType());                                      \
  ASSERT_FALSE(base::IsInvalidReference(a.GetValue<value_type>()));            \
  EXPECT_EQ(value.buffer_object.Get(),                                         \
            a.GetValue<value_type>().buffer_object.Get());                     \
  EXPECT_EQ(value.spec_index, a.GetValue<value_type>().spec_index)

  TEST_ATTRIBUTE_TYPE("myFloat", "Float", float, kFloatAttribute, 32.5f);
  TEST_VEC_ATTRIBUTE_TYPE("myVec2f", "FloatVector2", math::VectorBase2f,
                          kFloatVector2Attribute, math::Vector2f(1.0f, 2.0f));
  TEST_VEC_ATTRIBUTE_TYPE("myVec3f", "FloatVector3", math::VectorBase3f,
                          kFloatVector3Attribute,
                          math::Vector3f(1.0f, 2.0f, 3.0f));
  TEST_VEC_ATTRIBUTE_TYPE("myVec4f", "FloatVector4", math::VectorBase4f,
                          kFloatVector4Attribute,
                          math::Vector4f(1.0f, 2.0f, 3.0f, 4.0f));
  TEST_ATTRIBUTE_TYPE("myMat2f", "FloatMatrix2x2", math::Matrix2f,
                      kFloatMatrix2x2Attribute, math::Matrix2f(1.f, 2.f,
                                                               3.f, 4.f));
  TEST_ATTRIBUTE_TYPE("myMat3f", "FloatMatrix3x3", math::Matrix3f,
                      kFloatMatrix3x3Attribute, math::Matrix3f(1.f, 2.f, 3.f,
                                                               4.f, 5.f, 6.f,
                                                               7.f, 8.f, 9.f));
  TEST_ATTRIBUTE_TYPE("myMat4f", "FloatMatrix4x4", math::Matrix4f,
                      kFloatMatrix4x4Attribute,
                      math::Matrix4f(1.f, 2.f, 3.f, 4.f,
                                     5.f, 6.f, 7.f, 8.f,
                                     9.f, 10.f, 11.f, 12.f,
                                     13.f, 14.f, 15.f, 16.f));

  BufferObjectElement boe(vb, 0);
  TEST_BOEATTRIBUTE_TYPE("myBuffer", "BufferObjectElement", BufferObjectElement,
                         kBufferObjectElementAttribute, boe);

  // Test default case.
  EXPECT_EQ(0, strcmp("<UNKNOWN>", Attribute::GetValueTypeName(
      base::InvalidEnumValue<Attribute::ValueType>())));
  int bad_type = 128;
  EXPECT_EQ(0, strcmp("<UNKNOWN>", Attribute::GetValueTypeName(
      static_cast<Attribute::ValueType>(bad_type))));
}

}  // namespace gfx
}  // namespace ion
