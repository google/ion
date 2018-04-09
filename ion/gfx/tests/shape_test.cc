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

#include "ion/gfx/shape.h"

#include "ion/base/invalid.h"
#include "ion/base/logchecker.h"
#include "ion/gfx/attributearray.h"
#include "ion/gfx/indexbuffer.h"
#include "ion/math/range.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfx {

TEST(ShapeTest, SetLabel) {
  ShapePtr shape(new Shape);

  // Check that the initial label is empty.
  EXPECT_TRUE(shape->GetLabel().empty());

  shape->SetLabel("myLabel");
  // Check that the label is set.
  EXPECT_EQ("myLabel", shape->GetLabel());
}

TEST(ShapeTest, SetPrimitiveType) {
  ShapePtr shape(new Shape);

  // Check that default value is correct.
  EXPECT_EQ(Shape::kTriangles, shape->GetPrimitiveType());

  // Check that the value can change.
  shape->SetPrimitiveType(Shape::kLines);
  EXPECT_EQ(Shape::kLines, shape->GetPrimitiveType());
  shape->SetPrimitiveType(Shape::kPoints);
  EXPECT_EQ(Shape::kPoints, shape->GetPrimitiveType());
}

TEST(ShapeTest, SetAttributeArray) {
  ShapePtr shape(new Shape);
  AttributeArrayPtr ptr(new AttributeArray());

  // Check that it is possible to set a AttributeArray.
  EXPECT_EQ(nullptr, shape->GetAttributeArray().Get());
  shape->SetAttributeArray(ptr);
  EXPECT_EQ(ptr.Get(), shape->GetAttributeArray().Get());
}

TEST(ShapeTest, SetIndexBuffer) {
  ShapePtr shape(new Shape);
  IndexBufferPtr ptr(new IndexBuffer());

  // Check that it is possible to set a IndexBuffer.
  EXPECT_EQ(nullptr, shape->GetIndexBuffer().Get());
  shape->SetIndexBuffer(ptr);
  EXPECT_EQ(ptr.Get(), shape->GetIndexBuffer().Get());
}

TEST(ShapeTest, SetInstanceCount) {
  ShapePtr shape(new Shape);

  EXPECT_EQ(0, shape->GetInstanceCount());

  // Check that it is possible to set the prim count;
  shape->SetInstanceCount(1);
  EXPECT_EQ(1, shape->GetInstanceCount());
}

