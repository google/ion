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

#ifndef ION_TEXT_FONTIMAGE_H_
#define ION_TEXT_FONTIMAGE_H_

#include <algorithm>
#include <memory>
#include <vector>

#include "ion/base/allocator.h"
#include "ion/base/invalid.h"
#include "ion/base/readwritelock.h"
#include "ion/base/referent.h"
#include "ion/base/stlalloc/allocmap.h"
#include "ion/base/stlalloc/allocset.h"
#include "ion/base/stlalloc/allocvector.h"
#include "ion/gfx/image.h"
#include "ion/gfx/texture.h"
#include "ion/math/range.h"
#include "ion/text/font.h"

namespace ion {
namespace text {

class BinPacker;

//-----------------------------------------------------------------------------
//
// A FontImage contains image and texture coordinate information used to render
// font glyphs. An instance may contain one or more ImageData instances, each of
// which stores a single texture image, a set indicating which glyphs appear in
// the image, and a map from glyph index to a rectangle denoting the texture
// coordinates for that glyph.
//
// Each texture image is in Image::kLuminance format with power-of-2 dimensions.
// Each pixel is an 8-bit fixed-point version of a signed-distance field (SDF)
// value. Values between 0 and 126 are negative, 127 is zero, and values from
// 128 to 255 are positive. To recover the original signed distance, scale the
// resulting (-1,1) value by the Font's SDF padding amount.
//
// FontImage is an abstract base class with derived StaticFontImage and
// DynamicFontImage classes.
//
//-----------------------------------------------------------------------------

class FontImage : public base::Referent {
 public:
  enum Type {
    kStatic,
    kDynamic,
  };

  // Data for each image in the FontImage.
  struct ImageData {
    explicit ImageData(const base::AllocatorPtr& allocator);
    // Font glyph texture.
    gfx::TexturePtr texture;
    // Set of glyphs in the image.
    GlyphSet glyph_set;
    // Maps glyph index to a texture coordinate rectangle.
    typedef base::AllocMap<GlyphIndex, math::Range2f> TexRectMap;
    TexRectMap texture_rectangle_map;
    // Vector of SubImages to set on textures.
    typedef base::AllocVector<gfx::Texture::SubImage> SubImageVec;
  };

  // Returns the type of an instance.
  Type GetType() const { return type_; }

  // Returns the Font passed to the constructor.
  const FontPtr& GetFont() { return font_; }

  // Returns the maximum image size passed to the constructor.
  size_t GetMaxImageSize() { return max_image_size_; }

  // Returns a reference to an ImageData instance that best contains the
  // requested glyphs.  Derived classes may return invalid references
  // in certain cases. Note that references to ImageData instances may be
  // invalidated by subsequent calls to FindImageData().
  virtual const ImageData& FindImageData(const GlyphSet& glyph_set) = 0;

  // Convenience function that returns true if an ImageData instance contains
  // all glyphs in glyph_set.
  static bool HasAllGlyphs(const ImageData& image_data,
                           const GlyphSet& glyph_set) {
    const auto& iglyphs = image_data.glyph_set;
    return !base::IsInvalidReference(image_data) &&
           std::includes(iglyphs.cbegin(), iglyphs.cend(), glyph_set.cbegin(),
                         glyph_set.cend());
  }

  // Convenience function that returns true if an ImageData instance contains a
  // glyph with the given index.
  static bool HasGlyph(const ImageData& image_data, GlyphIndex glyph_index) {
    return !base::IsInvalidReference(image_data) &&
           image_data.glyph_set.count(glyph_index);
  }

  // Convenience function that sets rectangle to the texture coordinate
  // rectangle to use for the indexed glyph within an ImageData instance.
  // Returns false if the glyph is not in the ImageData.
  static bool GetTextureCoords(const ImageData& image_data,
                               GlyphIndex glyph_index,
                               math::Range2f* rectangle);

 protected:
  // The constructor is passed the type of derived class, the Font to use, and
  // the maximum image size (in each dimension) for an image. Derived classes
  // treat the maximum size in different ways; see their comments for details.
  FontImage(Type type, const FontPtr& font, size_t max_image_size);

  // The destructor is protected because all base::Referent classes must have
  // protected or private destructors.
  ~FontImage() override;

 private:
  const Type type_;
  FontPtr font_;
  const size_t max_image_size_;
};

// Convenience typedef for shared pointer to a FontImage.
using FontImagePtr = base::SharedPtr<FontImage>;

//-----------------------------------------------------------------------------
//
// StaticFontImage is a derived FontImage that contains a single ImageData
// instance that is created by the constructor and that cannot be modified
// afterward.
//
//-----------------------------------------------------------------------------

class StaticFontImage : public FontImage {
 public:
  // The constructor sets up the single ImageData instance to contain glyphs
  // for all the requested glyphs. If the Font is not valid, the
  // glyph set is empty, or the resulting image would exceed max_image_size
  // in either dimension, the ImageData instance will be empty.
  StaticFontImage(const FontPtr& font, size_t max_image_size,
                  const GlyphSet& glyph_set);

