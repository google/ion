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

#include "ion/gfxutils/buffertoattributebinder.h"

#include "ion/base/invalid.h"
#include "ion/base/logchecker.h"
#include "ion/gfx/attributearray.h"
#include "ion/gfx/bufferobject.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/math/vector.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfxutils {

using gfx::AttributeArray;
using gfx::AttributeArrayPtr;
using gfx::BufferObject;
using gfx::BufferObjectPtr;
using gfx::ShaderInputRegistry;
using gfx::ShaderInputRegistryPtr;

// Vertex structures for testing.
struct InternallyPaddedVertex {
  math::Vector2ui16 field1;
  uint8 pad;
  math::Vector4f field2;
};

struct EndPaddedVertex {
  math::Vector2ui16 field1;
  math::Vector4f field2;
  uint8 pad;
};


TEST(BufferToAttributeBinderTest, Add) {
  base::LogChecker log_checker;

  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "field1", gfx::kBufferObjectElementAttribute, ""));
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "field2", gfx::kBufferObjectElementAttribute, ""));
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "pad", gfx::kBufferObjectElementAttribute, ""));

  AttributeArrayPtr va(new AttributeArray);
  BufferObjectPtr vb(new BufferObject);

  // Check that there are no Attributes or Specs.
  EXPECT_EQ(0U, va->GetAttributeCount());
  EXPECT_EQ(0U, vb->GetSpecCount());

  InternallyPaddedVertex v;
  BufferToAttributeBinder<InternallyPaddedVertex> binder(v);
  binder
      .BindAndNormalize(v.field1, "field1")
      .Bind(v.field2, "field2")
      .Bind(v.pad, "pad")
      .Apply(reg, va, vb);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  // Both a field and the overall struct are not tightly packed.
  EXPECT_FALSE(binder.AreBindingsPacked(*reg.Get()));
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "field2' is not tightly"));
  EXPECT_FALSE(binder.AreBindingsPacked(*reg.Get()));
  EXPECT_TRUE(log_checker.HasMessage("WARNING",
                                     "Vertex struct is not tightly packed"));

  // Check that Attributes and Specs were created.
  EXPECT_EQ(3U, va->GetAttributeCount());
  EXPECT_EQ(3U, vb->GetSpecCount());
  EXPECT_TRUE(va->GetAttribute(0).IsFixedPointNormalized());
  EXPECT_FALSE(va->GetAttribute(1).IsFixedPointNormalized());
  EXPECT_FALSE(va->GetAttribute(2).IsFixedPointNormalized());
  EXPECT_EQ(0U, va->GetAttribute(0).GetDivisor());
  EXPECT_EQ(0U, va->GetAttribute(1).GetDivisor());
  EXPECT_EQ(0U, va->GetAttribute(2).GetDivisor());

  {
    // Check that a Spec can be described.
    const BufferObject::Spec& spec = vb->GetSpec(0);
    EXPECT_FALSE(base::IsInvalidReference(spec));
    EXPECT_EQ(BufferObject::kUnsignedShort, spec.type);
    EXPECT_EQ(2U, spec.component_count);
    EXPECT_EQ(0U, spec.byte_offset);
  }

  {
    // Check that another Spec can be described.
    const BufferObject::Spec& spec = vb->GetSpec(1);
    EXPECT_FALSE(base::IsInvalidReference(spec));
    EXPECT_EQ(BufferObject::kFloat, spec.type);
    EXPECT_EQ(4U, spec.component_count);
    EXPECT_EQ(reinterpret_cast<size_t>(&v.field2) -
              reinterpret_cast<size_t>(&v), spec.byte_offset);
  }

  // Check that a binder can be copied.
  va = new AttributeArray;
  vb = new BufferObject;
  EXPECT_EQ(0U, va->GetAttributeCount());
  EXPECT_EQ(0U, vb->GetSpecCount());
  // Use the same binder to create the same bindings again.
  binder.Apply(reg, va, vb);
  {
    // Check that a Spec can be described.
    const BufferObject::Spec& spec = vb->GetSpec(0);
    EXPECT_FALSE(base::IsInvalidReference(spec));
    EXPECT_EQ(BufferObject::kUnsignedShort, spec.type);
    EXPECT_EQ(2U, spec.component_count);
    EXPECT_EQ(0U, spec.byte_offset);
  }

  {
    // Check that another Spec can be described.
    const BufferObject::Spec& spec = vb->GetSpec(1);
    EXPECT_FALSE(base::IsInvalidReference(spec));
    EXPECT_EQ(BufferObject::kFloat, spec.type);
    EXPECT_EQ(4U, spec.component_count);
    EXPECT_EQ(reinterpret_cast<size_t>(&v.field2) -
              reinterpret_cast<size_t>(&v), spec.byte_offset);
  }

  // Check that adding invalid attributes fails with an error message.
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EndPaddedVertex v2;
  BufferToAttributeBinder<EndPaddedVertex> padded_binder1(v2);
  padded_binder1
      .Bind(v2.field1, "nosuchname")
      .BindAndNormalize(v2.field1, "field1")
      .Bind(v2.field2, "field2")
      .Bind(v2.pad, "pad")
      .Apply(reg, va, vb);
  EXPECT_FALSE(padded_binder1.AreBindingsPacked(*reg.Get()));

  // Check that for this structure the struct itself is not tightly packed.
  BufferToAttributeBinder<EndPaddedVertex> padded_binder2(v2);
  padded_binder2
      .BindAndNormalize(v2.field1, "field1")
      .Bind(v2.field2, "field2")
      .Bind(v2.pad, "pad")
      .Apply(reg, va, vb);
  // The individual fields are tightly packed.
  EXPECT_FALSE(padded_binder2.AreBindingsPacked(*reg.Get()));
  EXPECT_TRUE(log_checker.HasNoMessage("WARNING", "' is not tightly packed"));
  // The struct, however, is not.
  EXPECT_TRUE(log_checker.HasMessage("WARNING",
                                     "struct is not tightly packed"));

  EXPECT_FALSE(log_checker.HasAnyMessages());
}

