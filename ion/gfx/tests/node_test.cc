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

#include "ion/gfx/node.h"

#include "ion/base/invalid.h"
#include "ion/gfx/attribute.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfx/shaderprogram.h"
#include "ion/gfx/shape.h"
#include "ion/gfx/statetable.h"
#include "ion/gfx/uniform.h"
#include "ion/math/vector.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfx {

TEST(NodeTest, SetLabel) {
  NodePtr node(new Node);

  // Check that the initial label is empty.
  EXPECT_TRUE(node->GetLabel().empty());

  node->SetLabel("myLabel");
  // Check that the label is set.
  EXPECT_EQ("myLabel", node->GetLabel());
}

TEST(NodeTest, SetStateTable) {
  NodePtr node(new Node);
  StateTablePtr ptr(new StateTable(400, 300));

  // Check that it is possible to set a StateTable.
  EXPECT_FALSE(node->GetStateTable());
  node->SetStateTable(ptr);
  EXPECT_EQ(ptr, node->GetStateTable());
}

TEST(NodeTest, SetShaderProgram) {
  NodePtr node(new Node);
  ShaderInputRegistryPtr registry(new ShaderInputRegistry());
  ShaderProgramPtr ptr(new ShaderProgram(registry));

  // Check that it is possible to set a ShaderProgram.
  EXPECT_FALSE(node->GetShaderProgram());
  node->SetShaderProgram(ptr);
  EXPECT_EQ(ptr, node->GetShaderProgram());
}

TEST(NodeTest, AddClearUniformBlocks) {
  NodePtr node(new Node);
  UniformBlockPtr ptr1(new UniformBlock());
  UniformBlockPtr ptr2(new UniformBlock());
  UniformBlockPtr ptr3(new UniformBlock());

  // Check that there are no UniformBlocks.
  EXPECT_EQ(0U, node->GetUniformBlocks().size());

  // Check that a UniformBlock can be added.
  node->AddUniformBlock(ptr1);
  EXPECT_EQ(1U, node->GetUniformBlocks().size());
  EXPECT_EQ(ptr1.Get(), node->GetUniformBlocks()[0].Get());
  node->AddUniformBlock(ptr2);
  EXPECT_EQ(2U, node->GetUniformBlocks().size());
  EXPECT_EQ(ptr1.Get(), node->GetUniformBlocks()[0].Get());
  EXPECT_EQ(ptr2.Get(), node->GetUniformBlocks()[1].Get());

  // Check UniformBlock replacement.
  node->ReplaceUniformBlock(2U, ptr3);  // No effect - bad index.
  EXPECT_EQ(2U, node->GetUniformBlocks().size());
  EXPECT_EQ(ptr1.Get(), node->GetUniformBlocks()[0].Get());
  EXPECT_EQ(ptr2.Get(), node->GetUniformBlocks()[1].Get());
  node->ReplaceUniformBlock(0U, UniformBlockPtr());  // No effect - NULL.
  EXPECT_EQ(2U, node->GetUniformBlocks().size());
  EXPECT_EQ(ptr1.Get(), node->GetUniformBlocks()[0].Get());
  EXPECT_EQ(ptr2.Get(), node->GetUniformBlocks()[1].Get());
  node->ReplaceUniformBlock(0U, ptr3);
  EXPECT_EQ(2U, node->GetUniformBlocks().size());
  EXPECT_EQ(ptr3.Get(), node->GetUniformBlocks()[0].Get());
  EXPECT_EQ(ptr2.Get(), node->GetUniformBlocks()[1].Get());
  node->ReplaceUniformBlock(1U, ptr1);
  EXPECT_EQ(2U, node->GetUniformBlocks().size());
  EXPECT_EQ(ptr3.Get(), node->GetUniformBlocks()[0].Get());
  EXPECT_EQ(ptr1.Get(), node->GetUniformBlocks()[1].Get());

  // Check that the UniformBlocks were cleared.
  node->ClearUniformBlocks();
  EXPECT_EQ(0U, node->GetUniformBlocks().size());
}

