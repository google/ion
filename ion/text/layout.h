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

#ifndef ION_TEXT_LAYOUT_H_
#define ION_TEXT_LAYOUT_H_

//
// This file contains the classes used to define the layout requested for text,
// and the returned layout of glyphs to represent that text.
//

#include <vector>

#ifdef ION_PLATFORM_NACL
// Workaround for the fact the newlib's sys/types.h defines 'quad' to 'quad_t'
// (at least when _POSIX_SOURCE is not defined) and 'quad' is used a member
// name in the below Glyph struct.  While simply including this header early
// enough will fix the issue, we also undefine so the member has the correct
// name in the debugger.
#include <sys/types.h>
#undef quad
#endif

#include "ion/base/invalid.h"
#include "ion/base/stlalloc/allocset.h"
#include "ion/math/range.h"
#include "ion/math/vector.h"

namespace ion {
namespace text {

typedef uint64 GlyphIndex;
typedef base::AllocSet<GlyphIndex> GlyphSet;

//-----------------------------------------------------------------------------
// Alignment enums. These are used to position text glyphs relative to some
// point.

enum HorizontalAlignment {
  kAlignLeft,     // Put the left edge of the text at the point.
  kAlignHCenter,  // Put the horizontal center of the text at the point.
  kAlignRight,    // Put the right edge of the text at the point.
};

enum VerticalAlignment {
  kAlignTop,       // Put the top edge of the text at the point.
  kAlignVCenter,   // Put the vertical center of the text at the point.
  kAlignBaseline,  // Put the text baseline at the point.
  kAlignBottom,    // Put the bottom edge of the text at the point.
};

//-----------------------------------------------------------------------------
// This struct defines parameters affecting layout of a single text string when
// passed to BuildLayout(). The text string is assumed to be encoded as UTF-8;
// Ascii characters are treated normally.
//
// Placement:
//   The text is placed relative to target_point according to the alignment
//   enum values. For example, if kAlignLeft/kAlignBottom is specified, the
//   bottom-left corner of the text's bounding rectangle is placed at
//   target_point. If kAlignBaseline is specified for a multi-line text string,
//   the baseline of the first line is placed at target_point.
//
// Size:
//   The target_size field is interpreted as a width and height. If either
//   component of the size is positive and the other is zero, the text
//   rectangle will be scaled uniformly to match that component (width or
//   height). If both are positive, the rectangle will be scaled non-uniformly
//   to match both. If both are zero, then the text rectangle will not be scaled
//   and the output will be in pixels. If either is negative, the returned
//   Layout will be empty.
//   Caveat: For Freetype fonts, height is scaled relative to only the font's
//   SizeInPixels, excluding any space in-between lines. So, for multi-line
//   texts, the actual rectangle will end up slightly larger than target_size.
//
// Scaling:
//   If the SDF padding in the Font is positive, each Quad of the resulting
//   Layout is scaled up about its center to compensate, since increased
//   padding means the portion of the quad covered by each glyph effectively
//   shrinks.
//
// Line spacing:
//   A text string containing newline (\n) characters is treated as multi-line
//   text. The line_spacing field indicates how to space the lines. It is
//   expressed as a fraction of the font's maximum glyph height.
//
// Bad glyphs:
//   Any missing glyphs in the font will be treated as spaces.
struct LayoutOptions {
  LayoutOptions()
      : target_point(0.0f, 0.0f),
        target_size(0.0f, 1.0f),
        horizontal_alignment(kAlignLeft),
        vertical_alignment(kAlignBaseline),
        line_spacing(1.0f),
        glyph_spacing(0.0f),
        metrics_based_alignment(false) {}

  // Location of the text rectangle. (Default: origin)
  math::Point2f target_point;
  // Target width and height of the text rectangle. (Default: 0 in x, 1 in y)
  math::Vector2f target_size;
  // Text alignment in the horizontal direction. (Default: kAlignLeft)
  HorizontalAlignment horizontal_alignment;
  // Text alignment in the vertical direction. (Default: kAlignBaseline)
  VerticalAlignment vertical_alignment;
  // Spacing between baselines of lines of multi-line text, expressed as a
  // fraction of the font's FontMetrics::line_advance_height. (Default: 1.0)
  float line_spacing;
  // Horizontal spacing between two glyphs. The distance is in physical pixels,
  // scaled according to font scaling factor.
  float glyph_spacing;
  // When set to true, the size of the text for alignment purposes will be
  // computed from the reported font metrics rather than from the size of the
  // glyphs. This ensures that adding a letter with a descender or ascender to
  // a text that doesn't contain them will not change the placement of the
  // baseline; for example, changing a top-aligned "sea" to "seat" will not move
  // the letters down, and changing a bottom-aligned "snow" to "snowy" will not
  // move the letters up. (Default: false)
  bool metrics_based_alignment;
};

//-----------------------------------------------------------------------------
// A Layout instance specifies how glyphs are arranged to form text. Each glyph
// is represented by four 3D points forming a quadrilateral covered by the
// glyph and the index of the glyph's character within the font.
class ION_API Layout {
 public:
  // A Quad represents a 3D quadrilateral onto which a character glyph in the
  // layout will be drawn. The four points are stored counter-clockwise in this
  // order: lower-left, lower-right, upper-right, upper-left.
  struct Quad {
    // The default constructor sets all points to the origin.
    Quad() {
      points[0] = points[1] = points[2] = points[3] = math::Point3f::Zero();
    }

