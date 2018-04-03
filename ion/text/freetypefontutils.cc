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

#include "ion/text/freetypefontutils.h"

#include <algorithm>
#include <cctype>
#include <locale>

#include "ion/base/invalid.h"
#include "ion/base/utf8iterator.h"
#include "ion/math/range.h"
#include "ion/math/rangeutils.h"
#include "ion/text/freetypefont.h"
#include "ion/text/icuutils.h"
#include "absl/base/macros.h"

#if defined(ION_USE_ICU)
#include "third_party/icu/icu4c/source/common/unicode/uloc.h"
#include "third_party/icu/icu4c/source/common/unicode/unistr.h"
#include "third_party/iculx_hb/include/layout/ParagraphLayout.h"
#endif  // ION_USE_ICU

namespace ion {
namespace text {

using math::Point2f;
using math::Point2i;
using math::Point3f;
using math::Range2f;
using math::Vector2f;

//-----------------------------------------------------------------------------
//
// Helper functions.
//
//-----------------------------------------------------------------------------

// Returns the width in pixels of a single line of text. Returns 0 if there are
// any UTF8 encoding errors in the string.
//
// NOTE: could combine this (computing line width) with the
// actual layout done by the layout engine to avoid double-work.
static float ComputeLineWidth(const FreeTypeFont& font,
                              const LayoutOptions& options,
                              const std::string& line) {
  // x_min tracks the X coordinate of the left edge of the current glyph being
  // processed, and x_max is the right edge. Both are needed because x_min is
  // incremented by the glyph's advance value, which determines the left edge
  // of the next glyph, but the text width ends at the previous x_max.
  float x_min = 0.0f;
  float x_max = 0.0f;
  base::Utf8Iterator it(line);
  CharIndex prev_c = 0;
  CharIndex c;
  while ((c = it.Next()) != base::Utf8Iterator::kInvalidCharIndex) {
    const GlyphIndex g = font.GetDefaultGlyphForChar(c);
    const FreeTypeFont::GlyphMetrics& glyph_metrics = font.GetGlyphMetrics(g);
    if (base::IsInvalidReference(glyph_metrics)) {
      // Zero-width glyph.
      x_min = x_max;
    } else {
      if (prev_c) {
        const Vector2f kerning = font.GetKerning(prev_c, c);
        x_min += kerning[0];
      }
      if (options.metrics_based_alignment) {
        x_max = x_min + glyph_metrics.advance[0];
      } else {
        x_max = x_min + glyph_metrics.bitmap_offset[0] + glyph_metrics.size[0];
      }
      x_min += glyph_metrics.advance[0];
    }
    prev_c = c;
  }
  return it.GetState() == base::Utf8Iterator::kEndOfString ? x_max : 0.f;
}

const TextSize ComputeTextSize(const FreeTypeFont& font,
                               const LayoutOptions& options,
                               const Lines& lines) {
  const Font::FontMetrics& font_metrics = font.GetFontMetrics();

  TextSize text_size;
  text_size.line_height_in_pixels = font_metrics.line_advance_height;

  const size_t num_lines = lines.size();
  DCHECK(num_lines);

  // Compute the size in pixels that the text actually occupies (as opposed to
  // the size it would occupy if every line had a maximally-tall glyph for the
  // font).  First compute how far above the first line's baseline the tallest
  // glyph in the line extends.
  // If metrics_based_alignment then get the maximally-tall glyph height.
  float first_line_above_baseline = 0.f;
  if (options.metrics_based_alignment) {
    first_line_above_baseline = font_metrics.ascender;
  } else {
    CharIndex c;
    base::Utf8Iterator fit(lines.front());
    while ((c = fit.Next()) != base::Utf8Iterator::kInvalidCharIndex) {
      const GlyphIndex g = font.GetDefaultGlyphForChar(c);
      const FreeTypeFont::GlyphMetrics& metrics = font.GetGlyphMetrics(g);
      if (!base::IsInvalidReference(metrics)) {
        first_line_above_baseline = std::max(
            first_line_above_baseline, metrics.bitmap_offset[1]);
      }
    }
  }
  text_size.first_line_above_baseline = first_line_above_baseline;

  // Second, compute how far below the last line's baseline the lowest glyph in
  // the line extends.
  // If metrics_based_alignment then get the maximally-tall glyph height.
  float last_line_below_baseline = 0.f;
  if (options.metrics_based_alignment) {
    last_line_below_baseline =
        static_cast<float>(font.GetSizeInPixels()) - first_line_above_baseline;
  } else {
    CharIndex c;
    base::Utf8Iterator bit(lines.back());
    while ((c = bit.Next()) != base::Utf8Iterator::kInvalidCharIndex) {
      const GlyphIndex g = font.GetDefaultGlyphForChar(c);
      const FreeTypeFont::GlyphMetrics& metrics = font.GetGlyphMetrics(g);
      if (!base::IsInvalidReference(metrics)) {
        last_line_below_baseline = std::max(
            last_line_below_baseline,
            metrics.size[1] - metrics.bitmap_offset[1]);
      }
    }
  }

  // Finally, add up all the pixels taken up by text: all lines but the first
  // account for options.line_spacing each, the first line contributes the
  // height of its tallest glyph, and the last line contributes the height of
  // its lowest glyph below the baseline (or 0 if it has no descenders).
  const float spacing =
      options.line_spacing * static_cast<float>(num_lines - 1U);
  text_size.text_height_in_pixels =
      first_line_above_baseline + last_line_below_baseline +
      font_metrics.line_advance_height * spacing;

  // Height depends only on the number of lines and line spacing.
  const float height =
      (1.0f + spacing) * static_cast<float>(font.GetSizeInPixels());

  // Width is more complicated. We need the width of each line to handle
  // horizontal alignment properly.
  float width = 0.0f;
  text_size.line_widths_in_pixels.resize(num_lines);

  for (size_t i = 0; i < num_lines; ++i) {
    const std::string& line = lines[i];
    const float line_width = ComputeLineWidth(font, options, line);
    text_size.line_widths_in_pixels[i] = line_width;
    width = std::max(width, line_width);
  }
  text_size.rect_size_in_pixels.Set(width, height);

  return text_size;
}

// Returns the vertical (y) translation amount needed to achieve the correct
// alignment of a text rectangle with respect to a target point. This value is
// used for all text lines in a Layout.
static float ComputeVerticalAlignmentTranslation(
    const LayoutOptions& options,
    const TextSize& text_size, float scale) {
  // The text is initially positioned so that the baseline of the first (top)
  // line is at y = 0.
  float offset_in_pixels;  // Positive value means push text down.
  switch (options.vertical_alignment) {
    case kAlignTop:
      offset_in_pixels = text_size.first_line_above_baseline;
      break;
    case kAlignVCenter:
      offset_in_pixels = text_size.first_line_above_baseline -
                         0.5f * text_size.text_height_in_pixels;
      break;
    case kAlignBaseline:
      // No extra translation necessary.
      offset_in_pixels = 0.0f;
      break;
    case kAlignBottom:
      offset_in_pixels =
          text_size.first_line_above_baseline - text_size.text_height_in_pixels;
      break;
    default:
      LOG(ERROR) << "Invalid vertical alignment";
      offset_in_pixels = 0.0f;
  }
  return options.target_point[1] - scale * offset_in_pixels;
}

// Returns the horizontal (x) translation amount needed to achieve the correct
// alignment of a single line of text with respect to a target point.
static float ComputeHorizontalAlignmentTranslation(
    const LayoutOptions& options, float line_width_in_pixels,
    float rect_width_in_pixels, float scale) {
  // The line is initially positioned so that the first character is at x = 0.
  float offset_in_pixels;
  switch (options.horizontal_alignment) {
    case kAlignLeft:
      offset_in_pixels = 0.0f;
      break;
    case kAlignHCenter:
      offset_in_pixels = 0.5f * line_width_in_pixels;
      break;
    case kAlignRight:
      offset_in_pixels = line_width_in_pixels;
      break;
    default:
      LOG(ERROR) << "Invalid horizontal alignment";
      offset_in_pixels = 0.0f;
  }
  return options.target_point[0] - scale * offset_in_pixels;
}

// Sets the scale and translation fields of the LayoutData instance with the
// scale and translation required to transform the glyphs of a text string from
// canonical glyph coordinates to the correct target size, location, and
// alignment. Canonical glyph coordinates are in pixels, with the left end of
// the text baseline at the origin. Transformed coordinates are in the correct
// locations in the XY-plane. Also sets the line_y_offset field with the
// canonical translation in y for successive lines of text.
const FreeTypeFontTransformData ComputeTransformData(
    const Font& font, const LayoutOptions& options, const TextSize& text_size) {
  const Vector2f& target_size = options.target_size;
  const Vector2f& rect_size = text_size.rect_size_in_pixels;
  FreeTypeFontTransformData transform_data;

  // Compute the scale based on the text size in pixels and the target size. If
  // both the target size dimensions are 0, then do the layout in pixels with no
  // scaling. If only one of the target size dimensions is 0, use the other
  // dimension's scale.
  if (target_size == Vector2f::Zero()) {
    transform_data.scale = Vector2f::Fill(1.0f);
  } else if (target_size[0] == 0.0f) {
    DCHECK_GT(target_size[1], 0.0f);
    const float s = target_size[1] / rect_size[1];
    transform_data.scale.Set(s, s);
  } else if (target_size[1] == 0.0f) {
    DCHECK_GT(target_size[0], 0.0f);
    const float s = target_size[0] / rect_size[0];
    transform_data.scale.Set(s, s);
  } else {
    transform_data.scale.Set(target_size[0] / rect_size[0],
                             target_size[1] / rect_size[1]);
  }

  // Set the translation based on the alignment. The y translation is the same
  // for all lines of text, while the x translation may differ.
  const float y_translation = ComputeVerticalAlignmentTranslation(
      options, text_size, transform_data.scale[1]);
  const size_t num_lines = text_size.line_widths_in_pixels.size();
  transform_data.line_translations.resize(num_lines);
  float min_x_translation = 0.f;
  for (size_t i = 0; i < num_lines; ++i) {
    const float x_translation = ComputeHorizontalAlignmentTranslation(
        options, text_size.line_widths_in_pixels[i],
        rect_size[0], transform_data.scale[0]);
    transform_data.line_translations[i].Set(x_translation, y_translation);
    if (i == 0) {
      min_x_translation = x_translation;
    } else {
      min_x_translation = std::min(min_x_translation, x_translation);
    }
  }

  // Also compute the y offset for successive lines.
  transform_data.line_y_offset_in_pixels =
      -options.line_spacing * text_size.line_height_in_pixels;
  // Copy the horizontal spacing from |LayoutOptions| without any transform.
  transform_data.glyph_spacing = options.glyph_spacing;

  // Calculate the text rectangle's final bottom-left position and size.
  transform_data.position[0] = min_x_translation;
  // Start from y_translation as the baseline of the first line, add the
  // first line to get the top of the text rectangle, then subtract the
  // total height to get the bottom.
  transform_data.position[1] = y_translation + transform_data.scale[1] *
      (text_size.first_line_above_baseline - text_size.text_height_in_pixels);
  transform_data.size[0] = transform_data.scale[0] * rect_size[0];
  transform_data.size[1] =
      transform_data.scale[1] * text_size.text_height_in_pixels;
  return transform_data;
}

// Returns a Layout::Quad representing a rectangle in the XY-plane.
static const Layout::Quad BuildXyQuad(const Range2f& rect) {
  const Point2f& min = rect.GetMinPoint();
  const Point2f& max = rect.GetMaxPoint();
  return Layout::Quad(Point3f(min[0], min[1], 0.0f),
                      Point3f(max[0], min[1], 0.0f),
                      Point3f(max[0], max[1], 0.0f),
                      Point3f(min[0], max[1], 0.0f));
}

// Adds a transformed glyph to a Layout. The minimum point of the glyph is
// given in canonical coordinates. The sdf_padding is used to scale the Quad
// for the glyph so that the glyph covers the proper area.
static void AddGlyphToLayout(GlyphIndex glyph_index, size_t line_index,
                             const Point2f& glyph_min,
                             const FreeTypeFont::GlyphMetrics& glyph_metrics,
                             const FreeTypeFontTransformData& transform_data,
                             size_t sdf_padding, Layout* layout) {
  const Vector2f& glyph_size = glyph_metrics.size;

  Range2f quad_rect = Range2f::BuildWithSize(
      Point2f(glyph_min[0] * transform_data.scale[0],
              glyph_min[1] * transform_data.scale[1]) +
      transform_data.line_translations[line_index],
      Vector2f(glyph_size[0] * transform_data.scale[0],
               glyph_size[1] * transform_data.scale[1]));
  const Range2f tight_bounds(quad_rect);

  // Scale nonuniformly about the Quad center to compensate for the padding.
  if (sdf_padding && (glyph_size[0] * glyph_size[1] != 0.f)) {
    const float padding = static_cast<float>(2 * sdf_padding);
    const Vector2f scale((glyph_size[0] + padding) / glyph_size[0],
                         (glyph_size[1] + padding) / glyph_size[1]);
    quad_rect = math::ScaleRangeNonUniformly(quad_rect, scale);
  }
  const Vector2f offset(
      glyph_metrics.bitmap_offset[0] * transform_data.scale[0],
      // Convert offset to top of glyph in y-up coords to
      // offset to bottom of glyph in same coordinate system.
      (glyph_metrics.bitmap_offset[1] - glyph_metrics.size[1])
          * transform_data.scale[1]);

  CHECK(layout->AddGlyph(
      Layout::Glyph(glyph_index, BuildXyQuad(quad_rect),
                    tight_bounds, offset)));
}

// Lays out one line of text, adding glyphs to the Layout.
static void SimpleLayOutLine(
    const FreeTypeFont& font,
    const std::string& line,
    size_t line_index,
    const FreeTypeFontTransformData& transform_data,
    Layout* layout) {
  float x_min = 0.0f;
  base::Utf8Iterator it(line);
  CharIndex prev_c = 0;
  CharIndex c;
  while ((c = it.Next()) != base::Utf8Iterator::kInvalidCharIndex) {
    const GlyphIndex g = font.GetDefaultGlyphForChar(c);
    const FreeTypeFont::GlyphMetrics& glyph_metrics = font.GetGlyphMetrics(g);
    if (base::IsInvalidReference(glyph_metrics)) {
      // Zero-width invalid character.
    } else {
      float y_min =
          transform_data.line_y_offset_in_pixels *
          static_cast<float>(line_index) +
          (glyph_metrics.bitmap_offset[1] - glyph_metrics.size[1]);
      if (prev_c) {
        const Vector2f kerning = font.GetKerning(prev_c, c);
        x_min += kerning[0] + transform_data.glyph_spacing;
        y_min += kerning[1];
      }
      Point2f glyph_min(x_min + glyph_metrics.bitmap_offset[0], y_min);
      AddGlyphToLayout(g, line_index, glyph_min, glyph_metrics, transform_data,
                       font.GetSdfPadding(), layout);
      x_min += glyph_metrics.advance[0];
    }
    prev_c = c;
  }
}

#if defined(ION_USE_ICU)

// Get the glyph index and position offsets for a single glyph from a laid-out
// VisualRun.
static void GetGlyphFromRun(const iculx::ParagraphLayout::VisualRun& run,
                            int which_glyph_in_run, int32* glyph_index,
                            float* glyph_x, float* glyph_y) {
  *glyph_index = run.getGlyphs()[which_glyph_in_run];
  *glyph_x = run.getPositions()[which_glyph_in_run * 2];
  *glyph_y = -run.getPositions()[which_glyph_in_run * 2 + 1];
}

// Helper for laying out |text| into |layout| using ICU and |font|.  Returns
// the total X advance used or 0 in case of error.
static float IcuLayoutEngineLayoutLine(
    const FreeTypeFont& font,
    const std::string& text,
    size_t line_index,
    const FreeTypeFontTransformData& transform_data,
    Layout* layout) {
  if (!InitializeIcu("")) {
    // If ICU isn't initialized, fallback to simple layout.
    SimpleLayOutLine(font, text, line_index, transform_data, layout);
    return 0.0f;
  }

  // Convert the string to UTF-16.
  icu::UnicodeString chars = icu::UnicodeString::fromUTF8(text);
  if (chars.isEmpty()) {
    DLOG(ERROR) << "Empty text for layout, or corrupt utf8? [" << text << "]";
    return 0.0f;
  }

  // Generate a ParagraphLayout from the text.
  iculx::FontRuns runs(0);
  font.GetFontRunsForText(chars, &runs);
  LEErrorCode status = LE_NO_ERROR;
  std::unique_ptr<iculx::ParagraphLayout> icu_layout(new iculx::ParagraphLayout(
      chars.getBuffer(), chars.length(), &runs, nullptr, nullptr, nullptr,
      UBIDI_DEFAULT_LTR, false /* is_vertical */, status));
  if (status != LE_NO_ERROR) {
    DLOG(ERROR) << "new ParagraphLayout error: " << status;
    return 0.0f;
  }

  // Retrieve the glyphs from the layout, passing 0 to nextLine because we want
  // the entire string to fit on one line.
  icu_layout->reflow();
  std::unique_ptr<iculx::ParagraphLayout::Line> line(icu_layout->nextLine(0));
  if (!line.get()) {
    return 0.0f;
  }

  enum { kImpossibleGlyphIndex = -1 };
  int32 glyph_id = kImpossibleGlyphIndex;
  float glyph_x = -1;
  float glyph_y = -1;

  if (layout != nullptr) {  // Caller wants all the glyph descriptors
    layout->Reserve(chars.length());
    for (int i = 0; i < line->countRuns(); ++i) {
      const iculx::ParagraphLayout::VisualRun *run = line->getVisualRun(i);
      const icu::LEFontInstance* run_font = run->getFont();
      for (int j = 0; j < run->getGlyphCount(); ++j) {
        GetGlyphFromRun(*run, j, &glyph_id, &glyph_x, &glyph_y);
        if (glyph_id == 0 || glyph_id >= 0xffff)
          continue;
        GlyphIndex glyph_index = font.GlyphIndexForICUFont(run_font, glyph_id);
        const FreeTypeFont::GlyphMetrics& metrics =
            font.GetGlyphMetrics(glyph_index);
        if (base::IsInvalidReference(metrics))
          continue;
        glyph_x += metrics.bitmap_offset[0];
        glyph_y += transform_data.line_y_offset_in_pixels *
            static_cast<float>(line_index) +
            (metrics.bitmap_offset[1] - metrics.size[1]);
        AddGlyphToLayout(glyph_index, line_index, Point2f(glyph_x, glyph_y),
                         metrics, transform_data,
                         font.GetSdfPadding(), layout);
      }
    }
  } else {
    // Just find the final glyph to determine total advance
    for (int i = line->countRuns() - 1;
         i >= 0 && glyph_id == kImpossibleGlyphIndex; --i) {
      const iculx::ParagraphLayout::VisualRun *run = line->getVisualRun(i);
      for (int j = run->getGlyphCount() - 1; j >= 0; --j) {
        if (run->getGlyphs()[j] < 0xffff) {
          GetGlyphFromRun(*run, j, &glyph_id, &glyph_x, &glyph_y);
          break;
        }
      }
    }
  }

  if (glyph_id == kImpossibleGlyphIndex) {
    return 0.0f;
  }

  // Compute the total advance ourselves since ICU is known to lie.
  LEPoint advance_p;
  runs.getFont(runs.getCount() - 1)->getGlyphAdvance(glyph_id, advance_p);
  float final_advance = advance_p.fX;
  float final_position = glyph_x;
  return final_advance + final_position;
}

// Return true if no character in |text| is in a script that requires complex
// text layout.  In other words every character in |text| has a single
// reasonable glyph to represent it.  Examples of exceptions to this are:
// combining characters, characters from Indic/Arabic languages (or any others
// where ligatures are required), or characters requiring surrogate pairs.
static bool IsInFastUnicodeRange(const std::string& text) {
  // The range pairs in the table are begin (inclusive), end (exclusive), and
  // must stay sorted.
  static const CharIndex g_fast_unicode_ranges[] = {
      0x0020, 0x007f,  // Common punctuation, digits, LATIN
      0x00a0, 0x02b0,  // LATIN
      0x0370, 0x0483,  // GREEK, COPTIC, CYRILLIC
      0x048a, 0x0524,  // CYRILLIC
      0x3041, 0x3097,  // HIRAGANA
      0x30a0, 0x3100,  // KATAKANA
      0x31f0, 0x3200,  // KATAKANA LETTER SMALL
      0x3400, 0x4db5,  // CJK Ideograph Extension A
      0x4e00, 0x9fc4,  // CJK Ideographs
  };

  const CharIndex* begin = g_fast_unicode_ranges;
  const CharIndex* end = begin + ABSL_ARRAYSIZE(g_fast_unicode_ranges);
  base::Utf8Iterator it(text);
  CharIndex c;
  while ((c = it.Next()) != base::Utf8Iterator::kInvalidCharIndex) {
    const CharIndex* search = std::upper_bound(begin, end, c);
    // If the upper_bound points to a range start, that means that the
    // character is >= the prior range end, but < the range start, and
    // thus is out of range.  Range starts are at even positions in the table.
    if (((search - begin) & 1) == 0) return false;
  }
  return true;
}

#else
// See function comments above; these are do-nothing variants for when ICU is
// unavailable, and processing must be shunted towards the incorrect-but-fast
// path.
static bool IsInFastUnicodeRange(const std::string& text) { return true; }
static float IcuLayoutEngineLayoutLine(
    const FreeTypeFont& font,
    const std::string& text,
    size_t line_index,
    const FreeTypeFontTransformData& transform_data,
    Layout* layout) {
  return 0.0f;
}

#endif  // ION_USE_ICU

// Returns a Layout populated by glyphs representing |lines| of text.
const Layout LayOutText(const FreeTypeFont& font, bool use_icu,
                        const Lines& lines,
                        const FreeTypeFontTransformData& transform_data) {
  const size_t num_lines = lines.size();
  Layout layout;
  layout.SetLineAdvanceHeight(transform_data.scale[1] *
                              -transform_data.line_y_offset_in_pixels);
  layout.SetPosition(transform_data.position);
  layout.SetSize(transform_data.size);
  for (size_t i = 0; i < num_lines; ++i) {
    if (use_icu && !IsInFastUnicodeRange(lines[i])) {
      IcuLayoutEngineLayoutLine(
          font, lines[i], i, transform_data, &layout);
    } else {
      SimpleLayOutLine(font, lines[i], i, transform_data, &layout);
    }
  }
  return layout;
}

}  // namespace text
}  // namespace ion