TEST(NodeTest, AddClearShapes) {
  NodePtr node(new Node);
  ShapePtr ptr1(new Shape());
  ShapePtr ptr2(new Shape());
  ShapePtr ptr3(new Shape());

  // Check that there are no shapes.
  EXPECT_EQ(0U, node->GetShapes().size());
  EXPECT_EQ(base::kInvalidIndex, node->AddShape(ShapePtr()));
  EXPECT_EQ(0U, node->GetShapes().size());

  // Check that a shape can be added.
  const size_t idx1 = node->AddShape(ptr1);
  EXPECT_EQ(0U, idx1);
  EXPECT_EQ(1U, node->GetShapes().size());
  EXPECT_EQ(ptr1.Get(), node->GetShapes()[0].Get());
  const size_t idx2 = node->AddShape(ptr2);
  EXPECT_EQ(1U, idx2);
  EXPECT_EQ(2U, node->GetShapes().size());
  EXPECT_EQ(ptr1.Get(), node->GetShapes()[0].Get());
  EXPECT_EQ(ptr2.Get(), node->GetShapes()[1].Get());

  // Check shape replacement.
  node->ReplaceShape(2U, ptr3);  // No effect - bad index.
  EXPECT_EQ(2U, node->GetShapes().size());
  EXPECT_EQ(ptr1.Get(), node->GetShapes()[0].Get());
  EXPECT_EQ(ptr2.Get(), node->GetShapes()[1].Get());
  node->ReplaceShape(0U, ShapePtr());  // No effect - NULL shape.
  EXPECT_EQ(2U, node->GetShapes().size());
  EXPECT_EQ(ptr1.Get(), node->GetShapes()[0].Get());
  EXPECT_EQ(ptr2.Get(), node->GetShapes()[1].Get());
  node->ReplaceShape(0U, ptr3);
  EXPECT_EQ(2U, node->GetShapes().size());
  EXPECT_EQ(ptr3.Get(), node->GetShapes()[0].Get());
  EXPECT_EQ(ptr2.Get(), node->GetShapes()[1].Get());
  node->ReplaceShape(1U, ptr1);
  EXPECT_EQ(2U, node->GetShapes().size());
  EXPECT_EQ(ptr3.Get(), node->GetShapes()[0].Get());
  EXPECT_EQ(ptr1.Get(), node->GetShapes()[1].Get());

  // Check shape removal.
  node->RemoveShape(ShapePtr());
  EXPECT_EQ(2U, node->GetShapes().size());
  EXPECT_EQ(ptr3.Get(), node->GetShapes()[0].Get());
  EXPECT_EQ(ptr1.Get(), node->GetShapes()[1].Get());
  node->RemoveShape(ptr2);
  EXPECT_EQ(2U, node->GetShapes().size());
  EXPECT_EQ(ptr3.Get(), node->GetShapes()[0].Get());
  EXPECT_EQ(ptr1.Get(), node->GetShapes()[1].Get());
  node->RemoveShape(ptr3);
  EXPECT_EQ(1U, node->GetShapes().size());
  EXPECT_EQ(ptr1.Get(), node->GetShapes()[0].Get());
  node->RemoveShape(ptr1);
  EXPECT_EQ(0U, node->GetShapes().size());
  node->AddShape(ptr1);
  node->AddShape(ptr2);
  node->AddShape(ptr3);
  EXPECT_EQ(3U, node->GetShapes().size());
  EXPECT_EQ(ptr1.Get(), node->GetShapes()[0].Get());
  EXPECT_EQ(ptr2.Get(), node->GetShapes()[1].Get());
  EXPECT_EQ(ptr3.Get(), node->GetShapes()[2].Get());
  node->RemoveShapeAt(3U);
  EXPECT_EQ(3U, node->GetShapes().size());
  node->RemoveShapeAt(1U);
  EXPECT_EQ(2U, node->GetShapes().size());
  EXPECT_EQ(ptr1.Get(), node->GetShapes()[0].Get());
  EXPECT_EQ(ptr3.Get(), node->GetShapes()[1].Get());
  node->RemoveShapeAt(0U);
  EXPECT_EQ(1U, node->GetShapes().size());
  EXPECT_EQ(ptr3.Get(), node->GetShapes()[0].Get());
  node->RemoveShapeAt(1U);
  EXPECT_EQ(1U, node->GetShapes().size());
  EXPECT_EQ(ptr3.Get(), node->GetShapes()[0].Get());
  node->RemoveShapeAt(0U);
  EXPECT_EQ(0U, node->GetShapes().size());

  // Check that shapes can be cleared.
  node->AddShape(ptr1);
  node->AddShape(ptr2);
  node->AddShape(ptr3);
  EXPECT_EQ(3U, node->GetShapes().size());
  node->ClearShapes();
  EXPECT_EQ(0U, node->GetShapes().size());
}

