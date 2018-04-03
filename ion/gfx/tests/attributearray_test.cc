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

#include "ion/gfx/attributearray.h"

#include <memory>

#include "ion/base/invalid.h"
#include "ion/base/logchecker.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfx/tests/mockresource.h"
#include "ion/math/vector.h"

#include "absl/memory/memory.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfx {

typedef testing::MockResource<AttributeArray::kNumChanges>
    MockVertexArrayResource;
typedef testing::MockResource<BufferObject::kNumChanges>
    MockBufferResource;

class AttributeArrayTest : public ::testing::Test {
 protected:
  void SetUp() override {
    va_.Reset(new AttributeArray());
    resource_ = absl::make_unique<MockVertexArrayResource>();
    EXPECT_FALSE(resource_->AnyModifiedBitsSet());
    va_->SetResource(0U, 0U, resource_.get());
    EXPECT_EQ(resource_.get(), va_->GetResource(0U, 0U));
    EXPECT_TRUE(resource_->AnyModifiedBitsSet());
    resource_->ResetModifiedBits();
    EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  }

  // This is to ensure that the resource holder goes away before the resource.
  void TearDown() override { va_.Reset(nullptr); }

  AttributeArrayPtr va_;
  std::unique_ptr<MockVertexArrayResource> resource_;
};

template<typename T>
static bool Equal(const Attribute& a, const Attribute& b) {
  return &a.GetRegistry() == &b.GetRegistry() &&
      a.GetIndexInRegistry() == b.GetIndexInRegistry() &&
      a.GetType() == b.GetType() &&
      a.GetValue<T>() == b.GetValue<T>();
}

template<typename T>
static bool VectorEqual(const Attribute& a, const Attribute& b) {
  return &a.GetRegistry() == &b.GetRegistry() &&
      a.GetIndexInRegistry() == b.GetIndexInRegistry() &&
      a.GetType() == b.GetType() &&
      T::AreValuesEqual(a.GetValue<T>(), b.GetValue<T>());
}

