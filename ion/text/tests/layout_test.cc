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

#include "ion/text/layout.h"

#include "ion/base/invalid.h"
#include "ion/math/range.h"
#include "ion/math/vector.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace text {

using math::Point3f;
using math::Range2f;

namespace {

// Convenience function to build an axis-aligned Layout::Quad in the XY-plane.
static Layout::Quad BuildQuad(float left, float bottom,
                              float right, float top) {
  return Layout::Quad(Point3f(left, bottom, 0.0f),
                      Point3f(right, bottom, 0.0f),
                      Point3f(right, top, 0.0f),
                      Point3f(left, top, 0.0f));
}

// For testing Quad equality.
static bool AreQuadsEqual(const Layout::Quad& q0, const Layout::Quad& q1) {
  for (int i = 0; i < 4; ++i) {
    if (q0.points[i] != q1.points[i])
      return false;
  }
  return true;
}

}  // anonymous namespace

TEST(LayoutTest, Quad) {
  // Default construction.
  Layout::Quad quad1;
  EXPECT_EQ(Point3f::Zero(), quad1.points[0]);
  EXPECT_EQ(Point3f::Zero(), quad1.points[1]);
  EXPECT_EQ(Point3f::Zero(), quad1.points[2]);
  EXPECT_EQ(Point3f::Zero(), quad1.points[3]);

  // Constructor with individual points.
  Point3f p[4] = { Point3f(1.0f, 2.0f, 3.0f),
                   Point3f(4.0f, 5.0f, 6.0f),
                   Point3f(7.0f, 8.0f, 9.0f),
                   Point3f(10.0f, 11.0f, 12.0f) };
  Layout::Quad quad2(p[0], p[1], p[2], p[3]);
  EXPECT_EQ(p[0], quad2.points[0]);
  EXPECT_EQ(p[1], quad2.points[1]);
  EXPECT_EQ(p[2], quad2.points[2]);
  EXPECT_EQ(p[3], quad2.points[3]);

  // Constructor with point array.
  Layout::Quad quad3(p);
  EXPECT_EQ(p[0], quad3.points[0]);
  EXPECT_EQ(p[1], quad3.points[1]);
  EXPECT_EQ(p[2], quad3.points[2]);
  EXPECT_EQ(p[3], quad3.points[3]);
}

TEST(LayoutTest, Glyph) {
  // Default construction.
  Layout::Glyph glyph1;
  EXPECT_EQ(0U, glyph1.glyph_index);
  EXPECT_EQ(Point3f::Zero(), glyph1.quad.points[0]);
  EXPECT_EQ(Point3f::Zero(), glyph1.quad.points[1]);
  EXPECT_EQ(Point3f::Zero(), glyph1.quad.points[2]);
  EXPECT_EQ(Point3f::Zero(), glyph1.quad.points[3]);
  const Range2f bounds = Range2f::BuildWithSize(
      math::Point2f(0.5f, 1.5f), math::Vector2f(2.5f, 3.5f));
  const math::Vector2f offset(-1.0f, -2.0f);

  // Constructor with index and Quad.
  Layout::Glyph glyph2(10U, BuildQuad(1.0, 2.0, 3.0, 4.0), bounds, offset);
  EXPECT_EQ(10U, glyph2.glyph_index);
  EXPECT_TRUE(AreQuadsEqual(BuildQuad(1.0, 2.0, 3.0, 4.0), glyph2.quad));
  EXPECT_EQ(bounds, glyph2.bounds);
  EXPECT_EQ(offset, glyph2.offset);
}

TEST(LayoutTest, AddGlyph) {
  Layout layout;
  EXPECT_EQ(0U, layout.GetGlyphCount());
  EXPECT_TRUE(base::IsInvalidReference(layout.GetGlyph(0U)));

  const Range2f bounds = Range2f::BuildWithSize(
      math::Point2f(0.5f, 1.5f), math::Vector2f(2.5f, 3.5f));
  const math::Vector2f offset(-1.0f, -2.0f);

  EXPECT_TRUE(layout.AddGlyph(
      Layout::Glyph(14U, BuildQuad(0.0f, 0.0f, 1.0f, 2.0f), bounds, offset)));
  EXPECT_EQ(1U, layout.GetGlyphCount());
  EXPECT_EQ(14U, layout.GetGlyph(0).glyph_index);
  EXPECT_TRUE(AreQuadsEqual(BuildQuad(0.0f, 0.0f, 1.0f, 2.0f),
                            layout.GetGlyph(0).quad));
  EXPECT_EQ(bounds, layout.GetGlyph(0).bounds);
  EXPECT_EQ(offset, layout.GetGlyph(0).offset);

  const Range2f bounds2 = Range2f::BuildWithSize(
      math::Point2f(3.5f, 4.5f), math::Vector2f(5.5f, 6.5f));
  const math::Vector2f offset2(2.0f, 3.0f);
  EXPECT_TRUE(layout.AddGlyph(
      Layout::Glyph(100U, BuildQuad(1.f, 4.f, 3.f, 8.f), bounds2, offset2)));
  EXPECT_EQ(2U, layout.GetGlyphCount());
  EXPECT_EQ(14U, layout.GetGlyph(0).glyph_index);
  EXPECT_TRUE(AreQuadsEqual(BuildQuad(0.0f, 0.0f, 1.0f, 2.0f),
                            layout.GetGlyph(0).quad));
  EXPECT_EQ(100U, layout.GetGlyph(1).glyph_index);
  EXPECT_TRUE(AreQuadsEqual(BuildQuad(1.0f, 4.0f, 3.0f, 8.0f),
                            layout.GetGlyph(1).quad));
  EXPECT_EQ(bounds2, layout.GetGlyph(1).bounds);
  EXPECT_EQ(offset2, layout.GetGlyph(1).offset);

  // Invalid index.
  EXPECT_TRUE(base::IsInvalidReference(layout.GetGlyph(2)));
  EXPECT_FALSE(layout.AddGlyph(
      Layout::Glyph(0, BuildQuad(1.0f, 4.0f, 3.0f, 8.0f), bounds, offset)));

  GlyphSet glyphs(base::AllocatorPtr(nullptr));
  layout.GetGlyphSet(&glyphs);
  EXPECT_EQ(glyphs.size(), 2U);
  EXPECT_EQ(glyphs.count(14U), 1U);
  EXPECT_EQ(glyphs.count(100U), 1U);
}

