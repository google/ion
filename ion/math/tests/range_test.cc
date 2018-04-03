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

#include "ion/math/range.h"

#include <sstream>
#include <string>

#include "ion/math/vector.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace math {

// Helper function for ExtendByRange to test template specializations.
template <typename T>
void ExtendByRange3() {
  typedef Range<3, T> RangeT;
  typedef Point<3, T> PointT;

  RangeT r0;
  RangeT r1;

  // Extending by an empty Range is a no-op.
  r0.ExtendByRange(r1);
  EXPECT_TRUE(r0.IsEmpty());
  r0.Set(PointT(1, 2, 3), PointT(5, 6, 7));
  r0.ExtendByRange(r1);
  EXPECT_EQ(RangeT(PointT(1, 2, 3), PointT(5, 6, 7)), r0);

  // Extending an empty range.
  r1.ExtendByRange(r0);
  EXPECT_EQ(r0, r1);

  // Extending by same or contained range is a no-op.
  r0.ExtendByRange(r0);
  EXPECT_EQ(RangeT(PointT(1, 2, 3), PointT(5, 6, 7)), r0);
  r0.ExtendByRange(RangeT(PointT(1, 2, 4), PointT(4, 5, 6)));
  EXPECT_EQ(RangeT(PointT(1, 2, 3), PointT(5, 6, 7)), r0);

  // Various real changes.
  r0.ExtendByRange(RangeT(PointT(0, 3, 2), PointT(4, 5, 7)));
  EXPECT_EQ(RangeT(PointT(0, 2, 2), PointT(5, 6, 7)), r0);
  r0.ExtendByRange(RangeT(PointT(-10, -20, 0), PointT(40, 5, 70)));
  EXPECT_EQ(RangeT(PointT(-10, -20, 0), PointT(40, 6, 70)), r0);
}

// Helper function for ContainsPoint to test template specializations.
template <typename T>
void ContainsPoint3() {
  typedef Range<3, T> RangeT;
  typedef Point<3, T> PointT;

  // Empty range does not contain anything.
  EXPECT_FALSE(RangeT(PointT(1, 2, 99),
                      PointT(0, 4, 99)).ContainsPoint(PointT(0, 3, 99)));

  RangeT r(PointT(1, 2, 99), PointT(5, 6, 99));
  EXPECT_FALSE(r.ContainsPoint(PointT(0, 3, 99)));
  EXPECT_FALSE(r.ContainsPoint(PointT(4, 7, 99)));
  EXPECT_FALSE(r.ContainsPoint(PointT(3, 5, 100)));
  EXPECT_TRUE(r.ContainsPoint(PointT(3, 5, 99)));
  EXPECT_TRUE(r.ContainsPoint(PointT(1, 2, 99)));
  EXPECT_TRUE(r.ContainsPoint(PointT(5, 6, 99)));
}

TEST(Range, MinMax) {
  EXPECT_EQ(Point2f(2.5f, 3.5f), Range2f(Point2f(2.5f, 3.5f),
                                         Point2f(6.0f, 7.0f)).GetMinPoint());
  EXPECT_EQ(Point2f(6.0f, 7.0f), Range2f(Point2f(2.5f, 3.5f),
                                         Point2f(6.0f, 7.0f)).GetMaxPoint());
}

TEST(Range, Set) {
  Range2f r;
  r.Set(Point2f(2.5f, 3.5f), Point2f(6.0f, 7.0f));
  EXPECT_EQ(Point2f(2.5f, 3.5f), r.GetMinPoint());
  EXPECT_EQ(Point2f(6.0f, 7.0f), r.GetMaxPoint());

  r.SetMinPoint(Point2f(-4.0f, -7.0f));
  EXPECT_EQ(Point2f(-4.0f, -7.0f), r.GetMinPoint());

  r.SetMaxPoint(Point2f(10.0f, 20.0f));
  EXPECT_EQ(Point2f(10.0f, 20.0f), r.GetMaxPoint());

  r.SetMaxComponent(0, 12.0f);
  EXPECT_EQ(Point2f(12.0f, 20.0f), r.GetMaxPoint());

  r.SetMinComponent(1, -9.0f);
  EXPECT_EQ(Point2f(-4.0f, -9.0f), r.GetMinPoint());
}

