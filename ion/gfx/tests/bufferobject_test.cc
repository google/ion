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

#include "ion/gfx/bufferobject.h"

#include <memory>

#include "ion/base/datacontainer.h"
#include "ion/base/invalid.h"
#include "ion/base/logchecker.h"
#include "ion/gfx/tests/mockresource.h"
#include "ion/math/range.h"
#include "ion/math/vector.h"
#include "ion/port/nullptr.h"  // For kNullFunction.

#include "absl/memory/memory.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfx {

namespace {

typedef testing::MockResource<
    BufferObject::kNumChanges> MockBufferObjectResource;

class BufferObjectTest : public ::testing::Test {
 protected:
  void SetUp() override {
    bo_.Reset(new BufferObject);
    resource_ = absl::make_unique<MockBufferObjectResource>();
    EXPECT_FALSE(resource_->AnyModifiedBitsSet());
    bo_->SetResource(0U, 0, resource_.get());
    EXPECT_EQ(resource_.get(), bo_->GetResource(0U, 0));
    EXPECT_TRUE(resource_->AnyModifiedBitsSet());
    resource_->ResetModifiedBits();
    EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  }

  // This is to ensure that the resource holder goes away before the resource.
  void TearDown() override { bo_.Reset(nullptr); }

  BufferObjectPtr bo_;
  std::unique_ptr<MockBufferObjectResource> resource_;
};

// Vertex structure for testing.
struct MyVertex {
  float f;
  math::Vector3f f3;
};

static void DeleteVertexData(void* data) {
  delete [] reinterpret_cast<MyVertex*>(data);
}

}  // anonymous namespace

