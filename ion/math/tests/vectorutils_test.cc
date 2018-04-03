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

#include "ion/math/vectorutils.h"

#include <limits>

#include "ion/math/tests/testutils.h"
#include "ion/math/utils.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace math {

TEST(VectorUtils, WithoutDimension) {
  EXPECT_EQ(WithoutDimension(Vector4d(1.0, 2.0, 3.0, 4.0), 0),
            Vector3d(2.0, 3.0, 4.0));
  EXPECT_EQ(WithoutDimension(Vector4d(1.0, 2.0, 3.0, 4.0), 1),
            Vector3d(1.0, 3.0, 4.0));
  EXPECT_EQ(WithoutDimension(Vector4d(1.0, 2.0, 3.0, 4.0), 3),
            Vector3d(1.0, 2.0, 3.0));
}

TEST(VectorUtils, Dot) {
  EXPECT_EQ((2.0 * -6.0) + (3.0 * 7.5) + (4.0 * 8.0) + (-5.5 * -9.0),
            Dot(Vector4d(2.0, 3.0, 4.0, -5.5), Vector4d(-6.0, 7.5, 8.0, -9.0)));
}

TEST(VectorUtils, Cross) {
  EXPECT_EQ(Vector3d(0.0, 0.0, 1.0),
            Cross(Vector3d(1.0, 0.0, 0.0), Vector3d(0.0, 1.0, 0.0)));

  EXPECT_EQ(Vector3d(-3.0, 6.0, -3.0),
            Cross(Vector3d(1.0, 2.0, 3.0), Vector3d(4.0, 5.0, 6.0)));
}

TEST(VectorUtils, LengthSquared) {
  EXPECT_EQ((2.0 * 2.0) + (3.0 * 3.0) + (4.0 * 4.0) + (-5.5 * -5.5),
            LengthSquared(Vector4d(2.0, 3.0, 4.0, -5.5)));
  EXPECT_EQ((54 * 54) + (-13 * -13) + (7 * 7),
            LengthSquared(Vector3i(54, -13, 7)));
}

TEST(VectorUtils, Length) {
  EXPECT_NEAR(Sqrt((2.0 * 2.0) + (3.0 * 3.0) + (4.0 * 4.0) + (-5.5 * -5.5)),
              Length(Vector4d(2.0, 3.0, 4.0, -5.5)), 1e-10);
  EXPECT_EQ(Sqrt((54 * 54) + (-13 * -13) + (7 * 7)),
            Length(Vector3i(54, -13, 7)));
}

TEST(VectorUtils, DistanceSquared) {
  EXPECT_EQ(25, DistanceSquared(Point2i::Zero(), Point2i(3, 4)));
  EXPECT_NEAR(
      30.0,
      DistanceSquared(Point4d(1.0, 2.0, 3.0, 4.0), Point4d(2.0, 6.0, 5.0, 7.0)),
      1e-10);
  EXPECT_NEAR(
      30.0,
      DistanceSquared(Point4d(2.0, 6.0, 5.0, 7.0), Point4d(1.0, 2.0, 3.0, 4.0)),
      1e-10);
}

TEST(VectorUtils, Distance) {
  EXPECT_EQ(5, Distance(Point2i::Zero(), Point2i(3, 4)));
  EXPECT_NEAR(
      Sqrt(30.0),
      Distance(Point4d(1.0, 2.0, 3.0, 4.0), Point4d(2.0, 6.0, 5.0, 7.0)),
      1e-10);
  EXPECT_NEAR(
      Sqrt(30.0),
      Distance(Point4d(2.0, 6.0, 5.0, 7.0), Point4d(1.0, 2.0, 3.0, 4.0)),
      1e-10);
}

