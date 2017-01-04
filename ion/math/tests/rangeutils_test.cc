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

#include "ion/math/rangeutils.h"

#include "ion/math/tests/testutils.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace math {

TEST(RangeUtils, Union) {
  Range2i r0;
  Range2i r1;

  // Union with an empty Range is a no-op.
  EXPECT_TRUE(RangeUnion(r0, r1).IsEmpty());
  r0.Set(Point2i(1, 2), Point2i(5, 6));
  EXPECT_EQ(r0, RangeUnion(r0, r1));
  EXPECT_EQ(r0, RangeUnion(r1, r0));

  // Union with self or contained Range returns the same Range.
  EXPECT_EQ(r0, RangeUnion(r0, r0));
  EXPECT_EQ(r0, RangeUnion(r0, Range2i(Point2i(1, 4), Point2i(4, 5))));

  // Various real unions.
  r1.Set(Point2i(0, 3), Point2i(4, 5));
  EXPECT_EQ(Range2i(Point2i(0, 2), Point2i(5, 6)), RangeUnion(r0, r1));
  r1.Set(Point2i(-10, -20), Point2i(40, 3));
  EXPECT_EQ(Range2i(Point2i(-10, -20), Point2i(40, 6)), RangeUnion(r0, r1));
}

TEST(RangeUtils, Intersection) {
  Range2i r0;
  Range2i r1;

  // Intersection with an empty Range results in an empty Range.
  EXPECT_TRUE(RangeIntersection(r0, r1).IsEmpty());
  r0.Set(Point2i(1, 2), Point2i(5, 6));
  EXPECT_TRUE(RangeIntersection(r0, r1).IsEmpty());
  EXPECT_TRUE(RangeIntersection(r1, r0).IsEmpty());

  // Intersection with self or containing Range returns the same Range.
  EXPECT_EQ(r0, RangeIntersection(r0, r0));
  EXPECT_EQ(r0, RangeIntersection(r0, Range2i(Point2i(1, -1), Point2i(8, 6))));

  // Intersection of ranges not overlapping in all dimensions results in an
  // empty range.
  r1.Set(Point2i(2, 7), Point2i(4, 8));
  EXPECT_TRUE(RangeIntersection(r0, r1).IsEmpty());
  r1.Set(Point2i(6, 2), Point2i(7, 4));
  EXPECT_TRUE(RangeIntersection(r0, r1).IsEmpty());
  r1.Set(Point2i(6, -1), Point2i(6, 1));
  EXPECT_TRUE(RangeIntersection(r0, r1).IsEmpty());

  // Regular intersections.
  r1.Set(Point2i(0, 3), Point2i(4, 5));
  EXPECT_EQ(Range2i(Point2i(1, 3), Point2i(4, 5)), RangeIntersection(r0, r1));
  r1.Set(Point2i(-10, -20), Point2i(40, 3));
  EXPECT_EQ(Range2i(Point2i(1, 2), Point2i(5, 3)), RangeIntersection(r0, r1));
  r1.Set(Point2i(5, 6), Point2i(10, 20));
  EXPECT_EQ(Range2i(Point2i(5, 6), Point2i(5, 6)), RangeIntersection(r0, r1));
}

TEST(RangeUtils, NVolume) {
  Range1i r0;
  Range1d r1;
  Range2i r2;
  Range2d r3;
  Range3i r4;
  Range3d r5;

  // Empty range has NVolume of 0 for any dimension
  EXPECT_EQ(0, NVolume(r0));
  EXPECT_EQ(0.0, NVolume(r1));
  EXPECT_EQ(0, NVolume(r2));
  EXPECT_EQ(0.0, NVolume(r3));
  EXPECT_EQ(0, NVolume(r4));
  EXPECT_EQ(0.0, NVolume(r5));

  // Non-empty ranges
  r0.Set(Range1i::Endpoint(4), Range1i::Endpoint(6));
  EXPECT_EQ(2, NVolume(r0));
  r1.Set(Range1d::Endpoint(2.3), Range1d::Endpoint(5.9));
  EXPECT_DOUBLE_EQ(3.6, NVolume(r1));
  r2.Set(Point2i(-3, -4), Point2i(5, 2));
  EXPECT_EQ(48, NVolume(r2));
  r3.Set(Point2d(0.1, -0.1), Point2d(0.2, 0.1));
  EXPECT_DOUBLE_EQ(0.02, NVolume(r3));
  r4.Set(Point3i(1, 2, 3), Point3i(4, 5, 6));
  EXPECT_EQ(27, NVolume(r4));
  r5.Set(Point3d(-2.0, -4.2, 5.1), Point3d(1.5, 7.1, 8.4));
  EXPECT_DOUBLE_EQ(130.515, NVolume(r5));
}

