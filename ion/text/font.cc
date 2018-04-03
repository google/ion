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

#include "ion/text/font.h"

#include <cctype>
#include <limits>

#include "ion/base/invalid.h"
#include "ion/base/lockguards.h"
#include "ion/base/logging.h"
#include "ion/math/rangeutils.h"
#include "ion/math/vector.h"
#include "ion/text/layout.h"
#include "ion/text/sdfutils.h"

namespace ion {
namespace text {

Font::GlyphGrid::GlyphGrid(size_t width, size_t height)
  : pixels(width, height), is_sdf(false) {}

bool Font::GlyphGrid::IsZeroSize() const {
  return pixels.GetWidth() * pixels.GetHeight() == 0;
}

Font::Font(const std::string& name, size_t size_in_pixels, size_t sdf_padding)
    : size_in_pixels_(size_in_pixels),
      name_(name),
      sdf_padding_(sdf_padding),
      glyph_grid_map_(*this) {}

Font::~Font() {}

const Font::GlyphGrid& Font::GetGlyphGrid(GlyphIndex glyph_index) const {
  Font::GlyphGrid* mutable_grid = GetMutableGlyphGrid(glyph_index);
  return
      mutable_grid ? *mutable_grid : base::InvalidReference<Font::GlyphGrid>();
}

Font::GlyphGrid* Font::GetMutableGlyphGrid(GlyphIndex glyph_index) const {
  std::lock_guard<std::mutex> guard(mutex_);
  return GetMutableGlyphGridLocked(glyph_index);
}

Font::GlyphGrid* Font::GetMutableGlyphGridLocked(GlyphIndex glyph_index) const {
  DCHECK(!mutex_.try_lock());
  if (!glyph_index) {
    return nullptr;
  }
  const auto& it = glyph_grid_map_.find(glyph_index);
  if (it == glyph_grid_map_.end()) {
    GlyphGrid glyph;
    if (LoadGlyphGrid(glyph_index, &glyph)) {
      glyph_grid_map_[glyph_index] = glyph;
      return &glyph_grid_map_[glyph_index];
    }
    return nullptr;
  }
  return &it->second;
}

bool Font::LoadGlyphGrid(GlyphIndex glyph_index, GlyphGrid* glyph_data) const {
  return false;
}

const Font::GlyphGrid& Font::AddGlyph(GlyphIndex glyph_index,
                                      const GlyphGrid& glyph) const {
  if (base::IsInvalidReference(glyph))
    return glyph;
  std::lock_guard<std::mutex> guard(mutex_);
  return glyph_grid_map_[glyph_index] = glyph;
}

void Font::SetFontMetrics(const FontMetrics& metrics) {
  // Verify this is only called once.
  DCHECK_EQ(font_metrics_.line_advance_height, 0.0f);
  font_metrics_ = metrics;
}

void Font::CacheSdfGrids(const GlyphSet& glyph_set) {
  // This has to be implemented as a FontImage member function because
  // Font::CacheSdfGrid() is protected and FontImage is a friend.
  const size_t sdf_padding = GetSdfPadding();
  for (auto it = glyph_set.cbegin(); it != glyph_set.cend(); ++it) {
    GlyphGrid* glyph_grid = GetMutableGlyphGrid(*it);
    DCHECK(!base::IsInvalidReference(glyph_grid));
    // Make sure the glyph's grid stores SDF values.
    if (!glyph_grid->is_sdf) {
      CacheSdfGrid(*it, ComputeSdfGrid(glyph_grid->pixels, sdf_padding));
      DCHECK(glyph_grid->is_sdf);
    }
  }
}

bool Font::CacheSdfGrid(GlyphIndex glyph_index,
                        const base::Array2<double>& sdf_pixels) {
  GlyphGrid* grid = GetMutableGlyphGrid(glyph_index);
  if (!grid) {
    LOG(ERROR) << "Invalid glyph passed to SetSdfGrid";
  } else if (grid->is_sdf) {
    LOG(ERROR) << "Grid is already an SDF grid";
  } else {
    grid->pixels = sdf_pixels;
    grid->is_sdf = true;
    return true;
  }
  return false;
}

void Font::FilterGlyphs(GlyphSet* glyph_set) {
  std::lock_guard<std::mutex> guard(mutex_);
  for (auto it = glyph_set->begin(); it != glyph_set->end();) {
    GlyphGrid* glyph = GetMutableGlyphGridLocked(*it);
    if (!glyph || glyph->IsZeroSize()) {
      glyph_set->erase(it++);
    } else {
      ++it;
    }
  }
}

void Font::AddGlyphsForAsciiCharacterRange(ion::text::CharIndex start,
                                           ion::text::CharIndex finish,
                                           ion::text::GlyphSet* glyphs) {
  DCHECK_LE(start, finish);
  DCHECK_GE(start, static_cast<CharIndex>(1));
  DCHECK_LE(finish, static_cast<CharIndex>(127));
  for (auto i = start; i <= finish; ++i) {
    const GlyphIndex glyph_index = GetDefaultGlyphForChar(i);
    if (glyph_index) {
      glyphs->insert(glyph_index);
    }
  }
}

}  // namespace text
}  // namespace ion