TEST(VectorUtils, DistanceToSegment) {
  const Point3d start1(0, 0, 0);
  const Point3d end1(10, 0, 10);
  const Point3d start2(10, 0, 10);
  const Point3d end2(20, 0, 10);

  Point3d p, closest_point;

  // Segment is a point. Distance is just distance to that point.
  EXPECT_NEAR(10.0, DistanceToSegment(Point3d(0, 0, 10), start1, start1),
              0.0001);
  EXPECT_NEAR(200.0,
              DistanceSquaredToSegment(Point3d(10, 0, 10), start1, start1),
              0.0001);

  // Point is one of the end-points.
  p = end1;
  EXPECT_NEAR(0.0, DistanceToSegment(p, start1, end1), 0.0001);
  EXPECT_EQ(p, ClosestPointOnSegment(p, start1, end1));
  EXPECT_NEAR(0.0, DistanceSquaredToSegment(Point3d(10, 0, 10), start1, end1),
              0.0001);

  // Point is the other end-point.
  p = start1;
  EXPECT_NEAR(0.0, DistanceToSegment(p, start1, end1), 0.0001);
  EXPECT_NEAR(0.0, DistanceSquaredToSegment(p, start1, end1), 0.0001);
  EXPECT_EQ(p, ClosestPointOnSegment(p, start1, end1));

  // Point is off the line; closest point lies on the interior of line.
  p = Point3d(10, 0, 0);
  EXPECT_NEAR(Sqrt(50.), DistanceToSegment(p, start1, end1), 1e-6);
  EXPECT_NEAR(50., DistanceSquaredToSegment(p, start1, end1), 1e-6);
  EXPECT_EQ(Point3d(5, 0, 5), ClosestPointOnSegment(p, start1, end1));

  // Point is in the interior of the line.
  p = Point3d(15, 0, 10);
  EXPECT_NEAR(0.0, DistanceToSegment(p, start2, end2), 0.0001);
  EXPECT_NEAR(0.0, DistanceSquaredToSegment(p, start2, end2), 0.0001);
  EXPECT_EQ(p, ClosestPointOnSegment(p, start2, end2));

  // Point is off the line; closest point is one of the end-points.
  p = Point3d(25, 0, 20);
  EXPECT_NEAR(Sqrt(125.), DistanceToSegment(p, start2, end2), 1e-6);
  EXPECT_NEAR(25 + 100, DistanceSquaredToSegment(p, start2, end2), 1e-6);
  EXPECT_EQ(Point3d(20, 0, 10), ClosestPointOnSegment(p, start2, end2));

  // Point is off the line; closest point is the other end-point.
  p = Point3d(5, 0, 10);
  EXPECT_NEAR(5., DistanceToSegment(p, start2, end2), 1e-6);
  EXPECT_NEAR(25., DistanceSquaredToSegment(p, start2, end2), 1e-6);
  EXPECT_EQ(Point3d(10, 0, 10), ClosestPointOnSegment(p, start2, end2));

  // Test floating point version to ensure there are no double to float casts
  // which would generate compiler errors.
  EXPECT_EQ(Point3f(Point3d(10, 0, 10)),
            ClosestPointOnSegment(Point3f(p), Point3f(start2), Point3f(end2)));
}

TEST(VectorUtils, Normalize) {
  Vector4d v(2.0, 3.0, 4.0, -5.5);
  EXPECT_TRUE(Normalize(&v));
  EXPECT_NEAR(1.0, Length(v), 1e-10);

  Vector4d v_bad = Vector4d::Zero();
  EXPECT_FALSE(Normalize(&v_bad));
}

TEST(VectorUtils, Normalized) {
  EXPECT_EQ(Vector4d(1.0, 0.0, 0.0, 0.0),
            Normalized(Vector4d(10.0, 0.0, 0.0, 0.0)));
  EXPECT_EQ(Vector4d::Zero(), Normalized(Vector4d::Zero()));
}

TEST(VectorUtils, Orthogonal) {
  const Vector2d v2d(3.0, 4.0);
  Vector2d n2d = Orthogonal(v2d);
  EXPECT_GT(Length(n2d), 0.);
  EXPECT_NEAR(5., Length(n2d), 1e-8);
  EXPECT_NEAR(0., Dot(n2d, v2d), 1e-8);

  Vector2d n2d_zero = Orthogonal(Vector2d::Zero());
  EXPECT_NEAR(Length(n2d_zero), 0., 1e-8);

  const Vector3d v3d(2.0, 3.0, 4.0);
  Vector3d n3d = Orthogonal(v3d);
  EXPECT_NEAR(5., Length(n3d), 1e-8);
  EXPECT_NEAR(0., Dot(n3d, v3d), 1e-8);

  Vector3d n3d_zero = Orthogonal(Vector3d::Zero());
  EXPECT_NEAR(0., Length(n3d_zero), 1e-8);
}

