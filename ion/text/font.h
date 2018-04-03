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

#ifndef ION_TEXT_FONT_H_
#define ION_TEXT_FONT_H_

#include <mutex>  // NOLINT(build/c++11)
#include <string>

#include "ion/base/allocator.h"
#include "ion/base/array2.h"
#include "ion/base/invalid.h"
#include "ion/base/referent.h"
#include "ion/base/stlalloc/allocmap.h"
#include "ion/math/vector.h"
#include "ion/text/layout.h"

namespace ion {
namespace text {

// Typedef for a Unicode index of a character.
typedef uint32 CharIndex;

// Convenience typedef for shared pointer to a Font.
class Font;
using FontPtr = base::SharedPtr<Font>;

// Font is a base class for implementation-specific representations of fonts.
// It contains font metrics, glyph metrics, and rendered glyph grids.
class ION_API Font : public base::Referent {
 public:
  // A grid representing a rendered glyph, with each grid pixel representing
  // pixel coverage in the range (0,1). This is used internally to create
  // signed-distance field images for a font.
  struct GlyphGrid {
    GlyphGrid() : pixels(), is_sdf(false) {}
    GlyphGrid(size_t width, size_t height);

    // 
    base::Array2<double> pixels;

    // Returns true if glyph x- *or* y-size is zero.
    bool IsZeroSize() const;

    // When a Font is set up for rendering, the pixels are replaced with a
    // signed-distance field (SDF). This flag is set to true if the grid has
    // SDF data (vs. the original rendered data).
    bool is_sdf;
  };

  // This struct represents the cumulative metrics for the font.
  struct FontMetrics {
    // The default constructor initializes everything to 0.
    FontMetrics() : line_advance_height(0.f), ascender(0.f) {}

    // Nominal font-wide line-advance height, in pixels.
    float line_advance_height;

    // Height of the font-wide ascender or baseline, in pixels.
    float ascender;
  };

  // Returns the name of the font.
  const std::string& GetName() const { return name_; }

  // Returns the size of the font in pixels.
  size_t GetSizeInPixels() const { return size_in_pixels_; }

  // Returns the padding value used when generating SDF glyphs from the font.
  // Most SDF glyphs are larger than the original glyph so that the outer edges
  // have a nice distance fall-off. The SDF glyph grids are padded by this many
  // pixels on all sides.
  size_t GetSdfPadding() const { return sdf_padding_; }

  // Returns the FontMetrics for the font.
  const FontMetrics& GetFontMetrics() const { return font_metrics_; }

  // Returns the GlyphGrid for the indexed character. Returns an invalid
  // reference if the index does not refer to a glyph in the font.
  const GlyphGrid& GetGlyphGrid(GlyphIndex glyph_index) const;

  // Filter zero-size glyphs from |glyph_set|.
  void FilterGlyphs(GlyphSet* glyph_set);

  // Returns the index of a glyph corresponding to the given character in the
  // default ("unicode", in practice) charmap of the font.  Note that this is an
  // ill-defined concept, as a character may well require multiple glyphs to
  // render, or require different glyphs in different contexts, and so on.
  // This and AddGlyphsForAsciiCharacterRange are therefore intended as a quick
  // and dirty way to prepopulate a font with glyps so that a static FontImage
  // can be used, and cannot be assumed to work on non-trivial (i.e., non-latin)
  // characters.
  // Returns zero if no glyph available.
  virtual GlyphIndex GetDefaultGlyphForChar(CharIndex char_index) const = 0;

  // For each character in [start,finish] adds the default glyph from |font| to
  // |glyphs|.  Since this is not well-defined for all of Unicode, enforces
  // that [start,finish] lies within [1,127], where the character->glyph mapping
  // is simple enough.
  void AddGlyphsForAsciiCharacterRange(ion::text::CharIndex start,
                                       ion::text::CharIndex finish,
                                       ion::text::GlyphSet* glyphs);

  // Creates a layout as specified by |options| for a given single- or multi-
  // line string |text|.
  virtual const Layout BuildLayout(const std::string& text,
                                   const LayoutOptions& options) const = 0;

  // Makes sure that the GlyphData for each glyph in glyph_set has an SDF grid
  // cached inside the font. This assumes that the requested glyphs are
  // available in the font. There is no real need to call this outside of Ion's
  // internal code.
  void CacheSdfGrids(const GlyphSet& glyph_set);

  // Causes this font to use the font |fallback| as a fallback if a requested
  // glyph is not found. This is useful in internationalization cases, as few
  // fonts contain glyphs for enough unicode codepoints to satisfy most
  // languages.
  virtual void AddFallbackFont(const FontPtr& fallback) = 0;

 protected:
  // Convenience typedef for the map storing GlyphGrid instances.
  typedef base::AllocMap<GlyphIndex, GlyphGrid> GlyphMap;

  // The constructor is protected because this is an abstract base class.
  Font(const std::string& name, size_t size_in_pixels, size_t sdf_padding);

  // The destructor is protected because all base::Referent classes must have
  // protected or private destructors.
  ~Font() override;

  // Non-const version of GetGlyphGrid.
  GlyphGrid* GetMutableGlyphGrid(GlyphIndex glyph_index) const;

  // Prelocked version of GetMutableGlyphGrid.
  GlyphGrid* GetMutableGlyphGridLocked(GlyphIndex glyph_index) const;

  // Called by GetGlyphGrid() for missing glyphs. Child classes that load glyphs
  // on-demand should override this method to load the result into
  // glyph_grid. Returns true if a glyph was loaded.
  virtual bool LoadGlyphGrid(GlyphIndex glyph_index,
                             GlyphGrid* glyph_grid) const;

  // Adds a GlyphGrid to the GlyphMap.
  const GlyphGrid& AddGlyph(GlyphIndex glyph_index,
                            const GlyphGrid& glyph) const;

  // Sets FontMetrics.  SetFontMetrics() should only ever be called once.
  void SetFontMetrics(const FontMetrics& metrics);

  // Replaces the grid in a glyph with an SDF grid. This is used to cache the
  // SDF grid, since it is relatively expensive to compute. Logs an error and
  // returns false on error.
  bool CacheSdfGrid(GlyphIndex glyph_index,
                    const base::Array2<double>& sdf_pixels);

  // Size in pixels.
  const size_t size_in_pixels_;

 private:
  // Name of the font.
  const std::string name_;
  // Padding (in pixels) on each edge of each SDF glyph.
  const size_t sdf_padding_;
  // Metrics for the entire font.
  FontMetrics font_metrics_;
  // Grid for each glyph in the font, keyed by glyph index. Mutable to
  // support on-demand glyph loading from GetGlyphGrid() const method.
  mutable GlyphMap glyph_grid_map_;
  // Protect glyph_grid_map_. Mutable to allow locking from const methods.
  mutable std::mutex mutex_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Font);
};

}  // namespace text
}  // namespace ion

#endif  // ION_TEXT_FONT_H_