TEST_F(AttributeArrayTest, AddReplaceAttributes) {
  base::LogChecker log_checker;

  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "myBuffer", kBufferObjectElementAttribute, ""));
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "myBuffer2", kBufferObjectElementAttribute, ""));
  reg->Add(ShaderInputRegistry::AttributeSpec("myFloat", kFloatAttribute, ""));
  reg->Add(ShaderInputRegistry::AttributeSpec("myVec2f",
                                             kFloatVector2Attribute, ""));
  reg->Add(ShaderInputRegistry::AttributeSpec("myVec3f",
                                             kFloatVector3Attribute, ""));

  BufferObjectPtr vb(new BufferObject);
  BufferObjectElement element;
  Attribute a0 = reg->Create<Attribute>("myBuffer", BufferObjectElement(vb, 0));
  Attribute a1 = reg->Create<Attribute>("myFloat", 17.2f);
  Attribute a2 = reg->Create<Attribute>("myVec2f", math::Vector2f(0.f, 1.f));
  Attribute a3 = reg->Create<Attribute>("myVec2f", math::Vector2f(2.f, 3.f));
  Attribute a4 =
      reg->Create<Attribute>("myVec3f", math::Vector3f(2.f, 3.f, 4.f));
  Attribute a5 =
      reg->Create<Attribute>("myBuffer2", BufferObjectElement(vb, 1));
  Attribute a6 = reg->Create<Attribute>("myBuffer", BufferObjectElement(vb, 2));

  // Check that there are no attributes added.
  EXPECT_EQ(0U, va_->GetAttributeCount());
  EXPECT_EQ(base::kInvalidIndex, va_->GetAttributeIndexByName("myBuffer"));
  EXPECT_EQ(base::kInvalidIndex, va_->GetAttributeIndexByName("myFloat"));
  EXPECT_EQ(base::kInvalidIndex, va_->GetAttributeIndexByName("myVec2f"));

  // Check that no bits are set.
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  // Check that it is possible to add Attributes.
  va_->AddAttribute(a0);
  // Check that the proper attribute bit has been set.
  // Two bits should have been set, one for the Attribute and one for its
  // enabled state.
  EXPECT_EQ(2U, resource_->GetModifiedBitCount());
  EXPECT_TRUE(resource_->TestModifiedBit(AttributeArray::kAttributeChanged));
  EXPECT_TRUE(resource_->TestModifiedBit(
      AttributeArray::kAttributeEnabledChanged));
  resource_->ResetModifiedBits();
  EXPECT_EQ(1U, va_->GetAttributeCount());
  EXPECT_EQ(1U, va_->GetBufferAttributeCount());
  EXPECT_EQ(0U, va_->GetSimpleAttributeCount());
  EXPECT_EQ(0U, va_->GetAttributeIndexByName("myBuffer"));
  EXPECT_EQ(1U, vb->GetReceiverCount());
  EXPECT_EQ(base::kInvalidIndex, va_->GetAttributeIndexByName("myFloat"));
  EXPECT_EQ(base::kInvalidIndex, va_->GetAttributeIndexByName("myVec2f"));

  va_->AddAttribute(a1);
  // No bits should have been modified since a1 is a simple attribute.
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  EXPECT_EQ(2U, va_->GetAttributeCount());
  EXPECT_EQ(1U, va_->GetBufferAttributeCount());
  EXPECT_EQ(1U, va_->GetSimpleAttributeCount());
  EXPECT_EQ(0U, va_->GetAttributeIndexByName("myBuffer"));
  EXPECT_EQ(1U, va_->GetAttributeIndexByName("myFloat"));
  EXPECT_EQ(1U, vb->GetReceiverCount());
  EXPECT_EQ(base::kInvalidIndex, va_->GetAttributeIndexByName("myVec2f"));

  const size_t index = va_->AddAttribute(a2);
  EXPECT_EQ(3U, va_->GetAttributeCount());
  EXPECT_EQ(1U, va_->GetBufferAttributeCount());
  EXPECT_EQ(2U, va_->GetSimpleAttributeCount());
  EXPECT_TRUE(Equal<BufferObjectElement>(a0, va_->GetAttribute(0)));
  EXPECT_TRUE(Equal<float>(a1, va_->GetAttribute(1)));
  EXPECT_TRUE(VectorEqual<math::VectorBase2f>(a2, va_->GetAttribute(2)));
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  EXPECT_EQ(0U, va_->GetAttributeIndexByName("myBuffer"));
  EXPECT_EQ(1U, va_->GetAttributeIndexByName("myFloat"));
  EXPECT_EQ(1U, vb->GetReceiverCount());
  EXPECT_EQ(index, va_->GetAttributeIndexByName("myVec2f"));

  // Modifying vb should trigger a notification in the AttributeArray.
  vb->SetData(base::DataContainer::CreateOverAllocated<char>(
                  1, nullptr, base::AllocatorPtr()),
              1, 1, BufferObject::kStaticDraw);
  EXPECT_TRUE(resource_->TestModifiedBit(AttributeArray::kAttributeChanged));
  resource_->ResetModifiedBits();

  // Adding the same attribute twice does nothing, and returns the old index.
  EXPECT_EQ(index, va_->AddAttribute(a2));
  EXPECT_EQ(3U, va_->GetAttributeCount());
  EXPECT_EQ(1U, va_->GetBufferAttributeCount());
  EXPECT_EQ(2U, va_->GetSimpleAttributeCount());
  EXPECT_TRUE(Equal<BufferObjectElement>(a0, va_->GetAttribute(0)));
  EXPECT_TRUE(Equal<float>(a1, va_->GetAttribute(1)));
  EXPECT_TRUE(VectorEqual<math::VectorBase2f>(a2, va_->GetAttribute(2)));
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  EXPECT_EQ(0U, va_->GetAttributeIndexByName("myBuffer"));
  EXPECT_EQ(1U, va_->GetAttributeIndexByName("myFloat"));
  EXPECT_EQ(1U, vb->GetReceiverCount());
  EXPECT_EQ(index, va_->GetAttributeIndexByName("myVec2f"));

  EXPECT_EQ(0U, va_->AddAttribute(a0));
  EXPECT_EQ(3U, va_->GetAttributeCount());
  EXPECT_EQ(1U, va_->GetBufferAttributeCount());
  EXPECT_EQ(2U, va_->GetSimpleAttributeCount());
  EXPECT_TRUE(Equal<BufferObjectElement>(a0, va_->GetAttribute(0)));
  EXPECT_TRUE(Equal<float>(a1, va_->GetAttribute(1)));
  EXPECT_TRUE(VectorEqual<math::VectorBase2f>(a2, va_->GetAttribute(2)));
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  EXPECT_EQ(0U, va_->GetAttributeIndexByName("myBuffer"));
  EXPECT_EQ(1U, va_->GetAttributeIndexByName("myFloat"));
  EXPECT_EQ(1U, vb->GetReceiverCount());
  EXPECT_EQ(index, va_->GetAttributeIndexByName("myVec2f"));

  // Add an invalid attribute for better coverage.
  Attribute invalid;
  EXPECT_EQ(base::kInvalidIndex, va_->AddAttribute(invalid));
  EXPECT_EQ(3U, va_->GetAttributeCount());
  EXPECT_EQ(1U, va_->GetBufferAttributeCount());
  EXPECT_EQ(2U, va_->GetSimpleAttributeCount());
  EXPECT_EQ(1U, vb->GetReceiverCount());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  // Check buffer and simple getters.
  EXPECT_TRUE(Equal<BufferObjectElement>(a0, va_->GetBufferAttribute(0)));
  EXPECT_TRUE(Equal<float>(a1, va_->GetSimpleAttribute(0)));
  EXPECT_TRUE(VectorEqual<math::VectorBase2f>(a2, va_->GetSimpleAttribute(1)));

  // Check that it is possible to replace an attribute.
  EXPECT_TRUE(va_->ReplaceAttribute(2, a3));
  EXPECT_EQ(3U, va_->GetAttributeCount());
  EXPECT_EQ(1U, va_->GetBufferAttributeCount());
  EXPECT_EQ(1U, vb->GetReceiverCount());
  EXPECT_EQ(2U, va_->GetSimpleAttributeCount());
  EXPECT_TRUE(Equal<BufferObjectElement>(a0, va_->GetAttribute(0)));
  EXPECT_TRUE(Equal<float>(a1, va_->GetAttribute(1)));
  EXPECT_TRUE(VectorEqual<math::VectorBase2f>(a3, va_->GetAttribute(2)));
  // Check buffer and simple getters.
  EXPECT_TRUE(Equal<BufferObjectElement>(a0, va_->GetBufferAttribute(0)));
  EXPECT_TRUE(Equal<float>(a1, va_->GetSimpleAttribute(0)));
  EXPECT_TRUE(VectorEqual<math::VectorBase2f>(a3, va_->GetSimpleAttribute(1)));
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  EXPECT_EQ(0U, va_->GetAttributeIndexByName("myBuffer"));
  EXPECT_EQ(1U, va_->GetAttributeIndexByName("myFloat"));
  EXPECT_EQ(index, va_->GetAttributeIndexByName("myVec2f"));

  EXPECT_TRUE(va_->ReplaceAttribute(0, a5));
  EXPECT_EQ(3U, va_->GetAttributeCount());
  EXPECT_EQ(1U, va_->GetBufferAttributeCount());
  EXPECT_EQ(2U, va_->GetSimpleAttributeCount());
  EXPECT_EQ(1U, vb->GetReceiverCount());
  EXPECT_TRUE(Equal<BufferObjectElement>(a5, va_->GetAttribute(0)));
  EXPECT_TRUE(Equal<float>(a1, va_->GetAttribute(1)));
  EXPECT_TRUE(VectorEqual<math::VectorBase2f>(a3, va_->GetAttribute(2)));
  // Check buffer and simple getters.
  EXPECT_TRUE(Equal<BufferObjectElement>(a5, va_->GetBufferAttribute(0)));
  EXPECT_TRUE(Equal<float>(a1, va_->GetSimpleAttribute(0)));
  EXPECT_TRUE(VectorEqual<math::VectorBase2f>(a3, va_->GetSimpleAttribute(1)));
  EXPECT_EQ(1U, resource_->GetModifiedBitCount());
  EXPECT_TRUE(resource_->TestModifiedBit(AttributeArray::kAttributeChanged));
  // Reset the modified bits.
  resource_->ResetModifiedBits();
  EXPECT_EQ(0U, va_->GetAttributeIndexByName("myBuffer2"));
  EXPECT_EQ(1U, va_->GetAttributeIndexByName("myFloat"));
  EXPECT_EQ(index, va_->GetAttributeIndexByName("myVec2f"));

  // Replacing with an invalid attribute should do nothing.
  EXPECT_FALSE(va_->ReplaceAttribute(0, Attribute()));
  EXPECT_EQ(3U, va_->GetAttributeCount());
  EXPECT_EQ(1U, va_->GetBufferAttributeCount());
  EXPECT_EQ(2U, va_->GetSimpleAttributeCount());
  EXPECT_EQ(1U, vb->GetReceiverCount());
  EXPECT_TRUE(Equal<BufferObjectElement>(a5, va_->GetAttribute(0)));
  EXPECT_TRUE(Equal<float>(a1, va_->GetAttribute(1)));
  EXPECT_TRUE(VectorEqual<math::VectorBase2f>(a3, va_->GetAttribute(2)));
  // Check buffer and simple getters.
  EXPECT_TRUE(Equal<BufferObjectElement>(a5, va_->GetBufferAttribute(0)));
  EXPECT_TRUE(Equal<float>(a1, va_->GetSimpleAttribute(0)));
  EXPECT_TRUE(VectorEqual<math::VectorBase2f>(a3, va_->GetSimpleAttribute(1)));
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  EXPECT_EQ(base::kInvalidIndex, va_->GetAttributeIndexByName("myBuffer"));
  EXPECT_EQ(base::kInvalidIndex, va_->GetAttributeIndexByName("myVec3f"));
  EXPECT_EQ(1U, va_->GetAttributeIndexByName("myFloat"));
  EXPECT_EQ(index, va_->GetAttributeIndexByName("myVec2f"));

  // Replace a simple attribute with a buffer attribute.
  EXPECT_TRUE(va_->ReplaceAttribute(1, a6));
  EXPECT_EQ(3U, va_->GetAttributeCount());
  EXPECT_EQ(2U, va_->GetBufferAttributeCount());
  EXPECT_EQ(1U, va_->GetSimpleAttributeCount());
  EXPECT_EQ(1U, vb->GetReceiverCount());
  EXPECT_TRUE(Equal<BufferObjectElement>(a5, va_->GetAttribute(0)));
  EXPECT_TRUE(Equal<BufferObjectElement>(a6, va_->GetAttribute(1)));
  EXPECT_TRUE(VectorEqual<math::VectorBase2f>(a3, va_->GetAttribute(2)));
  EXPECT_EQ(base::kInvalidIndex, va_->GetAttributeIndexByName("myVec3f"));
  EXPECT_EQ(1U, va_->GetAttributeIndexByName("myBuffer"));
  EXPECT_EQ(index, va_->GetAttributeIndexByName("myVec2f"));
  // Check buffer and simple getters.
  EXPECT_TRUE(Equal<BufferObjectElement>(a5, va_->GetBufferAttribute(0)));
  EXPECT_TRUE(Equal<BufferObjectElement>(a6, va_->GetBufferAttribute(1)));
  EXPECT_TRUE(VectorEqual<math::VectorBase2f>(a3, va_->GetSimpleAttribute(0)));
  EXPECT_EQ(2U, resource_->GetModifiedBitCount());
  EXPECT_TRUE(resource_->TestModifiedBit(
      AttributeArray::kAttributeChanged + 1));
  EXPECT_TRUE(resource_->TestModifiedBit(
      AttributeArray::kAttributeEnabledChanged + 1));
  resource_->ResetModifiedBits();
  // Modifying vb should trigger a notification for all Attributes that use it.
  vb->SetData(base::DataContainer::CreateOverAllocated<char>(
                  1, nullptr, base::AllocatorPtr()),
              1, 1, BufferObject::kStaticDraw);
  EXPECT_EQ(2U, resource_->GetModifiedBitCount());
  EXPECT_TRUE(resource_->TestModifiedBit(AttributeArray::kAttributeChanged));
  EXPECT_TRUE(
      resource_->TestModifiedBit(AttributeArray::kAttributeChanged + 1));
  resource_->ResetModifiedBits();
  {
    std::unique_ptr<MockBufferResource> bo_resource(new MockBufferResource);
    vb->SetResource(0U, 0U, bo_resource.get());

    // Modifying vb's data should also trigger a notification. Calling
    // GetMutableData() will start the chain.
    vb->GetData()->GetMutableData<void*>();
    EXPECT_EQ(2U, resource_->GetModifiedBitCount());
    EXPECT_TRUE(resource_->TestModifiedBit(AttributeArray::kAttributeChanged));
    EXPECT_TRUE(
        resource_->TestModifiedBit(AttributeArray::kAttributeChanged + 1));
    resource_->ResetModifiedBits();

    vb->SetResource(0U, 0U, nullptr);
  }

  // Replace a buffer attribute with a simple attribute.
  EXPECT_TRUE(va_->ReplaceAttribute(0, a4));
  EXPECT_EQ(3U, va_->GetAttributeCount());
  EXPECT_EQ(1U, va_->GetBufferAttributeCount());
  EXPECT_EQ(2U, va_->GetSimpleAttributeCount());
  EXPECT_EQ(1U, vb->GetReceiverCount());
  EXPECT_TRUE(VectorEqual<math::VectorBase3f>(a4, va_->GetAttribute(0)));
  EXPECT_TRUE(Equal<BufferObjectElement>(a6, va_->GetAttribute(1)));
  EXPECT_TRUE(VectorEqual<math::VectorBase2f>(a3, va_->GetAttribute(2)));
  // Check buffer and simple getters. The simple attribute indices are reversed
  // since a3 became the first simple attribute above.
  EXPECT_TRUE(VectorEqual<math::VectorBase3f>(a4, va_->GetSimpleAttribute(1)));
  EXPECT_TRUE(Equal<BufferObjectElement>(a6, va_->GetBufferAttribute(0)));
  EXPECT_TRUE(VectorEqual<math::VectorBase2f>(a3, va_->GetSimpleAttribute(0)));
  // The other buffer attribute was moved to index 0 and will be marked as
  // changed.
  EXPECT_EQ(2U, resource_->GetModifiedBitCount());
  EXPECT_TRUE(resource_->TestModifiedBit(
      AttributeArray::kAttributeChanged));
  EXPECT_TRUE(resource_->TestModifiedBit(
      AttributeArray::kAttributeEnabledChanged));
  resource_->ResetModifiedBits();
  EXPECT_EQ(0U, va_->GetAttributeIndexByName("myVec3f"));
  EXPECT_EQ(1U, va_->GetAttributeIndexByName("myBuffer"));
  EXPECT_EQ(base::kInvalidIndex, va_->GetAttributeIndexByName("myFloat"));
  EXPECT_EQ(index, va_->GetAttributeIndexByName("myVec2f"));

  // Replacing an attribute with itself does nothing.
  EXPECT_FALSE(va_->ReplaceAttribute(0, a4));
  EXPECT_EQ(3U, va_->GetAttributeCount());
  EXPECT_EQ(1U, va_->GetBufferAttributeCount());
  EXPECT_EQ(2U, va_->GetSimpleAttributeCount());
  EXPECT_EQ(1U, vb->GetReceiverCount());
  EXPECT_TRUE(VectorEqual<math::VectorBase3f>(a4, va_->GetAttribute(0)));
  EXPECT_TRUE(Equal<BufferObjectElement>(a6, va_->GetAttribute(1)));
  EXPECT_TRUE(VectorEqual<math::VectorBase2f>(a3, va_->GetAttribute(2)));
  EXPECT_TRUE(VectorEqual<math::VectorBase3f>(a4, va_->GetSimpleAttribute(1)));
  EXPECT_TRUE(Equal<BufferObjectElement>(a6, va_->GetBufferAttribute(0)));
  EXPECT_TRUE(VectorEqual<math::VectorBase2f>(a3, va_->GetSimpleAttribute(0)));
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  // Trying to replacing an invalid index does nothing.
  EXPECT_FALSE(va_->ReplaceAttribute(4, a4));
  EXPECT_EQ(3U, va_->GetAttributeCount());
  EXPECT_EQ(1U, va_->GetBufferAttributeCount());
  EXPECT_EQ(2U, va_->GetSimpleAttributeCount());
  EXPECT_EQ(1U, vb->GetReceiverCount());
  EXPECT_TRUE(VectorEqual<math::VectorBase3f>(a4, va_->GetAttribute(0)));
  EXPECT_TRUE(Equal<BufferObjectElement>(a6, va_->GetAttribute(1)));
  EXPECT_TRUE(VectorEqual<math::VectorBase2f>(a3, va_->GetAttribute(2)));
  EXPECT_TRUE(VectorEqual<math::VectorBase3f>(a4, va_->GetSimpleAttribute(1)));
  EXPECT_TRUE(Equal<BufferObjectElement>(a6, va_->GetBufferAttribute(0)));
  EXPECT_TRUE(VectorEqual<math::VectorBase2f>(a3, va_->GetSimpleAttribute(0)));
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  // Check that getting a mutable attribute sets the appropriate bit only for
  // buffer attributes.
  EXPECT_TRUE(VectorEqual<math::VectorBase3f>(
      a4, *va_->GetMutableAttribute(0)));
  EXPECT_TRUE(VectorEqual<math::VectorBase2f>(
      a3, *va_->GetMutableAttribute(2)));
  EXPECT_TRUE(VectorEqual<math::VectorBase2f>(
      a3, *va_->GetMutableSimpleAttribute(0)));
  EXPECT_TRUE(VectorEqual<math::VectorBase3f>(
      a4, *va_->GetMutableSimpleAttribute(1)));
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  EXPECT_TRUE(Equal<BufferObjectElement>(a6, *va_->GetMutableAttribute(1)));
  EXPECT_EQ(1U, resource_->GetModifiedBitCount());
  EXPECT_TRUE(resource_->TestModifiedBit(
      AttributeArray::kAttributeChanged));
  resource_->ResetModifiedBits();

  // Check that you cannot get an invalid mutable attribute.
  // Check that getting a mutable attribute sets the appropriate bit.
  EXPECT_TRUE(va_->GetMutableAttribute(4) == nullptr);
  EXPECT_TRUE(va_->GetMutableSimpleAttribute(2) == nullptr);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_TRUE(va_->GetMutableBufferAttribute(1) == nullptr);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Invalid index"));

  // Check that attributes are enabled by default.
  EXPECT_TRUE(va_->IsAttributeEnabled(0));
  EXPECT_TRUE(va_->IsAttributeEnabled(1));
  EXPECT_TRUE(va_->IsAttributeEnabled(2));

  // Check that we can disable buffer Attributes.
  va_->EnableAttribute(0, false);
  EXPECT_TRUE(va_->IsAttributeEnabled(0));
  EXPECT_TRUE(va_->IsAttributeEnabled(1));
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  va_->EnableAttribute(1, false);
  EXPECT_TRUE(va_->IsAttributeEnabled(0));
  EXPECT_FALSE(va_->IsAttributeEnabled(1));
  EXPECT_EQ(1U, resource_->GetModifiedBitCount());
  EXPECT_TRUE(resource_->TestModifiedBit(
      AttributeArray::kAttributeEnabledChanged));
  resource_->ResetModifiedBits();

  va_->EnableAttribute(0, true);
  EXPECT_TRUE(va_->IsAttributeEnabled(0));
  EXPECT_FALSE(va_->IsAttributeEnabled(1));
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  // Use the buffer setter and getter
  va_->EnableBufferAttribute(0, true);
  EXPECT_TRUE(va_->IsAttributeEnabled(0));
  EXPECT_TRUE(va_->IsBufferAttributeEnabled(0));
  EXPECT_EQ(1U, resource_->GetModifiedBitCount());
  EXPECT_TRUE(resource_->TestModifiedBit(
      AttributeArray::kAttributeEnabledChanged));
  resource_->ResetModifiedBits();

  // Check that passing an invalid index to the buffer setter and getter
  // generates errors.
  EXPECT_FALSE(va_->IsBufferAttributeEnabled(1));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Invalid index"));
  va_->EnableBufferAttribute(1, false);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Invalid index"));

  // Check that trying to enable/disable an invalid index has no side effects.
  va_->EnableAttribute(3, true);
  EXPECT_TRUE(va_->IsAttributeEnabled(0));
  EXPECT_TRUE(va_->IsAttributeEnabled(1));
  // Setting invalid state should not set a bit.
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  // Getting the enabled state of a non-existent attribute does nothing.
  EXPECT_FALSE(va_->IsAttributeEnabled(3));
  EXPECT_EQ(1U, vb->GetReceiverCount());

  // Check that getting an invalid index produces an error message.
  EXPECT_TRUE(base::IsInvalidReference(va_->GetAttribute(5)));
  EXPECT_TRUE(base::IsInvalidReference(va_->GetBufferAttribute(2)));
  EXPECT_TRUE(base::IsInvalidReference(va_->GetSimpleAttribute(3)));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Invalid index"));

  // Check that adding too many attributes prints an error message.
  std::string attribute_name("a");
  for (size_t i = 0; i < kAttributeSlotCount; ++i) {
    reg->Add(ShaderInputRegistry::AttributeSpec(attribute_name,
                                                kBufferObjectElementAttribute,
                                                ""));
    Attribute a = reg->Create<Attribute>(attribute_name,
                                         BufferObjectElement(vb, i));
    va_->AddAttribute(a);
    attribute_name.append("a");
  }
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Too many entries added"));

  // Check that destroying the AttributeArray removes it from its receivers'
  // notify list.
  va_.Reset(nullptr);
  EXPECT_EQ(0U, vb->GetReceiverCount());
}

}  // namespace gfx
}  // namespace ion