// Duplicate the part of the test above, except that we are binding with divisor
// set.
TEST(BufferToAttributeBinderTest, AddWithDivisorSet) {
  base::LogChecker log_checker;

  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "field1", gfx::kBufferObjectElementAttribute, ""));
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "field2", gfx::kBufferObjectElementAttribute, ""));
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "pad", gfx::kBufferObjectElementAttribute, ""));

  AttributeArrayPtr va(new AttributeArray);
  BufferObjectPtr vb(new BufferObject);

  // Check that there are no Attributes or Specs.
  EXPECT_EQ(0U, va->GetAttributeCount());
  EXPECT_EQ(0U, vb->GetSpecCount());

  InternallyPaddedVertex v;
  BufferToAttributeBinder<InternallyPaddedVertex> binder(v);
  binder.BindAndNormalize(v.field1, "field1", 1)
      .Bind(v.field2, "field2", 2)
      .Bind(v.pad, "pad", 3)
      .Apply(reg, va, vb);
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // Check for the values of divisor.
  EXPECT_EQ(1U, va->GetAttribute(0).GetDivisor());
  EXPECT_EQ(2U, va->GetAttribute(1).GetDivisor());
  EXPECT_EQ(3U, va->GetAttribute(2).GetDivisor());
}

TEST(BufferToAttributeBinderTest, BuiltInTypes) {
#define TestComponentFunctionPair(c_type, bo_type, count)       \
  EXPECT_EQ(BufferObject::bo_type, GetComponentType<c_type>()); \
  EXPECT_EQ(count, GetComponentCount<c_type>());

#define TestComponentFunctions(c_type, bo_type, suffix)              \
  TestComponentFunctionPair(c_type, bo_type, 1U);                    \
  TestComponentFunctionPair(math::VectorBase1##suffix, bo_type, 1U); \
  TestComponentFunctionPair(math::VectorBase2##suffix, bo_type, 2U); \
  TestComponentFunctionPair(math::VectorBase3##suffix, bo_type, 3U); \
  TestComponentFunctionPair(math::VectorBase4##suffix, bo_type, 4U)

  TestComponentFunctions(char, kByte, i8);
  TestComponentFunctions(unsigned char, kUnsignedByte, ui8);
  TestComponentFunctions(int16, kShort, i16);
  TestComponentFunctions(uint16, kUnsignedShort, ui16);
  TestComponentFunctions(int32, kInt, i);
  TestComponentFunctions(uint32, kUnsignedInt, ui);
  TestComponentFunctions(float, kFloat, f);

#undef TestComponentFunctions
#undef TestComponentFunctionPair
}

}  // namespace gfxutils
}  // namespace ion