TEST(LayoutTest, ReplaceGlyph) {
  Layout layout;
  const Range2f bounds;
  const math::Vector2f offset(math::Vector2f::Zero());
  layout.AddGlyph(Layout::Glyph(
      14U, BuildQuad(0.0f, 0.0f, 1.0f, 2.0f), bounds, offset));
  layout.AddGlyph(Layout::Glyph(
      22U, BuildQuad(10.0f, 20.0f, 1.0f, 2.0f), bounds, offset));
  layout.AddGlyph(Layout::Glyph(
      47U, BuildQuad(30.0f, 40.0f, 1.0f, 2.0f), bounds, offset));
  EXPECT_EQ(3U, layout.GetGlyphCount());

  EXPECT_TRUE(layout.ReplaceGlyph(
      1U, Layout::Glyph(19U, BuildQuad(50.0f, 60.0f, 1.0f, 2.0f),
                        bounds, offset)));
  EXPECT_EQ(19U, layout.GetGlyph(1).glyph_index);
  EXPECT_TRUE(AreQuadsEqual(BuildQuad(50.0f, 60.0f, 1.0f, 2.0f),
                            layout.GetGlyph(1).quad));

  // Bad glyph index.
  EXPECT_FALSE(layout.ReplaceGlyph(
      3U, Layout::Glyph(1U, BuildQuad(70.0f, 80.0f, 1.0f, 2.0f),
                        bounds, offset)));

  // Bad character index.
  EXPECT_FALSE(layout.ReplaceGlyph(
      3U, Layout::Glyph(2000U, BuildQuad(70.0f, 80.0f, 1.0f, 2.0f),
                        bounds, offset)));
}

TEST(LayoutTest, StringOperators) {
  Layout::Quad quad(ion::math::Point3f(2, 3, 4),
                    ion::math::Point3f(5, 6, 7),
                    ion::math::Point3f(8, 9, 10),
                    ion::math::Point3f(11, 12, 13));
  std::ostringstream quad_stream;
  quad_stream << quad;
  EXPECT_EQ("QUAD { P[2, 3, 4], P[5, 6, 7], P[8, 9, 10], P[11, 12, 13] }",
            quad_stream.str());
  const Range2f bounds;
  const math::Vector2f offset(math::Vector2f::Zero());

  Layout::Glyph glyph(14, quad, bounds, offset);
  std::ostringstream glyph_stream;
  glyph_stream << glyph;
  EXPECT_EQ("GLYPH { 14: "
            "QUAD { P[2, 3, 4], P[5, 6, 7], P[8, 9, 10], P[11, 12, 13] } }",
            glyph_stream.str());

  Layout layout;
  std::ostringstream layout_stream;
  layout_stream << layout;
  EXPECT_EQ("LAYOUT { }", layout_stream.str());

  layout.AddGlyph(glyph);
  std::ostringstream layout_with_glyph_stream;
  layout_with_glyph_stream << layout;
  EXPECT_EQ("LAYOUT { GLYPH { 14: QUAD { "
            "P[2, 3, 4], P[5, 6, 7], P[8, 9, 10], P[11, 12, 13] } }, }",
            layout_with_glyph_stream.str());

  layout.AddGlyph(ion::text::Layout::Glyph(16, ion::text::Layout::Quad(
      ion::math::Point3f(16, 18, 19),
      ion::math::Point3f(20, 21, 22),
      ion::math::Point3f(23, 24, 25),
      ion::math::Point3f(26, 27, 28)), bounds, offset));
  std::ostringstream layout_with_additional_glyph_stream;
  layout_with_additional_glyph_stream << layout;
  EXPECT_EQ("LAYOUT { GLYPH { 14: QUAD { "
            "P[2, 3, 4], P[5, 6, 7], P[8, 9, 10], P[11, 12, 13] } }, "
            "GLYPH { 16: QUAD { "
            "P[16, 18, 19], P[20, 21, 22], P[23, 24, 25], P[26, 27, 28] "
            "} }, }",
            layout_with_additional_glyph_stream.str());
}

}  // namespace text
}  // namespace ion