TEST(VectorUtils, Orthonormal) {
  const Vector2d v2d(2.0, 3.0);
  Vector2d n2d = Orthonormal(v2d);
  EXPECT_GT(Length(n2d), 0.);
  EXPECT_NEAR(1., Length(n2d), 1e-8);
  EXPECT_NEAR(0., Dot(n2d, v2d), 1e-8);

  Vector2d n2d_zero = Orthonormal(Vector2d::Zero());
  EXPECT_NEAR(Length(n2d_zero), 0., 1e-8);

  const Vector3d v3d(2.0, 3.0, 4.0);
  Vector3d n3d = Orthonormal(v3d);
  EXPECT_NEAR(1., Length(n3d), 1e-8);
  EXPECT_NEAR(0., Dot(n3d, v3d), 1e-8);

  Vector3d n3d_zero = Orthonormal(Vector3d::Zero());
  EXPECT_NEAR(0., Length(n3d_zero), 1e-8);
}

TEST(VectorUtils, Rescale) {
  EXPECT_EQ(Vector3d(3.0, 4.0, 0.0), Rescale(Vector3d(30.0, 40.0, 0.0), 5.0));
  EXPECT_EQ(Vector2d(-6.0, 8.0), Rescale(Vector2d(-30.0, 40.0), 10.0));
  EXPECT_EQ(Vector4d::Zero(), Rescale(Vector4d(-30.0, 40.0, 50.0, 60.0), 0.0));
  EXPECT_EQ(Vector2d::Zero(), Rescale(Vector2d::Zero(), 2.));
  EXPECT_EQ(Vector3d::Zero(), Rescale(Vector3d::Zero(), 5.));
  EXPECT_EQ(Vector4d::Zero(), Rescale(Vector4d::Zero(), 9.));
}

TEST(VectorUtils, Projection) {
  Vector3d v(2.0, 3.0, 4.0);
  Vector3d onto_v(0.0, 5.0, 0.0);

  EXPECT_EQ(Vector3d(0.0, 3.0, 0.0), Projection(v, onto_v));
  EXPECT_EQ(v, Projection(v, v));
}

TEST(VectorUtils, VectorsAlmostEqual) {
  EXPECT_TRUE(VectorsAlmostEqual(Vector2i(1, -1), Vector2i(1, -1), 0));
  EXPECT_TRUE(VectorsAlmostEqual(Vector2i(1, -1), Vector2i(2, -2), 1));
  EXPECT_FALSE(VectorsAlmostEqual(Vector2i(1, -1), Vector2i(2, -3), 1));
  EXPECT_FALSE(VectorsAlmostEqual(Vector2i(1, -1), Vector2i(3, -2), 1));
  EXPECT_TRUE(VectorsAlmostEqual(Vector3d(1.0, 2.0, -3.0),
                                 Vector3d(1.0, 2.0, -3.0), 0.0));
  EXPECT_TRUE(VectorsAlmostEqual(Vector3d(1.0, 2.0, -3.0),
                                 Vector3d(1.1, 1.9, -2.9), 0.11));
  EXPECT_FALSE(VectorsAlmostEqual(Vector3d(1.0, 2.0, -3.0),
                                  Vector3d(1.2, 1.9, -2.9), 0.11));
  EXPECT_FALSE(VectorsAlmostEqual(Vector3d(1.0, 2.0, -3.0),
                                  Vector3d(1.0, 2.0, -2.8), 0.11));

  // Negative tolerance should work.
  EXPECT_TRUE(VectorsAlmostEqual(Vector3d(1.0, 2.0, -3.0),
                                 Vector3d(1.0, 2.0, -3.1), -0.2));
}

