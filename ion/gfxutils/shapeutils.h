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

#ifndef ION_GFXUTILS_SHAPEUTILS_H_
#define ION_GFXUTILS_SHAPEUTILS_H_

// This file contains utility functions for operating on shapes and creating
// basic shapes such as rectangles, boxes, spheres, and so on.

#include <functional>
#include <istream>  // NOLINT

#include "ion/base/allocator.h"
#include "ion/gfx/bufferobject.h"
#include "ion/gfx/indexbuffer.h"
#include "ion/gfx/shape.h"
#include "ion/math/angle.h"
#include "ion/math/matrix.h"
#include "ion/math/vector.h"

namespace ion {
namespace gfxutils {

// This struct contains specifications common to all basic shapes. Default
// values are listed in parentheses in the member field comments.
struct ShapeSpec {
  // This enum is used to specify what per-vertex attributes should be included
  // in a Shape. Only the three geometric attributes available in the global
  // registry ("aVertex", "aNormal", and "aTexCoords") are supported.
  enum VertexType {
    kPosition,                 // Position only.
    kPositionTexCoords,        // Position and texture coordinates.
    kPositionNormal,           // Position and normal.
    kPositionTexCoordsNormal,  // Position, texture coordinates, and normal.
  };
  ShapeSpec() : translation(math::Point3f::Zero()),
                scale(1.f),
                rotation(math::Matrix3f::Identity()),
                vertex_type(kPositionTexCoordsNormal),
                usage_mode(gfx::BufferObject::kStaticDraw) {}
  base::AllocatorPtr allocator;  // Used for all allocations (NULL).
  // The order of operations is: scale, then rotate, then translate.
  math::Point3f translation;     // Translation (0, 0, 0).
  float scale;                   // Scale factor (1).
  math::Matrix3f rotation;       // Rotation (Identity).
  VertexType vertex_type;        // Type of vertices (kPositionTexCoordsNormal).
  // UsageMode for all created BufferObject instances. This also affects
  // whether data is considered wipeable. (gfx::BufferObject::kStaticDraw).
  gfx::BufferObject::UsageMode usage_mode;
};

//-----------------------------------------------------------------------------
//
// Generic shape utilities.
//
//-----------------------------------------------------------------------------

// This function can be used to create wireframe versions of filled shapes.
// Given an IndexBuffer representing indices for triangles, it creates and
// returns an IndexBuffer representing indices for lines forming the triangle
// edges. The new IndexBuffer uses the same index type, Allocator, and usage
// mode as the passed one. If there is any reason that the indices cannot be
// converted (NULL pointer, no data, bad number of indices, wiped indices, and
// so on), this returns a NULL pointer.
ION_API const gfx::IndexBufferPtr BuildWireframeIndexBuffer(
    const gfx::IndexBufferPtr& tri_index_buffer);

//-----------------------------------------------------------------------------
//
// External geometry formats.
//
//-----------------------------------------------------------------------------

struct ExternalShapeSpec : public ShapeSpec {
  // The set of external geometry file formats that can be read with
  // LoadExternalShape().
  enum Format {
    k3ds,      // Autodesk 3D Studio format.
    kDae,      // Collada Digital Asset Exchange 1.4/1.5 formats.
    kLwo,      // Lightwave Object format.
    kObj,      // Wavefront Object format.
    kOff,      // Geomview file format.
    kUnknown,  // Used as initial value in spec.
  };
  // The size of the vertex index data type. Some platforms (OpenGL ES2) don't
  // support 32-bit indices, resulting in an error when the shape is drawn.
  enum IndexSize {
    k16Bit,  // 16-bit indices (unsigned short integer).
    k32Bit,  // 32-bit indices (unsigned integer).
  };
  ExternalShapeSpec()
      : format(kUnknown), center_at_origin(true), index_size(k16Bit) {}
  Format format;
  // Whether to center the loaded object at the origin (defaults to true).
  // This centering is done in initial model space, before any transformations
  // are applied.
  bool center_at_origin;
  // The size of the vertex index data type.
  IndexSize index_size;
};

// Loads a Shape with the specified format from the passed stream. On a
// successful load, returns a Shape that contains vertices of the type
// specified. The returned Shape is centered at the origin, unless
// center_at_origin is set to false in the spec. If anything goes wrong, returns
// a NULL Shape.
ION_API const gfx::ShapePtr LoadExternalShape(const ExternalShapeSpec& spec,
                                              std::istream& in);  // NOLINT

//-----------------------------------------------------------------------------
//
// Planar shape.
//
//-----------------------------------------------------------------------------

// This struct defines common enums and settings for planar i.e., flat shapes.
// It can't be used to generate geometry directly, as it only serves as a base
// class for other objects below.
struct PlanarShapeSpec : public ShapeSpec {
  // This enum specifies the principal Cartesian plane containing the rectangle
  // by its directed normal. Note that this affects the orientation of the
  // rectangle, the direction of its normal, and the orientation of its S/T
  // texture coordinates, as commented below.
  enum PlaneNormal {
    kPositiveX,  // In YZ-plane, facing +X, width in Z, +S with -Z, +T with +Y.
    kNegativeX,  // In YZ-plane, facing -X, width in Z, +S with +Z, +T with +Y.
    kPositiveY,  // In XZ-plane, facing +Y, width in X, +S with +X, +T with -Z.
    kNegativeY,  // In XZ-plane, facing -Y, width in X, +S with +X, +T with +Z.
    kPositiveZ,  // In XY-plane, facing +Z, width in X, +S with +X, +T with +Y.
    kNegativeZ,  // In XY-plane, facing -Z, width in X, +S with -X, +T with +Y.
  };
  PlanarShapeSpec() : plane_normal(kPositiveZ) {}
  PlaneNormal plane_normal;  // Orientation of plane (kPositiveZ).
};

//-----------------------------------------------------------------------------
//
// Rectangle.
//
//-----------------------------------------------------------------------------

// This struct is used to specify details of the construction of a rectangle
// shape for BuildRectangleShape(). Use plane_normal from the base class to
// specify the orientation.
struct RectangleSpec : public PlanarShapeSpec {
  RectangleSpec() : size(1.f, 1.f) {}
  math::Vector2f size;       // Size of rectangle (1x1).
};

// Builds and returns a Shape representing a rectangle in one of the principal
// Cartesian planes.
ION_API const gfx::ShapePtr BuildRectangleShape(const RectangleSpec& spec);

//-----------------------------------------------------------------------------
//
// Regular polygon.
//
//-----------------------------------------------------------------------------

// This struct defines a flat regular polygon with n sides and a radius of 1.
// It can approximate a circle or disc if used with a high number of sides.
// Use plane_normal from the base class to specify the orientation. The default
// result is a triangle in the Z plane with the normal facing +Z.
struct RegularPolygonSpec : public PlanarShapeSpec {
  RegularPolygonSpec() : sides(3) {}
  int sides;     // The number of sides in the polygon.
};

// Builds and returns a Shape representing a flat regular polygon.
ION_API const gfx::ShapePtr BuildRegularPolygonShape(
    const RegularPolygonSpec& spec);

//-----------------------------------------------------------------------------
//
// Box.
//
//-----------------------------------------------------------------------------

// This struct is used to specify details of the construction of a box
// shape for BuildBoxShape(). The box is axis-aligned.
struct BoxSpec : public ShapeSpec {
  BoxSpec() : size(1.f, 1.f, 1.f) {}
  math::Vector3f size;       // Size of box (1x1x1).
};

// Builds and returns a Shape representing an axis-aligned box.
ION_API const gfx::ShapePtr BuildBoxShape(const BoxSpec& spec);

//-----------------------------------------------------------------------------
//
// Ellipsoid.
//
//-----------------------------------------------------------------------------

// This struct is used to specify details of the construction of an ellipsoid
// shape for BuildEllipsoidShape(). An ellipsoid is axis-aligned and consists
// of a series of latitudinal bands, each of which is divided into longitudinal
// sectors. If the number of specified bands is less than 2, it is considered
// to be 2. If the number of specified sectors is less than 3, it is considered
// to be 3. The ellipsoid is oriented with the north pole at +Y. S texture
// coordinates increase from west to east, with the seam at -Z. T texture
// coordinates range from 0 at the south pole to 1 at the north pole.
// It is possible to build a fraction of an ellipsoid by specifying start and
// end angles for longitude and latitude. Latitude of 90 degrees corresponds to
// the north pole, latitude of -90 degrees corresponds to the south pole.
// Longitude of 0 corresponds to the seam at -Z, longitude of 180 degrees
// corresponds to +Z. If a fraction of an ellipsoid is requested, the size
// parameter still corresponds to the bounding box that the full ellipsoid would
// occupy.
struct EllipsoidSpec : public ShapeSpec {
  EllipsoidSpec()
      : longitude_start(math::Anglef::FromDegrees(0.f)),
        longitude_end(math::Anglef::FromDegrees(360.f)),
        latitude_start(math::Anglef::FromDegrees(-90.f)),
        latitude_end(math::Anglef::FromDegrees(90.f)),
        band_count(10U),
        sector_count(10U),
        size(1.f, 1.f, 1.f) {}