TEST(Range, BuildWithSize) {
  Range2f r = Range2f::BuildWithSize(Point2f(2.5f, 3.5f), Vector2f(6.0f, 7.0f));
  EXPECT_EQ(Point2f(2.5f, 3.5f), r.GetMinPoint());
  EXPECT_EQ(Point2f(8.5f, 10.5f), r.GetMaxPoint());
}

TEST(Range, SetWithSize) {
  Range2f r;
  r.SetWithSize(Point2f(2.5f, 3.5f), Vector2f(6.0f, 7.0f));
  EXPECT_EQ(Point2f(2.5f, 3.5f), r.GetMinPoint());
  EXPECT_EQ(Point2f(8.5f, 10.5f), r.GetMaxPoint());
}

TEST(Range, IsEmpty) {
  // Default constructor gives an empty range.
  EXPECT_TRUE(Range1i().IsEmpty());
  EXPECT_TRUE(Range2d().IsEmpty());
  EXPECT_TRUE(Range3f().IsEmpty());

  // Any min > max results in an empty range.
  EXPECT_FALSE(Range3d(Point3d(1.0, 2.0, 3.0),
                       Point3d(1.0, 2.0, 3.0)).IsEmpty());
  EXPECT_TRUE(Range3d(Point3d(1.0, 2.0, 3.0),
                      Point3d(0.999, 2.0, 3.0)).IsEmpty());
  EXPECT_TRUE(Range3d(Point3d(1.0, 2.0, 3.0),
                      Point3d(0.999, 2.0, 3.0)).IsEmpty());
  EXPECT_TRUE(Range3d(Point3d(1.0, 2.0, 3.0),
                      Point3d(1.0, 1.999, 3.0)).IsEmpty());
  EXPECT_TRUE(Range3d(Point3d(1.0, 2.0, 3.0),
                      Point3d(1.0, 2.0, 2.999)).IsEmpty());
}

TEST(Range, MakeEmpty) {
  Range1ui r1;
  EXPECT_TRUE(r1.IsEmpty());
  r1.Set(1, static_cast<Range1ui::Endpoint>(-1));
  EXPECT_FALSE(r1.IsEmpty());
  r1.MakeEmpty();
  EXPECT_TRUE(r1.IsEmpty());

  Range2f r2;
  r2.Set(Point2f(2.5f, 3.5f), Point2f(6.0f, 7.0f));
  EXPECT_FALSE(r2.IsEmpty());
  r2.MakeEmpty();
  EXPECT_TRUE(r2.IsEmpty());

  Range3d r3d;
  EXPECT_TRUE(r3d.IsEmpty());
  r3d.Set(Point3d(2.5, 3.5, 1.5), Point3d(6.0, 7.0, 2.0));
  EXPECT_FALSE(r3d.IsEmpty());
  r3d.MakeEmpty();
  EXPECT_TRUE(r3d.IsEmpty());

  Range3f r3f;
  EXPECT_TRUE(r3f.IsEmpty());
  r3f.Set(Point3f(2.5f, 3.5f, 1.5f), Point3f(6.0f, 7.0f, 2.0f));
  EXPECT_FALSE(r3f.IsEmpty());
  r3f.MakeEmpty();
  EXPECT_TRUE(r3f.IsEmpty());
}

TEST(Range, Convert) {
  {
    // Convert Range1f to Range1d.
    Range1f r1f;
    r1f.SetMinPoint(2.0f);
    r1f.SetMaxPoint(4.0f);
    Range1d r1d(r1f);
    EXPECT_EQ(r1d.GetMinPoint()[0], 2.0);
    EXPECT_EQ(r1d.GetMaxPoint()[0], 4.0);
  }

  {
    // Convert Range2d to Range2f.
    Range2d r2d;
    r2d.SetMinPoint(Point2d(1.0, 2.0));
    r2d.SetMaxPoint(Point2d(4.0, 8.0));
    Range2f r2f(r2d);
    EXPECT_EQ(r2f.GetMinPoint(), Point2f(1.0f, 2.0f));
    EXPECT_EQ(r2f.GetMaxPoint(), Point2f(4.0f, 8.0f));
  }
}