TEST(VectorUtils, MinMaxBoundPoint) {
  const Point3i p0(4, 1, 2);
  const Point3i p1(6, 4, -5);
  EXPECT_EQ(Point3i(4, 1, -5), MinBoundPoint(p0, p1));
  EXPECT_EQ(Point3i(6, 4, 2), MaxBoundPoint(p0, p1));
}

TEST(PointUtils, PointsAlmostEqual) {
  using ::testing::Not;
  using ion::math::testing::IsAlmostEqual;
  EXPECT_THAT(Point2i(1, -1), IsAlmostEqual(Point2i(1, -1), 0));
  EXPECT_THAT(Point2i(1, -1), IsAlmostEqual(Point2i(2, -2), 1));
  EXPECT_THAT(Point2i(1, -1), Not(IsAlmostEqual(Point2i(2, -3), 1)));
  EXPECT_THAT(Point2i(1, -1), Not(IsAlmostEqual(Point2i(3, -2), 1)));
  EXPECT_THAT(Point3d(1.0, 2.0, -3.0),
              IsAlmostEqual(Point3d(1.0, 2.0, -3.0), 0.0));
  EXPECT_THAT(Point3d(1.0, 2.0, -3.0),
              IsAlmostEqual(Point3d(1.1, 1.9, -2.9), 0.11));
  EXPECT_THAT(Point3d(1.0, 2.0, -3.0),
              Not(IsAlmostEqual(Point3d(1.2, 1.9, -2.9), 0.11)));
  EXPECT_THAT(Point3d(1.0, 2.0, -3.0),
              Not(IsAlmostEqual(Point3d(1.0, 2.0, -2.8), 0.11)));

  // Negative tolerance should work.
  EXPECT_THAT(Point3d(1.0, 2.0, -3.0),
              IsAlmostEqual(Point3d(1.0, 2.0, -3.1), -0.2));
}