TEST(RangeUtils, RangesAlmostEqual) {
  EXPECT_TRUE(RangesAlmostEqual(Range1i(2, 3), Range1i(2, 3), 0));
  EXPECT_FALSE(RangesAlmostEqual(Range1d(2.0, 3.0), Range1d(2.2, 3.0), 0.1));
  EXPECT_TRUE(RangesAlmostEqual(Range2i(Point2i(1, -1), Point2i(2, 2)),
                                Range2i(Point2i(1, -1), Point2i(2, 2)),
                                0));
  EXPECT_TRUE(RangesAlmostEqual(Range2i(Point2i(1, -1), Point2i(2, 2)),
                                Range2i(Point2i(2, -2), Point2i(3, 3)),
                                1));
  EXPECT_FALSE(RangesAlmostEqual(Range2i(Point2i(1, -1), Point2i(2, 2)),
                                 Range2i(Point2i(2, -3), Point2i(3, 4)),
                                 1));
  EXPECT_FALSE(RangesAlmostEqual(Range2i(Point2i(1, -1), Point2i(2, 2)),
                                 Range2i(Point2i(3, -2), Point2i(4, 1)),
                                 1));
  EXPECT_TRUE(RangesAlmostEqual(
      Range3d(Point3d(1.0, 2.0, -3.0), Point3d(2.0, 3.0, -2.0)),
      Range3d(Point3d(1.0, 2.0, -3.0), Point3d(2.0, 3.0, -2.0)),
      0.0));
  EXPECT_TRUE(RangesAlmostEqual(
      Range3d(Point3d(1.0, 2.0, -3.0), Point3d(2.0, 3.0, -2.0)),
      Range3d(Point3d(1.1, 1.9, -2.9), Point3d(2.1, 2.9, -1.9)),
      0.11));
  EXPECT_FALSE(RangesAlmostEqual(
      Range3d(Point3d(1.0, 2.0, -3.0), Point3d(2.0, 3.0, -2.0)),
      Range3d(Point3d(1.2, 1.9, -2.9), Point3d(2.2, 2.9, -1.9)),
      0.11));
  EXPECT_FALSE(RangesAlmostEqual(
      Range3d(Point3d(1.0, 2.0, -3.0), Point3d(2.0, 3.0, -2.0)),
      Range3d(Point3d(1.0, 2.0, -2.8), Point3d(2.0, 3.0, -2.0)),
      0.11));
  EXPECT_FALSE(RangesAlmostEqual(
      Range3d(Point3d(1.0, 2.0, -3.0), Point3d(2.0, 3.0, -2.0)),
      Range3d(Point3d(1.0, 2.0, -3.0), Point3d(2.0, 3.0, -1.8)),
      0.11));
  EXPECT_TRUE(RangesAlmostEqual(
      Range3d(Point3d(1.0, 2.0, -3.0), Point3d(2.0, 3.0, -2.0)),
      Range3d(Point3d(1.0, 2.0, -3.1), Point3d(2.0, 3.0, -2.1)),
      -0.2));
  EXPECT_FALSE(RangesAlmostEqual(
      Range3d(Point3d(1.0, 2.0, -3.0), Point3d(2.0, 1.0, -2.0)),
      Range3d(Point3d(1.0, 2.0, -3.0), Point3d(2.0, 1.0, -2.0)),
      0.1));
}

