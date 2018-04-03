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

namespace ion {
namespace text {

bool Layout::AddGlyph(const Glyph& glyph) {
  if (glyph.glyph_index) {
    glyphs_.push_back(glyph);
    return true;
  }
  return false;
}

size_t Layout::GetGlyphCount() const { return glyphs_.size(); }

void Layout::Reserve(size_t s) { glyphs_.reserve(s); }

const Layout::Glyph& Layout::GetGlyph(size_t i) const {
  return i < glyphs_.size() ? glyphs_[i] : base::InvalidReference<Glyph>();
}

bool Layout::ReplaceGlyph(size_t i, const Glyph& new_glyph) {
  if (i < glyphs_.size() && new_glyph.glyph_index) {
    glyphs_[i] = new_glyph;
    return true;
  }
  return false;
}

void Layout::GetGlyphSet(GlyphSet* glyphs) const {
  for (size_t i = 0; i < glyphs_.size(); ++i)
    glyphs->insert(glyphs_[i].glyph_index);
}

float Layout::GetLineAdvanceHeight() const {
  return line_advance_height_;
}

void Layout::SetLineAdvanceHeight(float line_advance) {
  line_advance_height_ = line_advance;
}

const math::Point2f& Layout::GetPosition() const {
  return position_;
}

void Layout::SetPosition(const math::Point2f& position) {
  position_ = position;
}

const math::Vector2f& Layout::GetSize() const {
  return size_;
}

void Layout::SetSize(const math::Vector2f& size) {
  size_ = size;
}

// Helpers for logging Layouts/Glyphs/Quads.
std::ostream& operator<<(std::ostream& out, const Layout::Quad& q) {
  return out << "QUAD { "
             << q.points[0] << ", "
             << q.points[1] << ", "
             << q.points[2] << ", "
             << q.points[3] << " }";
}

std::ostream& operator<<(std::ostream& out, const Layout::Glyph& g) {
  return out << "GLYPH { " << g.glyph_index << ": " << g.quad << " }";
}

std::ostream& operator<<(std::ostream& os, const Layout& layout) {
  os << "LAYOUT { ";
  for (size_t i = 0; i < layout.GetGlyphCount(); ++i)
    os << layout.GetGlyph(i) << ", ";
  os << "}";
  return os;
}

}  // namespace text
}  // namespace ion
