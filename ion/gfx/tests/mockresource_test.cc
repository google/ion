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

#include "ion/gfx/tests/mockresource.h"

#include <memory>

#include "ion/gfx/resourceholder.h"

#include "absl/memory/memory.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfx {

namespace {

typedef testing::MockResource<7> MyMockResource;

class MyHolder : public ResourceHolder {
 public:
  MyHolder()
      : field0(0, 0, this),
        field1(1, 0, this),
        field2(2, 0, this),
        field3(3, 0, this),
        field4(4, 0, this),
        field5(5, 0, this),
        field6(6, 0, this) {}
  ~MyHolder() override {}

  // Change the i-th field by incrementing it.
  void Change(int i) { (&field0)[i].Set((&field0)[i].Get() + 1); }

 private:
  Field<int> field0;
  Field<int> field1;
  Field<int> field2;
  Field<int> field3;
  Field<int> field4;
  Field<int> field5;
  Field<int> field6;
};

using MyHolderPtr = base::SharedPtr<MyHolder>;

class MockResourceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    holder_.Reset(new MyHolder);
    resource_ = absl::make_unique<MyMockResource>(0);
    resource2_ = absl::make_unique<MyMockResource>(2);
    resource3_ = absl::make_unique<MyMockResource>(3);
    EXPECT_FALSE(resource_->AnyModifiedBitsSet());
    holder_->SetResource(0U, 0U, resource_.get());
    EXPECT_EQ(resource_.get(), holder_->GetResource(0U, 0U));
    resource_->ResetModifiedBits();
    EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  }

  // This is to ensure that the resource holder goes away before the resource.
  void TearDown() override { holder_.Reset(nullptr); }

  MyHolderPtr holder_;
  std::unique_ptr<MyMockResource> resource_;
  std::unique_ptr<MyMockResource> resource2_;
  std::unique_ptr<MyMockResource> resource3_;
};

}  // anonymous namespace

TEST_F(MockResourceTest, MockResource) {
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  EXPECT_EQ(0U, resource_->GetGpuMemoryUsed());

  holder_->Change(0);
  EXPECT_TRUE(resource_->AnyModifiedBitsSet());
  EXPECT_EQ(1U, resource_->GetModifiedBitCount());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(0));
  EXPECT_TRUE(resource_->TestModifiedBitRange(0, 1));

  holder_->Change(1);
  EXPECT_TRUE(resource_->AnyModifiedBitsSet());
  EXPECT_EQ(2U, resource_->GetModifiedBitCount());
  EXPECT_FALSE(resource_->TestOnlyModifiedBit(0));
  EXPECT_TRUE(resource_->TestModifiedBit(0));
  EXPECT_TRUE(resource_->TestModifiedBit(1));
  EXPECT_TRUE(resource_->TestModifiedBitRange(0, 1));
  EXPECT_FALSE(resource_->TestModifiedBitRange(3, 5));

  resource_->ResetModifiedBit(1);
  EXPECT_TRUE(resource_->AnyModifiedBitsSet());
  EXPECT_EQ(1U, resource_->GetModifiedBitCount());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(0));
  EXPECT_TRUE(resource_->TestModifiedBitRange(0, 1));

  holder_->Change(2);
  EXPECT_TRUE(resource_->AnyModifiedBitsSet());
  EXPECT_EQ(2U, resource_->GetModifiedBitCount());
  EXPECT_FALSE(resource_->TestOnlyModifiedBit(0));
  EXPECT_TRUE(resource_->TestModifiedBit(0));
  EXPECT_TRUE(resource_->TestModifiedBit(2));
  EXPECT_TRUE(resource_->TestModifiedBitRange(0, 2));
  EXPECT_FALSE(resource_->TestModifiedBitRange(1, 1));
  EXPECT_TRUE(resource_->TestModifiedBitRange(0, 0));
  EXPECT_TRUE(resource_->TestModifiedBitRange(2, 2));

  resource_->ResetModifiedBit(0);
  EXPECT_TRUE(resource_->AnyModifiedBitsSet());
  EXPECT_TRUE(resource_->TestModifiedBitRange(2, 2));
  EXPECT_FALSE(resource_->TestModifiedBitRange(0, 1));

  holder_->Change(4);
  holder_->Change(5);
  holder_->Change(6);
  EXPECT_TRUE(resource_->AnyModifiedBitsSet());
  EXPECT_TRUE(resource_->TestModifiedBitRange(2, 5));
  EXPECT_EQ(4U, resource_->GetModifiedBitCount());

  resource_->ResetModifiedBits();
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  EXPECT_FALSE(resource_->TestModifiedBitRange(0, 6));
}