TEST(RangeUtils, ScaleRange) {
  // Doubles:
  // Empty range.
  EXPECT_TRUE(ScaleRange(Range2d(), 0.0).IsEmpty());
  EXPECT_TRUE(ScaleRange(Range2d(), -1.0).IsEmpty());
  EXPECT_TRUE(ScaleRange(Range2d(), 1.0).IsEmpty());

  // Non-empty range, non-positive scale factor.
  const Range2d r2d(Point2d(2.0, 4.0), Point2d(5.0, 8.0));
  EXPECT_TRUE(ScaleRange(r2d, 0.0).IsEmpty());
  EXPECT_TRUE(ScaleRange(r2d, -1.0).IsEmpty());

  // Non-empty range, positive scale factors.
  EXPECT_EQ(r2d, ScaleRange(r2d, 1.0));
  EXPECT_EQ(Range2d(Point2d(0.5, 2.0), Point2d(6.5, 10.0)),
            ScaleRange(r2d, 2.0));

  // Integers:
  // Empty range.
  EXPECT_TRUE(ScaleRange(Range2i(), 0).IsEmpty());
  EXPECT_TRUE(ScaleRange(Range2i(), -1).IsEmpty());
  EXPECT_TRUE(ScaleRange(Range2i(), 1).IsEmpty());

  // Non-empty range, non-positive scale factor.
  const Range2i r2i(Point2i(2, 3), Point2i(6, 11));
  EXPECT_TRUE(ScaleRange(r2i, 0).IsEmpty());
  EXPECT_TRUE(ScaleRange(r2i, -1).IsEmpty());

  // Non-empty range, positive scale factors.
  EXPECT_EQ(r2i, ScaleRange(r2i, 1));
  EXPECT_EQ(Range2i(Point2i(0, -1), Point2i(8, 15)), ScaleRange(r2i, 2));
}

TEST(RangeUtils, ScaleRangeNonUniformly) {
  // Empty range.
  EXPECT_TRUE(ScaleRangeNonUniformly(Range2d(), Vector2d(0.0, 1.0)).IsEmpty());
  EXPECT_TRUE(ScaleRangeNonUniformly(Range2d(), Vector2d(1.0, -1.0)).IsEmpty());
  EXPECT_TRUE(ScaleRangeNonUniformly(Range2d(), Vector2d(1.0, 2.0)).IsEmpty());

  // Non-empty range, non-positive scale factor.
  const Range2d r2d(Point2d(2.0, 4.0), Point2d(5.0, 8.0));
  EXPECT_TRUE(ScaleRangeNonUniformly(r2d, Vector2d(0.0, 1.0)).IsEmpty());
  EXPECT_TRUE(ScaleRangeNonUniformly(r2d, Vector2d(1.0, 0.0)).IsEmpty());
  EXPECT_TRUE(ScaleRangeNonUniformly(r2d, Vector2d(0.0, -1.0)).IsEmpty());
  EXPECT_TRUE(ScaleRangeNonUniformly(r2d, Vector2d(-1.0, 0.0)).IsEmpty());

  // Non-empty range, positive scale factors.
  EXPECT_EQ(r2d, ScaleRangeNonUniformly(r2d, Vector2d(1.0, 1.0)));
  EXPECT_EQ(Range2d(Point2d(0.5, 2.0), Point2d(6.5, 10.0)),
            ScaleRangeNonUniformly(r2d, Vector2d(2.0, 2.0)));
  EXPECT_EQ(Range2d(Point2d(0.5, -2.0), Point2d(6.5, 14.0)),
            ScaleRangeNonUniformly(r2d, Vector2d(2.0, 4.0)));
}

