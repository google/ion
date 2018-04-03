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

#include <sstream>

#include "base/integral_types.h"
#include "ion/base/allocationmanager.h"
#include "ion/base/invalid.h"
#include "ion/base/logchecker.h"
#include "ion/base/tests/multilinestringsequal.h"
#include "ion/base/zipassetmanager.h"
#include "ion/base/zipassetmanagermacros.h"
#include "ion/gfx/attributearray.h"
#include "ion/gfx/bufferobject.h"
#include "ion/gfx/shape.h"
#include "ion/math/angle.h"
#include "ion/math/transformutils.h"
#include "ion/math/vector.h"
#include "ion/math/vectorutils.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

ION_REGISTER_ASSETS(ShapeUtilsTest);

namespace ion {
namespace gfxutils {

using math::Point2f;
using math::Point3f;
using math::Vector3f;

namespace {

const float kSqrt2Over4 = static_cast<float>(M_SQRT2 / 4);

//-----------------------------------------------------------------------------
//
// Helper types and functions.
//
//-----------------------------------------------------------------------------

// This struct is used to test values inside buffer objects.
template <typename T>
struct BufferObjectValue {
  BufferObjectValue(size_t index_in, const T& value_in)
      : index(index_in), value(value_in) {}
  size_t index;
  T value;
};

// Shorthand.
typedef BufferObjectValue<Point2f> TexBov;
typedef BufferObjectValue<Point3f> PosBov;
typedef BufferObjectValue<Vector3f> NormBov;
typedef BufferObjectValue<uint16> IndexBov16;
typedef BufferObjectValue<uint32> IndexBov32;

template <typename T>
class IndexBovTraits;

template <>
class IndexBovTraits<IndexBov16> {
 public:
  typedef uint16 ComponentTypeC;
  static gfx::BufferObject::ComponentType ComponentType() {
    return gfx::BufferObject::kUnsignedShort;
  }
};

template <>
class IndexBovTraits<IndexBov32> {
 public:
  typedef uint32 ComponentTypeC;
  static gfx::BufferObject::ComponentType ComponentType() {
    return gfx::BufferObject::kUnsignedInt;
  }
};

// Tests for equality of values (in a BufferObjectElement). Specialized for
// Points/Vectors to test near-equality.
template <typename T>
static bool ValuesEqual(const T& v0, const T& v1) {
  return v0 == v1;
}
template <>
bool ValuesEqual<Point3f>(const Point3f& v0, const Point3f& v1) {
  return math::PointsAlmostEqual(v0, v1, 1e-4f);
}
template <>
bool ValuesEqual<Point2f>(const Point2f& v0, const Point2f& v1) {
  return math::PointsAlmostEqual(v0, v1, 1e-4f);
}
template <>
bool ValuesEqual<Vector3f>(const Vector3f& v0, const Vector3f& v1) {
  return math::VectorsAlmostEqual(v0, v1, 1e-4f);
}

// Validates the indexed BufferObjectElement inside an AttributeArray. The
// component count, component type, and value count must match exactly.
// The expected_buffer_object_values vector contains some sample index/value
// pairs that are also tested against the buffer contents.
template <typename T>
static ::testing::AssertionResult TestBoe(
    const gfx::AttributeArrayPtr& aa, size_t index,
    size_t expected_component_count,
    gfx::BufferObject::ComponentType expected_type, size_t expected_value_count,
    const std::vector<BufferObjectValue<T> >& expected_buffer_object_values) {
  const gfx::BufferObjectElement& boe =
      aa->GetBufferAttribute(index).GetValue<gfx::BufferObjectElement>();
  const gfx::BufferObject::Spec& spec =
      boe.buffer_object->GetSpec(boe.spec_index);
  if (base::IsInvalidReference(spec)) {
    return ::testing::AssertionFailure() << "No spec for BOE with index "
                                         << index;
  }

  // Component count.
  if (spec.component_count != expected_component_count) {
    return ::testing::AssertionFailure()
           << "Wrong component count for BOE with index " << index
           << ": expected " << expected_component_count << ", got "
           << spec.component_count;
  }

  // Component type.
  if (spec.type != expected_type) {
    return ::testing::AssertionFailure()
           << "Wrong type for BOE with index " << index << ": expected "
           << expected_type << ", got " << spec.type;
  }

  // BufferObject and data pointers.
  const gfx::BufferObjectPtr& bo = boe.buffer_object;
  const char* data = static_cast<const char*>(bo->GetData()->GetData());
  if (!data) {
    return ::testing::AssertionFailure() << "Null data for BOE with index "
                                         << index;
  }
  if (!bo.Get()) {
    return ::testing::AssertionFailure()
           << "Null BufferObject for BOE with index " << index;
  }

  // Value count.
  const size_t num_values = bo->GetCount();
  if (num_values != expected_value_count) {
    return ::testing::AssertionFailure()
           << "Wrong value count for BOE with index " << index << ": expected "
           << expected_value_count << ", got " << num_values;
  }

  // Selected values.
  for (size_t i = 0; i < expected_buffer_object_values.size(); ++i) {
    const BufferObjectValue<T>& bov = expected_buffer_object_values[i];
    if (bov.index >= num_values) {
      return ::testing::AssertionFailure()
             << "Invalid value index " << bov.index
             << " for bov specified for BOE with index " << index
             << " (count is " << num_values << ")";
    }
    const size_t stride = bo->GetStructSize();
    const char* ptr = &data[stride * bov.index + spec.byte_offset];
    const T* typed_ptr = reinterpret_cast<const T*>(ptr);
    if (!ValuesEqual(*typed_ptr, bov.value)) {
      return ::testing::AssertionFailure()
             << "Wrong value for entry " << bov.index << " in BOE with index "
             << index << ": expected " << bov.value << ", got " << *typed_ptr;
    }
  }
  return ::testing::AssertionSuccess();
}

// Validates an IndexBuffer. The component count must be 1 and the component
// type must be kUnsignedShort.  The expected_buffer_object_values vector
// contains some sample index/value pairs that are also tested against the
// buffer contents.
template <typename IndexBovType>
static ::testing::AssertionResult TestIndexBuffer(
    const gfx::IndexBufferPtr& ib, size_t expected_value_count,
    const std::vector<IndexBovType>& expected_buffer_object_values) {
  if (!ib.Get()) return ::testing::AssertionFailure() << "Null IndexBuffer";

  // IndexBuffers must have exactly one spec.
  if (ib->GetSpecCount() != 1U) {
    return ::testing::AssertionFailure()
           << "Wrong number of specs in IndexBuffer: " << ib->GetSpecCount();
  }
  const gfx::BufferObject::Spec& spec = ib->GetSpec(0);

  // Component count.
  if (spec.component_count != 1U) {
    return ::testing::AssertionFailure()
           << "Wrong component count for IndexBuffer: expected 1, got "
           << spec.component_count;
  }

  // Component type.
  if (spec.type != IndexBovTraits<IndexBovType>::ComponentType()) {
    return ::testing::AssertionFailure()
           << "Wrong type for IndexBuffer: expected "
           << IndexBovTraits<IndexBovType>::ComponentType() << ", got "
           << spec.type;
  }

  // BufferObject and data pointers.
  const typename IndexBovTraits<IndexBovType>::ComponentTypeC* data =
      static_cast<const typename IndexBovTraits<IndexBovType>::ComponentTypeC*>(
          ib->GetData()->GetData());
  if (!data)
    return ::testing::AssertionFailure() << "Null data for IndexBuffer";

  // Value count.
  const size_t num_values = ib->GetCount();
  if (num_values != expected_value_count) {
    return ::testing::AssertionFailure()
           << "Wrong value count for IndexBuffer: expected "
           << expected_value_count << ", got " << num_values;
  }

  // Selected values.
  for (size_t i = 0; i < expected_buffer_object_values.size(); ++i) {
    const IndexBovType& bov = expected_buffer_object_values[i];
    if (bov.index >= num_values) {
      return ::testing::AssertionFailure()
             << "Invalid value index " << bov.index
             << " for bov specified for IndexBuffer (count is " << num_values
             << ")";
    }
    if (data[bov.index] != bov.value) {
      return ::testing::AssertionFailure()
             << "Wrong value for entry " << bov.index
             << " in IndexBuffer: expected " << bov.value << ", got "
             << data[bov.index];
    }
  }
  return ::testing::AssertionSuccess();
}

}  // anonymous namespace

//-----------------------------------------------------------------------------
//
// Tests.
//
//-----------------------------------------------------------------------------

TEST(ShapeUtilsTest, BuildWireframeIndexBuffer) {
  static const size_t kNumIndices = 6U;
  static const uint8 kByteIndices[kNumIndices] = {1U, 2U, 3U, 4U, 5U, 6U};
  static const uint16 kShortIndices[kNumIndices] = {6U, 5U, 4U, 3U, 2U, 1U};

  // Null pointer.
  gfx::IndexBufferPtr tri_ib;
  EXPECT_TRUE(BuildWireframeIndexBuffer(tri_ib).Get() == nullptr);

  // No data.
  tri_ib = new gfx::IndexBuffer;
  EXPECT_TRUE(BuildWireframeIndexBuffer(tri_ib).Get() == nullptr);

  const base::AllocatorPtr& al =
      base::AllocationManager::GetDefaultAllocatorForLifetime(base::kShortTerm);

  tri_ib->AddSpec(gfx::BufferObject::kUnsignedByte, 1, 0);

  // Bad number of indices.
  base::DataContainerPtr dc = base::DataContainer::CreateAndCopy<uint8>(
      kByteIndices, kNumIndices - 1U, false, al);
  tri_ib->SetData(dc, sizeof(kByteIndices[0]), kNumIndices - 1U,
                  gfx::BufferObject::kStaticDraw);
  EXPECT_TRUE(BuildWireframeIndexBuffer(tri_ib).Get() == nullptr);

  // This should work ok.
  dc = base::DataContainer::CreateAndCopy<uint8>(kByteIndices, kNumIndices,
                                                 false, al);
  tri_ib->SetData(dc, sizeof(kByteIndices[0]), kNumIndices,
                  gfx::BufferObject::kStaticDraw);
  gfx::IndexBufferPtr line_ib = BuildWireframeIndexBuffer(tri_ib);
  ASSERT_FALSE(line_ib.Get() == nullptr);
  ASSERT_FALSE(line_ib->GetData().Get() == nullptr);
  EXPECT_EQ(al, line_ib->GetAllocator());
  EXPECT_EQ(12U, line_ib->GetCount());
  const uint8* line_bytes = line_ib->GetData()->GetData<uint8>();
  EXPECT_EQ(1U, line_bytes[0]);
  EXPECT_EQ(2U, line_bytes[1]);
  EXPECT_EQ(2U, line_bytes[2]);
  EXPECT_EQ(3U, line_bytes[3]);
  EXPECT_EQ(3U, line_bytes[4]);
  EXPECT_EQ(1U, line_bytes[5]);
  EXPECT_EQ(4U, line_bytes[6]);
  EXPECT_EQ(5U, line_bytes[7]);
  EXPECT_EQ(5U, line_bytes[8]);
  EXPECT_EQ(6U, line_bytes[9]);
  EXPECT_EQ(6U, line_bytes[10]);
  EXPECT_EQ(4U, line_bytes[11]);

  // Repeat with unsigned shorts.
  tri_ib = new gfx::IndexBuffer;
  tri_ib->AddSpec(gfx::BufferObject::kUnsignedShort, 1, 0);
  dc = base::DataContainer::CreateAndCopy<uint16>(kShortIndices, kNumIndices,
                                                  false, al);
  tri_ib->SetData(dc, sizeof(kShortIndices[0]), kNumIndices,
                  gfx::BufferObject::kStaticDraw);
  line_ib = BuildWireframeIndexBuffer(tri_ib);
  ASSERT_FALSE(line_ib.Get() == nullptr);
  ASSERT_FALSE(line_ib->GetData().Get() == nullptr);
  EXPECT_EQ(al, line_ib->GetAllocator());
  EXPECT_EQ(12U, line_ib->GetCount());
  const uint16* line_shorts = line_ib->GetData()->GetData<uint16>();
  EXPECT_EQ(6U, line_shorts[0]);
  EXPECT_EQ(5U, line_shorts[1]);
  EXPECT_EQ(5U, line_shorts[2]);
  EXPECT_EQ(4U, line_shorts[3]);
  EXPECT_EQ(4U, line_shorts[4]);
  EXPECT_EQ(6U, line_shorts[5]);
  EXPECT_EQ(3U, line_shorts[6]);
  EXPECT_EQ(2U, line_shorts[7]);
  EXPECT_EQ(2U, line_shorts[8]);
  EXPECT_EQ(1U, line_shorts[9]);
  EXPECT_EQ(1U, line_shorts[10]);
  EXPECT_EQ(3U, line_shorts[11]);
}

TEST(ShapeUtilsTest, Rectangle) {
  RectangleSpec spec;

  // Texture coordinates are the same regardless of any spec settings.
  std::vector<TexBov> tex_bovs;
  tex_bovs.push_back(TexBov(0, Point2f(0.f, 0.f)));
  tex_bovs.push_back(TexBov(1, Point2f(1.f, 0.f)));
  tex_bovs.push_back(TexBov(2, Point2f(1.f, 1.f)));
  tex_bovs.push_back(TexBov(3, Point2f(0.f, 1.f)));

  // So are indices.
  static const size_t kNumIndices = 6U;
  static const uint16 kIndices[kNumIndices] = {0, 1, 2, 0, 2, 3};
  std::vector<IndexBov16> index_bovs;
  for (size_t i = 0; i < kNumIndices; ++i)
    index_bovs.push_back(IndexBov16(i, kIndices[i]));

  {
    // Build with default RectangleSpec.
    gfx::ShapePtr rect = BuildRectangleShape(spec);
    ASSERT_FALSE(rect.Get() == nullptr);

    const gfx::AttributeArrayPtr& aa = rect->GetAttributeArray();
    ASSERT_FALSE(aa.Get() == nullptr);
    EXPECT_EQ(3U, aa->GetAttributeCount());
    EXPECT_EQ(3U, aa->GetBufferAttributeCount());

    // Vertex positions.
    std::vector<PosBov> pos_bovs;
    pos_bovs.push_back(PosBov(0, Point3f(-0.5f, -0.5f, 0.f)));
    pos_bovs.push_back(PosBov(1, Point3f(0.5f, -0.5f, 0.f)));
    pos_bovs.push_back(PosBov(2, Point3f(0.5f, 0.5f, 0.f)));
    pos_bovs.push_back(PosBov(3, Point3f(-0.5f, 0.5f, 0.f)));
    EXPECT_TRUE(TestBoe(aa, 0, 3U, gfx::BufferObject::kFloat, 4U, pos_bovs));

    // Texture coordinates.
    EXPECT_TRUE(TestBoe(aa, 1, 2U, gfx::BufferObject::kFloat, 4U, tex_bovs));

    // Normals.
    std::vector<NormBov> norm_bovs;
    for (int i = 0; i < 4; ++i)
      norm_bovs.push_back(NormBov(i, Vector3f(0.f, 0.f, 1.f)));
    EXPECT_TRUE(TestBoe(aa, 2, 3U, gfx::BufferObject::kFloat, 4U, norm_bovs));

    EXPECT_TRUE(
        TestIndexBuffer(rect->GetIndexBuffer(), kNumIndices, index_bovs));
  }

  {
    // Use a different plane normal, size, translation, scale and rotation.
    spec.translation.Set(1.f, 2.f, 3.f);
    spec.plane_normal = RegularPolygonSpec::kPositiveX;
    spec.size.Set(10.f, 20.f);
    spec.scale = 2.f;
    spec.rotation = math::RotationMatrixAxisAngleNH(
        math::Vector3f::AxisZ(), math::Anglef::FromDegrees(90.f));
    gfx::ShapePtr rect = BuildRectangleShape(spec);
    ASSERT_FALSE(rect.Get() == nullptr);

    const gfx::AttributeArrayPtr& aa = rect->GetAttributeArray();
    ASSERT_FALSE(aa.Get() == nullptr);
    EXPECT_EQ(3U, aa->GetAttributeCount());
    EXPECT_EQ(3U, aa->GetBufferAttributeCount());

    {
      // Vertex positions.
      std::vector<PosBov> pos_bovs;
      pos_bovs.push_back(PosBov(0, Point3f(21., 2.f, 13.f)));
      pos_bovs.push_back(PosBov(1, Point3f(21.f, 2.f, -7.f)));
      pos_bovs.push_back(PosBov(2, Point3f(-19.f, 2.f, -7.f)));
      pos_bovs.push_back(PosBov(3, Point3f(-19.f, 2.f, 13.f)));
      EXPECT_TRUE(TestBoe(aa, 0, 3U, gfx::BufferObject::kFloat, 4U, pos_bovs));
    }

    // Texture coordinates.
    EXPECT_TRUE(TestBoe(aa, 1, 2U, gfx::BufferObject::kFloat, 4U, tex_bovs));

    // Normals.
    std::vector<NormBov> norm_bovs;
    for (int i = 0; i < 4; ++i)
      norm_bovs.push_back(NormBov(i, Vector3f(0.f, 1.f, 0.f)));
    EXPECT_TRUE(TestBoe(aa, 2, 3U, gfx::BufferObject::kFloat, 4U, norm_bovs));
  }
}

TEST(ShapeUtilsTest, RectanglePlaneNormals) {
  RectangleSpec spec;

  gfx::ShapePtr rect;
  spec.plane_normal = RegularPolygonSpec::kPositiveX;
  rect = BuildRectangleShape(spec);
  EXPECT_TRUE(
      TestBoe(rect->GetAttributeArray(), 0, 3U, gfx::BufferObject::kFloat, 4U,
              std::vector<PosBov>(1, PosBov(0, Point3f(0.f, -.5f, .5f)))));

  spec.plane_normal = RegularPolygonSpec::kNegativeX;
  rect = BuildRectangleShape(spec);
  EXPECT_TRUE(
      TestBoe(rect->GetAttributeArray(), 0, 3U, gfx::BufferObject::kFloat, 4U,
              std::vector<PosBov>(1, PosBov(0, Point3f(0.f, -.5f, -0.5f)))));

  spec.plane_normal = RegularPolygonSpec::kPositiveY;
  rect = BuildRectangleShape(spec);
  EXPECT_TRUE(
      TestBoe(rect->GetAttributeArray(), 0, 3U, gfx::BufferObject::kFloat, 4U,
              std::vector<PosBov>(1, PosBov(0, Point3f(-.5f, 0.f, 0.5f)))));

  spec.plane_normal = RegularPolygonSpec::kNegativeY;
  rect = BuildRectangleShape(spec);
  EXPECT_TRUE(
      TestBoe(rect->GetAttributeArray(), 0, 3U, gfx::BufferObject::kFloat, 4U,
              std::vector<PosBov>(1, PosBov(0, Point3f(-.5f, 0.f, -0.5f)))));

  spec.plane_normal = RegularPolygonSpec::kPositiveZ;
  rect = BuildRectangleShape(spec);
  EXPECT_TRUE(
      TestBoe(rect->GetAttributeArray(), 0, 3U, gfx::BufferObject::kFloat, 4U,
              std::vector<PosBov>(1, PosBov(0, Point3f(-.5f, -.5f, 0.f)))));

  spec.plane_normal = RegularPolygonSpec::kNegativeZ;
  rect = BuildRectangleShape(spec);
  EXPECT_TRUE(
      TestBoe(rect->GetAttributeArray(), 0, 3U, gfx::BufferObject::kFloat, 4U,
              std::vector<PosBov>(1, PosBov(0, Point3f(.5f, -.5f, 0.f)))));
}

TEST(ShapeUtilsTest, RegularPolygon) {
  RegularPolygonSpec spec;

  {
    // Build with default RegularPolygonSpec.
    const int num_vertices = spec.sides + 2;
    gfx::ShapePtr rect = BuildRegularPolygonShape(spec);
    ASSERT_FALSE(rect.Get() == nullptr);

    const gfx::AttributeArrayPtr& aa = rect->GetAttributeArray();
    ASSERT_FALSE(aa.Get() == nullptr);
    EXPECT_EQ(3U, aa->GetAttributeCount());
    EXPECT_EQ(3U, aa->GetBufferAttributeCount());

    // Vertex positions.
    std::vector<PosBov> pos_bovs;
    pos_bovs.push_back(PosBov(0, Point3f(0.f, 0.f, 0.f)));
    pos_bovs.push_back(PosBov(1, Point3f(1.f, 0.f, 0.f)));
    pos_bovs.push_back(PosBov(2, Point3f(-0.5f, 0.86602f, 0.f)));
    pos_bovs.push_back(PosBov(3, Point3f(-0.5f, -0.86602f, 0.f)));
    pos_bovs.push_back(PosBov(4, Point3f(1.f, 0.f, 0.f)));
    EXPECT_TRUE(
        TestBoe(aa, 0, 3U, gfx::BufferObject::kFloat, num_vertices, pos_bovs));

    // Texture coordinates.
    std::vector<TexBov> tex_bovs;
    tex_bovs.push_back(TexBov(0, Point2f(0.5f, 0.5f)));
    tex_bovs.push_back(TexBov(1, Point2f(1.f, 0.5f)));
    tex_bovs.push_back(TexBov(2, Point2f(0.25f, 0.93301f)));
    tex_bovs.push_back(TexBov(3, Point2f(0.25f, 0.06698f)));
    tex_bovs.push_back(TexBov(4, Point2f(1.f, 0.5f)));
    EXPECT_TRUE(
        TestBoe(aa, 1, 2U, gfx::BufferObject::kFloat, num_vertices, tex_bovs));

    // Normals.
    std::vector<NormBov> norm_bovs;
    for (int i = 0; i < num_vertices; ++i)
      norm_bovs.push_back(NormBov(i, Vector3f(0.f, 0.f, 1.f)));
    EXPECT_TRUE(
        TestBoe(aa, 2, 3U, gfx::BufferObject::kFloat, num_vertices, norm_bovs));
  }

  {
    // Should create a diamond in the Y plane.
    spec.plane_normal = RegularPolygonSpec::kNegativeY;
    spec.sides = 4;
    const int num_vertices = spec.sides + 2;
    gfx::ShapePtr rect = BuildRegularPolygonShape(spec);
    ASSERT_FALSE(rect.Get() == nullptr);

    const gfx::AttributeArrayPtr& aa = rect->GetAttributeArray();
    ASSERT_FALSE(aa.Get() == nullptr);
    EXPECT_EQ(3U, aa->GetAttributeCount());
    EXPECT_EQ(3U, aa->GetBufferAttributeCount());

    // Vertex positions.
    std::vector<PosBov> pos_bovs;
    pos_bovs.push_back(PosBov(0, Point3f(0.f, 0.f, 0.f)));
    pos_bovs.push_back(PosBov(1, Point3f(1.f, 0.f, 0.f)));
    pos_bovs.push_back(PosBov(2, Point3f(0.f, 0.f, 1.f)));
    pos_bovs.push_back(PosBov(3, Point3f(-1.f, 0.f, 0.f)));
    pos_bovs.push_back(PosBov(4, Point3f(0.f, 0.f, -1.f)));
    pos_bovs.push_back(PosBov(5, Point3f(1.f, 0.f, 0.f)));
    EXPECT_TRUE(
        TestBoe(aa, 0, 3U, gfx::BufferObject::kFloat, num_vertices, pos_bovs));

    // Texture coordinates.
    std::vector<TexBov> tex_bovs;
    tex_bovs.push_back(TexBov(0, Point2f(0.5f, 0.5f)));
    tex_bovs.push_back(TexBov(1, Point2f(1.f, 0.5f)));
    tex_bovs.push_back(TexBov(2, Point2f(0.5f, 1.f)));
    tex_bovs.push_back(TexBov(3, Point2f(0.f, 0.5f)));
    tex_bovs.push_back(TexBov(4, Point2f(0.5f, 0.f)));
    tex_bovs.push_back(TexBov(5, Point2f(1.f, 0.5f)));
    EXPECT_TRUE(
        TestBoe(aa, 1, 2U, gfx::BufferObject::kFloat, num_vertices, tex_bovs));

    // Normals.
    std::vector<NormBov> norm_bovs;
    for (int i = 0; i < num_vertices; ++i)
      norm_bovs.push_back(NormBov(i, Vector3f(0.f, -1.f, 0.f)));
    EXPECT_TRUE(
        TestBoe(aa, 2, 3U, gfx::BufferObject::kFloat, num_vertices, norm_bovs));
  }

  {
    // Use a different plane normal, sides, translation, scale and rotation.
    spec.translation.Set(1.f, 2.f, 3.f);
    spec.plane_normal = RegularPolygonSpec::kPositiveX;
    spec.sides = 5;
    spec.scale = 2.f;
    spec.rotation = math::RotationMatrixAxisAngleNH(
        math::Vector3f::AxisZ(), math::Anglef::FromDegrees(90.f));
    const int num_vertices = spec.sides + 2;
    gfx::ShapePtr rect = BuildRegularPolygonShape(spec);
    ASSERT_FALSE(rect.Get() == nullptr);

    const gfx::AttributeArrayPtr& aa = rect->GetAttributeArray();
    ASSERT_FALSE(aa.Get() == nullptr);
    EXPECT_EQ(3U, aa->GetAttributeCount());
    EXPECT_EQ(3U, aa->GetBufferAttributeCount());

    // Vertex positions.
    std::vector<PosBov> pos_bovs;
    pos_bovs.push_back(PosBov(0, Point3f(1.f, 2.f, 3.f)));
    pos_bovs.push_back(PosBov(1, Point3f(1.f, 2.f, 5.f)));
    pos_bovs.push_back(PosBov(num_vertices - 1, Point3f(1.f, 2.f, 5.f)));
    EXPECT_TRUE(
        TestBoe(aa, 0, 3U, gfx::BufferObject::kFloat, num_vertices, pos_bovs));

    // Texture coordinates.
    std::vector<TexBov> tex_bovs;
    tex_bovs.push_back(TexBov(0, Point2f(0.5f, 0.5f)));
    tex_bovs.push_back(TexBov(1, Point2f(1.f, 0.5f)));
    tex_bovs.push_back(TexBov(num_vertices - 1, Point2f(1.f, 0.5f)));
    EXPECT_TRUE(
        TestBoe(aa, 1, 2U, gfx::BufferObject::kFloat, num_vertices, tex_bovs));

    // Normals.
    std::vector<NormBov> norm_bovs;
    for (int i = 0; i < num_vertices; ++i)
      norm_bovs.push_back(NormBov(i, Vector3f(0.f, 1.f, 0.f)));
    EXPECT_TRUE(
        TestBoe(aa, 2, 3U, gfx::BufferObject::kFloat, num_vertices, norm_bovs));
  }
}

TEST(ShapeUtilsTest, VertexTypes) {
  // This test uses a rectangle to verify that all vertex types are handled
  // properly.
  RectangleSpec spec;
  gfx::ShapePtr rect;

  std::vector<PosBov> pos_bovs;
  pos_bovs.push_back(PosBov(0, Point3f(-0.5f, -0.5f, 0.f)));
  pos_bovs.push_back(PosBov(1, Point3f(0.5f, -0.5f, 0.f)));
  pos_bovs.push_back(PosBov(2, Point3f(0.5f, 0.5f, 0.f)));
  pos_bovs.push_back(PosBov(3, Point3f(-0.5f, 0.5f, 0.f)));

  std::vector<TexBov> tex_bovs;
  tex_bovs.push_back(TexBov(0, Point2f(0.f, 0.f)));
  tex_bovs.push_back(TexBov(1, Point2f(1.f, 0.f)));
  tex_bovs.push_back(TexBov(2, Point2f(1.f, 1.f)));
  tex_bovs.push_back(TexBov(3, Point2f(0.f, 1.f)));

  std::vector<NormBov> norm_bovs;
  for (int i = 0; i < 4; ++i)
    norm_bovs.push_back(NormBov(i, Vector3f(0.f, 0.f, 1.f)));

  // Positions only.
  spec.vertex_type = ShapeSpec::kPosition;
  rect = BuildRectangleShape(spec);
  {
    const gfx::AttributeArrayPtr& aa = rect->GetAttributeArray();
    ASSERT_FALSE(aa.Get() == nullptr);
    EXPECT_EQ(1U, aa->GetAttributeCount());
    EXPECT_EQ(1U, aa->GetBufferAttributeCount());
    EXPECT_TRUE(TestBoe(rect->GetAttributeArray(), 0, 3U,
                        gfx::BufferObject::kFloat, 4U, pos_bovs));
  }

  // Positions and texture coordinates.
  spec.vertex_type = ShapeSpec::kPositionTexCoords;
  rect = BuildRectangleShape(spec);
  {
    const gfx::AttributeArrayPtr& aa = rect->GetAttributeArray();
    ASSERT_FALSE(aa.Get() == nullptr);
    EXPECT_EQ(2U, aa->GetAttributeCount());
    EXPECT_EQ(2U, aa->GetBufferAttributeCount());
    EXPECT_TRUE(TestBoe(rect->GetAttributeArray(), 0, 3U,
                        gfx::BufferObject::kFloat, 4U, pos_bovs));
    EXPECT_TRUE(TestBoe(rect->GetAttributeArray(), 1, 2U,
                        gfx::BufferObject::kFloat, 4U, tex_bovs));
  }

  // Positions and normals.
  spec.vertex_type = ShapeSpec::kPositionNormal;
  rect = BuildRectangleShape(spec);
  {
    const gfx::AttributeArrayPtr& aa = rect->GetAttributeArray();
    ASSERT_FALSE(aa.Get() == nullptr);
    EXPECT_EQ(2U, aa->GetAttributeCount());
    EXPECT_EQ(2U, aa->GetBufferAttributeCount());
    EXPECT_TRUE(TestBoe(rect->GetAttributeArray(), 0, 3U,
                        gfx::BufferObject::kFloat, 4U, pos_bovs));
    EXPECT_TRUE(TestBoe(rect->GetAttributeArray(), 1, 3U,
                        gfx::BufferObject::kFloat, 4U, norm_bovs));
  }

  // The ShapeSpec::kPositionTexCoordsNormal case is the default and has been
  // tested already.
}

TEST(ShapeUtilsTest, Box) {
  BoxSpec spec;

  // Texture coordinates and normals are the same regardless of any spec
  // settings.  These sample a few selected values for a box.
  std::vector<TexBov> tex_bovs;
  tex_bovs.push_back(TexBov(0, Point2f(0.f, 0.f)));
  tex_bovs.push_back(TexBov(7, Point2f(0.f, 1.f)));
  tex_bovs.push_back(TexBov(18, Point2f(1.f, 1.f)));
  tex_bovs.push_back(TexBov(23, Point2f(0.f, 1.f)));

  std::vector<NormBov> norm_bovs;
  norm_bovs.push_back(NormBov(0, Vector3f(0.f, 0.f, 1.f)));
  norm_bovs.push_back(NormBov(5, Vector3f(0.f, 0.f, -1.f)));
  norm_bovs.push_back(NormBov(14, Vector3f(-1.f, 0.f, 0.f)));
  norm_bovs.push_back(NormBov(22, Vector3f(0.f, -1.f, 0.f)));

  {
    // Build with default BoxSpec.
    gfx::ShapePtr box = BuildBoxShape(spec);
    ASSERT_FALSE(box.Get() == nullptr);

    const gfx::AttributeArrayPtr& aa = box->GetAttributeArray();
    ASSERT_FALSE(aa.Get() == nullptr);
    EXPECT_EQ(3U, aa->GetAttributeCount());
    EXPECT_EQ(3U, aa->GetBufferAttributeCount());

    // Vertex positions.
    std::vector<PosBov> pos_bovs;
    pos_bovs.push_back(PosBov(0, Point3f(-0.5f, -0.5f, 0.5f)));
    pos_bovs.push_back(PosBov(1, Point3f(0.5f, -0.5f, 0.5f)));
    pos_bovs.push_back(PosBov(13, Point3f(-0.5f, -0.5f, 0.5f)));
    pos_bovs.push_back(PosBov(21, Point3f(0.5f, -0.5f, -0.5f)));
    EXPECT_TRUE(TestBoe(aa, 0, 3U, gfx::BufferObject::kFloat, 24U, pos_bovs));

    // Texture coordinates.
    EXPECT_TRUE(TestBoe(aa, 1, 2U, gfx::BufferObject::kFloat, 24U, tex_bovs));

    // Normals.
    EXPECT_TRUE(TestBoe(aa, 2, 3U, gfx::BufferObject::kFloat, 24U, norm_bovs));
  }

  {
    // Use a different size, translation, scale, and rotation.
    spec.translation.Set(1.f, 2.f, 3.f);
    spec.size.Set(10.f, 20.f, 30.f);
    spec.scale = 2.f;
    spec.rotation = math::RotationMatrixAxisAngleNH(
        math::Vector3f::AxisX(), math::Anglef::FromDegrees(90.f));
    gfx::ShapePtr box = BuildBoxShape(spec);
    ASSERT_FALSE(box.Get() == nullptr);

    const gfx::AttributeArrayPtr& aa = box->GetAttributeArray();
    ASSERT_FALSE(aa.Get() == nullptr);
    EXPECT_EQ(3U, aa->GetAttributeCount());
    EXPECT_EQ(3U, aa->GetBufferAttributeCount());

    // Vertex positions.
    std::vector<PosBov> pos_bovs;
    pos_bovs.push_back(PosBov(0, Point3f(-9.f, -28.f, -17.f)));
    pos_bovs.push_back(PosBov(1, Point3f(11.f, -28.f, -17.f)));
    pos_bovs.push_back(PosBov(13, Point3f(-9.f, -28.f, -17.f)));
    pos_bovs.push_back(PosBov(21, Point3f(11.f, 32.f, -17.f)));
    EXPECT_TRUE(TestBoe(aa, 0, 3U, gfx::BufferObject::kFloat, 24U, pos_bovs));

    // Texture coordinates.
    EXPECT_TRUE(TestBoe(aa, 1, 2U, gfx::BufferObject::kFloat, 24U, tex_bovs));

    // Normals.
    std::vector<NormBov> norm_bovs;
    norm_bovs.push_back(NormBov(0, Vector3f(0.f, -1.f, 0.f)));
    norm_bovs.push_back(NormBov(5, Vector3f(0.f, 1.f, 0.f)));
    norm_bovs.push_back(NormBov(14, Vector3f(-1.f, 0.f, 0.f)));
    norm_bovs.push_back(NormBov(22, Vector3f(0.f, 0.f, -1.f)));
    EXPECT_TRUE(TestBoe(aa, 2, 3U, gfx::BufferObject::kFloat, 24U, norm_bovs));
  }
}

TEST(ShapeUtilsTest, Ellipsoid) {
  EllipsoidSpec spec;

  {
    // Build with default EllipsoidSpec. This has 10 bands and 10 sectors for a
    // total of 11 * 11 = 121 points.
    gfx::ShapePtr ellipsoid = BuildEllipsoidShape(spec);
    ASSERT_FALSE(ellipsoid.Get() == nullptr);

    const gfx::AttributeArrayPtr& aa = ellipsoid->GetAttributeArray();
    ASSERT_FALSE(aa.Get() == nullptr);
    EXPECT_EQ(3U, aa->GetAttributeCount());
    EXPECT_EQ(3U, aa->GetBufferAttributeCount());

    // Vertex positions. (First and last N are at the north and south poles.)
    std::vector<PosBov> pos_bovs;
    pos_bovs.push_back(PosBov(0, Point3f(0.f, .5f, 0.f)));
    pos_bovs.push_back(PosBov(10, Point3f(0.f, .5f, 0.f)));
    pos_bovs.push_back(PosBov(110, Point3f(0.f, -.5f, 0.f)));
    pos_bovs.push_back(PosBov(120, Point3f(0.f, -.5f, 0.f)));
    EXPECT_TRUE(TestBoe(aa, 0, 3U, gfx::BufferObject::kFloat, 121U, pos_bovs));

    // Texture coordinates.
    std::vector<TexBov> tex_bovs;
    tex_bovs.push_back(TexBov(0, Point2f(0.f, 1.f)));
    tex_bovs.push_back(TexBov(10, Point2f(1.f, 1.f)));
    tex_bovs.push_back(TexBov(110, Point2f(0.f, 0.f)));
    tex_bovs.push_back(TexBov(120, Point2f(1.f, 0.f)));
    EXPECT_TRUE(TestBoe(aa, 1, 2U, gfx::BufferObject::kFloat, 121U, tex_bovs));

    // Normals.
    std::vector<NormBov> norm_bovs;
    norm_bovs.push_back(NormBov(0, Vector3f(0.f, 1.f, 0.f)));
    norm_bovs.push_back(NormBov(10, Vector3f(0.f, 1.f, 0.f)));
    norm_bovs.push_back(NormBov(110, Vector3f(0.f, -1.f, 0.f)));
    norm_bovs.push_back(NormBov(120, Vector3f(0.f, -1.f, 0.f)));
    EXPECT_TRUE(TestBoe(aa, 2, 3U, gfx::BufferObject::kFloat, 121U, norm_bovs));
  }

  {
    // Use a different number of bands/sectors, and a different size,
    // translation, scale, and rotation.
    spec.band_count = 4;
    spec.sector_count = 8;
    spec.translation.Set(1.f, 2.f, 3.f);
    spec.size.Set(10.f, 20.f, 30.f);
    spec.scale = 2.f;
    spec.rotation = math::RotationMatrixAxisAngleNH(
        math::Vector3f::AxisY(), math::Anglef::FromDegrees(180.f));
    gfx::ShapePtr ellipsoid = BuildEllipsoidShape(spec);
    ASSERT_FALSE(ellipsoid.Get() == nullptr);

    const gfx::AttributeArrayPtr& aa = ellipsoid->GetAttributeArray();
    ASSERT_FALSE(aa.Get() == nullptr);
    EXPECT_EQ(3U, aa->GetAttributeCount());
    EXPECT_EQ(3U, aa->GetBufferAttributeCount());

    // Vertex positions.
    std::vector<PosBov> pos_bovs;
    pos_bovs.push_back(PosBov(0, Point3f(1.f, 22.f, 3.f)));
    pos_bovs.push_back(PosBov(22, Point3f(1.f, 2.f, -27.f)));
    pos_bovs.push_back(PosBov(32, Point3f(-4.f, -12.14214f, -12.f)));
    pos_bovs.push_back(PosBov(43, Point3f(1.f, -18.f, 3.f)));
    EXPECT_TRUE(TestBoe(aa, 0, 3U, gfx::BufferObject::kFloat, 45U, pos_bovs));

    // Texture coordinates.
    std::vector<TexBov> tex_bovs;
    tex_bovs.push_back(TexBov(0, Point2f(0.f, 1.f)));
    tex_bovs.push_back(TexBov(22, Point2f(.5f, .5f)));
    tex_bovs.push_back(TexBov(32, Point2f(.625f, .25f)));
    tex_bovs.push_back(TexBov(43, Point2f(.875f, 0.f)));
    EXPECT_TRUE(TestBoe(aa, 1, 2U, gfx::BufferObject::kFloat, 45U, tex_bovs));

    // Normals.
    std::vector<NormBov> norm_bovs;
    norm_bovs.push_back(NormBov(0, Vector3f(0.f, 1.f, 0.f)));
    norm_bovs.push_back(NormBov(22, Vector3f(0.f, 0.f, -1.f)));
    norm_bovs.push_back(NormBov(43, Vector3f(0.f, -1.f, 0.f)));
    EXPECT_TRUE(TestBoe(aa, 2, 3U, gfx::BufferObject::kFloat, 45U, norm_bovs));
  }
}

TEST(ShapeUtilsTest, EllipsoidWithCustomLongitude) {
  // Construct a portion of a sphere with half the longitudinal range.
  // This has 10 bands and 10 sectors for a total of 11 * 11 = 121 points.
  EllipsoidSpec spec;
  spec.longitude_start = math::Anglef::FromDegrees(0.f);
  spec.longitude_end = math::Anglef::FromDegrees(180.f);

  gfx::ShapePtr ellipsoid = BuildEllipsoidShape(spec);
  ASSERT_FALSE(ellipsoid.Get() == nullptr);

  const gfx::AttributeArrayPtr& aa = ellipsoid->GetAttributeArray();
  ASSERT_FALSE(aa.Get() == nullptr);
  EXPECT_EQ(3U, aa->GetAttributeCount());
  EXPECT_EQ(3U, aa->GetBufferAttributeCount());

  // Vertex positions.
  // First and last N are at the north and south poles.
  std::vector<PosBov> pos_bovs;
  pos_bovs.push_back(PosBov(0, Point3f(0.f, .5f, 0.f)));
  pos_bovs.push_back(PosBov(10, Point3f(0.f, .5f, 0.f)));
  pos_bovs.push_back(PosBov(110, Point3f(0.f, -.5f, 0.f)));
  pos_bovs.push_back(PosBov(120, Point3f(0.f, -.5f, 0.f)));
  // First and last points of the middle band are on opposite sides of the
  // equator line.
  pos_bovs.push_back(PosBov(55, Point3f(0.f, 0.f, -0.5f)));
  pos_bovs.push_back(PosBov(65, Point3f(0.f, 0.f, 0.5f)));
  EXPECT_TRUE(TestBoe(aa, 0, 3U, gfx::BufferObject::kFloat, 121U, pos_bovs));

  // Texture coordinates and normals at north and south poles behave the same
  // as for the default ellipsoid.
  std::vector<TexBov> tex_bovs;
  tex_bovs.push_back(TexBov(0, Point2f(0.f, 1.f)));
  tex_bovs.push_back(TexBov(10, Point2f(1.f, 1.f)));
  tex_bovs.push_back(TexBov(110, Point2f(0.f, 0.f)));
  tex_bovs.push_back(TexBov(120, Point2f(1.f, 0.f)));
  EXPECT_TRUE(TestBoe(aa, 1, 2U, gfx::BufferObject::kFloat, 121U, tex_bovs));

  // Normals.
  std::vector<NormBov> norm_bovs;
  norm_bovs.push_back(NormBov(0, Vector3f(0.f, 1.f, 0.f)));
  norm_bovs.push_back(NormBov(10, Vector3f(0.f, 1.f, 0.f)));
  norm_bovs.push_back(NormBov(110, Vector3f(0.f, -1.f, 0.f)));
  norm_bovs.push_back(NormBov(120, Vector3f(0.f, -1.f, 0.f)));
  EXPECT_TRUE(TestBoe(aa, 2, 3U, gfx::BufferObject::kFloat, 121U, norm_bovs));
}

TEST(ShapeUtilsTest, EllipsoidWithCustomLongitudeAndLatitude) {
  // Construct a portion of a sphere that represents one eighth of the full
  // sphere -- half of the latitudinal range (north pole to equator line) and
  // one fourth of the longitudinal range (-Z to -X).
  EllipsoidSpec spec;
  spec.longitude_start = math::Anglef::FromDegrees(0.f);
  spec.longitude_end = math::Anglef::FromDegrees(90.f);
  spec.latitude_start = math::Anglef::FromDegrees(0.f);
  spec.latitude_end = math::Anglef::FromDegrees(90.f);

  gfx::ShapePtr ellipsoid = BuildEllipsoidShape(spec);
  ASSERT_FALSE(ellipsoid.Get() == nullptr);

  const gfx::AttributeArrayPtr& aa = ellipsoid->GetAttributeArray();
  ASSERT_FALSE(aa.Get() == nullptr);
  EXPECT_EQ(3U, aa->GetAttributeCount());
  EXPECT_EQ(3U, aa->GetBufferAttributeCount());

  // Vertex positions.
  // First and last points of first band are at the north pole.
  std::vector<PosBov> pos_bovs;
  pos_bovs.push_back(PosBov(0, Point3f(0.f, .5f, 0.f)));
  pos_bovs.push_back(PosBov(10, Point3f(0.f, .5f, 0.f)));

  // Points on the last band should be on the equator line:
  // * first point at the -Z seam.
  // * last point at -X, 90 degrees eastward of first point.
  // * middle point should be in-between, 45 degrees eastward of first point.
  pos_bovs.push_back(PosBov(110, Point3f(0.f, 0.f, -0.5f)));
  pos_bovs.push_back(PosBov(120, Point3f(-0.5f, 0.f, 0.0f)));
  pos_bovs.push_back(PosBov(115, Point3f(-kSqrt2Over4, 0.f, -kSqrt2Over4)));
  EXPECT_TRUE(TestBoe(aa, 0, 3U, gfx::BufferObject::kFloat, 121U, pos_bovs));

  // Texture coordinates should behave the same as for the default ellipsoid.
  std::vector<TexBov> tex_bovs;
  tex_bovs.push_back(TexBov(0, Point2f(0.f, 1.f)));
  tex_bovs.push_back(TexBov(10, Point2f(1.f, 1.f)));
  tex_bovs.push_back(TexBov(110, Point2f(0.f, 0.f)));
  tex_bovs.push_back(TexBov(120, Point2f(1.f, 0.f)));
  EXPECT_TRUE(TestBoe(aa, 1, 2U, gfx::BufferObject::kFloat, 121U, tex_bovs));

  // Normals.
  std::vector<NormBov> norm_bovs;
  norm_bovs.push_back(NormBov(0, Vector3f(0.f, 1.f, 0.f)));
  norm_bovs.push_back(NormBov(10, Vector3f(0.f, 1.f, 0.f)));
  norm_bovs.push_back(NormBov(110, Vector3f(0.f, 0.f, -1.f)));
  norm_bovs.push_back(NormBov(120, Vector3f(-1.f, 0.f, 0.f)));
  EXPECT_TRUE(TestBoe(aa, 2, 3U, gfx::BufferObject::kFloat, 121U, norm_bovs));
}

TEST(ShapeUtilsTest, EllipsoidWithCustomLatLonAndInvertedDirection) {
  // Construct a portion of a sphere that represents one eighth of the full
  // sphere -- half of the latitudinal range (south pole to equator line) and
  // one fourth of the longitudinal range (-Z to +X).
  EllipsoidSpec spec;
  spec.longitude_start = math::Anglef::FromDegrees(0.f);
  spec.longitude_end = math::Anglef::FromDegrees(-90.f);
  spec.latitude_start = math::Anglef::FromDegrees(0.f);
  spec.latitude_end = math::Anglef::FromDegrees(-90.f);

  gfx::ShapePtr ellipsoid = BuildEllipsoidShape(spec);
  ASSERT_FALSE(ellipsoid.Get() == nullptr);

  const gfx::AttributeArrayPtr& aa = ellipsoid->GetAttributeArray();
  ASSERT_FALSE(aa.Get() == nullptr);
  EXPECT_EQ(3U, aa->GetAttributeCount());
  EXPECT_EQ(3U, aa->GetBufferAttributeCount());

  // Vertex positions.
  // First and last points of first band are at the south pole.
  std::vector<PosBov> pos_bovs;
  pos_bovs.push_back(PosBov(0, Point3f(0.f, -.5f, 0.f)));
  pos_bovs.push_back(PosBov(10, Point3f(0.f, -.5f, 0.f)));

  // Points on the last band should be on the equator line:
  // * first point at the -Z seam.
  // * last point at +X, 90 degrees westward of first point.
  // * middle point should be in-between, 45 degrees westward of first point.
  pos_bovs.push_back(PosBov(110, Point3f(0.f, 0.f, -0.5f)));
  pos_bovs.push_back(PosBov(120, Point3f(0.5f, 0.f, 0.0f)));
  pos_bovs.push_back(PosBov(115, Point3f(kSqrt2Over4, 0.f, -kSqrt2Over4)));
  EXPECT_TRUE(TestBoe(aa, 0, 3U, gfx::BufferObject::kFloat, 121U, pos_bovs));

  // Texture coordinates should behave the same as for the default ellipsoid.
  std::vector<TexBov> tex_bovs;
  tex_bovs.push_back(TexBov(0, Point2f(0.f, 1.f)));
  tex_bovs.push_back(TexBov(10, Point2f(1.f, 1.f)));
  tex_bovs.push_back(TexBov(110, Point2f(0.f, 0.f)));
  tex_bovs.push_back(TexBov(120, Point2f(1.f, 0.f)));
  EXPECT_TRUE(TestBoe(aa, 1, 2U, gfx::BufferObject::kFloat, 121U, tex_bovs));

  // Normals.
  std::vector<NormBov> norm_bovs;
  norm_bovs.push_back(NormBov(0, Vector3f(0.f, -1.f, 0.f)));
  norm_bovs.push_back(NormBov(10, Vector3f(0.f, -1.f, 0.f)));
  norm_bovs.push_back(NormBov(110, Vector3f(0.f, 0.f, -1.f)));
  norm_bovs.push_back(NormBov(120, Vector3f(1.f, 0.f, 0.f)));
  EXPECT_TRUE(TestBoe(aa, 2, 3U, gfx::BufferObject::kFloat, 121U, norm_bovs));
}

TEST(ShapeUtilsTest, DefaultCylinder) {
  CylinderSpec spec;

  // Build with default CylinderSpec. The shaft has 1 band and 10 sectors for
  // a total of 2 * 11 = 22 points, and each cap has 1 band for a total of
  // 2 * (1 + 11) = 24 points, for a grand total of 46 points.
  gfx::ShapePtr cylinder = BuildCylinderShape(spec);
  ASSERT_FALSE(cylinder.Get() == nullptr);

  const gfx::AttributeArrayPtr& aa = cylinder->GetAttributeArray();
  ASSERT_FALSE(aa.Get() == nullptr);
  EXPECT_EQ(3U, aa->GetAttributeCount());
  EXPECT_EQ(3U, aa->GetBufferAttributeCount());

  // Vertex positions.
  std::vector<PosBov> pos_bovs;
  pos_bovs.push_back(PosBov(0, Point3f(0.f, .5f, -.5f)));    // Top ring.
  pos_bovs.push_back(PosBov(16, Point3f(0.f, -.5f, .5f)));   // Bottom ring.
  pos_bovs.push_back(PosBov(22, Point3f(0.f, .5f, 0.f)));    // Top center.
  pos_bovs.push_back(PosBov(23, Point3f(0.f, .5f, -.5f)));   // Top cap ring.
  pos_bovs.push_back(PosBov(34, Point3f(0.f, -.5f, 0.f)));   // Bottom center.
  pos_bovs.push_back(PosBov(45, Point3f(0.f, -.5f, -.5f)));  // Bot cap ring.
  EXPECT_TRUE(TestBoe(aa, 0, 3U, gfx::BufferObject::kFloat, 46U, pos_bovs));

  // Texture coordinates.
  std::vector<TexBov> tex_bovs;
  tex_bovs.push_back(TexBov(0, Point2f(0.f, 1.f)));
  tex_bovs.push_back(TexBov(16, Point2f(.5f, 0.f)));
  tex_bovs.push_back(TexBov(22, Point2f(.5f, .5f)));
  tex_bovs.push_back(TexBov(23, Point2f(.5f, 1.f)));
  tex_bovs.push_back(TexBov(34, Point2f(.5f, .5f)));
  tex_bovs.push_back(TexBov(45, Point2f(.5f, 0.f)));
  EXPECT_TRUE(TestBoe(aa, 1, 2U, gfx::BufferObject::kFloat, 46U, tex_bovs));

  // Normals.
  std::vector<NormBov> norm_bovs;
  norm_bovs.push_back(NormBov(0, Vector3f(0.f, 0., -1.f)));
  norm_bovs.push_back(NormBov(16, Vector3f(0.f, 0.f, 1.f)));
  norm_bovs.push_back(NormBov(22, Vector3f(0.f, 1.f, 0.f)));
  norm_bovs.push_back(NormBov(23, Vector3f(0.f, 1.f, 0.f)));
  norm_bovs.push_back(NormBov(34, Vector3f(0.f, -1.f, 0.f)));
  norm_bovs.push_back(NormBov(45, Vector3f(0.f, -1.f, 0.f)));
  EXPECT_TRUE(TestBoe(aa, 2, 3U, gfx::BufferObject::kFloat, 46U, norm_bovs));
}

TEST(ShapeUtilsTest, ModifiedCylinder) {
  CylinderSpec spec;

  // Texture coordinates are the same for all of this test.
  std::vector<TexBov> tex_bovs;
  tex_bovs.push_back(TexBov(0, Point2f(0.f, 1.f)));
  tex_bovs.push_back(TexBov(7, Point2f(.5f, .5f)));
  tex_bovs.push_back(TexBov(12, Point2f(.5f, 0.f)));
  tex_bovs.push_back(TexBov(15, Point2f(.5f, .5f)));
  tex_bovs.push_back(TexBov(16, Point2f(.5f, .75f)));
  tex_bovs.push_back(TexBov(24, Point2f(1.f, .5f)));
  tex_bovs.push_back(TexBov(26, Point2f(.5f, .5f)));
  tex_bovs.push_back(TexBov(27, Point2f(.5f, .25f)));
  tex_bovs.push_back(TexBov(35, Point2f(1.f, .5f)));

  // Use a different number of bands/sectors, and a different size, center,
  // scale and rotation.
  spec.shaft_band_count = 2;
  spec.cap_band_count = 2;
  spec.sector_count = 4;
  spec.translation.Set(1.f, 2.f, 3.f);
  spec.scale = 2.f;
  spec.rotation = math::RotationMatrixAxisAngleNH(
      math::Vector3f::AxisY(), math::Anglef::FromDegrees(180.f));
  spec.top_radius = 10.f;
  spec.bottom_radius = 30.f;
  spec.height = 20.f;
  {
    gfx::ShapePtr cylinder = BuildCylinderShape(spec);
    ASSERT_FALSE(cylinder.Get() == nullptr);

    const gfx::AttributeArrayPtr& aa = cylinder->GetAttributeArray();
    ASSERT_FALSE(aa.Get() == nullptr);
    EXPECT_EQ(3U, aa->GetAttributeCount());
    EXPECT_EQ(3U, aa->GetBufferAttributeCount());

    // Vertex positions.
    std::vector<PosBov> pos_bovs;
    pos_bovs.push_back(PosBov(0, Point3f(1.f, 22.f, 23.f)));     // Top ring.
    pos_bovs.push_back(PosBov(7, Point3f(1.f, 2.f, -37.f)));     // Mid ring.
    pos_bovs.push_back(PosBov(12, Point3f(1.f, -18.f, -57.f)));  // Bot ring.
    pos_bovs.push_back(PosBov(15, Point3f(1.f, 22.f, 3.f)));     // Tcap cen.
    pos_bovs.push_back(PosBov(16, Point3f(1.f, 22.f, 13.)));     // Tcap md r.
    pos_bovs.push_back(PosBov(24, Point3f(-19.f, 22.f, 3.f)));   // Tcap out r.
    pos_bovs.push_back(PosBov(26, Point3f(1.f, -18.f, 3.f)));    // Bcap cen.
    pos_bovs.push_back(PosBov(27, Point3f(1.f, -18.f, 33.f)));   // Bcap md r.
    pos_bovs.push_back(PosBov(35, Point3f(-59.f, -18.f, 3.f)));  // Bcap out r.
    EXPECT_TRUE(TestBoe(aa, 0, 3U, gfx::BufferObject::kFloat, 37U, pos_bovs));

    // Texture coordinates.
    EXPECT_TRUE(TestBoe(aa, 1, 2U, gfx::BufferObject::kFloat, 37U, tex_bovs));

    // Normals.
    std::vector<NormBov> norm_bovs;
    const float s2 = 0.5f * sqrtf(2.f);
    norm_bovs.push_back(NormBov(0, Vector3f(0.f, s2, s2)));
    norm_bovs.push_back(NormBov(7, Vector3f(0.f, s2, -s2)));
    norm_bovs.push_back(NormBov(12, Vector3f(0.f, s2, -s2)));
    norm_bovs.push_back(NormBov(15, Vector3f(0.f, 1.f, 0.f)));
    norm_bovs.push_back(NormBov(16, Vector3f(0.f, 1.f, 0.f)));
    norm_bovs.push_back(NormBov(24, Vector3f(0.f, 1.f, 0.f)));
    norm_bovs.push_back(NormBov(26, Vector3f(0.f, -1.f, 0.f)));
    norm_bovs.push_back(NormBov(27, Vector3f(0.f, -1.f, 0.f)));
    norm_bovs.push_back(NormBov(35, Vector3f(0.f, -1.f, 0.f)));
    EXPECT_TRUE(TestBoe(aa, 2, 3U, gfx::BufferObject::kFloat, 37U, norm_bovs));
  }

  spec.scale = 1.f;
  spec.rotation = math::Matrix3f::Identity();
  // Invert radii (top > bottom) for full coverage.
  spec.top_radius = 30.f;
  spec.bottom_radius = 10.f;
  {
    gfx::ShapePtr cylinder = BuildCylinderShape(spec);
    ASSERT_FALSE(cylinder.Get() == nullptr);

    const gfx::AttributeArrayPtr& aa = cylinder->GetAttributeArray();
    ASSERT_FALSE(aa.Get() == nullptr);
    EXPECT_EQ(3U, aa->GetAttributeCount());
    EXPECT_EQ(3U, aa->GetBufferAttributeCount());

    // Vertex positions.
    std::vector<PosBov> pos_bovs;
    pos_bovs.push_back(PosBov(0, Point3f(1.f, 12.f, -27.f)));  // Top ring.
    pos_bovs.push_back(PosBov(7, Point3f(1.f, 2.f, 23.f)));    // Middle ring.
    pos_bovs.push_back(PosBov(12, Point3f(1.f, -8.f, 13.f)));  // Bottom ring.
    pos_bovs.push_back(PosBov(15, Point3f(1.f, 12.f, 3.f)));   // Tcap center.
    pos_bovs.push_back(PosBov(16, Point3f(1.f, 12.f, -12.)));  // Tcap md ring.
    pos_bovs.push_back(PosBov(24, Point3f(31.f, 12.f, 3.f)));  // Tcap out ring.
    pos_bovs.push_back(PosBov(26, Point3f(1.f, -8.f, 3.f)));   // Bcap center.
    pos_bovs.push_back(PosBov(27, Point3f(1.f, -8.f, -2.f)));  // Bcap md ring.
    pos_bovs.push_back(PosBov(35, Point3f(11.f, -8.f, 3.f)));  // Bcap out ring.
    EXPECT_TRUE(TestBoe(aa, 0, 3U, gfx::BufferObject::kFloat, 37U, pos_bovs));

    // Texture coordinates.
    EXPECT_TRUE(TestBoe(aa, 1, 2U, gfx::BufferObject::kFloat, 37U, tex_bovs));

    // Normals.
    std::vector<NormBov> norm_bovs;
    const float s2 = 0.5f * sqrtf(2.f);
    norm_bovs.push_back(NormBov(0, Vector3f(0.f, -s2, -s2)));
    norm_bovs.push_back(NormBov(7, Vector3f(0.f, -s2, s2)));
    norm_bovs.push_back(NormBov(12, Vector3f(0.f, -s2, s2)));
    norm_bovs.push_back(NormBov(15, Vector3f(0.f, 1.f, 0.f)));
    norm_bovs.push_back(NormBov(16, Vector3f(0.f, 1.f, 0.f)));
    norm_bovs.push_back(NormBov(24, Vector3f(0.f, 1.f, 0.f)));
    norm_bovs.push_back(NormBov(26, Vector3f(0.f, -1.f, 0.f)));
    norm_bovs.push_back(NormBov(27, Vector3f(0.f, -1.f, 0.f)));
    norm_bovs.push_back(NormBov(35, Vector3f(0.f, -1.f, 0.f)));
    EXPECT_TRUE(TestBoe(aa, 2, 3U, gfx::BufferObject::kFloat, 37U, norm_bovs));
  }

  {
    // Turn top cap off.
    spec.has_top_cap = false;
    gfx::ShapePtr cylinder = BuildCylinderShape(spec);
    std::vector<PosBov> pos_bovs;
    // Index 15 should now be the bottom cap center.
    pos_bovs.push_back(PosBov(15, Point3f(1.f, -8.f, 3.f)));
    EXPECT_TRUE(TestBoe(cylinder->GetAttributeArray(), 0, 3U,
                        gfx::BufferObject::kFloat, 26U, pos_bovs));

    // Turn both caps off.
    spec.has_bottom_cap = false;
    cylinder = BuildCylinderShape(spec);
    pos_bovs.clear();
    // Only the shaft should remain.
    EXPECT_TRUE(TestBoe(cylinder->GetAttributeArray(), 0, 3U,
                        gfx::BufferObject::kFloat, 15U, pos_bovs));

    // Turn top cap on, leave bottom cap off.
    spec.has_top_cap = true;
    cylinder = BuildCylinderShape(spec);
    pos_bovs.clear();
    // Index 15 should be the top cap center.
    pos_bovs.push_back(PosBov(15, Point3f(1.f, 12.f, 3.f)));
    EXPECT_TRUE(TestBoe(cylinder->GetAttributeArray(), 0, 3U,
                        gfx::BufferObject::kFloat, 26U, pos_bovs));

    // Set top radius to 0, make sure top cap is gone.
    spec.has_top_cap = spec.has_bottom_cap = true;
    spec.top_radius = 0.f;
    cylinder = BuildCylinderShape(spec);
    pos_bovs.clear();
    // Index 15 should be the bottom cap center.
    pos_bovs.push_back(PosBov(15, Point3f(1.f, -8.f, 3.f)));
    EXPECT_TRUE(TestBoe(cylinder->GetAttributeArray(), 0, 3U,
                        gfx::BufferObject::kFloat, 26U, pos_bovs));

    // Same for bottom radius.
    spec.bottom_radius = 0.f;
    cylinder = BuildCylinderShape(spec);
    pos_bovs.clear();
    // Only the shaft should remain.
    EXPECT_TRUE(TestBoe(cylinder->GetAttributeArray(), 0, 3U,
                        gfx::BufferObject::kFloat, 15U, pos_bovs));
  }
}

void VerifyExternalModelLoading(ExternalShapeSpec spec,
                                const std::string& base_name,
                                const std::vector<PosBov>& vertices,
                                const std::vector<NormBov>& normals,
                                const std::vector<TexBov>& texcoords) {
  static const char* kExtensions[] = {"3ds", "dae", "lwo", "obj", "off"};
  for (int i = 0; i <= ExternalShapeSpec::kOff; ++i) {
    const std::string asset_name = base_name + kExtensions[i];
    SCOPED_TRACE("Testing asset " + asset_name);
    const std::string& asset_data =
        base::ZipAssetManager::GetFileData(asset_name);
    EXPECT_TRUE(!base::IsInvalidReference(asset_data));
    std::istringstream in;
    if (kExtensions[i] == std::string("off") ||
        kExtensions[i] == std::string("obj")) {
      // Text-based formats need to be sanitized, as though the file were
      // opened in text mode.
      in.str(base::testing::SanitizeLineEndings(asset_data));
    } else {
      in.str(asset_data);
    }

    spec.format = static_cast<ExternalShapeSpec::Format>(i);
    gfx::ShapePtr box = LoadExternalShape(spec, in);
    EXPECT_TRUE(box.Get() != nullptr);

    const gfx::AttributeArrayPtr& aa = box->GetAttributeArray();
    ASSERT_FALSE(aa.Get() == nullptr);
    EXPECT_EQ(3U, aa->GetAttributeCount());
    EXPECT_EQ(3U, aa->GetBufferAttributeCount());

    EXPECT_TRUE(TestBoe(aa, 0, 3U, gfx::BufferObject::kFloat, 24U, vertices));

    // Normals.
    if (spec.format == ExternalShapeSpec::kDae ||
        spec.format == ExternalShapeSpec::kObj)
      EXPECT_TRUE(TestBoe(aa, 2, 3U, gfx::BufferObject::kFloat, 24U, normals));

    // Texture coordinates.
    if (spec.format != ExternalShapeSpec::kOff) {
      EXPECT_TRUE(
          TestBoe(aa, 1, 2U, gfx::BufferObject::kFloat, 24U, texcoords));
    }
  }
}

TEST(ShapeUtilsTest, ExternalFormats) {
  ShapeUtilsTest::RegisterAssets();

  // Texture coordinates are the same regardless of any spec
  // settings.  These sample a few selected values for a box.
  std::vector<TexBov> tex_bovs{
      {0, {0.f, 0.f}}, {7, {0.f, 1.f}}, {18, {1.f, 1.f}}, {23, {0.f, 1.f}},
  };

  // Normals are affected by rotations, but not translation or scaling.
  std::vector<NormBov> norm_bovs{
      {0, {0.f, 0.f, 1.f}},
      {5, {0.f, 0.f, -1.f}},
      {14, {-1.f, 0.f, 0.f}},
      {22, {0.f, -1.f, 0.f}},
  };

  std::vector<NormBov> rotated_norm_bovs{
      {0, {0.f, 0.f, 1.f}},
      {5, {0.f, 0.f, -1.f}},
      {14, {0.f, 1.f, 0.f}},
      {22, {-1.f, 0.f, 0.f}},
  };

  // Vertex positions.
  std::vector<PosBov> pos_bovs{
      {0, {-0.5f, -0.5f, 0.5f}},
      {1, {0.5f, -0.5f, 0.5f}},
      {13, {-0.5f, -0.5f, 0.5f}},
      {21, {0.5f, -0.5f, -0.5f}},
  };

  // Non-centered vertex positions.
  std::vector<PosBov> non_centered_pos_bovs{
      {0, {-0.25f, -0.25f, 0.75f}},
      {1, {0.75f, -0.25f, 0.75f}},
      {13, {-0.25f, -0.25f, 0.75f}},
      {21, {0.75f, -0.25f, -0.25f}},
  };

  // Translated vertex positions.
  std::vector<PosBov> translated_pos_bovs{
      {0, {0.75f, 0.75f, 1.75f}},
      {1, {1.75f, 0.75f, 1.75f}},
      {13, {0.75f, 0.75f, 1.75f}},
      {21, {1.75f, 0.75f, 0.75f}},
  };

  // Centered and rotated vertex positions (rotate 270 deg. about Z).
  std::vector<PosBov> centered_rotated_pos_bovs{
      {0, {-0.5f, 0.5f, 0.5f}},
      {1, {-0.5f, -0.5f, 0.5f}},
      {13, {-0.5f, 0.5f, 0.5f}},
      {21, {-0.5f, -0.5f, -0.5f}},
  };

  // Vertex position after complex transform: scale x2,
  // rotate 270 deg. about Z, translate by Vector3f(1, 2, 3).
  std::vector<PosBov> transformed_pos_bovs{
      {0, {0.5f, 2.5f, 4.5f}},
      {1, {0.5f, 0.5f, 4.5f}},
      {13, {0.5f, 2.5f, 4.5f}},
      {21, {0.5f, 0.5f, 2.5f}},
  };

  // Same as above, but centered first.
  std::vector<PosBov> centered_transformed_pos_bovs{
      {0, {0.f, 3.f, 4.f}},
      {1, {0.f, 1.f, 4.f}},
      {13, {0.f, 3.f, 4.f}},
      {21, {0.f, 1.f, 2.f}},
  };

  const std::string base_name("model.");

  {
    SCOPED_TRACE("Load centered");
    ExternalShapeSpec spec;
    VerifyExternalModelLoading(spec, base_name, pos_bovs, norm_bovs, tex_bovs);
  }
  {
    SCOPED_TRACE("Load without centering");
    ExternalShapeSpec spec;
    spec.center_at_origin = false;
    VerifyExternalModelLoading(spec, base_name, non_centered_pos_bovs,
                               norm_bovs, tex_bovs);
  }
  {
    SCOPED_TRACE("Load translated");
    ExternalShapeSpec spec;
    spec.center_at_origin = false;
    spec.translation = Point3f::Fill(1.f);
    VerifyExternalModelLoading(spec, base_name, translated_pos_bovs, norm_bovs,
                               tex_bovs);
  }
  {
    SCOPED_TRACE("Load centered and rotated");
    ExternalShapeSpec spec;
    spec.rotation = ion::math::RotationMatrixAxisAngleNH(
        Vector3f::AxisZ(), ion::math::Anglef::FromDegrees(270.f));
    VerifyExternalModelLoading(spec, base_name, centered_rotated_pos_bovs,
                               rotated_norm_bovs, tex_bovs);
  }
  {
    SCOPED_TRACE("Load transformed");
    ExternalShapeSpec spec;
    spec.center_at_origin = false;
    spec.scale = 2.0f;
    spec.rotation = ion::math::RotationMatrixAxisAngleNH(
        Vector3f::AxisZ(), ion::math::Anglef::FromDegrees(270.f));
    spec.translation = Point3f(1.0f, 2.0f, 3.0f);
    VerifyExternalModelLoading(spec, base_name, transformed_pos_bovs,
                               rotated_norm_bovs, tex_bovs);
  }
  {
    SCOPED_TRACE("Load centered and transformed");
    ExternalShapeSpec spec;
    spec.scale = 2.0f;
    spec.rotation = ion::math::RotationMatrixAxisAngleNH(
        Vector3f::AxisZ(), ion::math::Anglef::FromDegrees(270.f));
    spec.translation = Point3f(1.0f, 2.0f, 3.0f);
    VerifyExternalModelLoading(spec, base_name, centered_transformed_pos_bovs,
                               rotated_norm_bovs, tex_bovs);
  }

// These two tests take way too long on Android (> 10 min), so skip them.
// They get plenty of coverage on other platforms.
#if !defined(ION_PLATFORM_ANDROID)

  // Load mesh with indices >= 65536 into 16-bit index buffer. Expect to fail.
  {
    base::LogChecker log_checker;
    ExternalShapeSpec spec;
    spec.format = ExternalShapeSpec::kObj;
    spec.center_at_origin = false;
    const std::string& asset_data = base::testing::SanitizeLineEndings(
        base::ZipAssetManager::GetFileData("model_with_32bit_indices.obj"));
    std::istringstream in(base::testing::SanitizeLineEndings(asset_data));
    gfx::ShapePtr grid = LoadExternalShape(spec, in);
    EXPECT_FALSE(grid.Get() == nullptr);
    EXPECT_TRUE(grid->GetIndexBuffer().Get() == nullptr);
    EXPECT_TRUE(log_checker.HasMessage(
        "ERROR", "Vertex index 65536 is too large to store as uint16"));
  }

  // Load mesh with indices >= 65536 into 32-bit index buffer.
  {
    ExternalShapeSpec spec;
    spec.format = ExternalShapeSpec::kObj;
    spec.center_at_origin = false;
    spec.index_size = ExternalShapeSpec::IndexSize::k32Bit;
    const std::string& asset_data = base::testing::SanitizeLineEndings(
        base::ZipAssetManager::GetFileData("model_with_32bit_indices.obj"));
    std::istringstream in(base::testing::SanitizeLineEndings(asset_data));
    gfx::ShapePtr grid = LoadExternalShape(spec, in);
    EXPECT_FALSE(grid.Get() == nullptr);
    EXPECT_FALSE(grid->GetIndexBuffer().Get() == nullptr);

    // The mesh has 257 * 257 unique vertices.
    const uint32 kNumVertices = 257 * 257;
    // The mesh has 256 x 256 faces that get tessellated into 2 triangles each.
    const uint32 kNumIndices = 256 * 256 * 2 * 3;
    // Check the index buffer with an empy reference array, because the actual
    // indices are depend on the implemention of the importer.
    std::vector<IndexBov32> index_bovs;
    EXPECT_TRUE(
        TestIndexBuffer(grid->GetIndexBuffer(), kNumIndices, index_bovs));
    // Now we check that the vertex indices are 32-bit values by comparing the
    // largest index to the number of vertices in the mesh.
    const uint32* data = static_cast<const uint32*>(
        grid->GetIndexBuffer()->GetData()->GetData());
    EXPECT_FALSE(data == nullptr);
    uint32 max_index = 0;
    for (uint32 i = 0; i < kNumIndices; ++i) {
      max_index = std::max(max_index, data[i]);
    }
    EXPECT_EQ(kNumVertices - 1, max_index);
  }

#endif

  // Try an invalid format for coverage.
  const std::string& asset_data =
      base::ZipAssetManager::GetFileData("model.3ds");
  // The default format of the spec is invalid.
  ExternalShapeSpec spec;
  std::istringstream in(asset_data);
  gfx::ShapePtr box = LoadExternalShape(spec, in);
  EXPECT_TRUE(box.Get() == nullptr);
}

TEST(ShapeUtilsTest, PrimitiveList) {
  gfx::ShapePtr p = gfxutils::BuildPrimitivesList(
      ion::gfx::Shape::kTriangles, 6);
  EXPECT_EQ(1U, p->GetVertexRangeCount());
  EXPECT_EQ(6, p->GetVertexRange(0).GetSize());
  EXPECT_NE(nullptr, p->GetAttributeArray().Get());
  EXPECT_EQ(0U, p->GetAttributeArray()->GetAttributeCount());
}

}  // namespace gfxutils
}  // namespace ion