TEST(VectorUtils, Swizzle) {
  // Test all valid individual components.
  Vector4d v4_in(1.0, 2.0, 3.0, 4.0);
  Vector1d v1;

  EXPECT_TRUE(Swizzle(v4_in, "x", &v1));
  EXPECT_EQ(1.0, v1[0]);
  EXPECT_TRUE(Swizzle(v4_in, "y", &v1));
  EXPECT_EQ(2.0, v1[0]);
  EXPECT_TRUE(Swizzle(v4_in, "z", &v1));
  EXPECT_EQ(3.0, v1[0]);
  EXPECT_TRUE(Swizzle(v4_in, "w", &v1));
  EXPECT_EQ(4.0, v1[0]);
  EXPECT_TRUE(Swizzle(v4_in, "r", &v1));
  EXPECT_EQ(1.0, v1[0]);
  EXPECT_TRUE(Swizzle(v4_in, "g", &v1));
  EXPECT_EQ(2.0, v1[0]);
  EXPECT_TRUE(Swizzle(v4_in, "b", &v1));
  EXPECT_EQ(3.0, v1[0]);
  EXPECT_TRUE(Swizzle(v4_in, "a", &v1));
  EXPECT_EQ(4.0, v1[0]);
  EXPECT_TRUE(Swizzle(v4_in, "s", &v1));
  EXPECT_EQ(1.0, v1[0]);
  EXPECT_TRUE(Swizzle(v4_in, "t", &v1));
  EXPECT_EQ(2.0, v1[0]);
  EXPECT_TRUE(Swizzle(v4_in, "p", &v1));
  EXPECT_EQ(3.0, v1[0]);
  EXPECT_TRUE(Swizzle(v4_in, "q", &v1));
  EXPECT_EQ(4.0, v1[0]);
  EXPECT_TRUE(Swizzle(v4_in, "X", &v1));
  EXPECT_EQ(1.0, v1[0]);
  EXPECT_TRUE(Swizzle(v4_in, "Y", &v1));
  EXPECT_EQ(2.0, v1[0]);
  EXPECT_TRUE(Swizzle(v4_in, "Z", &v1));
  EXPECT_EQ(3.0, v1[0]);
  EXPECT_TRUE(Swizzle(v4_in, "W", &v1));
  EXPECT_EQ(4.0, v1[0]);
  EXPECT_TRUE(Swizzle(v4_in, "R", &v1));
  EXPECT_EQ(1.0, v1[0]);
  EXPECT_TRUE(Swizzle(v4_in, "G", &v1));
  EXPECT_EQ(2.0, v1[0]);
  EXPECT_TRUE(Swizzle(v4_in, "B", &v1));
  EXPECT_EQ(3.0, v1[0]);
  EXPECT_TRUE(Swizzle(v4_in, "A", &v1));
  EXPECT_EQ(4.0, v1[0]);
  EXPECT_TRUE(Swizzle(v4_in, "S", &v1));
  EXPECT_EQ(1.0, v1[0]);
  EXPECT_TRUE(Swizzle(v4_in, "T", &v1));
  EXPECT_EQ(2.0, v1[0]);
  EXPECT_TRUE(Swizzle(v4_in, "P", &v1));
  EXPECT_EQ(3.0, v1[0]);
  EXPECT_TRUE(Swizzle(v4_in, "Q", &v1));
  EXPECT_EQ(4.0, v1[0]);

  // Test multiple components.
  Vector2d v2;
  Vector3d v3;
  Vector4d v4;

  EXPECT_TRUE(Swizzle(v4_in, "xz", &v2));
  EXPECT_EQ(Vector2d(1.0, 3.0), v2);
  EXPECT_TRUE(Swizzle(v4_in, "aBr", &v3));
  EXPECT_EQ(Vector3d(4.0, 3.0, 1.0), v3);
  EXPECT_TRUE(Swizzle(v4_in, "QgYy", &v4));
  EXPECT_EQ(Vector4d(4.0, 2.0, 2.0, 2.0), v4);

  // Should work across vector types.
  Point4d p4;
  EXPECT_TRUE(Swizzle(v4_in, "bByx", &p4));
  EXPECT_EQ(Point4d(3.0, 3.0, 2.0, 1.0), p4);

  // Invalid input component dimensions.
  Vector2d v2_in(1.0, 2.0);
  EXPECT_FALSE(Swizzle(v2_in, "w", &v1));
  EXPECT_FALSE(Swizzle(v2_in, "z", &v1));
  EXPECT_FALSE(Swizzle(v2_in, "B", &v1));
  EXPECT_FALSE(Swizzle(v2_in, "a", &v1));
  EXPECT_FALSE(Swizzle(v2_in, "P", &v1));
  EXPECT_FALSE(Swizzle(v2_in, "q", &v1));
  EXPECT_FALSE(Swizzle(v2_in, "xXxz", &v4));

  // Invalid component letters.
  EXPECT_FALSE(Swizzle(v4_in, "2", &v1));
  EXPECT_FALSE(Swizzle(v4_in, "-", &v1));
  EXPECT_FALSE(Swizzle(v4_in, "f", &v1));
  EXPECT_FALSE(Swizzle(v4_in, "K", &v1));
  EXPECT_FALSE(Swizzle(v4_in, "xxx3", &v4));

  // Missing components in string cause an error.
  EXPECT_FALSE(Swizzle(v4_in, "xyz", &v4));

  // Extra components in string are ignored.
  EXPECT_TRUE(Swizzle(v4_in, "xyz", &v2));
}

TEST(VectorUtils, IsVectorFinite) {
  EXPECT_TRUE(IsVectorFinite(Point3d(1.0, 2.0, 3.0)));
  EXPECT_TRUE(IsVectorFinite(Vector4d(1.0, 2.0, 3.0, 4.0)));
  EXPECT_FALSE(IsVectorFinite(
      Point3d(1.0, std::numeric_limits<double>::infinity(), 3.0)));
  EXPECT_FALSE(IsVectorFinite(
      Vector4d(1.0, 2.0, -std::numeric_limits<double>::infinity(), 4.0)));
  EXPECT_FALSE(IsVectorFinite(Vector2d(sqrt(-1.0f), 1.0)));
}

}  // namespace math
}  // namespace ion