  // Returns the single ImageData instance.
  const ImageData& GetImageData() const { return image_data_; }

  // Implements this function to return the single ImageData instance, whether
  // or not it contains all the requested glyphs. You can use HasAllGlyphs() to
  // test whether all of the glyphs appear in the ImageData, or use HasGlyph()
  // or GetTextureCoords() to test individual glyphs.
  const ImageData& FindImageData(const GlyphSet& glyph_sets) override;

 protected:
  // Protected constructor for MockFontImage that bypasses normal processing
  // and allows ImageData to be set directly.
  StaticFontImage(const FontPtr& font, size_t max_image_size,
                  const ImageData& image_data);
  ~StaticFontImage() override;

 private:
  // Does most of the work of setting up the ImageData instance. The ImageData's
  // Texture is labeled with the passed name.
  const ImageData InitImageData(const std::string& texture_name,
                                const GlyphSet& glyph_set);

  const ImageData image_data_;
};

// Convenience typedef for shared pointer to a StaticFontImage.
using StaticFontImagePtr = base::SharedPtr<StaticFontImage>;

//-----------------------------------------------------------------------------
//
// DynamicFontImage is a derived FontImage that may contain any number of
// ImageData instances. New glyphs may be added at any time, modifying an
// existing ImageData or adding a new one.
//
// New glyphs are added to an image only in empty space so that texture
// coordinate rectangles for previously-added glyphs remain valid.
//
// Since updates require adding sub-images to Textures, calling FindImageData*()
// while the DynamicFontImage's texture is being rendered in another thread can
// cause undefined behavior. To safely update DynamicFontImages on worker
// threads, enable deferred updates and call ProcessDeferredUpdates() when it is
// safe to update texture data.
//
//-----------------------------------------------------------------------------

class DynamicFontImage : public FontImage {
 public:
  // The constructor sets up the DynamicFontImage to use the Font. The
  // image_size value is used as the constant size (in both dimensions) of the
  // image created for each ImageData.
  DynamicFontImage(const FontPtr& font, size_t image_size);

  // Returns the current count of ImageData instances.
  size_t GetImageDataCount() const;

  // Returns the indexed ImageData instance, or an invalid reference if the
  // index is out of range.
  const ImageData& GetImageData(size_t index) const;

  // Returns the area covered by glyphs in the indexed ImageData, or 0 if the
  // index is out of range. This is useful primarily for testing.
  float GetImageDataUsedAreaFraction(size_t index) const;

  // Sets whether deferred updates are enabled.
  void EnableDeferredUpdates(bool enable) { updates_deferred_ = enable; }

  // Returns whether updates are deferred.
  bool AreUpdatesDeferred() const { return updates_deferred_; }

  // Updates internal texture data with any deferred updates. The caller must
  // ensure that the DynamicFontImage's Textures are not being rendered when
  // this is called.
  void ProcessDeferredUpdates();

  // Implements this function to find an existing ImageData that already
  // contains all of the glyphs (present in the Font) in glyph_set. If there is
  // no such ImageData, it then tries to add the necessary glyphs to an existing
  // ImageData instance. If that doesn't work, it tries to add them all to a new
  // ImageData instance. If none of these is successful (or if there were no
  // valid glyphs to add), this returns an invalid reference.
  const ImageData& FindImageData(const GlyphSet& glyph_set) override;

  // This is the same as FindImageData(), but instead returns the index of the
  // ImageData, or kInvalidIndex if unsuccessful.
  size_t FindImageDataIndex(const GlyphSet& unfiltered_glyph_set);

  // Returns the index of an ImageData instance that contains all of the
  // glyphs (present in the Font) in glyph_set, or kInvalidIndex if there
  // are none.
  size_t FindContainingImageDataIndex(const GlyphSet& unfiltered_glyph_set);

 protected:
  ~DynamicFontImage() override;

 private:
  // Helper class that encapsulates some internal data of the DynamicFontImage.
  class Helper;

  // Implements FindContainingImageDataIndex() for a GlyphSet that has
  // already been filtered to contain only glyphs present in the Font.
  // This is used to look for an index when it is known that the GlyphSet
  // has already been filtered.
  size_t FindContainingImageDataIndexPrefiltered(const GlyphSet& glyph_set);

  // Returns the index of an ImageData instance that can have the glyphs in
  // glyph_set added to it, or kInvalidIndex if there is none.
  size_t FindImageDataThatFits(const GlyphSet& glyph_set);

  // Creates a new ImageData and adds the glyphs in glyph_set to it.
  // Returns kInvalidIndex if there is no way to fit them in a single image.
  size_t AddImageData(const GlyphSet& glyph_set);

  std::unique_ptr<Helper> helper_;

  // Whether updates are deferred or immediate.
  bool updates_deferred_;

  // Protects access to deferred updates.
  base::ReadWriteLock update_lock_;
};

// Convenience typedef for shared pointer to a DynamicFontImage.
using DynamicFontImagePtr = base::SharedPtr<DynamicFontImage>;

}  // namespace text
}  // namespace ion

#endif  // ION_TEXT_FONTIMAGE_H_
