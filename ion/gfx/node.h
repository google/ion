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

#ifndef ION_GFX_NODE_H_
#define ION_GFX_NODE_H_

#include "ion/base/invalid.h"
#include "ion/base/referent.h"
#include "ion/base/stlalloc/allocvector.h"
#include "ion/gfx/shaderprogram.h"
#include "ion/gfx/shape.h"
#include "ion/gfx/statetable.h"
#include "ion/gfx/uniformblock.h"

namespace ion {
namespace gfx {

// Convenience typedef for shared pointer to a Node.
class Node;
using NodePtr = base::SharedPtr<Node>;

// A Node instance represents a node in a scene graph. It can have any or all
// of the following:
//   - Shapes to draw.
//   - A shader program to apply to all shapes in the node's subgraph.
//   - Uniform variables (including textures) used by shaders in the
//     node's subgraph.
//   - UniformBlocks containing Uniforms. These are sent _after_ the uniforms
//     above.
//   - Child nodes.
class ION_API Node : public base::Referent, public UniformHolder {
 public:
  Node();

  // Returns/sets the label of this.
  const std::string& GetLabel() const { return label_; }
  void SetLabel(const std::string& label) { label_ = label; }

  // StateTable management.
  void SetStateTable(const StateTablePtr& state_table) {
    state_table_ = state_table;
  }
  const StateTablePtr& GetStateTable() const { return state_table_; }

  // Shader program management.
  void SetShaderProgram(const ShaderProgramPtr& shader_program) {
    shader_program_ = shader_program;
  }
  const ShaderProgramPtr& GetShaderProgram() const { return shader_program_; }

  // UniformBlock management. NULL blocks are not added, and
  // ReplaceUniformBlock() does nothing if the index is invalid.
  void AddUniformBlock(const UniformBlockPtr& block) {
    if (block.Get())
      uniform_blocks_.push_back(block);
  }
  void ReplaceUniformBlock(size_t index, const UniformBlockPtr& block) {
    if (index < uniform_blocks_.size() && block.Get())
      uniform_blocks_[index] = block;
  }
  void ClearUniformBlocks() { uniform_blocks_.clear(); }
  const base::AllocVector<UniformBlockPtr>& GetUniformBlocks() const {
    return uniform_blocks_;
  }

  // Child node management.  NULL children are not added, and ReplaceChild()
  // Shape management.  NULL shapes are not added, and ReplaceShape() does
  // nothing if the index is invalid. AddShape returns the index of the Shape
  // added, or base::kInvaidIndex if the Shape is NULL. Note that the index may
  // change if RemoveShape[At]() is called.
  size_t AddShape(const ShapePtr& shape) {
    size_t index = base::kInvalidIndex;
    if (shape.Get()) {
      index = shapes_.size();
      shapes_.push_back(shape);
    }
    return index;
  }
  void ReplaceShape(size_t index, const ShapePtr& shape) {
    if (index < shapes_.size() && shape.Get())
      shapes_[index] = shape;
  }
  // Removes all instances of the shape if it is contained in this' shapes. Note
  // that this is not an efficient operation if this contains many Shapes.
  void RemoveShape(const ShapePtr& shape) {
    for (auto it = shapes_.begin(); it != shapes_.end();) {
      if (*it == shape)
        it = shapes_.erase(it);
      else
        ++it;
    }
  }
  // Removes the Shape at the passed index if the index is valid. Note that this
  // is not an efficient operation if this contains many Shapes.
  void RemoveShapeAt(size_t index) {
    if (index < shapes_.size()) {
      auto it = shapes_.begin() + static_cast<std::ptrdiff_t>(index);
      shapes_.erase(it);
    }
  }
  void ClearShapes() { shapes_.clear(); }
  const base::AllocVector<ShapePtr>& GetShapes() const { return shapes_; }

  // Child node management.  NULL children are not added, and ReplaceChild()
  // does nothing if the index is invalid. AddChild() returns the index of the
  // child added, or base::kInvalidIndex if the child is NULL. Note that the
  // index may change after a call to RemoveChild[At]().
  size_t AddChild(const NodePtr& child) {
    size_t index = base::kInvalidIndex;
    if (child.Get()) {
      index = children_.size();
      children_.push_back(child);
    }
    return index;
  }
  void ReplaceChild(size_t index, const NodePtr& child) {
    if (index < children_.size() && child.Get())
      children_[index] = child;
  }
  // Removes all instances of child from this' children if it is actually a
  // child of this. Note that this is not an efficient operation if there are
  // many children.
  void RemoveChild(const NodePtr& child) {
    for (auto it = children_.begin(); it != children_.end();) {
      if (*it == child)
        it = children_.erase(it);
      else
        ++it;
    }
  }
  // Removes the child Node at the passed index if the index is valid. Note that
  // this is not an efficient operation if there are many children.
  void RemoveChildAt(size_t index) {
    if (index < children_.size()) {
      auto it = children_.begin() + static_cast<std::ptrdiff_t>(index);
      children_.erase(it);
    }
  }
  void ClearChildren() { children_.clear(); }
  const base::AllocVector<NodePtr>& GetChildren() const { return children_; }

 protected:
  // The destructor is protected because all base::Referent classes must have
  // protected or private destructors.
  ~Node() override;

 private:
  StateTablePtr state_table_;
  ShaderProgramPtr shader_program_;
  base::AllocVector<ShapePtr> shapes_;
  base::AllocVector<NodePtr> children_;
  base::AllocVector<UniformBlockPtr> uniform_blocks_;
  // An identifying name for this Node that can appear in debug streams and
  // printouts of a scene.
  std::string label_;
};

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_NODE_H_