TEST(RangeUtils, ModulateRange) {
  // Empty range modulated by own type.
  EXPECT_TRUE(ModulateRange(Range2i(), Vector2i::Zero()).IsEmpty());
  EXPECT_TRUE(ModulateRange(Range2f(), Vector2f::Zero()).IsEmpty());
  EXPECT_TRUE(ModulateRange(Range2d(), Vector2d::Zero()).IsEmpty());

  // Empty range modulated by a different type.
  EXPECT_TRUE(ModulateRange(Range2i(), Vector2f::Zero()).IsEmpty());
  EXPECT_TRUE(ModulateRange(Range2f(), Vector2d::Zero()).IsEmpty());
  EXPECT_TRUE(ModulateRange(Range2d(), Vector2i::Zero()).IsEmpty());

  // Ranges for testing.
  const Range2i r2i(Point2i(12, 4), Point2i(9, 8));
  const Range2f r2f(Point2f(12.1f, 4.0f), Point2f(15.0f, 8.0f));
  const Range2d r2d(Point2d(12.1, 4.0), Point2d(15.0, 8.0));

  // Range scaled by non-positive value, should be empty.  One test for
  // each permutation of the three types above.
  EXPECT_TRUE(ModulateRange(r2i, Vector2i(0, 1)).IsEmpty());
  EXPECT_TRUE(ModulateRange(r2i, Vector2f(1.0f, 0.0f)).IsEmpty());
  EXPECT_TRUE(ModulateRange(r2i, Vector2d(0.0, 0.0)).IsEmpty());
  EXPECT_TRUE(ModulateRange(r2f, Vector2i(-1, 0)).IsEmpty());
  EXPECT_TRUE(ModulateRange(r2f, Vector2f(0.0f, -1.0f)).IsEmpty());
  EXPECT_TRUE(ModulateRange(r2f, Vector2d(-1.0, -1.0)).IsEmpty());
  EXPECT_TRUE(ModulateRange(r2d, Vector2i(1, 0)).IsEmpty());
  EXPECT_TRUE(ModulateRange(r2d, Vector2f(0.0f, 1.0f)).IsEmpty());
  EXPECT_TRUE(ModulateRange(r2d, Vector2d(-1.0, 1.0)).IsEmpty());

  // Range modulated by positive modulation values.  One test for each
  // permutation of the three types defined above.
  EXPECT_EQ(Range2i(Point2i(24, 4), Point2i(18, 8)),
            ModulateRange(r2i, Vector2i(2, 1)));
  EXPECT_EQ(Range2i(Point2i(4, 4), Point2i(3, 8)),
            ModulateRange(r2i, Vector2f(0.3f, 1.0f)));
  EXPECT_EQ(Range2i(Point2i(12, 2), Point2i(9, 4)),
            ModulateRange(r2i, Vector2d(1.0, 0.5)));

  EXPECT_EQ(Range2f(Point2f(24.0f, 4.0f), Point2f(30.0f, 8.0f)),
            ModulateRange(r2f, Vector2i(2, 1)));
  EXPECT_EQ(Range2f(Point2f(6.05f, 4.0f), Point2f(7.5f, 8.0f)),
            ModulateRange(r2f, Vector2f(0.5f, 1.0f)));
  EXPECT_EQ(Range2f(Point2f(12.1f, 2.0f), Point2f(15.0f, 4.0f)),
            ModulateRange(r2f, Vector2d(1.0, 0.5)));

  EXPECT_EQ(Range2d(Point2d(24.0, 4.0), Point2d(30.0, 8.0)),
            ModulateRange(r2d, Vector2i(2, 1)));
  EXPECT_EQ(Range2d(Point2d(12.1, 2.0), Point2d(15.0, 4.0)),
            ModulateRange(r2d, Vector2d(1.0, 0.5)));

  // Converting from double to float causes rounding error and requires
  // EXPECT_NEAR instead of EXPECT_EQ.
  const Range2d result = ModulateRange(r2d, Vector2f(0.5f, 1.0f));
  const double kEpsilon = 0.000001;
  EXPECT_NEAR(6.05, result.GetMinPoint()[0], kEpsilon);
  EXPECT_NEAR(4.0, result.GetMinPoint()[1], kEpsilon);
  EXPECT_NEAR(7.5, result.GetMaxPoint()[0], kEpsilon);
  EXPECT_NEAR(8.0, result.GetMaxPoint()[1], kEpsilon);
}

}  // namespace math
}  // namespace ion