TEST(NodeTest, Children) {
  NodePtr node(new Node);
  NodePtr ptr1(new Node());
  NodePtr ptr2(new Node());
  NodePtr ptr3(new Node());

  // Check that there are no children.
  EXPECT_EQ(0U, node->GetChildren().size());
  EXPECT_EQ(base::kInvalidIndex, node->AddChild(NodePtr()));
  EXPECT_EQ(0U, node->GetChildren().size());

  // Check that a child can be added.
  const size_t idx1 = node->AddChild(ptr1);
  EXPECT_EQ(0U, idx1);
  EXPECT_EQ(1U, node->GetChildren().size());
  EXPECT_EQ(ptr1.Get(), node->GetChildren()[0].Get());
  const size_t idx2 = node->AddChild(ptr2);
  EXPECT_EQ(1U, idx2);
  EXPECT_EQ(2U, node->GetChildren().size());
  EXPECT_EQ(ptr1.Get(), node->GetChildren()[0].Get());
  EXPECT_EQ(ptr2.Get(), node->GetChildren()[1].Get());

  // Check child replacement.
  node->ReplaceChild(2U, ptr3);  // No effect - bad index.
  EXPECT_EQ(2U, node->GetChildren().size());
  EXPECT_EQ(ptr1.Get(), node->GetChildren()[0].Get());
  EXPECT_EQ(ptr2.Get(), node->GetChildren()[1].Get());
  node->ReplaceChild(0U, NodePtr());  // No effect - NULL node.
  EXPECT_EQ(2U, node->GetChildren().size());
  EXPECT_EQ(ptr1.Get(), node->GetChildren()[0].Get());
  EXPECT_EQ(ptr2.Get(), node->GetChildren()[1].Get());
  node->ReplaceChild(0U, ptr3);
  EXPECT_EQ(2U, node->GetChildren().size());
  EXPECT_EQ(ptr3.Get(), node->GetChildren()[0].Get());
  EXPECT_EQ(ptr2.Get(), node->GetChildren()[1].Get());
  node->ReplaceChild(1U, ptr1);
  EXPECT_EQ(2U, node->GetChildren().size());
  EXPECT_EQ(ptr3.Get(), node->GetChildren()[0].Get());
  EXPECT_EQ(ptr1.Get(), node->GetChildren()[1].Get());

  // Check child removal.
  node->RemoveChild(NodePtr());
  EXPECT_EQ(2U, node->GetChildren().size());
  EXPECT_EQ(ptr3.Get(), node->GetChildren()[0].Get());
  EXPECT_EQ(ptr1.Get(), node->GetChildren()[1].Get());
  node->RemoveChild(ptr2);
  EXPECT_EQ(2U, node->GetChildren().size());
  EXPECT_EQ(ptr3.Get(), node->GetChildren()[0].Get());
  EXPECT_EQ(ptr1.Get(), node->GetChildren()[1].Get());
  node->RemoveChild(ptr3);
  EXPECT_EQ(1U, node->GetChildren().size());
  EXPECT_EQ(ptr1.Get(), node->GetChildren()[0].Get());
  node->RemoveChild(ptr1);
  EXPECT_EQ(0U, node->GetChildren().size());
  node->AddChild(ptr1);
  node->AddChild(ptr2);
  node->AddChild(ptr3);
  EXPECT_EQ(3U, node->GetChildren().size());
  EXPECT_EQ(ptr1.Get(), node->GetChildren()[0].Get());
  EXPECT_EQ(ptr2.Get(), node->GetChildren()[1].Get());
  EXPECT_EQ(ptr3.Get(), node->GetChildren()[2].Get());
  node->RemoveChildAt(3U);
  EXPECT_EQ(3U, node->GetChildren().size());
  node->RemoveChildAt(1U);
  EXPECT_EQ(2U, node->GetChildren().size());
  EXPECT_EQ(ptr1.Get(), node->GetChildren()[0].Get());
  EXPECT_EQ(ptr3.Get(), node->GetChildren()[1].Get());
  node->RemoveChildAt(0U);
  EXPECT_EQ(1U, node->GetChildren().size());
  EXPECT_EQ(ptr3.Get(), node->GetChildren()[0].Get());
  node->RemoveChildAt(1U);
  EXPECT_EQ(1U, node->GetChildren().size());
  EXPECT_EQ(ptr3.Get(), node->GetChildren()[0].Get());
  node->RemoveChildAt(0U);
  EXPECT_EQ(0U, node->GetChildren().size());

  // Check that children can be cleared.
  node->AddChild(ptr1);
  node->AddChild(ptr2);
  node->AddChild(ptr3);
  node->ClearChildren();
  EXPECT_EQ(0U, node->GetChildren().size());
}

}  // namespace gfx
}  // namespace ion
