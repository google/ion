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

#ifndef ION_GFX_SHAPE_H_
#define ION_GFX_SHAPE_H_

#include <string>

#include "ion/base/referent.h"
#include "ion/gfx/attributearray.h"
#include "ion/gfx/indexbuffer.h"
#include "ion/math/range.h"

namespace ion {
namespace gfx {

// A Shape object represents a shape (vertices + indices) to draw.
class ION_API Shape : public base::Referent {
 public:
  // Supported primitive types.
  enum PrimitiveType {
    kLines,
    kLineLoop,
    kLineStrip,
    kPoints,
    kTriangles,  // Default.
    kTriangleFan,
    kTriangleStrip,
    kPatches,
  };

  Shape();

  // Creates a shallow copy of the shape that shares the same vertex attribute
  // buffer data and index buffer data.
  Shape(const Shape& from);

  // Returns/sets the label of this.
  const std::string& GetLabel() const { return label_; }
  void SetLabel(const std::string& label) { label_ = label; }

  // Sets/returns the type of primitive to draw.
  void SetPrimitiveType(PrimitiveType type) { primitive_type_ = type; }
  PrimitiveType GetPrimitiveType() const { return primitive_type_; }

  // Sets/returns the vertices used to create the primitives.
  void SetAttributeArray(const AttributeArrayPtr& attribute_array) {
    attribute_array_ = attribute_array;
  }
  const AttributeArrayPtr& GetAttributeArray() const {
    return attribute_array_;
  }

  // Sets/returns the index buffer. A NULL index buffer (the default) signifies
  // that the vertices of the shape are not indexed.
  void SetIndexBuffer(const IndexBufferPtr& index_buffer) {
    index_buffer_ = index_buffer;
  }
  const IndexBufferPtr& GetIndexBuffer() const {
    return index_buffer_;
  }

  // By default, a Shape draws all of its vertices (if there is no IndexBuffer)
  // or all of its indexed vertices (if there is an IndexBuffer). This function
  // modifies this behavior by adding a vertex range [min, max) to the
  // Shape. The vertex range applies to the indices in the IndexBuffer if there
  // is one; otherwise it applies to the vertices themselves. The new range is
  // enabled by default. If the range is empty, a warning is printed and the
  // range is not added. Returns the index of the added range, or
  // base::kInvalidIndex if the range was not added.
  size_t AddVertexRange(const math::Range1i& range);
  // Modifies the specified vertex range if the passed index is valid (i.e.,
  // there is a range at i that has been added with AddVertexRange());
  // otherwise this logs an error message.
  void SetVertexRange(size_t i, const math::Range1i& range);
  // Removes all ranges from the Shape.
  void ClearVertexRanges() { vertex_ranges_.clear(); }
  // Returns the i-th vertex range. Returns an empty range if the index is not
  // valid.
  const math::Range1i GetVertexRange(size_t i) const;
  // Returns the number of vertex ranges in the Shape.
  size_t GetVertexRangeCount() const { return vertex_ranges_.size(); }
  // Enables or disables a specific vertex range. Does nothing if i does not
  // refer to a valid range.
  void EnableVertexRange(size_t i, bool enable);
  // Returns whether the i-th vertex range is enabled, or false if i does not
  // refer to a valid range.
  bool IsVertexRangeEnabled(size_t i) const;

  // Sets the instance count of the shape. If non-zero value is set, the shape
  // will be drawn multiple times using the instanced drawing functions. The
  // number of instances drawn depends on what instance_count is set to. Zero
  // means instanced drawing is disabled and the shape will be drawn with the
  // regular drawing functions.
  void SetInstanceCount(int count) { instance_count_ = count; }
  // Returns the instance count that the shape is set to.
  int GetInstanceCount() const { return instance_count_; }

  // Sets/Gets the instance count of vertex range. If non-zero value is set, the
  // vertex range will be drawn multiple times using the instanced drawing
  // functions. The number of instances drawn depends on what instance_count is
  // set to. Zero means instanced drawing is disabled and the vertex range will
  // be drawn with the regular drawing functions.
  void SetVertexRangeInstanceCount(size_t i, int instance_count);
  // Returns the instance count that the vertex range is set to.
  int GetVertexRangeInstanceCount(size_t i) const;

  void SetPatchVertices(int count) { patch_vertices_ = count; }
  int GetPatchVertices() const { return patch_vertices_; }

 protected:
  // The destructor is protected because all base::Referent classes must have
  // protected or private destructors.
  ~Shape() override;

 private:
  // A VertexRange represents a range of vertices or vertex indices (depending
  // on whether or not the Shape has an IndexBuffer). Each VertexRange can be
  // enabled or disabled individually.
  struct VertexRange {
    VertexRange() : is_enabled(false), instance_count(0) {}  // For STL only.
    VertexRange(const math::Range1i& range_in, bool is_enabled_in,
                int instance_count_in)
        : range(range_in),
          is_enabled(is_enabled_in),
          instance_count(instance_count_in) {}
    math::Range1i range;
    bool is_enabled;
    // The number of instances of the vertex range to be rendered. Both 0 and 1
    // draws a single instance but with different methods. If instance_count
    // is 1, the vertex range is drawn with instanced functions.
    // If instance_count is 0, the vertex range is drawn with regular functions.
    int instance_count;
  };

  // Returns whether a vertex range index is valid and logs an error message if
  // it is not.
  bool CheckRangeIndex(size_t i, const char* name) const;

  // Type of primitives represented by the shape.
  PrimitiveType primitive_type_;
  // Attributes for the shape.
  AttributeArrayPtr attribute_array_;
  // Indices used to draw the primitives of the shape. If this is NULL then all
  // of the vertices in the shape will be drawn sequentially.
  IndexBufferPtr index_buffer_;
  // Ranges of vertices or indices to draw. An empty vector means draw them all.
  base::AllocVector<VertexRange> vertex_ranges_;
  // The number of instances of the shape to be rendered. Both 0 and 1 draws a
  // single instance but with different methods. If instance_count_ is 1,
  // the shape is drawn with instanced functions. If instance_count_ is 0,
  // the shape is drawn with regular functions. Note that this value will be
  // ignored if vertex range is enabled.
  int instance_count_;
  // Number of vertex per patch this shape contains. Only used when primitive
  // type is kPatches. Default value is 3.
  int patch_vertices_;
  // An identifying name for this Shape that can appear in debug streams and
  // printouts of a scene.
  std::string label_;
};

// Convenience typedef for shared pointer to a Shape.
using ShapePtr = base::SharedPtr<Shape>;

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_SHAPE_H_