    // Constructor taking all four individual quadrilateral points in the
    // correct order.
    Quad(const math::Point3f& lower_left, const math::Point3f& lower_right,
         const math::Point3f& upper_right, const math::Point3f& upper_left) {
      points[0] = lower_left;
      points[1] = lower_right;
      points[2] = upper_right;
      points[3] = upper_left;
    }

    // Constructor taking all four quadrilateral points as an array.
    explicit Quad(const math::Point3f points_in[4]) {
      for (int i = 0; i < 4; ++i)
        points[i] = points_in[i];
    }

    math::Point3f points[4];  // Quadrilateral points.
  };

  // A Glyph represents one character glyph in the layout. It contains the
  // index of the glyph within the font and the Quad defining the 3D
  // quadrilateral onto which the glyph is mapped.
  struct Glyph {
    // The default constructor sets an invalid index (0, the NUL character) and
    // sets all quadrilateral points to the origin, and has empty tight bounds.
    Glyph() : glyph_index(0) {}

    // Constructor taking specifics.
    Glyph(GlyphIndex glyph_index_in,
          const Quad& quad_in,
          const math::Range2f& bounds_in,
          const math::Vector2f& offset_in)
      : glyph_index(glyph_index_in), quad(quad_in),
        bounds(bounds_in), offset(offset_in) {}

    GlyphIndex glyph_index;  // Index of the glyph in the font.
    Quad quad;               // Quadrilateral points to use for rendering.
    math::Range2f bounds;    // Tight bounds of the glyph.
    // Offset from text insertion point to glyph bounds' lower left corner.
    // For example a '1' glyph often has a positive x offset to keep it from
    // appearing too tight relative to surrounding glyphs, and glyphs with
    // descenders like 'g', 'j', etc. will have a negative y offset.
    math::Vector2f offset;
  };

  Layout() : line_advance_height_(0.f) {}
  ~Layout() {}

  // Adds a Glyph to the layout. Does nothing but return false if the index is
  // invalid.
  bool AddGlyph(const Glyph& glyph);

  // Returns the number of glyphs added to the layout.
  size_t GetGlyphCount() const;

  // Reserves space for at least |s| glyphs.
  void Reserve(size_t s);

  // Returns the indexed glyph. Returns an invalid reference if the index does
  // not refer to a previously-added glyph.
  const Glyph& GetGlyph(size_t i) const;

  // Modifies the indexed glyph. Does nothing but return false if the index
  // does not refer to a previously-added glyph or the glyph's index is
  // invalid.
  bool ReplaceGlyph(size_t i, const Glyph& new_glyph);

  // Populates |glyphs| with the glyph indexes appearing in this Layout.
  void GetGlyphSet(GlyphSet* glyphs) const;

  // Returns the vertical distance between successive baselines in multiline
  // text, scaled to the same units as the glyph's Quads.
  float GetLineAdvanceHeight() const;
  // Sets the vertical distance between successive baselines.
  void SetLineAdvanceHeight(float line_advance);

  // Returns the bottom-left point of the text rectangle, scaled to the same
  // units as the glyph's Quads.
  const math::Point2f& GetPosition() const;
  // Sets the bottom-left point of the text rectangle.
  void SetPosition(const math::Point2f& position);

  // Returns the width and height of the text rectangle, scaled to the same
  // units as the glyph's Quads.
  const math::Vector2f& GetSize() const;
  // Sets the width and height of the text rectangle.
  void SetSize(const math::Vector2f& size);

 private:
  std::vector<Glyph> glyphs_;
  float line_advance_height_;
  math::Point2f position_;
  math::Vector2f size_;
};

// Debugging aids.
std::ostream& operator<<(std::ostream& os, const Layout& layout);
std::ostream& operator<<(std::ostream& out, const Layout::Glyph& g);
std::ostream& operator<<(std::ostream& out, const Layout::Quad& q);

}  // namespace text
}  // namespace ion

#endif  // ION_TEXT_LAYOUT_H_