TEST_F(MockResourceTest, SetResource) {
  // SetUp() sets the initial resource.
  EXPECT_EQ(resource_.get(), holder_->GetResource(0U, 0));
  EXPECT_EQ(1, holder_->GetResourceCount());

  // Setting the resource to a non-nullptr value shouldn't increase the count.
  holder_->SetResource(0U, 0, resource_.get());
  EXPECT_EQ(resource_.get(), holder_->GetResource(0U, 0));
  EXPECT_EQ(1, holder_->GetResourceCount());

  // Setting the resource to nullptr should decrease it.
  holder_->SetResource(0U, 0, resource_.get());
  EXPECT_EQ(resource_.get(), holder_->GetResource(0U, 0));
  EXPECT_EQ(1, holder_->GetResourceCount());

  // Repeatedly setting nullptr should not affect the count.
  holder_->SetResource(0U, 0, nullptr);
  EXPECT_TRUE(holder_->GetResource(0U, 0) == nullptr);
  EXPECT_EQ(0, holder_->GetResourceCount());
  holder_->SetResource(0U, 0, nullptr);
  EXPECT_TRUE(holder_->GetResource(0U, 0) == nullptr);
  EXPECT_EQ(0, holder_->GetResourceCount());

  // Setting the resource to non-nullptr should increase the count.
  holder_->SetResource(0U, 0, resource_.get());
  EXPECT_EQ(resource_.get(), holder_->GetResource(0U, 0));
  EXPECT_EQ(1, holder_->GetResourceCount());

  // Same.
  holder_->SetResource(1U, 0, resource_.get());
  EXPECT_EQ(resource_.get(), holder_->GetResource(1U, 0));
  EXPECT_EQ(2, holder_->GetResourceCount());

  // Adding another resource with different key should increase the count.
  holder_->SetResource(1U, 2, resource2_.get());
  EXPECT_EQ(resource_.get(), holder_->GetResource(1U, 0));
  EXPECT_EQ(resource2_.get(), holder_->GetResource(1U, 2));
  EXPECT_EQ(3, holder_->GetResourceCount());

  // Nulling resource should decrease the count.
  holder_->SetResource(1U, 0, nullptr);
  EXPECT_EQ(nullptr, holder_->GetResource(1U, 0));
  EXPECT_EQ(resource2_.get(), holder_->GetResource(1U, 2));
  EXPECT_EQ(2, holder_->GetResourceCount());

  // Setting a new index to nullptr shouldn't increase or decrease the count.
  holder_->SetResource(2U, 0, nullptr);
  EXPECT_TRUE(holder_->GetResource(2U, 0) == nullptr);
  EXPECT_EQ(2, holder_->GetResourceCount());

  // Setting an index larger than the inline group count should work.
  const size_t kLargeIndex = ResourceHolder::kInlineResourceGroups + 3U;
  holder_->SetResource(kLargeIndex, 3, resource3_.get());
  EXPECT_EQ(resource3_.get(), holder_->GetResource(kLargeIndex, 3));
  EXPECT_EQ(3, holder_->GetResourceCount());
  holder_->SetResource(kLargeIndex, 3, nullptr);
  EXPECT_TRUE(holder_->GetResource(kLargeIndex, 3) == nullptr);
  EXPECT_EQ(2, holder_->GetResourceCount());

  // Should decrease the count.
  holder_->SetResource(1U, 2, nullptr);
  EXPECT_TRUE(holder_->GetResource(1U, 2) == nullptr);
  EXPECT_EQ(1, holder_->GetResourceCount());

  // Should increase it again.
  holder_->SetResource(2U, 0, resource_.get());
  EXPECT_EQ(resource_.get(), holder_->GetResource(2U, 0));
  EXPECT_EQ(2, holder_->GetResourceCount());

  // Should decrease the count.
  holder_->SetResource(2U, 0, nullptr);
  EXPECT_TRUE(holder_->GetResource(2U, 0) == nullptr);
  EXPECT_EQ(1, holder_->GetResourceCount());

  // Remove the last one.
  holder_->SetResource(0U, 0, nullptr);
  EXPECT_TRUE(holder_->GetResource(0U, 0) == nullptr);
  EXPECT_TRUE(holder_->GetResource(1U, 0) == nullptr);
  EXPECT_TRUE(holder_->GetResource(2U, 0) == nullptr);
  EXPECT_EQ(0, holder_->GetResourceCount());
}