  math::Anglef longitude_start;   // Start longitude angle (0 degrees)
  math::Anglef longitude_end;     // End longitude angle (360 degrees)
  math::Anglef latitude_start;    // Start latitude angle (-90 degrees)
  math::Anglef latitude_end;      // End latitude angle (90 degrees)
  size_t band_count;              // Number of latitudinal bands (10).
  size_t sector_count;            // Number of longitudinal sectors (10).
  math::Vector3f size;            // Size of ellipsoid (1x1x1).
};

// Builds and returns a Shape representing an axis-aligned ellipsoid.
ION_API const gfx::ShapePtr BuildEllipsoidShape(const EllipsoidSpec& spec);

//-----------------------------------------------------------------------------
//
// Cylinder.
//
//-----------------------------------------------------------------------------

// This struct is used to specify details of the construction of an cylinder
// shape for BuildCylinderShape(). A cylinder is centered on the Y axis and may
// have different top and bottom radii. The top and bottom caps will be
// included if the corresponding flag is set in the CylinderSpec and the
// corresponding radius is not 0. The shaft consists of a series of cylindrical
// bands, each of which is divided into longitudinal sectors. If the number of
// specified bands is less than 1, it is considered to be 1. If the number of
// specified sectors is less than 3, it is considered to be 3. The caps, if
// present, are divided into concentric bands, each of which is divided into
// sectors. S texture coordinates increase on the shaft from west to east
// around the cylinder (with +Y to the north) with the seam at -Z. T texture
// coordinates on the shaft range from 0 at the bottom to 1 at the top. S
// texture coordinates on both caps range from 0 at the -X side to 1 at the +X
// side. T texture coordinates on the top cap range from 0 at the +Z side to 1
// at the -Z side, while the reverse is true on the bottom cap.
struct CylinderSpec : public ShapeSpec {
  CylinderSpec()
      : has_top_cap(true),
        has_bottom_cap(true),
        shaft_band_count(1U),
        cap_band_count(1U),
        sector_count(10U),
        top_radius(.5f),
        bottom_radius(.5f),
        height(1.f) {}
  bool has_top_cap;           // Whether the top cap is present (true).
  bool has_bottom_cap;        // Whether the bottom cap is present (true).
  size_t shaft_band_count;    // Number of bands in the shaft (1).
  size_t cap_band_count;      // Number of bands in each cap (1).
  size_t sector_count;        // Number of longitudinal sectors (10).
  float top_radius;           // Radius of top of cylinder (.5).
  float bottom_radius;        // Radius of bottom of cylinder (.5).
  float height;               // Height of cylinder (1).
};

// Builds and returns a Shape representing an axis-aligned cylinder.
ION_API const gfx::ShapePtr BuildCylinderShape(const CylinderSpec& spec);

// Builds and returns a Shape that does not have any per-vertex attributes at
// all. Adding this shape to the graph will result in a draw call being
// emitted to render num_vertices in mode specified by primitive_type.
ION_API const gfx::ShapePtr BuildPrimitivesList(
      const gfx::Shape::PrimitiveType primitive_type,
      const int num_vertices);

}  // namespace gfxutils
}  // namespace ion

#endif  // ION_GFXUTILS_SHAPEUTILS_H_