TEST_F(BufferObjectTest, AddSpecs) {
  base::LogChecker log_checker;

  // Check that there are no Specs.
  EXPECT_EQ(0U, bo_->GetSpecCount());

  // Check that an Element can be described.
  size_t index0 = bo_->AddSpec(BufferObject::kByte, 2U, 0U);
  EXPECT_EQ(0U, index0);
  EXPECT_EQ(1U, bo_->GetSpecCount());
  {
    const BufferObject::Spec& spec = bo_->GetSpec(0);
    EXPECT_FALSE(base::IsInvalidReference(spec));
    EXPECT_EQ(BufferObject::kByte, spec.type);
    EXPECT_EQ(2U, spec.component_count);
    EXPECT_EQ(0U, spec.byte_offset);
    // Check that adding a spec with the same parameters as an existing spec
    // has no effect and returns the original index.
    size_t index1 = bo_->AddSpec(BufferObject::kByte, 2U, 0U);
    EXPECT_EQ(index0, index1);
    EXPECT_EQ(1U, bo_->GetSpecCount());
  }

  {
    // Check that another Spec can be described.
    size_t index1 = bo_->AddSpec(BufferObject::kFloat, 4U, 2U);
    EXPECT_EQ(1U, index1);
    EXPECT_EQ(2U, bo_->GetSpecCount());
    const BufferObject::Spec& spec = bo_->GetSpec(1);
    EXPECT_FALSE(base::IsInvalidReference(spec));
    EXPECT_EQ(BufferObject::kFloat, spec.type);
    EXPECT_EQ(4U, spec.component_count);
    EXPECT_EQ(2U, spec.byte_offset);

    // Check that adding a spec with the same parameters as an existing spec
    // has no effect and returns the original index.
    size_t index2 = bo_->AddSpec(BufferObject::kFloat, 4U, 2U);
    EXPECT_EQ(index1, index2);
    EXPECT_EQ(2U, bo_->GetSpecCount());
  }

  // Check that an invalid index returns an invalid reference.
  EXPECT_TRUE(base::IsInvalidReference(bo_->GetSpec(2)));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Invalid element index"));

  // Check that creating an element with more than 4 components fails.
  EXPECT_EQ(base::kInvalidIndex, bo_->AddSpec(BufferObject::kFloat, 5U, 4U));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "no more than four components"));

  {
    // Check that the original Element is unchanged.
    const BufferObject::Spec& spec = bo_->GetSpec(0);
    EXPECT_FALSE(base::IsInvalidReference(spec));
    EXPECT_EQ(BufferObject::kByte, spec.type);
    EXPECT_EQ(2U, spec.component_count);
    EXPECT_EQ(0U, spec.byte_offset);
  }

  // Check that no bits have been set.
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST_F(BufferObjectTest, SetData) {
  base::LogChecker log_checker;

  // Create vertices.
  const size_t vertex_count = 16U;
  MyVertex* vertices = new MyVertex[vertex_count];
  for (size_t i = 0; i < vertex_count; ++i) {
    const float f = static_cast<float>(i);
    vertices[i].f = f;
    vertices[i].f3.Set(f, f + 1.f, f + 2.f);
  }

  // Check initial state.
  EXPECT_TRUE(bo_->GetData().Get() == nullptr);
  EXPECT_EQ(0U, bo_->GetStructSize());
  EXPECT_EQ(0U, bo_->GetCount());
  EXPECT_EQ(BufferObject::kArrayBuffer, bo_->GetInitialTarget());
  EXPECT_EQ(BufferObject::kStaticDraw, bo_->GetUsageMode());

  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  // Set the vertex data.
  base::DataContainerPtr data(base::DataContainer::Create<MyVertex>(
      vertices, DeleteVertexData, false, bo_->GetAllocator()));
  bo_->SetData(
      data, sizeof(MyVertex), vertex_count, BufferObject::kStreamDraw);
  // Check that the data has been flagged as changed.
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(BufferObject::kDataChanged));

  // Check everything was set correctly.
  EXPECT_EQ(vertices, bo_->GetData()->GetData());
  EXPECT_EQ(sizeof(MyVertex), bo_->GetStructSize());
  EXPECT_EQ(vertex_count, bo_->GetCount());
  EXPECT_EQ(BufferObject::kStreamDraw, bo_->GetUsageMode());

  // Check that the data is valid.
  for (size_t i = 0; i < vertex_count; ++i) {
    const float f = static_cast<float>(i);
    EXPECT_EQ(f, vertices[i].f);
    EXPECT_EQ(math::Vector3f(f, f + 1.f, f + 2.f), vertices[i].f3);
  }

  // Check that no other bits have changed.
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(BufferObject::kDataChanged));

  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST_F(BufferObjectTest, ModifyRanges) {
  // Create vertices.
  const size_t vertex_count = 16U;
  MyVertex* vertices = new MyVertex[vertex_count];
  for (size_t i = 0; i < vertex_count; ++i) {
    const float f = static_cast<float>(i);
    vertices[i].f = f;
    vertices[i].f3.Set(f, f + 1.f, f + 2.f);
  }

  // Check that no bits have changed.
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  // Set the vertex data.
  base::DataContainerPtr data(base::DataContainer::Create<MyVertex>(
      vertices, DeleteVertexData, false, bo_->GetAllocator()));
  bo_->SetData(
      data, sizeof(MyVertex), vertex_count, BufferObject::kStreamDraw);
  // Check that the data has been flagged as changed.
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(BufferObject::kDataChanged));
  resource_->ResetModifiedBits();

  // Modify some of the data directly.
  vertices = data->GetMutableData<MyVertex>();
  for (size_t i = 2; i < 4; ++i) {
    const float f = static_cast<float>(i);
    vertices[i].f = f + 1.f;
    vertices[i].f3.Set(f + 1.f, f + 2.f, f + 3.f);
  }
  for (size_t i = 8; i < 14; ++i) {
    const float f = static_cast<float>(i);
    vertices[i].f = f + 1.f;
    vertices[i].f3.Set(f + 1.f, f + 2.f, f + 3.f);
  }

  base::DataContainerPtr data_null(base::DataContainer::Create<MyVertex>(
      nullptr, kNullFunction, false, bo_->GetAllocator()));
  base::DataContainerPtr data4(base::DataContainer::Create<MyVertex>(
      &vertices[4], kNullFunction, false, bo_->GetAllocator()));
  base::DataContainerPtr data8(base::DataContainer::Create<MyVertex>(
      &vertices[8], kNullFunction, false, bo_->GetAllocator()));
  // Check that the proper bit is set when adding a subdata range.
  resource_->ResetModifiedBits();
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  bo_->SetSubData(
      math::Range1ui(static_cast<uint32>(bo_->GetStructSize() * 2U),
                     static_cast<uint32>(bo_->GetStructSize() * 4U)),
      data4);
  bo_->SetSubData(
      math::Range1ui(static_cast<uint32>(bo_->GetStructSize() * 8U),
                     static_cast<uint32>(bo_->GetStructSize() * 14U)),
      data8);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(BufferObject::kSubDataChanged));
  resource_->ResetModifiedBits();

  const base::AllocVector<BufferObject::BufferSubData>& sub_data =
      bo_->GetSubData();
  EXPECT_EQ(2U, sub_data.size());
  EXPECT_EQ(math::Range1ui(static_cast<uint32>(bo_->GetStructSize() * 2U),
                           static_cast<uint32>(bo_->GetStructSize() * 4U)),
            sub_data[0].range);
  EXPECT_EQ(data4.Get(), sub_data[0].data.Get());
  EXPECT_EQ(math::Range1ui(static_cast<uint32>(bo_->GetStructSize() * 8U),
                           static_cast<uint32>(bo_->GetStructSize() * 14U)),
            sub_data[1].range);
  EXPECT_EQ(data8.Get(), sub_data[1].data.Get());

  // Clearing the sub data does not set a bit.
  bo_->ClearSubData();
  EXPECT_EQ(0U, sub_data.size());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  // Empty ranges do nothing.
  bo_->SetSubData(math::Range1ui(), data4);
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  bo_->SetSubData(math::Range1ui(10, 9), data4);
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  // An empty container does nothing.
  bo_->SetSubData(math::Range1ui(0, 10), base::DataContainerPtr());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  bo_->SetSubData(math::Range1ui(0, 10), data_null);
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  // Expect CopySubData to set bit.
  bo_->CopySubData(
      bo_,
      math::Range1ui(static_cast<uint32>(bo_->GetStructSize() * 8U),
                     static_cast<uint32>(bo_->GetStructSize() * 14U)),
      static_cast<uint32>(bo_->GetStructSize()));
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(BufferObject::kSubDataChanged));
  resource_->ResetModifiedBits();
  EXPECT_EQ(1U, sub_data.size());
  EXPECT_EQ(math::Range1ui(static_cast<uint32>(bo_->GetStructSize() * 8U),
                           static_cast<uint32>(bo_->GetStructSize() * 14U)),
            sub_data[0].range);
  EXPECT_EQ(static_cast<uint32>(bo_->GetStructSize()), sub_data[0].read_offset);

  // Empty ranges do nothing.
  bo_->CopySubData(bo_, math::Range1ui(), 0);
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  bo_->CopySubData(bo_, math::Range1ui(10, 9), 0);
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  // NULL source BufferObject does nothing.
  bo_->CopySubData(BufferObjectPtr(), math::Range1ui(1, 2), 0);
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
}

