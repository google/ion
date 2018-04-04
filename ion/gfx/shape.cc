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

#include "ion/base/enumhelper.h"
#include "ion/base/invalid.h"
#include "ion/base/static_assert.h"
#include "ion/portgfx/glheaders.h"
#include "absl/base/macros.h"

namespace ion {
namespace gfx {

Shape::Shape()
    : primitive_type_(kTriangles),
      vertex_ranges_(*this),
      instance_count_(0),
      patch_vertices_(3) {}

Shape::Shape(const Shape& from)
    : primitive_type_(from.primitive_type_),
      attribute_array_(from.attribute_array_),
      index_buffer_(from.index_buffer_),
      vertex_ranges_(*this, from.vertex_ranges_.begin(),
                     from.vertex_ranges_.end()),
      instance_count_(from.instance_count_),
      label_(from.label_) {}

Shape::~Shape() {
}

size_t Shape::AddVertexRange(const math::Range1i& range) {
  if (range.IsEmpty()) {
    LOG(WARNING) << "Ignoring empty range passed to Shape::AddVertexRange.";
    return base::kInvalidIndex;
  } else {
    const size_t index = vertex_ranges_.size();
    vertex_ranges_.push_back(VertexRange(range, true, 0));
    return index;
  }
}

void Shape::SetVertexRange(size_t i, const math::Range1i& range) {
  if (CheckRangeIndex(i, "SetVertexRange")) {
    if (range.IsEmpty())
      LOG(WARNING) << "Ignoring empty range passed to Shape::SetVertexRange.";
    else
      vertex_ranges_[i].range = range;
  }
}

const math::Range1i Shape::GetVertexRange(size_t i) const {
  if (CheckRangeIndex(i, "GetVertexRange"))
    return vertex_ranges_[i].range;
  return math::Range1i();
}

void Shape::EnableVertexRange(size_t i, bool enable) {
  if (CheckRangeIndex(i, "EnableVertexRange"))
    vertex_ranges_[i].is_enabled = enable;
}

bool Shape::IsVertexRangeEnabled(size_t i) const {
  if (CheckRangeIndex(i, "IsVertexRangeEnabled"))
    return vertex_ranges_[i].is_enabled;
  return false;
}

void Shape::SetVertexRangeInstanceCount(size_t i, int instance_count) {
  if (CheckRangeIndex(i, "SetVertexRangeInstanceCount"))
    vertex_ranges_[i].instance_count = instance_count;
}

int Shape::GetVertexRangeInstanceCount(size_t i) const {
  if (CheckRangeIndex(i, "GetVertexRangeInstanceCount"))
    return vertex_ranges_[i].instance_count;
  return 0;
}

bool Shape::CheckRangeIndex(size_t i, const char* name) const {
  // 
  // crashes and failures there.
  if (i < vertex_ranges_.size()) {
    return true;
  } else {
    LOG(WARNING) << "Out of bounds index " << i << " passed to "
                 << "Shape::" << name << "; shape has "
                 << vertex_ranges_.size() << " ranges";
    return false;
  }
}

}  // namespace gfx

namespace base {

using gfx::Shape;

// Specialize for Shape::PrimitiveType.
template <> ION_API const EnumHelper::EnumData<Shape::PrimitiveType>
EnumHelper::GetEnumData() {
  static const GLenum kValues[] = {
    GL_LINES, GL_LINE_LOOP, GL_LINE_STRIP, GL_POINTS, GL_TRIANGLES,
    GL_TRIANGLE_FAN, GL_TRIANGLE_STRIP, GL_PATCHES
  };
  static const char* kStrings[] = {
    "Lines", "Line Loop", "Line Strip", "Points", "Triangles", "Triangle Fan",
    "Triangle Strip", "Patches"
  };
  ION_STATIC_ASSERT(ABSL_ARRAYSIZE(kValues) == ABSL_ARRAYSIZE(kStrings),
                    "EnumHelper size mismatch");
  return EnumData<Shape::PrimitiveType>(
      base::IndexMap<Shape::PrimitiveType, GLenum>(kValues,
                                                   ABSL_ARRAYSIZE(kValues)),
      kStrings);
}

}  // namespace base

}  // namespace ion