TEST(Range, GetSize) {
  EXPECT_EQ(Vector2f::Zero(), Range2f().GetSize());
  EXPECT_EQ(Vector2i(5, 4), Range2i(Point2i(4, 6), Point2i(9, 10)).GetSize());
  EXPECT_EQ(Vector3d(5.0, 6.0, 7.0),
            Range3d(Point3d(-2.0, 5.0, 10.5),
                    Point3d(3.0, 11.0, 17.5)).GetSize());
}

TEST(Range, GetCenter) {
  EXPECT_EQ(0, Range1i().GetCenter());
  EXPECT_EQ(5, Range1i(-20, 30).GetCenter());
  EXPECT_EQ(Point2f::Zero(), Range2f().GetCenter());
  EXPECT_EQ(Point2i(6, 8), Range2i(Point2i(4, 6), Point2i(9, 10)).GetCenter());
  EXPECT_EQ(Point3d(0.5, 8.0, 14.0),
            Range3d(Point3d(-2.0, 5.0, 10.5),
                    Point3d(3.0, 11.0, 17.5)).GetCenter());
}

TEST(Range, EqualityOperators) {
  // All empty ranges are considered equal.
  EXPECT_TRUE(Range3d() == Range3d());
  EXPECT_TRUE(Range2d() == Range2d(Point2d(3.0, 4.0), Point2d(2.5, 10.0)));
  EXPECT_FALSE(Range3d() != Range3d());
  EXPECT_FALSE(Range2d() != Range2d(Point2d(3.0, 4.0), Point2d(2.5, 10.0)));

  // Empty vs. non-empty.
  EXPECT_FALSE(Range3d() ==
               Range3d(Point3d(-2.0, 5.0, 10.5), Point3d(3.0, 11.0, 17.5)));
  EXPECT_TRUE(Range3d() !=
              Range3d(Point3d(-2.0, 5.0, 10.5), Point3d(3.0, 11.0, 17.5)));
  EXPECT_FALSE(Range3d(Point3d(-2.0, 5.0, 10.5), Point3d(3.0, 11.0, 17.5)) ==
               Range3d());
  EXPECT_TRUE(Range3d(Point3d(-2.0, 5.0, 10.5), Point3d(3.0, 11.0, 17.5)) !=
              Range3d());

  // Same ranges.
  EXPECT_TRUE(Range3d(Point3d(-2.0, 5.0, 10.5), Point3d(3.0, 11.0, 17.5)) ==
              Range3d(Point3d(-2.0, 5.0, 10.5), Point3d(3.0, 11.0, 17.5)));
  EXPECT_FALSE(Range3d(Point3d(-2.0, 5.0, 10.5), Point3d(3.0, 11.0, 17.5)) !=
               Range3d(Point3d(-2.0, 5.0, 10.5), Point3d(3.0, 11.0, 17.5)));

  // Slightly different ranges.
  EXPECT_FALSE(Range3d(Point3d(-2.0, 5.0, 10.5), Point3d(3.0, 11.0, 17.5)) ==
               Range3d(Point3d(-2.0, 5.1, 10.5), Point3d(3.0, 11.0, 17.5)));
  EXPECT_TRUE(Range3d(Point3d(-2.0, 5.0, 10.5), Point3d(3.0, 11.0, 17.5)) !=
              Range3d(Point3d(-2.0, 5.1, 10.5), Point3d(3.0, 11.0, 17.5)));
}