TEST_F(BufferObjectTest, MappedData) {
  EXPECT_TRUE(bo_->GetMappedData().range.IsEmpty());
  EXPECT_TRUE(bo_->GetMappedPointer() == nullptr);
  void* data = bo_.Get();
  bo_->SetMappedData(math::Range1ui(10, 1000), data,
                     BufferObject::MappedBufferData::kGpuMapped, false);
  EXPECT_EQ(math::Range1ui(10, 1000), bo_->GetMappedData().range);
  EXPECT_EQ(BufferObject::MappedBufferData::kGpuMapped,
            bo_->GetMappedData().data_source);
  EXPECT_EQ(data, bo_->GetMappedPointer());
  EXPECT_FALSE(bo_->GetMappedData().read_only);

  data = resource_.get();
  bo_->SetMappedData(math::Range1ui(), data,
                     BufferObject::MappedBufferData::kAllocated, true);
  EXPECT_TRUE(bo_->GetMappedData().range.IsEmpty());
  EXPECT_EQ(data, bo_->GetMappedPointer());
  EXPECT_EQ(BufferObject::MappedBufferData::kAllocated,
            bo_->GetMappedData().data_source);
  EXPECT_TRUE(bo_->GetMappedData().read_only);

  bo_->SetMappedData(math::Range1ui(), nullptr,
                     BufferObject::MappedBufferData::kAllocated, true);
  EXPECT_TRUE(bo_->GetMappedData().range.IsEmpty());
  EXPECT_TRUE(bo_->GetMappedPointer() == nullptr);
  EXPECT_EQ(BufferObject::MappedBufferData::kAllocated,
            bo_->GetMappedData().data_source);
  EXPECT_TRUE(bo_->GetMappedData().read_only);
}

TEST_F(BufferObjectTest, Notifications) {
  const size_t vertex_count = 16U;
  MyVertex* vertices = new MyVertex[vertex_count];
  MyVertex* vertices2 = new MyVertex[vertex_count];
  for (size_t i = 0; i < vertex_count; ++i) {
    const float f = static_cast<float>(i);
    vertices[i].f = f;
    vertices[i].f3.Set(f, f + 1.f, f + 2.f);
    vertices2[i].f = f;
    vertices2[i].f3.Set(f, f + 1.f, f + 2.f);
  }

  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  // Set the vertex data.
  base::DataContainerPtr data(base::DataContainer::Create<MyVertex>(
      vertices, DeleteVertexData, false, bo_->GetAllocator()));
  bo_->SetData(
      data, sizeof(MyVertex), vertex_count, BufferObject::kStreamDraw);
  EXPECT_EQ(1U, data->GetReceiverCount());
  // Check that the data has changed.
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(BufferObject::kDataChanged));
  resource_->ResetModifiedBits();

  // Modify the container.
  data->GetMutableData<void*>();
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(BufferObject::kDataChanged));
  resource_->ResetModifiedBits();

  base::DataContainerPtr data2(base::DataContainer::Create<MyVertex>(
      vertices2, DeleteVertexData, false, bo_->GetAllocator()));

  // Unlink the buffer from the first DataContainer.
  bo_->SetData(
      data2, sizeof(MyVertex), vertex_count, BufferObject::kStreamDraw);
  EXPECT_EQ(0U, data->GetReceiverCount());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(BufferObject::kDataChanged));
  resource_->ResetModifiedBits();

  data->GetMutableData<void*>();
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  data2->GetMutableData<void*>();
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(BufferObject::kDataChanged));
  resource_->ResetModifiedBits();

  bo_.Reset();
  EXPECT_EQ(0U, data->GetReceiverCount());
}

}  // namespace gfx
}  // namespace ion