TEST(ShapeTest, AddSetAndEnableVertexRanges) {
  ShapePtr shape(new Shape);
  base::LogChecker log_checker;

  // Check defaults.
  EXPECT_EQ(0U, shape->GetVertexRangeCount());

  // Try to add an invalid range.
  EXPECT_EQ(base::kInvalidIndex, shape->AddVertexRange(math::Range1i()));
  EXPECT_EQ(0U, shape->GetVertexRangeCount());
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "Ignoring empty range"));

  // Add valid ranges
  math::Range1i r0(0, 10);
  math::Range1i r1(8, 20);
  math::Range1i r3(2, 4);
  EXPECT_EQ(0U, shape->AddVertexRange(r0));
  EXPECT_EQ(1U, shape->GetVertexRangeCount());
  EXPECT_EQ(1U, shape->AddVertexRange(r1));
  EXPECT_EQ(2U, shape->GetVertexRangeCount());
  EXPECT_EQ(0, shape->GetVertexRangeInstanceCount(0));
  EXPECT_EQ(0, shape->GetVertexRangeInstanceCount(1));
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // Check the ranges.
  EXPECT_EQ(r0, shape->GetVertexRange(0));
  EXPECT_EQ(r1, shape->GetVertexRange(1));
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // Check that you can change the range prim count.
  shape->SetVertexRangeInstanceCount(3, 5);
  // range index is wrong. Nothing gets changed.
  EXPECT_EQ(0, shape->GetVertexRangeInstanceCount(0));
  EXPECT_EQ(0, shape->GetVertexRangeInstanceCount(1));
  EXPECT_EQ(0, shape->GetVertexRangeInstanceCount(3));
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "Out of bounds index"));

  shape->SetVertexRangeInstanceCount(0, 1);
  shape->SetVertexRangeInstanceCount(1, 2);
  EXPECT_EQ(1, shape->GetVertexRangeInstanceCount(0));
  EXPECT_EQ(2, shape->GetVertexRangeInstanceCount(1));
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // Check that you can set a range.
  shape->SetVertexRange(0, r3);
  EXPECT_EQ(r3, shape->GetVertexRange(0));
  EXPECT_EQ(r1, shape->GetVertexRange(1));
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // Check that setting an invalid index generates an error message.
  shape->SetVertexRange(3, r3);
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "Out of bounds index"));
  EXPECT_EQ(r3, shape->GetVertexRange(0));
  EXPECT_EQ(r1, shape->GetVertexRange(1));

  // Check that setting an empty range generates an error message.
  shape->SetVertexRange(1, math::Range1i());
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "Ignoring empty range"));
  EXPECT_EQ(r3, shape->GetVertexRange(0));
  EXPECT_EQ(r1, shape->GetVertexRange(1));

  // Check that you cannot get an invalid range.
  EXPECT_EQ(math::Range1i(), shape->GetVertexRange(2));
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "Out of bounds index"));

  // Enable and disable ranges.
  EXPECT_TRUE(shape->IsVertexRangeEnabled(0));
  EXPECT_TRUE(shape->IsVertexRangeEnabled(1));

  shape->EnableVertexRange(0, false);
  EXPECT_FALSE(shape->IsVertexRangeEnabled(0));
  EXPECT_TRUE(shape->IsVertexRangeEnabled(1));

  // Check that you cannot alter invalid indices.
  shape->EnableVertexRange(2, false);
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "Out of bounds index"));
  EXPECT_FALSE(shape->IsVertexRangeEnabled(0));
  EXPECT_TRUE(shape->IsVertexRangeEnabled(1));
  shape->EnableVertexRange(2, true);
  EXPECT_FALSE(shape->IsVertexRangeEnabled(0));
  EXPECT_TRUE(shape->IsVertexRangeEnabled(1));
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "Out of bounds index"));
  EXPECT_FALSE(shape->IsVertexRangeEnabled(2));
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "Out of bounds index"));

  shape->EnableVertexRange(1, false);
  EXPECT_FALSE(shape->IsVertexRangeEnabled(0));
  EXPECT_FALSE(shape->IsVertexRangeEnabled(1));

  shape->EnableVertexRange(0, true);
  EXPECT_TRUE(shape->IsVertexRangeEnabled(0));
  EXPECT_FALSE(shape->IsVertexRangeEnabled(1));

  shape->EnableVertexRange(1, true);
  EXPECT_TRUE(shape->IsVertexRangeEnabled(0));
  EXPECT_TRUE(shape->IsVertexRangeEnabled(1));

  // Clear the ranges.
  shape->ClearVertexRanges();
  EXPECT_EQ(0U, shape->GetVertexRangeCount());
  EXPECT_EQ(math::Range1i(), shape->GetVertexRange(0));
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "Out of bounds index"));
}

TEST(ShapeTest, CopyConstructor) {
  ShapePtr nshape;
  AttributeArrayPtr aptr(new AttributeArray());
  IndexBufferPtr iptr(new IndexBuffer());
  math::Range1i r0(0, 10);
  {
    ShapePtr oshape(new Shape);
    // Scope the original shape's lifetime so that we don't accidentally refer
    // to it when verifying the cloned data in the new shape.
    oshape->SetLabel("myLabel");
    oshape->SetPrimitiveType(Shape::kLines);
    oshape->SetAttributeArray(aptr);
    oshape->SetIndexBuffer(iptr);
    oshape->SetInstanceCount(33);
    oshape->AddVertexRange(r0);
    oshape->SetVertexRangeInstanceCount(0, 1);
    nshape.Reset(new Shape(*oshape));
  }
  EXPECT_EQ("myLabel", nshape->GetLabel());
  EXPECT_EQ(Shape::kLines, nshape->GetPrimitiveType());
  EXPECT_EQ(aptr.Get(), nshape->GetAttributeArray().Get());
  EXPECT_EQ(iptr.Get(), nshape->GetIndexBuffer().Get());
  EXPECT_EQ(33, nshape->GetInstanceCount());
  EXPECT_EQ(r0, nshape->GetVertexRange(0));
  EXPECT_EQ(1, nshape->GetVertexRangeInstanceCount(0));
}

}  // namespace gfx
}  // namespace ion