TEST(Range, ExtendByPoint) {
  // Extending empty Range by point results in just that point.
  Range2d r;
  r.ExtendByPoint(Point2d(3.2, -4.5));
  EXPECT_EQ(Range2d(Point2d(3.2, -4.5), Point2d(3.2, -4.5)), r);

  // New minimum.
  r.ExtendByPoint(Point2d(-10.0, -20.0));
  EXPECT_EQ(Range2d(Point2d(-10.0, -20.0), Point2d(3.2, -4.5)), r);

  // New maximum.
  r.ExtendByPoint(Point2d(30.0, 40.0));
  EXPECT_EQ(Range2d(Point2d(-10.0, -20.0), Point2d(30.0, 40.0)), r);

  // Point inside = no change.
  r.ExtendByPoint(Point2d(-9.0, -5.0));
  EXPECT_EQ(Range2d(Point2d(-10.0, -20.0), Point2d(30.0, 40.0)), r);

  // Points on edges = no change.
  r.ExtendByPoint(Point2d(-10.0, -20.0));
  r.ExtendByPoint(Point2d(30.0, 40.0));
  EXPECT_EQ(Range2d(Point2d(-10.0, -20.0), Point2d(30.0, 40.0)), r);

  // Extend in one dimension only.
  r.ExtendByPoint(Point2d(-50.0, 0.0));
  EXPECT_EQ(Range2d(Point2d(-50.0, -20.0), Point2d(30.0, 40.0)), r);
  r.ExtendByPoint(Point2d(0.0, -30.0));
  EXPECT_EQ(Range2d(Point2d(-50.0, -30.0), Point2d(30.0, 40.0)), r);
  r.ExtendByPoint(Point2d(60.0, 0.0));
  EXPECT_EQ(Range2d(Point2d(-50.0, -30.0), Point2d(60.0, 40.0)), r);
  r.ExtendByPoint(Point2d(0.0, 70.0));
  EXPECT_EQ(Range2d(Point2d(-50.0, -30.0), Point2d(60.0, 70.0)), r);

  // Test unsigned ranges.
  Range1ui r2;
  EXPECT_TRUE(r2.IsEmpty());
  r2.ExtendByPoint(0);
  EXPECT_FALSE(r2.IsEmpty());
  EXPECT_EQ(0U, r2.GetMinPoint());
  EXPECT_EQ(0U, r2.GetMaxPoint());
  r2.ExtendByPoint(2);
  EXPECT_FALSE(r2.IsEmpty());
  EXPECT_EQ(0U, r2.GetMinPoint());
  EXPECT_EQ(2U, r2.GetMaxPoint());
}

TEST(Range, ExtendByRange) {
  Range2i r0;
  Range2i r1;

  // Extending by an empty Range is a no-op.
  r0.ExtendByRange(r1);
  EXPECT_TRUE(r0.IsEmpty());
  r0.Set(Point2i(1, 2), Point2i(5, 6));
  r0.ExtendByRange(r1);
  EXPECT_EQ(Range2i(Point2i(1, 2), Point2i(5, 6)), r0);

  // Extending an empty range.
  r1.ExtendByRange(r0);
  EXPECT_EQ(r0, r1);

  // Extending by same or contained range is a no-op.
  r0.ExtendByRange(r0);
  EXPECT_EQ(Range2i(Point2i(1, 2), Point2i(5, 6)), r0);
  r0.ExtendByRange(Range2i(Point2i(1, 4), Point2i(4, 5)));
  EXPECT_EQ(Range2i(Point2i(1, 2), Point2i(5, 6)), r0);

  // Various real changes.
  r0.ExtendByRange(Range2i(Point2i(0, 3), Point2i(4, 5)));
  EXPECT_EQ(Range2i(Point2i(0, 2), Point2i(5, 6)), r0);
  r0.ExtendByRange(Range2i(Point2i(-10, -20), Point2i(40, 6)));
  EXPECT_EQ(Range2i(Point2i(-10, -20), Point2i(40, 6)), r0);

  // Test specializations for Point3f and Point3d.
  ExtendByRange3<float>();
  ExtendByRange3<double>();
}

TEST(Range, ContainsPoint) {
  // Empty range does not contain anything.
  EXPECT_FALSE(Range2i(Point2i(1, 2),
                       Point2i(0, 4)).ContainsPoint(Point2i(0, 3)));

  Range2i r(Point2i(1, 2), Point2i(5, 6));
  EXPECT_FALSE(r.ContainsPoint(Point2i(0, 3)));
  EXPECT_FALSE(r.ContainsPoint(Point2i(4, 7)));
  EXPECT_FALSE(r.ContainsPoint(Point2i(-3, 4)));
  EXPECT_TRUE(r.ContainsPoint(Point2i(3, 5)));
  EXPECT_TRUE(r.ContainsPoint(Point2i(1, 2)));
  EXPECT_TRUE(r.ContainsPoint(Point2i(5, 6)));

  // Test specializations for Point3f and Point3d.
  ContainsPoint3<float>();
  ContainsPoint3<double>();
}