TEST_F(MockResourceTest, SetResourceNonzeroStart) {
  holder_.Reset(new MyHolder);

  // Start adding resources at non-zero indices and then remove.
  holder_->SetResource(2U, 0, resource_.get());
  EXPECT_EQ(resource_.get(), holder_->GetResource(2U, 0));
  EXPECT_EQ(1, holder_->GetResourceCount());

  holder_->SetResource(1U, 0, resource_.get());
  EXPECT_EQ(resource_.get(), holder_->GetResource(1U, 0));
  EXPECT_EQ(2, holder_->GetResourceCount());

  holder_->SetResource(1U, 0, nullptr);
  EXPECT_TRUE(holder_->GetResource(1U, 0) == nullptr);
  EXPECT_EQ(1, holder_->GetResourceCount());

  holder_->SetResource(2U, 0, nullptr);
  EXPECT_TRUE(holder_->GetResource(2U, 0) == nullptr);
  EXPECT_EQ(0, holder_->GetResourceCount());

  // Test setting multiple keys on the same index.
  holder_->SetResource(3U, 0, resource_.get());
  holder_->SetResource(3U, 2, resource2_.get());
  holder_->SetResource(3U, 3, resource3_.get());
  EXPECT_EQ(resource_.get(), holder_->GetResource(3U, 0));
  EXPECT_EQ(resource2_.get(), holder_->GetResource(3U, 2));
  EXPECT_EQ(resource3_.get(), holder_->GetResource(3U, 3));
  EXPECT_EQ(3, holder_->GetResourceCount());

  holder_->SetResource(3U, 2, nullptr);
  EXPECT_EQ(resource_.get(), holder_->GetResource(3U, 0));
  EXPECT_TRUE(holder_->GetResource(3U, 2) == nullptr);
  EXPECT_EQ(resource3_.get(), holder_->GetResource(3U, 3));
  EXPECT_EQ(2, holder_->GetResourceCount());

  holder_->SetResource(3U, 0, nullptr);
  EXPECT_TRUE(holder_->GetResource(3U, 0) == nullptr);
  EXPECT_TRUE(holder_->GetResource(3U, 2) == nullptr);
  EXPECT_EQ(resource3_.get(), holder_->GetResource(3U, 3));
  EXPECT_EQ(1, holder_->GetResourceCount());

  holder_->SetResource(3U, 3, nullptr);
  EXPECT_TRUE(holder_->GetResource(3U, 0) == nullptr);
  EXPECT_TRUE(holder_->GetResource(3U, 2) == nullptr);
  EXPECT_TRUE(holder_->GetResource(3U, 3) == nullptr);
  EXPECT_EQ(0, holder_->GetResourceCount());
}

}  // namespace gfx
}  // namespace ion
