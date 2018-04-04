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

#include "ion/gfxutils/shapeutils.h"

#include <algorithm>

#include "base/integral_types.h"
#include "ion/base/allocationmanager.h"
#include "ion/base/datacontainer.h"
#include "ion/base/invalid.h"
#include "ion/base/logging.h"
#include "ion/base/scopedallocation.h"
#include "ion/base/zipassetmanager.h"
#include "ion/gfx/attributearray.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfxutils/buffertoattributebinder.h"
#include "ion/math/angle.h"
#include "ion/math/matrixutils.h"
#include "ion/math/range.h"
#include "ion/math/vectorutils.h"
#include "third_party/openctm/tools/3ds.h"
#include "third_party/openctm/tools/dae.h"
#include "third_party/openctm/tools/lwo.h"
#include "third_party/openctm/tools/mesh.h"
#include "third_party/openctm/tools/obj.h"
#include "third_party/openctm/tools/off.h"

namespace ion {
namespace gfxutils {

using math::Anglef;
using math::Matrix3f;
using math::Point2f;
using math::Point3f;
using math::Range3f;
using math::Vector3f;

namespace {

//-----------------------------------------------------------------------------
//
// Different vertex types, depending on whether texture coordinates and
// normals are requested.
//
//-----------------------------------------------------------------------------

// Vertex with just position.
struct VertexP {
  VertexP() : position(Point3f::Zero()) {}
  Point3f position;
};

// Vertex with position and texture coordinates.
struct VertexPT {
  VertexPT() : position(Point3f::Zero()), texture_coords(Point2f::Zero()) {}
  Point3f position;
  Point2f texture_coords;
};

// Vertex with position and surface normal.
struct VertexPN {
  VertexPN() : position(Point3f::Zero()), normal(Vector3f::Zero()) {}
  Point3f position;
  Vector3f normal;
};

// Vertex with position, texture coordinates, and surface normal.
struct VertexPTN {
  VertexPTN() :
      position(Point3f::Zero()),
      texture_coords(Point2f::Zero()),
      normal(Vector3f::Zero()) {}
  Point3f position;
  Point2f texture_coords;
  Vector3f normal;
};

//-----------------------------------------------------------------------------
//
// The templated CompactVertices function is used to convert an array of
// vertices of type VertexPTN (which has all components) to one of the other
// types.
//
//-----------------------------------------------------------------------------

template <typename VertexType>
static void CompactVertices(size_t count, const VertexPTN vertices_in[],
                            VertexType vertices_out[]) {
#if !defined(ION_COVERAGE)  // COV_NF_START
  DCHECK(false) << "Unspecialized CompactVertices called";
#endif  // COV_NF_END
}

// Specialize for VertexP.
template <>
void CompactVertices<VertexP>(size_t count, const VertexPTN vertices_in[],
                              VertexP vertices_out[]) {
  for (size_t i = 0; i < count; ++i)
    vertices_out[i].position = vertices_in[i].position;
}

// Specialize for VertexPT.
template <>
void CompactVertices<VertexPT>(size_t count, const VertexPTN vertices_in[],
                               VertexPT vertices_out[]) {
  for (size_t i = 0; i < count; ++i) {
    vertices_out[i].position = vertices_in[i].position;
    vertices_out[i].texture_coords = vertices_in[i].texture_coords;
  }
}

// Specialize for VertexPN.
template <>
void CompactVertices<VertexPN>(size_t count, const VertexPTN vertices_in[],
                               VertexPN vertices_out[]) {
  for (size_t i = 0; i < count; ++i) {
    vertices_out[i].position = vertices_in[i].position;
    vertices_out[i].normal = vertices_in[i].normal;
  }
}

//-----------------------------------------------------------------------------
//
// The templated BindVertices function uses a BufferToAttributeBinder for the
// VertexType to bind vertices in an AttributeArray.
//
//-----------------------------------------------------------------------------

template <typename VertexType>
static void BindVertices(const gfx::AttributeArrayPtr& attribute_array,
                         const gfx::BufferObjectPtr& buffer_object) {
#if !defined(ION_COVERAGE)  // COV_NF_START
  DCHECK(false) << "Unspecialized BindVertices called";
#endif  // COV_NF_END
}

// Specialize for VertexP.
template <>
void BindVertices<VertexP>(const gfx::AttributeArrayPtr& attribute_array,
                           const gfx::BufferObjectPtr& buffer_object) {
  VertexP v;
  BufferToAttributeBinder<VertexP>(v)
      .Bind(v.position, "aVertex")
      .Apply(gfx::ShaderInputRegistry::GetGlobalRegistry(),
             attribute_array, buffer_object);
}

// Specialize for VertexPT.
template <>
void BindVertices<VertexPT>(const gfx::AttributeArrayPtr& attribute_array,
                            const gfx::BufferObjectPtr& buffer_object) {
  VertexPT v;
  BufferToAttributeBinder<VertexPT>(v)
      .Bind(v.position, "aVertex")
      .Bind(v.texture_coords, "aTexCoords")
      .Apply(gfx::ShaderInputRegistry::GetGlobalRegistry(),
             attribute_array, buffer_object);
}

// Specialize for VertexPN.
template <>
void BindVertices<VertexPN>(const gfx::AttributeArrayPtr& attribute_array,
                            const gfx::BufferObjectPtr& buffer_object) {
  VertexPN v;
  BufferToAttributeBinder<VertexPN>(v)
      .Bind(v.position, "aVertex")
      .Bind(v.normal, "aNormal")
      .Apply(gfx::ShaderInputRegistry::GetGlobalRegistry(),
             attribute_array, buffer_object);
}

// Specialize for VertexPTN.
template <>
void BindVertices<VertexPTN>(const gfx::AttributeArrayPtr& attribute_array,
                             const gfx::BufferObjectPtr& buffer_object) {
  VertexPTN v;
  BufferToAttributeBinder<VertexPTN>(v)
      .Bind(v.position, "aVertex")
      .Bind(v.texture_coords, "aTexCoords")
      .Bind(v.normal, "aNormal")
      .Apply(gfx::ShaderInputRegistry::GetGlobalRegistry(),
             attribute_array, buffer_object);
}

//-----------------------------------------------------------------------------
//
// Generic helper functions.
//
//-----------------------------------------------------------------------------

// Returns a short-term Allocator for use in temporary instances, based on the
// given Allocator, which may be NULL.
static const base::AllocatorPtr& GetShortTermAllocator(
    const base::AllocatorPtr& allocator) {
  return allocator.Get() ?
      allocator->GetAllocatorForLifetime(base::kShortTerm) :
      base::AllocationManager::GetDefaultAllocatorForLifetime(base::kShortTerm);
}

// Convenience function that swizzles one Vector3f into another.
static const Vector3f SwizzleVector3f(const Vector3f& v,
                                      const char swizzle[3]) {
  Vector3f swizzled;
  math::Swizzle(v, swizzle, &swizzled);
  return swizzled;
}

// This takes an array of all-component (VertexPTN) vertices and compacts it
// into an array of VertexType vertices, storing the results in the returned
// DataContainer. The Allocator is used for all allocations.
template <typename VertexType>
const base::DataContainerPtr CompactVerticesIntoDataContainer(
    const base::AllocatorPtr& allocator, size_t count, bool is_wipeable,
    const VertexPTN vertices[]) {
  // Create a ScopedAllocation of the correct type on the stack.
  base::ScopedAllocation<VertexType> sa(allocator, count);

  // Compact the vertices into the ScopedAllocation.
  CompactVertices<VertexType>(count, vertices, sa.Get());

  // Transfer the memory from the ScopedAllocation to a DataContainer and
  // return it.
  return sa.TransferToDataContainer(is_wipeable);
}

// Returns true if data in a DataContainer should be wipeable, according to a
// ShapeSpec.
static bool IsWipeable(const ShapeSpec& spec) {
  return spec.usage_mode == gfx::BufferObject::kStaticDraw;
}

// Returns true if texture coordinates should be supplied, according to a
// ShapeSpec.
static bool HasTextureCoordinates(const ShapeSpec& spec) {
  return spec.vertex_type == ShapeSpec::kPositionTexCoords ||
      spec.vertex_type == ShapeSpec::kPositionTexCoordsNormal;
}

// Returns true if normals should be supplied, according to a ShapeSpec.
static bool HasNormals(const ShapeSpec& spec) {
  return spec.vertex_type == ShapeSpec::kPositionNormal ||
      spec.vertex_type == ShapeSpec::kPositionTexCoordsNormal;
}

// Builds and returns an AttributeArray with the given vertex BufferObject
// bound to it. The vertex_type in the ShapeSpec is used to determine how to
// bind the vertices. The Allocator in the spec is used for all allocations.
static const gfx::AttributeArrayPtr BuildAttributeArray(
    const ShapeSpec& spec, const gfx::BufferObjectPtr& buffer_object) {
  gfx::AttributeArrayPtr attribute_array(
      new(spec.allocator) gfx::AttributeArray);
  switch (spec.vertex_type) {
    case ShapeSpec::kPosition:
      BindVertices<VertexP>(attribute_array, buffer_object);
      break;
    case ShapeSpec::kPositionTexCoords:
      BindVertices<VertexPT>(attribute_array, buffer_object);
      break;
    case ShapeSpec::kPositionNormal:
      BindVertices<VertexPN>(attribute_array, buffer_object);
      break;
    case ShapeSpec::kPositionTexCoordsNormal:
    default:
      BindVertices<VertexPTN>(attribute_array, buffer_object);
      break;
  }
  return attribute_array;
}

// Builds and returns a BufferObject representing vertices. The vertices are
// passed in as an array of full-component (VertexPTN) instances, but the
// vertex_type in the ShapeSpec is used to determine the actual type in the
// BufferObject. The Allocator in the spec is used for all allocations.
static const gfx::BufferObjectPtr BuildBufferObject(
    const ShapeSpec& spec, size_t vertex_count, const VertexPTN vertices[]) {
  gfx::BufferObjectPtr buffer_object(new(spec.allocator) gfx::BufferObject);
  const bool is_wipeable = IsWipeable(spec);
  base::DataContainerPtr container;
  size_t vertex_size;
  // Compact to Vertex structures of the correct type if necessary.
  switch (spec.vertex_type) {
    case ShapeSpec::kPosition:
      container = CompactVerticesIntoDataContainer<VertexP>(
          spec.allocator, vertex_count, is_wipeable, vertices);
      vertex_size = sizeof(VertexP);
      break;
    case ShapeSpec::kPositionTexCoords:
      container = CompactVerticesIntoDataContainer<VertexPT>(
          spec.allocator, vertex_count, is_wipeable, vertices);
      vertex_size = sizeof(VertexPT);
      break;
    case ShapeSpec::kPositionNormal:
      container = CompactVerticesIntoDataContainer<VertexPN>(
          spec.allocator, vertex_count, is_wipeable, vertices);
      vertex_size = sizeof(VertexPN);
      break;
    case ShapeSpec::kPositionTexCoordsNormal:
    default:
      // No need to compact - vertices are already the correct type.
      container = base::DataContainer::CreateAndCopy<VertexPTN>(
          vertices, vertex_count, is_wipeable, spec.allocator);
      vertex_size = sizeof(vertices[0]);
      break;
  }
  buffer_object->SetData(container, vertex_size, vertex_count, spec.usage_mode);
  return buffer_object;
}

// Builds and returns an IndexBuffer representing the given indices. The
// Allocator in the spec is used for all allocations.
static const gfx::IndexBufferPtr BuildIndexBuffer(
    const ShapeSpec& spec, size_t num_indices, const uint16 indices[]) {
  gfx::IndexBufferPtr index_buffer(new(spec.allocator) gfx::IndexBuffer);
  base::DataContainerPtr container =
      base::DataContainer::CreateAndCopy<uint16>(
          indices, num_indices, IsWipeable(spec), spec.allocator);

  index_buffer->AddSpec(gfx::BufferObject::kUnsignedShort, 1, 0);
  index_buffer->SetData(container, sizeof(indices[0]), num_indices,
                        spec.usage_mode);

  return index_buffer;
}

// This is used by BuildWireframeIndexBuffer to create a DataContainer holding
// the line indices, given a DataContainer holding the triangle indices. It is
// templated by the index type.
template <typename T>
static const base::DataContainerPtr TriIndicesToLineIndices(
    const base::DataContainerPtr& tri_data, size_t tri_index_count,
    size_t line_index_count, const base::AllocatorPtr& allocator) {
  const T* tri_indices = tri_data->GetData<T>();

  // Create a temporary vector to hold the indices.
  base::AllocVector<T> line_indices(GetShortTermAllocator(allocator),
                                    line_index_count, static_cast<T>(0));

  // Convert from 3 triangle indices to 6 line (segment) indices.
  const size_t num_tris = tri_index_count / 3;
  size_t tri_index = 0;
  size_t line_index = 0;
  for (size_t i = 0; i < num_tris; ++i) {
    line_indices[line_index + 0] = tri_indices[tri_index + 0];
    line_indices[line_index + 1] = tri_indices[tri_index + 1];
    line_indices[line_index + 2] = tri_indices[tri_index + 1];
    line_indices[line_index + 3] = tri_indices[tri_index + 2];
    line_indices[line_index + 4] = tri_indices[tri_index + 2];
    line_indices[line_index + 5] = tri_indices[tri_index + 0];
    tri_index += 3U;
    line_index += 6U;
  }
  DCHECK_EQ(tri_index, tri_index_count);
  DCHECK_EQ(line_index, line_index_count);

  base::DataContainerPtr line_data = base::DataContainer::CreateAndCopy<T>(
          &line_indices[0], line_index_count, tri_data->IsWipeable(),
          allocator);

  return line_data;
}

//-----------------------------------------------------------------------------
//
// Shape-specific helper functions.
//
//-----------------------------------------------------------------------------

// Given a PlanarShapeSpec::PlaneNormal, this returns the three dimensions
// (width, height, plane) in order as a character string that can be used
// to swizzle a vector.
static const char* GetPlanarShapeSwizzle(
    PlanarShapeSpec::PlaneNormal plane_normal) {
  switch (plane_normal) {
    case PlanarShapeSpec::kPositiveX:
    case PlanarShapeSpec::kNegativeX:
      return "zyx";
    case PlanarShapeSpec::kPositiveY:
    case PlanarShapeSpec::kNegativeY:
      return "xzy";
    case PlanarShapeSpec::kPositiveZ:
    case PlanarShapeSpec::kNegativeZ:
    default:
      return "xyz";
  }
}

// Given a PlanarShapeSpec::PlaneNormal, this returns the three signs (-1 or +1)
// in the width, height, and normal dimensions as a Vector3f. The width and
// height signs indicate the correct direction as the corresponding texture
// coordinate increases.
static const Vector3f GetPlanarShapeSigns(
    PlanarShapeSpec::PlaneNormal plane_normal) {
  switch (plane_normal) {
    case PlanarShapeSpec::kPositiveX:
      return Vector3f(-1.f, 1.f, 1.f);
    case PlanarShapeSpec::kNegativeX:
      return Vector3f(1.f, 1.f, -1.f);
    case PlanarShapeSpec::kPositiveY:
      return Vector3f(1.f, -1.f, 1.f);
    case PlanarShapeSpec::kNegativeY:
      return Vector3f(1.f, 1.f, -1.f);
    case PlanarShapeSpec::kPositiveZ:
      return Vector3f(1.f, 1.f, 1.f);
    case PlanarShapeSpec::kNegativeZ:
    default:
      return Vector3f(-1.f, 1.f, -1.f);
  }
}

// Stores the 4 vertices of a rectangle. The swizzle string indicates how to
// swizzle a vector from the canonical +Z orientation. The first 2 components
// of the signs vector indicate whether S and T texture coordinates increase
// with the width and height dimensions (+1) or in the opposite direction
// (-1). The third component indicates whether the normal points in the
// positive or negative direction.
static void GetRectangleVertices(
    const ShapeSpec& spec, float width, float height,
    const char* swizzle, const Vector3f& signs, VertexPTN vertices[]) {
  // Create positions based on size.
  const float half_w = signs[0] * 0.5f * width;
  const float half_h = signs[1] * 0.5f * height;
  math::Swizzle(Point3f(-half_w, -half_h, 0.f), swizzle, &vertices[0].position);
  math::Swizzle(Point3f(half_w, -half_h, 0.f), swizzle, &vertices[1].position);
  math::Swizzle(Point3f(half_w, half_h, 0.f), swizzle, &vertices[2].position);
  math::Swizzle(Point3f(-half_w, half_h, 0.f), swizzle, &vertices[3].position);

  // Move to center.
  const Vector3f translation = spec.translation - Point3f::Zero();
  for (int i = 0; i < 4; ++i)
    vertices[i].position =
        spec.rotation * (vertices[i].position * spec.scale) + translation;

  // Set texture coordinates if requested.
  if (HasTextureCoordinates(spec)) {
    vertices[0].texture_coords.Set(0.f, 0.f);
    vertices[1].texture_coords.Set(1.f, 0.f);
    vertices[2].texture_coords.Set(1.f, 1.f);
    vertices[3].texture_coords.Set(0.f, 1.f);
  }

  // Set normals if requested.
  if (HasNormals(spec)) {
    Vector3f normal;
    math::Swizzle(Vector3f(0.f, 0.f, signs[2]), swizzle, &normal);
    for (int i = 0; i < 4; ++i)
      vertices[i].normal = spec.rotation * normal;
  }
}

// Converts a 2D polygon to 3D vertices in the requested plane, adding texture
// coordinates and normals if requested.
static void GetRegularPolygonVertices(
    const ShapeSpec& spec, const base::AllocVector<Point2f>& points,
    const char* swizzle, const Vector3f& signs, VertexPTN vertices[]) {
  const Vector3f translation = spec.translation - Point3f::Zero();
  const bool has_textures = HasTextureCoordinates(spec);
  const bool has_normals = HasNormals(spec);
  Vector3f normal;
  if (has_normals) {
    math::Swizzle(Vector3f(0.f, 0.f, signs[2]), swizzle, &normal);
  }

  const size_t num_vertices = points.size();
  for (size_t i = 0; i < num_vertices; i++) {
    math::Swizzle(Point3f(points[i][0], points[i][1], 0.0f), swizzle,
                  &vertices[i].position);
    vertices[i].position =
        spec.rotation * (vertices[i].position * spec.scale) + translation;
    if (has_textures) {
      // The untransformed 2D points have a range of -1 to +1, so use them to
      // calculate texture coordinates for simplicity.
      vertices[i].texture_coords.Set((points[i][0] + 1.0f) * 0.5f,
                                     (points[i][1] + 1.0f) * 0.5f);
    }
    if (has_normals) {
      vertices[i].normal = spec.rotation * normal;
    }
  }
}

// Returns (sector_count + 1) points evenly distributed along a partial 2D
// unity circle defined by angle_start and angle_end.
// Points are in the standard counter-clockwise direction.
static void GetPartialCirclePoints(size_t sector_count,
                                   const math::Anglef& angle_start,
                                   const math::Anglef& angle_end,
                                   Point2f points[]) {
  const Anglef sector_angle =
      (angle_end - angle_start) / static_cast<float>(sector_count);
  for (size_t i = 0; i < sector_count + 1; ++i) {
    const Anglef angle = angle_start + sector_angle * static_cast<float>(i);
    const float radians = angle.Radians();
    points[i].Set(cosf(radians), sinf(radians));
  }
}

// Returns the points of a 2D unit circle with sector_count sectors. There will
// be (sector_count + 1) points, with both the first and last points at (1, 0).
// Points are in the standard counter-clockwise direction.
static void GetCirclePoints(size_t sector_count, Point2f points[]) {
  GetPartialCirclePoints(sector_count,
                         math::Anglef(),
                         math::Anglef::FromDegrees(360.f),
                         points);
}

//-----------------------------------------------------------------------------
//
// External Shape helper functions.
//
//-----------------------------------------------------------------------------

// Loads an external geometry model using OpenCTM.
static void LoadExternalShapeData(ExternalShapeSpec::Format format,
                                  std::istream& in,  // NOLINT
                                  Mesh* mesh) {
  switch (format) {
    case ExternalShapeSpec::k3ds:
      Import_3DS(in, mesh);
      break;
    case ExternalShapeSpec::kDae:
      Import_DAE(in, mesh);
      break;
    case ExternalShapeSpec::kLwo:
      Import_LWO(in, mesh);
      break;
    case ExternalShapeSpec::kObj:
      Import_OBJ(in, mesh);
      break;
    case ExternalShapeSpec::kOff:
      Import_OFF(in, mesh);
      break;
    default:
      break;
  }
}

// Builds and returns a BufferObject representing the vertices of an external
// format.
static gfx::BufferObjectPtr BuildExternalBufferObject(
    const ExternalShapeSpec& spec, const Mesh& mesh) {
  // Set up vertices, scaling and centering the model at the spec's center.
  const size_t vertex_count = mesh.mVertices.size();
  base::AllocVector<VertexPTN> vertices(GetShortTermAllocator(spec.allocator),
                                        vertex_count, VertexPTN());
  // We can't cast a Point3f here since it might be aligned.
  Vector3f center = Vector3f::Zero();
  if (spec.center_at_origin) {
    Vector3 bmin, bmax;
    mesh.BoundingBox(bmin, bmax);
    const Point3f mesh_min = Point3f(bmin.x, bmin.y, bmin.z);
    const Point3f mesh_max = Point3f(bmax.x, bmax.y, bmax.z);
    center = Range3f(mesh_min, mesh_max).GetCenter() - Point3f::Zero();
  }
  for (size_t i = 0; i < vertex_count; ++i) {
    const Point3f point(mesh.mVertices[i].x, mesh.mVertices[i].y,
                        mesh.mVertices[i].z);
    vertices[i].position = spec.rotation * ((point - center) * spec.scale) +
        spec.translation;
    if (mesh.HasNormals())
      vertices[i].normal =
          spec.rotation *
          Vector3f(mesh.mNormals[i].x, mesh.mNormals[i].y, mesh.mNormals[i].z);
    if (mesh.HasTexCoords())
      vertices[i]
          .texture_coords.Set(mesh.mTexCoords[i].u, mesh.mTexCoords[i].v);
  }

  return BuildBufferObject(spec, vertex_count, vertices.data());
}

// Builds and returns an IndexBuffer for an external format.
static gfx::IndexBufferPtr BuildExternalIndexBuffer(
    const ExternalShapeSpec& spec, const Mesh& mesh) {
  switch (spec.index_size) {
    case ExternalShapeSpec::IndexSize::k16Bit: {
      const size_t index_count = mesh.mIndices.size();
      base::AllocVector<uint16> indices(GetShortTermAllocator(spec.allocator),
                                        index_count, static_cast<uint16>(0));
      for (size_t i = 0; i < index_count; ++i) {
        if (mesh.mIndices[i] >= (1 << 16)) {
          LOG(ERROR) << "Vertex index " << mesh.mIndices[i]
                     << " is too large to store as uint16.";
          return gfx::IndexBufferPtr();
        }
        indices[i] = static_cast<uint16>(mesh.mIndices[i]);
      }
      return BuildIndexBuffer(spec, index_count, indices.data());
    }
    case ExternalShapeSpec::IndexSize::k32Bit: {
      gfx::IndexBufferPtr index_buffer(new (spec.allocator) gfx::IndexBuffer);
      base::DataContainerPtr container = base::DataContainer::CreateAndCopy(
          mesh.mIndices.data(), mesh.mIndices.size(), IsWipeable(spec),
          spec.allocator);
      index_buffer->AddSpec(gfx::BufferObject::kUnsignedInt, 1, 0);
      index_buffer->SetData(container, sizeof(mesh.mIndices[0]),
                            mesh.mIndices.size(), spec.usage_mode);
      return index_buffer;
    }
    default:
      DCHECK(false) << "Unknown vertex size.";
      return gfx::IndexBufferPtr();
  }
}

//-----------------------------------------------------------------------------
//
// Rectangle Shape helper functions.
//
//-----------------------------------------------------------------------------

// Builds and returns a BufferObject representing the vertices of a rectangle.
static gfx::BufferObjectPtr BuildRectangleBufferObject(
    const RectangleSpec& spec) {
  VertexPTN vertices[4];
  GetRectangleVertices(spec, spec.size[0], spec.size[1],
                       GetPlanarShapeSwizzle(spec.plane_normal),
                       GetPlanarShapeSigns(spec.plane_normal),
                       vertices);
  return BuildBufferObject(spec, 4U, vertices);
}

// Builds and returns an IndexBuffer representing the indices of a rectangle.
static const gfx::IndexBufferPtr BuildRectangleIndexBuffer(
    const RectangleSpec& spec) {
  static const int kNumIndices = 6;
  static const uint16 kIndices[kNumIndices] = { 0, 1, 2, 0, 2, 3 };
  return BuildIndexBuffer(spec, kNumIndices, kIndices);
}

//-----------------------------------------------------------------------------
//
// RegularPolygon Shape helper functions.
//
//-----------------------------------------------------------------------------

// Creates a buffer object representing a flat polygon which will be drawn as
// a triangle fan.
static gfx::BufferObjectPtr BuildRegularPolygonBufferObject(
    const RegularPolygonSpec& spec) {
  // In order to close the polygon, the first and last perimeter points will
  // be the same, hence the number of sides plus 1, plus 1 more for the center.
  const int kNumVertices = spec.sides + 2;
  const base::AllocatorPtr& allocator = GetShortTermAllocator(spec.allocator);

  // The center of the polygon is at points[0], so generate the perimeter
  // starting at index 1.
  base::AllocVector<Point2f> points(allocator, kNumVertices, Point2f::Zero());
  GetCirclePoints(static_cast<size_t>(spec.sides), &points[1]);

  base::AllocVector<VertexPTN> vertices(allocator, kNumVertices, VertexPTN());
  GetRegularPolygonVertices(
      spec, points, GetPlanarShapeSwizzle(spec.plane_normal),
      GetPlanarShapeSigns(spec.plane_normal), vertices.data());
  return BuildBufferObject(spec, kNumVertices, vertices.data());
}

//-----------------------------------------------------------------------------
//
// Box Shape helper functions.
//
//-----------------------------------------------------------------------------

// Stores the 4 vertices of one face of a box, specified by PlaneNormal.
static void GetBoxFaceVertices(const BoxSpec& spec,
                               PlanarShapeSpec::PlaneNormal plane_normal,
                               VertexPTN vertices[]) {
  // Determine the swizzling and signs for ordered dimensions (width, height,
  // plane).
  const char* swizzle = GetPlanarShapeSwizzle(plane_normal);
  const Vector3f signs = GetPlanarShapeSigns(plane_normal);
  const Vector3f swizzled_size = SwizzleVector3f(spec.size, swizzle);

  // Get the vertices of the rectangle for the face at the center point.
  GetRectangleVertices(spec, swizzled_size[0], swizzled_size[1],
                       swizzle, signs, vertices);

  // Translate the rectangle to the cube face position.
  const Vector3f translation = spec.rotation * SwizzleVector3f(
      Vector3f(0.f, 0.f, 0.5f * signs[2] * swizzled_size[2] * spec.scale),
      swizzle);
  // The vertices have already been scaled, rotated, and translated based on the
  // spec.
  for (int i = 0; i < 4; ++i)
    vertices[i].position = vertices[i].position + translation;
}

// Builds and returns a BufferObject representing the vertices of a box.
static gfx::BufferObjectPtr BuildBoxBufferObject(const BoxSpec& spec) {
  static const size_t kNumVertices = 6 * 4;
  VertexPTN verts[kNumVertices];
  GetBoxFaceVertices(spec, RectangleSpec::kPositiveZ, &verts[0]);   // Front.
  GetBoxFaceVertices(spec, RectangleSpec::kNegativeZ, &verts[4]);   // Back.
  GetBoxFaceVertices(spec, RectangleSpec::kPositiveX, &verts[8]);   // Right.
  GetBoxFaceVertices(spec, RectangleSpec::kNegativeX, &verts[12]);  // Left.
  GetBoxFaceVertices(spec, RectangleSpec::kPositiveY, &verts[16]);  // Top.
  GetBoxFaceVertices(spec, RectangleSpec::kNegativeY, &verts[20]);  // Bottom.
  return BuildBufferObject(spec, kNumVertices, verts);
}

// Builds and returns an IndexBuffer representing the indices of a box.
static gfx::IndexBufferPtr BuildBoxIndexBuffer(const BoxSpec& spec) {
  // Set up the triangle vertex indices.
  static const size_t kNumFaces = 6;
  static const size_t kNumIndices = kNumFaces * 6;
  uint16 indices[kNumIndices];
  for (uint16 i = 0; i < kNumFaces; ++i) {
    indices[6 * i + 0] = static_cast<uint16>(4 * i + 0);
    indices[6 * i + 1] = static_cast<uint16>(4 * i + 1);
    indices[6 * i + 2] = static_cast<uint16>(4 * i + 2);
    indices[6 * i + 3] = static_cast<uint16>(4 * i + 0);
    indices[6 * i + 4] = static_cast<uint16>(4 * i + 2);
    indices[6 * i + 5] = static_cast<uint16>(4 * i + 3);
  }
  return BuildIndexBuffer(spec, kNumIndices, indices);
}

//-----------------------------------------------------------------------------
//
// Ellipsoid Shape helper types and functions.
//
//-----------------------------------------------------------------------------

// This struct stores additional data used in Ellipsoid shape construction.
struct EllipsoidData {
  size_t band_count;         // Number of latitudinal bands.
  size_t sector_count;       // Number of longitudinal sectors.
  size_t vertices_per_ring;  // Number of vertices in a latitudinal ring.
  size_t vertex_count;       // Total number of vertices.
};

static const EllipsoidData GetEllipsoidData(const EllipsoidSpec& spec) {
  EllipsoidData data;

  // Use sane values for the band and sector counts.
  data.band_count = std::max(static_cast<size_t>(2), spec.band_count);
  data.sector_count = std::max(static_cast<size_t>(3), spec.sector_count);

  // We need (data.sector_count + 1) vertices to make data.sector_count sectors.
  data.vertices_per_ring = data.sector_count + 1;

  // There are vertices_per_ring vertices at the north pole, at the south pole,
  // and for each of the (band_count - 1) seams between the bands.
  data.vertex_count = (data.band_count + 1U) * data.vertices_per_ring;

  return data;
}

// Builds and returns a BufferObject representing the vertices of a ellipsoid.
static gfx::BufferObjectPtr BuildEllipsoidBufferObject(
    const EllipsoidSpec& spec) {
  // Use a short-term allocator for the local vectors.
  const base::AllocatorPtr& allocator = GetShortTermAllocator(spec.allocator);

  const EllipsoidData data = GetEllipsoidData(spec);
  const bool has_tex_coords = HasTextureCoordinates(spec);
  const bool has_normals = HasNormals(spec);
  base::AllocVector<VertexPTN> vertices(allocator, data.vertex_count,
                                        VertexPTN());

  // Get the points for a latitudinal ring of radius 1.
  base::AllocVector<Point2f> ring_points(allocator, data.vertices_per_ring,
                                         Point2f::Zero());
  GetPartialCirclePoints(data.sector_count,
                         spec.longitude_start,
                         spec.longitude_end,
                         &ring_points[0]);

  // The circle has a radius of 1, and the default ellipsoid is a sphere of
  // radius 0.5 (for size 1x1x1). Create a scale that handles both the change
  // in radius and the target size.
  const Vector3f scale = 0.5f * spec.size;
  const Vector3f inv_scale = 1.0f / scale;

  // Set up vertices. The first N (where N is sector_count + 1) vertices are at
  // the northern most position (north pole when default lat long angles are
  // used), the next N are the first ring below that, and so on, up to
  // the last N at the southern most position (south pole when default lat long
  // angles are used). There are band_count + 1 rings all together.
  // The y coordinate is computed from the latitude angle, which goes from
  // spec.latitude_end to spec.latitude_start.
  const Anglef delta_angle = (spec.latitude_end - spec.latitude_start)
      / static_cast<float>(data.band_count);
  size_t cur_vertex = 0;
  for (size_t ring = 0; ring <= data.band_count; ++ring) {
    const Anglef latitude_angle = spec.latitude_end -
        delta_angle * static_cast<float>(ring);
    const float ring_radius = cosf(latitude_angle.Radians());
    const float sphere_y = sinf(latitude_angle.Radians());
    Point3f pos;
    for (size_t s = 0; s <= data.sector_count; ++s) {
      VertexPTN& v = vertices[cur_vertex];

      // Scale the ring points, rotate them so the seam is at -Z, and move them
      // to the center.
      const Point2f& ring_pt = ring_points[s];
      const Vector3f sphere_pt_vec(ring_radius * -ring_pt[1],
                                   sphere_y,
                                   ring_radius * -ring_pt[0]);
      v.position = spec.rotation * ((scale * sphere_pt_vec) * spec.scale) +
                   spec.translation;

      // Set texture coordinates if requested.
      if (has_tex_coords) {
        const float ts = static_cast<float>(s) /
                         static_cast<float>(data.sector_count);
        const float tt = static_cast<float>(data.band_count - ring) /
                         static_cast<float>(data.band_count);
        v.texture_coords.Set(ts, tt);
      }

      // Set normal if requested. To compute the normal, transform the sphere
      // normal (the normalized sphere position vector) by the inverse of the
      // scale.
      if (has_normals)
        v.normal = spec.rotation * math::Normalized(inv_scale * sphere_pt_vec);

      ++cur_vertex;
    }
  }
  DCHECK_EQ(cur_vertex, data.vertex_count);

  return BuildBufferObject(spec, vertices.size(), &vertices[0]);
}

// Builds and returns an IndexBuffer representing the indices of a ellipsoid.
static gfx::IndexBufferPtr BuildEllipsoidIndexBuffer(
    const EllipsoidSpec& spec) {
  const EllipsoidData data = GetEllipsoidData(spec);

  // Each band uses 2 * sector_count triangles, so they each contain 6 *
  // sector_count indices.
  const size_t index_count = 6U * data.band_count * data.sector_count;
  base::AllocVector<uint16> indices(GetShortTermAllocator(spec.allocator),
                                    index_count, static_cast<uint16>(0));

  size_t cur_index = 0;
  const uint16 ring_offset = static_cast<uint16>(data.vertices_per_ring);
  for (uint16 band = 0; band < data.band_count; ++band) {
    const uint16 first_band_vertex = static_cast<uint16>(band * ring_offset);
    for (uint16 s = 0; s < data.sector_count; ++s) {
      const uint16 v = static_cast<uint16>(first_band_vertex + s);
      indices[cur_index + 0] = v;
      indices[cur_index + 1] = static_cast<uint16>(v + ring_offset);
      indices[cur_index + 2] = static_cast<uint16>(v + 1U);
      indices[cur_index + 3] = static_cast<uint16>(v + 1U);
      indices[cur_index + 4] = static_cast<uint16>(v + ring_offset);
      indices[cur_index + 5] = static_cast<uint16>(v + ring_offset + 1U);
      DCHECK_LE(indices[cur_index + 5U], data.vertex_count);
      cur_index += 6U;
    }
  }
  DCHECK_EQ(cur_index, index_count);

  return BuildIndexBuffer(spec, index_count, &indices[0]);
}

//-----------------------------------------------------------------------------
//
// Cylinder Shape helper types and functions.
//
//-----------------------------------------------------------------------------

// This struct stores additional data used in Cylinder shape construction.
struct CylinderData {
  bool add_top_cap;           // Whether to add the top cap.
  bool add_bottom_cap;        // Whether to add the bottom cap.
  size_t num_caps;            // Number of caps that are included.
  size_t shaft_band_count;    // Number of bands in the shaft.
  size_t cap_band_count;      // Number of bands in the cap.
  size_t sector_count;        // Number of longitudinal sectors.
  size_t vertices_per_ring;   // Number of vertices in a latitudinal ring.
  size_t shaft_vertex_count;  // Total number of vertices in the shaft.
  size_t cap_vertex_count;    // Total number of vertices in each cap.
  size_t vertex_count;        // Total number of vertices.
};

static const CylinderData GetCylinderData(const CylinderSpec& spec) {
  CylinderData data;

  data.add_top_cap = spec.has_top_cap && spec.top_radius != 0.f;
  data.add_bottom_cap = spec.has_bottom_cap && spec.bottom_radius != 0.f;

  data.num_caps = (data.add_top_cap ? 1 : 0) + (data.add_bottom_cap ? 1 : 0);

  // Use sane values for the band and sector counts.
  data.shaft_band_count = std::max(static_cast<size_t>(1),
                                   spec.shaft_band_count);
  data.cap_band_count = std::max(static_cast<size_t>(1), spec.cap_band_count);
  data.sector_count = std::max(static_cast<size_t>(3), spec.sector_count);

  // The first point of each latitudinal ring is duplicated at the end.
  data.vertices_per_ring = data.sector_count + 1;

  data.shaft_vertex_count =
      (data.shaft_band_count + 1) * data.vertices_per_ring;

  // Each cap has a single vertex in the center plus vertex rings.
  data.cap_vertex_count = 1U + data.cap_band_count * data.vertices_per_ring;

  // Add up the vertices.
  data.vertex_count =
      data.shaft_vertex_count + data.num_caps * data.cap_vertex_count;

  return data;
}

// Fills in the shaft_normals array with the normals for the vertices forming a
// ring around the cylinder. The shaft normals do not vary with the y
// coordinate, so these normals can be used for all rings.
static void GetCylinderShaftNormals(
    size_t count, const Point2f ring_points[], float top_radius,
    float bottom_radius, float height, Vector3f shaft_normals[]) {
  if (top_radius == bottom_radius) {
    // If the cylinder has constant radius, normals are all in the XZ plane.
    for (size_t i = 0; i < count; ++i) {
      const Point2f& ring_pt = ring_points[i];
      shaft_normals[i] = math::Normalized(Vector3f(-ring_pt[1], 0.f,
                                                   -ring_pt[0]));
    }
  } else {
    // The cylinder has slanted sides, so the normals are a bit harder to
    // compute. Consider the cone formed by extending the cylinder sides if
    // necessary. Put the base of this cone at y=0 and compute the y value of
    // the apex.
    float base_radius;
    float apex_y;
    if (top_radius < bottom_radius) {
      base_radius = bottom_radius;
      apex_y = height + (top_radius * height) / (bottom_radius - top_radius);
    } else {
      base_radius = top_radius;
      apex_y = -(height + (bottom_radius * height) /
                 (top_radius - bottom_radius));
    }

    // The normal N is perpendicular to the vector from the base point to the
    // apex. Solve for Ny, which is the only unknown, and which is constant for
    // all normals. The math here is pretty easy because the base point has y =
    // 0, the apex point has x = z = 0, and the ring has radius 1. Using
    // similar triangles, ny = (Bx * Bx + Bz * Bz) / apex_y for base point B.
    // But since (Bx, Bz) is on a circle of radius base_radius, the numerator
    // is just base_radius^2.
    const float base_radius_squared = math::Square(base_radius);
    const float ny = base_radius_squared / apex_y;

    // Also compute the length of the unnormalized normal vectors to make
    // normalization faster.
    const float inv_length = 1.f / math::Sqrt(base_radius_squared + ny * ny);

    for (size_t i = 0; i < count; ++i) {
      const Point2f& ring_pt = ring_points[i];
      shaft_normals[i] = inv_length * Vector3f(base_radius * -ring_pt[1], ny,
                                               base_radius * -ring_pt[0]);
    }
  }
}

// Adds vertices representing one cap of a cylinder to the vertices array.
static size_t AddCylinderCapVertices(const CylinderSpec& spec,
                                     const Point2f ring_points[], bool is_top,
                                     VertexPTN* vertices) {
  const Vector3f normal =
      spec.rotation * (is_top ? Vector3f::AxisY() : -Vector3f::AxisY());
  const Vector3f scale =
      is_top ? Vector3f(spec.top_radius, spec.height, spec.top_radius)
             : Vector3f(spec.bottom_radius, spec.height, spec.bottom_radius);
  const bool has_tex_coords = HasTextureCoordinates(spec);
  const bool has_normals = HasNormals(spec);
  const CylinderData data = GetCylinderData(spec);
  const float y = is_top ? .5f : -.5f;

  size_t cur_vertex = 0;

  // Vertex 0 is the center of the cap.
  VertexPTN& center_v = vertices[0];
  center_v.position =
      spec.rotation * ((scale * Vector3f(0.f, y, 0.f)) * spec.scale) +
      spec.translation;
  if (has_tex_coords)
    center_v.texture_coords.Set(.5f, .5f);
  ++cur_vertex;

  // The other vertices form rings from the center outward.
  float radius = 0.f;
  const float delta_radius = 1.f / static_cast<float>(data.cap_band_count);
  const float s_scale = .5f;
  const float t_scale = is_top ? -.5f : .5f;
  for (size_t ring = 0; ring < data.cap_band_count; ++ring) {
    radius += delta_radius;
    for (size_t s = 0; s <= data.sector_count; ++s) {
      VertexPTN& v = vertices[cur_vertex];
      const Point2f& ring_pt = ring_points[s];

      // Scale the ring points by the current radius, rotate them so the seam
      // is at -Z, and move them to the center.
      const Vector3f pt_vec(radius * -ring_pt[1], y, -radius * ring_pt[0]);
      v.position =
          spec.rotation * ((scale * pt_vec) * spec.scale) + spec.translation;

      // Set texture coordinates if requested. They are the unscaled XZ
      // coordinates scaled and translated to the range (0,1).
      if (has_tex_coords)
        v.texture_coords.Set(.5f + s_scale * pt_vec[0],
                             .5f + t_scale * pt_vec[2]);

      ++cur_vertex;
    }
  }

  // All cap normals are identical.
  if (has_normals) {
    for (size_t i = 0; i < cur_vertex; ++i)
      vertices[i].normal = normal;
  }

  return cur_vertex;
}

// Adds indices representing one cap of a cylinder to the indices array.
// start_index is the index of the first vertex (the center point) of the cap.
static size_t AddCylinderCapIndices(
    const CylinderData& data, size_t start_index, bool invert_orientation,
    uint16 indices[]) {
  // The center vertex is at start_index.
  const uint16 center_index = static_cast<uint16>(start_index);
  size_t cur_index = 0;

  // These are used to get the correct triangle orientation.
  const int i0 = invert_orientation ? 1 : 0;
  const int i1 = 1 - i0;

  // Store indices for the innermost band, which is a triangle fan.
  for (uint16 s = 0; s < data.sector_count; ++s) {
    const uint16 v = static_cast<uint16>(center_index + 1U + s);
    indices[cur_index + 0] = center_index;
    indices[cur_index + 1 + i0] = v;
    indices[cur_index + 1 + i1] = static_cast<uint16>(v + 1U);
    cur_index += 3U;
  }

  // Store indices for all other bands, which use 2 triangles per sector.
  const uint16 ring_offset = static_cast<uint16>(data.vertices_per_ring);
  uint16 first_band_vertex = static_cast<uint16>(center_index + 1U);
  for (uint16 band = 1; band < data.cap_band_count; ++band) {
    for (uint16 s = 0; s < data.sector_count; ++s) {
      const uint16 v = static_cast<uint16>(first_band_vertex + s);
      indices[cur_index + 0] = v;
      indices[cur_index + 1 + i0] = static_cast<uint16>(v + ring_offset);
      indices[cur_index + 1 + i1] = static_cast<uint16>(v + 1U);
      indices[cur_index + 3] = static_cast<uint16>(v + 1U);
      indices[cur_index + 4 + i0] = static_cast<uint16>(v + ring_offset);
      indices[cur_index + 4 + i1] = static_cast<uint16>(v + ring_offset + 1U);
      DCHECK_LE(indices[cur_index + 4], data.vertex_count);
      DCHECK_LE(indices[cur_index + 5], data.vertex_count);
      cur_index += 6U;
    }
    first_band_vertex = static_cast<uint16>(first_band_vertex + ring_offset);
  }
  return cur_index;
}

// Builds and returns a BufferObject representing the vertices of a cylinder.
static gfx::BufferObjectPtr BuildCylinderBufferObject(
    const CylinderSpec& spec) {
  // Use a short-term allocator for the local vectors.
  const base::AllocatorPtr& allocator = GetShortTermAllocator(spec.allocator);

  const CylinderData data = GetCylinderData(spec);
  const bool has_tex_coords = HasTextureCoordinates(spec);
  const bool has_normals = HasNormals(spec);
  base::AllocVector<VertexPTN> vertices(allocator, data.vertex_count,
                                        VertexPTN());

  // Get the points for a latitudinal ring of radius 1.
  base::AllocVector<Point2f> ring_points(allocator, data.vertices_per_ring,
                                         Point2f::Zero());
  GetCirclePoints(data.sector_count, &ring_points[0]);

  // Compute the shaft normals as well, since they don't vary by height.
  base::AllocVector<Vector3f> shaft_normals(allocator, data.vertices_per_ring,
                                            Vector3f::Zero());
  GetCylinderShaftNormals(data.vertices_per_ring, &ring_points[0],
                          spec.top_radius, spec.bottom_radius, spec.height,
                          &shaft_normals[0]);

  // Store shaft vertices. Rings start at the top and proceed to the bottom.
  const float delta_y = 1.f / static_cast<float>(data.shaft_band_count);
  const float delta_radius = (spec.top_radius - spec.bottom_radius) /
                             static_cast<float>(data.shaft_band_count);
  float ring_y = .5f;
  float ring_radius = spec.top_radius;
  size_t cur_vertex = 0;
  for (size_t ring = 0; ring <= data.shaft_band_count; ++ring) {
    const float ring_t = ring_y + .5f;
    // The circle in ring_points has a radius of 1; scale to the correct sizes.
    const Vector3f scale(ring_radius, spec.height, ring_radius);
    for (size_t s = 0; s <= data.sector_count; ++s) {
      VertexPTN& v = vertices[cur_vertex];
      const Point2f& ring_pt = ring_points[s];
      // Scale the ring points, rotate them so the seam is at -Z, and move them
      // to the center.
      const Vector3f shaft_pt_vec(-ring_pt[1], ring_y, -ring_pt[0]);
      v.position = spec.rotation * ((scale * shaft_pt_vec) * spec.scale) +
                   spec.translation;
      // Set texture coordinates if requested.
      if (has_tex_coords)
        v.texture_coords.Set(static_cast<float>(s) /
                             static_cast<float>(data.sector_count), ring_t);
      // Set normal if requested.
      if (has_normals)
        v.normal = spec.rotation * shaft_normals[s];

      ++cur_vertex;
    }
    ring_y -= delta_y;
    ring_radius -= delta_radius;
  }

  // Store cap vertices.
  if (data.add_top_cap)
    cur_vertex += AddCylinderCapVertices(spec, &ring_points[0], true,
                                         &vertices[cur_vertex]);
  if (data.add_bottom_cap)
    cur_vertex += AddCylinderCapVertices(spec, &ring_points[0], false,
                                         &vertices[cur_vertex]);

  DCHECK_EQ(cur_vertex, data.vertex_count);

  return BuildBufferObject(spec, vertices.size(), &vertices[0]);
}

// Builds and returns an IndexBuffer representing the indices of a cylinder.
static gfx::IndexBufferPtr BuildCylinderIndexBuffer(
    const CylinderSpec& spec) {
  const CylinderData data = GetCylinderData(spec);

  // Each shaft band uses 2 * sector_count triangles, so they each contain 6 *
  // sector_count indices.
  const size_t shaft_index_count =
      6U * data.shaft_band_count * data.sector_count;
  // Each cap uses sector_count triangles (3 vertices) for the innermost band
  // and 2 * sector_count triangles (6 vertices) for every other band.
  const size_t cap_index_count =
      3U * data.sector_count +
      6U * data.sector_count * (data.cap_band_count - 1U);
  const size_t index_count =
      shaft_index_count + data.num_caps * cap_index_count;
  base::AllocVector<uint16> indices(GetShortTermAllocator(spec.allocator),
                                    index_count, static_cast<uint16>(0));

  size_t cur_index = 0;

  // Add shaft indices.
  const uint16 ring_offset = static_cast<uint16>(data.vertices_per_ring);
  for (uint16 band = 0; band < data.shaft_band_count; ++band) {
    const uint16 first_band_vertex = static_cast<uint16>(band * ring_offset);
    for (uint16 s = 0; s < data.sector_count; ++s) {
      const uint16 v = static_cast<uint16>(first_band_vertex + s);
      indices[cur_index + 0] = v;
      indices[cur_index + 1] = static_cast<uint16>(v + ring_offset);
      indices[cur_index + 2] = static_cast<uint16>(v + 1U);
      indices[cur_index + 3] = static_cast<uint16>(v + 1U);
      indices[cur_index + 4] = static_cast<uint16>(v + ring_offset);
      indices[cur_index + 5] = static_cast<uint16>(v + ring_offset + 1U);
      DCHECK_LE(indices[cur_index + 5U], data.vertex_count);
      cur_index += 6U;
    }
  }

  // Add cap indices.
  size_t first_cap_vertex = data.shaft_vertex_count;
  if (data.add_top_cap) {
    cur_index += AddCylinderCapIndices(data, first_cap_vertex, false,
                                       &indices[cur_index]);
    first_cap_vertex += data.cap_vertex_count;
  }

  if (data.add_bottom_cap) {
    cur_index += AddCylinderCapIndices(data, first_cap_vertex, true,
                                       &indices[cur_index]);
    first_cap_vertex += data.cap_vertex_count;
  }
  DCHECK_EQ(cur_index, index_count);

  return BuildIndexBuffer(spec, index_count, &indices[0]);
}

}  // anonymous namespace

//-----------------------------------------------------------------------------
//
// Public functions.
//
//-----------------------------------------------------------------------------

const gfx::IndexBufferPtr BuildWireframeIndexBuffer(
    const gfx::IndexBufferPtr& tri_index_buffer) {
  gfx::IndexBufferPtr line_index_buffer;

  if (tri_index_buffer.Get()) {
    // The index count must be a multiple of 3, and there has to be data.
    const size_t tri_index_count = tri_index_buffer->GetCount();
    const base::DataContainerPtr& tri_data = tri_index_buffer->GetData();
    if (tri_index_count % 3U == 0U && tri_data.Get() && tri_data->GetData()) {
      const base::AllocatorPtr& al = tri_index_buffer->GetAllocator();
      const size_t line_index_count = 2U * tri_index_count;

      // The IndexBuffer must have just unsigned byte or short indices.
      const gfx::BufferObject::Spec& spec = tri_index_buffer->GetSpec(0);
      DCHECK(!base::IsInvalidReference(spec));
      DCHECK_EQ(spec.byte_offset, 0U);
      base::DataContainerPtr line_data;
      if (spec.type == gfx::BufferObject::kUnsignedByte)
        line_data = TriIndicesToLineIndices<uint8>(
            tri_data, tri_index_count, line_index_count, al);
      else if (spec.type == gfx::BufferObject::kUnsignedShort)
        line_data = TriIndicesToLineIndices<uint16>(
            tri_data, tri_index_count, line_index_count, al);

      if (line_data.Get()) {
        line_index_buffer = new(al) gfx::IndexBuffer;
        line_index_buffer->AddSpec(spec.type, 1, 0);
        line_index_buffer->SetData(line_data, tri_index_buffer->GetStructSize(),
                                   line_index_count,
                                   tri_index_buffer->GetUsageMode());
      }
    }
  }
  return line_index_buffer;
}

const gfx::ShapePtr LoadExternalShape(const ExternalShapeSpec& spec,
                                      std::istream& in) {  // NOLINT
  Mesh mesh;
  LoadExternalShapeData(spec.format, in, &mesh);

  // If there are no vertices or indices then there is nothing to return.
  if (mesh.mIndices.empty() || mesh.mVertices.empty())
    return gfx::ShapePtr();

  gfx::BufferObjectPtr buffer_object = BuildExternalBufferObject(spec, mesh);
  gfx::ShapePtr shape(new(spec.allocator) gfx::Shape);
  shape->SetLabel("External geometry");
  shape->SetPrimitiveType(gfx::Shape::kTriangles);
  shape->SetAttributeArray(BuildAttributeArray(spec, buffer_object));
  shape->SetIndexBuffer(BuildExternalIndexBuffer(spec, mesh));
  return shape;
}

const gfx::ShapePtr BuildRectangleShape(const RectangleSpec& spec) {
  gfx::ShapePtr shape(new(spec.allocator) gfx::Shape);
  shape->SetLabel("Rectangle");
  shape->SetPrimitiveType(gfx::Shape::kTriangles);
  shape->SetAttributeArray(
      BuildAttributeArray(spec, BuildRectangleBufferObject(spec)));
  shape->SetIndexBuffer(BuildRectangleIndexBuffer(spec));
  return shape;
}

const gfx::ShapePtr BuildRegularPolygonShape(const RegularPolygonSpec& spec) {
  DCHECK_LE(3, spec.sides) << "Polygons must have at least 3 sides";
  gfx::ShapePtr shape(new (spec.allocator) gfx::Shape);
  shape->SetLabel("Polygon");
  shape->SetPrimitiveType(gfx::Shape::kTriangleFan);
  shape->SetAttributeArray(
      BuildAttributeArray(spec, BuildRegularPolygonBufferObject(spec)));
  return shape;
}

const gfx::ShapePtr BuildBoxShape(const BoxSpec& spec) {
  gfx::ShapePtr shape(new(spec.allocator) gfx::Shape);
  shape->SetLabel("Box");
  shape->SetPrimitiveType(gfx::Shape::kTriangles);
  shape->SetAttributeArray(
      BuildAttributeArray(spec, BuildBoxBufferObject(spec)));
  shape->SetIndexBuffer(BuildBoxIndexBuffer(spec));
  return shape;
}

const gfx::ShapePtr BuildEllipsoidShape(const EllipsoidSpec& spec) {
  gfx::ShapePtr shape(new(spec.allocator) gfx::Shape);
  shape->SetLabel("Ellipsoid");
  shape->SetPrimitiveType(gfx::Shape::kTriangles);
  shape->SetAttributeArray(
      BuildAttributeArray(spec, BuildEllipsoidBufferObject(spec)));
  shape->SetIndexBuffer(BuildEllipsoidIndexBuffer(spec));
  return shape;
}

const gfx::ShapePtr BuildCylinderShape(const CylinderSpec& spec) {
  gfx::ShapePtr shape(new(spec.allocator) gfx::Shape);
  shape->SetLabel("Cylinder");
  shape->SetPrimitiveType(gfx::Shape::kTriangles);
  shape->SetAttributeArray(
      BuildAttributeArray(spec, BuildCylinderBufferObject(spec)));
  shape->SetIndexBuffer(BuildCylinderIndexBuffer(spec));
  return shape;
}

const gfx::ShapePtr BuildPrimitivesList(
      const gfx::Shape::PrimitiveType primitive_type,
      const int num_vertices) {
  gfx::ShapePtr shape(new gfx::Shape());
  shape->SetPrimitiveType(primitive_type);
  shape->SetAttributeArray(gfx::AttributeArrayPtr(new gfx::AttributeArray()));
  shape->AddVertexRange(math::Range1i(0, num_vertices));
  return shape;
}

}  // namespace gfxutils
}  // namespace ion