TEST(Range, ContainsRange) {
  Range2i inner(Point2i(1, 1), Point2i(2, 2));
  Range2i straddle(Point2i(1, 1), Point2i(4, 4));
  Range2i outer(Point2i(0, 0), Point2i(3, 3));
  EXPECT_TRUE(outer.ContainsRange(inner));
  EXPECT_FALSE(inner.ContainsRange(outer));
  EXPECT_FALSE(outer.ContainsRange(straddle));
  EXPECT_FALSE(straddle.ContainsRange(outer));
  EXPECT_TRUE(straddle.ContainsRange(inner));
}

TEST(Range, IntersectsRange) {
  {
    // No overlap in any dimension.
    Range2i a(Point2i(0, 0), Point2i(1, 1));
    Range2i b(Point2i(2, 2), Point2i(3, 3));
    EXPECT_FALSE(a.IntersectsRange(b));
    EXPECT_FALSE(b.IntersectsRange(a));
  }
  {
    // Overlap in one dimension but not the other.
    Range2i a(Point2i(0, 0), Point2i(2, 1));
    Range2i b(Point2i(1, 2), Point2i(3, 3));
    EXPECT_FALSE(a.IntersectsRange(b));
    EXPECT_FALSE(b.IntersectsRange(a));
  }
  {
    // Overlap in both dimensions.
    Range2i a(Point2i(0, 0), Point2i(2, 2));
    Range2i b(Point2i(1, 1), Point2i(3, 3));
    EXPECT_TRUE(a.IntersectsRange(b));
    EXPECT_TRUE(b.IntersectsRange(a));
  }
  {
    // Containment.
    Range2i a(Point2i(0, 0), Point2i(3, 3));
    Range2i b(Point2i(1, 1), Point2i(2, 2));
    EXPECT_TRUE(a.IntersectsRange(b));
    EXPECT_TRUE(b.IntersectsRange(a));
  }
}

TEST(Range, Streaming) {
  {
    std::ostringstream out;
    out << Range2d();
    EXPECT_EQ(std::string("R[EMPTY]"), out.str());
  }
  {
    std::ostringstream out;
    out << Range2i(Point2i(1, 2), Point2i(10, 20));
    EXPECT_EQ(std::string("R[P[1, 2], P[10, 20]]"), out.str());
  }
  {
    std::ostringstream out;
    out << Range1i(1, 10);
    EXPECT_EQ(std::string("R[1, 10]"), out.str());
  }
  {
    std::istringstream in("R[EMPTY]");
    Range2f r(Point2f(2.5f, 3.5f), Point2f(6.0f, 7.0f));
    EXPECT_FALSE(r.IsEmpty());
    in >> r;
    EXPECT_TRUE(r.IsEmpty());
  }
  {
    std::istringstream in("R[P[1, 2], P[10, 20]]");
    Range2i r;
    in >> r;
    EXPECT_EQ(Range2i(Point2i(1, 2), Point2i(10, 20)), r);
  }
  {
    // This will fail since 1D ranges use the base type rather than a Point.
    std::istringstream in("R[ P[1.], P[3. ] ]");
    Range1d r;
    in >> r;
    EXPECT_EQ(Range1d(), r);
  }
  {
    std::istringstream in("R[ 1.,3. ]");
    Range1d r;
    in >> r;
    EXPECT_EQ(Range1d(1., 3.), r);
  }
  {
    std::istringstream in("R[ 1.3. ]");
    Range1d r;
    in >> r;
    EXPECT_TRUE(r.IsEmpty());
  }
  {
    std::istringstream in("[ 1., 3. ]");
    Range1d r;
    in >> r;
    EXPECT_TRUE(r.IsEmpty());
  }
  {
    std::istringstream in("R[ 1., 3. ");
    Range1d r;
    in >> r;
    EXPECT_TRUE(r.IsEmpty());
  }
  {
    std::istringstream in("R1., 3. ]");
    Range1d r;
    in >> r;
    EXPECT_TRUE(r.IsEmpty());
  }
  {
    std::istringstream in("Range[1., 3. ]");
    Range1d r;
    in >> r;
    EXPECT_TRUE(r.IsEmpty());
  }
}

}  // namespace math
}  // namespace ion
