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

#ifndef ION_TEXT_FREETYPEFONTUTILS_H_
#define ION_TEXT_FREETYPEFONTUTILS_H_

//
// This file contains functions that help with FreeTypeFont::BuildLayout.
// These should only be used from within the FreeTypeFont implementation.
//

#include <string>
#include <vector>

#include "ion/math/vector.h"
#include "ion/text/layout.h"

namespace ion {
namespace text {

class Font;
class FreeTypeFont;

// TextSize contains information about the size of multi-line text.
struct TextSize {
  // Height of a single line of text in pixels.
  float line_height_in_pixels;
  // Size of the entire text rectangle in pixels.
  math::Vector2f rect_size_in_pixels;
  // Height of the text _inside_ the rectangle in pixels.
  float text_height_in_pixels;
  // Max height above baseline of the first line of text (depends on contents!).
  float first_line_above_baseline;
  // Width of each line of text in pixels.
  std::vector<float> line_widths_in_pixels;
};

// This contains the values needed to transform glyph rectangles into the
// correct coordinates.
struct FreeTypeFontTransformData {
  // Scale to apply to resize glyphs.
  math::Vector2f scale;
  // Translation to apply to position glyphs for each line of text.
  std::vector<math::Vector2f> line_translations;
  // How much to translate each successive line in y, in pixels.
  float line_y_offset_in_pixels;
  // Additional horizontal distance between glyphs in physical pixels.
  float glyph_spacing;
  // Bottom-left position of the entire text rectangle in physical pixels.
  math::Point2f position;
  // Size of the entire text rectangle in physical pixels.
  math::Vector2f size;
};

// Lines of text from a single string (usually split on '\n').
typedef std::vector<std::string> Lines;

// Computes the size of text and returns it as a TextSize instance. Widths
// include spaces at the ends of the text lines, if any.
//
// NOTE: could combine this (computing ascent/descent) with
// the actual layout done by the layout engine to avoid double-work.
const TextSize ComputeTextSize(const FreeTypeFont& font,
                               const LayoutOptions& options,
                               const Lines& lines);

// Sets the scale and translation fields of the LayoutData instance with the
// scale and translation required to transform the glyphs of a text string from
// canonical glyph coordinates to the correct target size, location, and
// alignment. Canonical glyph coordinates are in pixels, with the left end of
// the text baseline at the origin. Transformed coordinates are in the correct
// locations in the XY-plane. Also sets the line_y_offset field with the
// canonical translation in y for successive lines of text.
const FreeTypeFontTransformData ComputeTransformData(
    const Font& font, const LayoutOptions& options, const TextSize& text_size);

// Returns a Layout populated by glyphs representing |lines| of text.
const Layout LayOutText(const FreeTypeFont& font, bool use_icu,
                        const Lines& lines,
                        const FreeTypeFontTransformData& transform_data);

}  // namespace text
}  // namespace ion

#endif  // ION_TEXT_FREETYPEFONTUTILS_H_
