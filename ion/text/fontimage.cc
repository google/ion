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

#include "ion/text/fontimage.h"

#include <iterator>
#include <vector>

#include "base/integral_types.h"
#include "ion/base/array2.h"
#include "ion/base/invalid.h"
#include "ion/base/lockguards.h"
#include "ion/base/logging.h"
#include "ion/base/serialize.h"
#include "ion/base/staticsafedeclare.h"
#include "ion/gfx/sampler.h"
#include "ion/math/utils.h"
#include "ion/math/vector.h"
#include "ion/text/binpacker.h"
#include "ion/text/font.h"
#include "ion/text/sdfutils.h"

namespace ion {
namespace text {

namespace {

using gfx::Image;
using gfx::ImagePtr;
using gfx::Sampler;
using gfx::SamplerPtr;
using gfx::Texture;
using gfx::TexturePtr;
using math::Point2f;
using math::Point2ui;
using math::Range2f;
using math::Vector2f;
using math::Vector2ui;

//-----------------------------------------------------------------------------
//
// Helper types and typedefs.
//
//-----------------------------------------------------------------------------

// An SdfGrid is a 2D array of doubles representing a signed-distance field.
typedef base::Array2<double> SdfGrid;

// An SdfGridMap maps a GlyphIndex (font specific index) to an SdfGrid
// representing the glyph. The SdfGrid is a copy of the SDF grid in
// the Font so that the FontImage can normalize the copy (based on the full
// FontImage) without affecting the Font's version.
typedef base::AllocMap<GlyphIndex, SdfGrid> SdfGridMap;

// Convenience typedef for a TexRectMap.
typedef FontImage::ImageData::TexRectMap TexRectMap;

// This struct wraps an ImageData and contains other items (BinPacker, counts,
// etc.) that help choose the best ImageData to add to.
struct ImageDataWrapper  {
  explicit ImageDataWrapper(const base::AllocatorPtr& allocator)
      : image_data(allocator),
        packed_area(0),
        used_area_fraction(0.f) {}

  // The wrapped ImageData instance.
  FontImage::ImageData image_data;

  // BinPacker used to pack glyphs into the FontImage.
  BinPacker bin_packer;

  // Area (in pixels) of the glyphs already packed into the ImageData.
  size_t packed_area;

  // Fraction of area used.
  float used_area_fraction;
};

// This struct wraps the Texture and sub-image data it needs for deferred
// updates.
struct DeferredUpdate : Texture::SubImage {
  DeferredUpdate() {}

  DeferredUpdate(const TexturePtr& texture_in, size_t level_in,
                 const math::Point2ui& offset_in, const ImagePtr& image_in)
      : Texture::SubImage(level_in, offset_in, image_in),
        texture(texture_in) {}

  // The Texture to add sub-image data to.
  TexturePtr texture;
};

//-----------------------------------------------------------------------------
//
// Helper functions.
//
//-----------------------------------------------------------------------------

// Returns a SdfGridMap (created with the specified allocator) storing SdfGrids
// for all glyphs in glyph_set.
static const SdfGridMap BuildSdfGridMap(const Font& font,
                                        const GlyphSet& glyph_set,
                                        const base::AllocatorPtr& allocator) {
  SdfGridMap grid_map(allocator);
  for (auto it = glyph_set.begin(); it != glyph_set.end(); ++it) {
    const Font::GlyphGrid& glyph_grid = font.GetGlyphGrid(*it);
    DCHECK(!base::IsInvalidReference(glyph_grid));
    // Copy the grid so it can be normalized without affecting the Font.
    grid_map[*it] = glyph_grid.pixels;
  }
  return grid_map;
}

// Adds rectangles for a collection of SdfGrids to a BinPacker. It skips any
// grids with 0 area.
static void AddGridsToBinPacker(const SdfGridMap& grids, BinPacker* packer) {
  for (SdfGridMap::const_iterator it = grids.begin(); it != grids.end(); ++it) {
    const SdfGrid& grid = it->second;
    const uint32 width = static_cast<uint32>(grid.GetWidth());
    const uint32 height = static_cast<uint32>(grid.GetHeight());
    if (width * height)
      packer->AddRectangle(it->first, Vector2ui(width, height));
  }
}

// Computes and returns the total area used by a collection of SdfGrids.
static size_t ComputeTotalGridArea(const SdfGridMap& grids) {
  size_t area = 0;
  for (SdfGridMap::const_iterator it = grids.begin(); it != grids.end(); ++it) {
    const SdfGrid& grid = it->second;
    area += grid.GetWidth() * grid.GetHeight();
  }
  return area;
}

// Inserts a single SdfGrid into a composite SdfGrid.
static void InsertGrid(const SdfGrid& grid, const Point2ui& bottom_left,
                       SdfGrid* composite_grid) {
  DCHECK_LE(bottom_left[0] + grid.GetWidth(), composite_grid->GetWidth());
  DCHECK_LE(bottom_left[1] + grid.GetHeight(), composite_grid->GetHeight());

  const size_t width = grid.GetWidth();
  const size_t height = grid.GetHeight();
  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < width; ++x) {
      composite_grid->Set(x + bottom_left[0], y + bottom_left[1],
                          grid.Get(x, y));
    }
  }
}

// Scales each value in an SdfGrid by a constant factor, clamps the result to
// [-1,1], and then transforms it into [0,1].
static void NormalizeGrid(const double scale_factor, SdfGrid* grid) {
  const size_t width = grid->GetWidth();
  const size_t height = grid->GetHeight();

  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < width; ++x) {
      // Scale.
      const double d = scale_factor * grid->Get(x, y);
      // Clamp to [-1,1], and transform to [0,1].
      grid->Set(x, y, (math::Clamp(d, -1.0, 1.0) + 1.0) * 0.5);
    }
  }
}

// Calls NormalizeGrid() to normalize all of the grids in the passed map. The
// padding is used to calculate the scale (see NormalizeGrid(), above).
static void NormalizeGridsInMap(size_t sdf_padding, SdfGridMap* grids) {
  const float scale_factor =
      sdf_padding ? 1.0f / static_cast<float>(sdf_padding) : 1.0f;
  for (SdfGridMap::iterator it = grids->begin(); it != grids->end(); ++it)
    NormalizeGrid(scale_factor, &it->second);
}

// Creates an SdfGrid of a given size, adds a collection of SdfGrids to it
// using the BinPacker for placement, normalizes it based on a padding value,
// and returns it.
static const SdfGrid CreatePackedGrid(
    const SdfGridMap& grids, const BinPacker& bin_packer,
    uint32 width, uint32 height, size_t sdf_padding) {
  // Use a large value for the initial values of the packed grid so background
  // pixels correspond to the maximum SDF distance.
  const double initial_value = static_cast<double>(width + height);
  SdfGrid packed_grid(width, height, initial_value);

  const std::vector<BinPacker::Rectangle>& rects = bin_packer.GetRectangles();
  const size_t count = rects.size();
  for (size_t i = 0; i < count; ++i) {
    const BinPacker::Rectangle& rect = rects[i];
    SdfGridMap::const_iterator it =
        grids.find(static_cast<GlyphIndex>(rect.id));
    DCHECK(it != grids.end());
    InsertGrid(it->second, rect.bottom_left, &packed_grid);
  }

  const float scale_factor =
      sdf_padding ? 1.0f / static_cast<float>(sdf_padding) : 1.0f;
  NormalizeGrid(scale_factor, &packed_grid);
  return packed_grid;
}

// Repeatedly tries to use a BinPacker to fit a collection of SdfGrids into a
// single packed SdfGrid with power-of-2 dimensions and the specified SDF
// padding, doubling the width or height as necessary to make them fit. If
// successful, the BinPacker's rectangles are updated with the grid locations
// and the resulting SdfGrid is returned. Otherwise, an empty SdfGrid is
// returned.
static const SdfGrid PackIntoMinimalGrid(
    const SdfGridMap& grids, size_t max_image_size, size_t sdf_padding,
    BinPacker* bin_packer) {
  // Compute the total area of the grids to aid with packing.
  const size_t total_area = ComputeTotalGridArea(grids);
  DCHECK_GT(total_area, 0U);

  // If the total area is greater than the maximum allowable area, there is no
  // way packing will be successful.
  if (total_area > math::Square(max_image_size))
    return SdfGrid();

  // Start with a reasonable power-of-2 size for the final grid and increase if
  // necessary until everything fits.
  const size_t initial_size =
      math::NextPowerOf2(static_cast<uint32>(math::Sqrt(total_area))) / 2;
  uint32 image_width = static_cast<uint32>(initial_size);
  uint32 image_height = image_width;
  bool double_the_width = true;
  while (!bin_packer->Pack(Vector2ui(image_width, image_height))) {
    // Alternate doubling width/height.
    // 
    // smaller result.
    if (double_the_width)
      image_width *= 2;
    else
      image_height *= 2;
    double_the_width = !double_the_width;
    if (image_width > max_image_size || image_height > max_image_size)
      return SdfGrid();
  }

  return CreatePackedGrid(grids, *bin_packer, image_width, image_height,
                          sdf_padding);
}

// Allocates and returns a pointer to a SamplerPtr to use for all FontImage
// Textures. Only needs to be called once by CreateTexture().
static SamplerPtr* CreateSampler() {
  base::AllocatorPtr allocator =
      base::AllocationManager::GetDefaultAllocatorForLifetime(base::kLongTerm);
  SamplerPtr* sampler_ptr = new SamplerPtr;
  SamplerPtr sampler(new(allocator)Sampler);
  *sampler_ptr = sampler;
  // This is required for textures on iOS. No other texture wrap mode seems
  // to be supported.
  sampler->SetMinFilter(ion::gfx::Sampler::kLinear);
  sampler->SetMagFilter(ion::gfx::Sampler::kLinear);
  sampler->SetWrapS(ion::gfx::Sampler::kClampToEdge);
  sampler->SetWrapT(ion::gfx::Sampler::kClampToEdge);
  return sampler_ptr;
}

// Allocates and returns a Texture using a global FontImage sampler using the
// passed allocator. The image is left uninitialized.
static TexturePtr CreateTexture(const base::AllocatorPtr& allocator) {
  ION_DECLARE_SAFE_STATIC_POINTER_WITH_CONSTRUCTOR(SamplerPtr, sampler,
                                                   CreateSampler());
  TexturePtr texture(new(allocator) Texture);
  texture->SetSampler(*sampler);
  return texture;
}

// Allocates and returns a 1-channel 8-bit luminance image of the given size,
// using the allocator. The image data is left uninitialized.
static ImagePtr CreateImage(size_t width, size_t height,
                            const base::AllocatorPtr& allocator) {
  // Create a uint8 buffer of the correct size.
  std::vector<uint8> data_buf(width * height);

  // Store the data in the Image. The data is wipeable because any future
  // updates to the FontImage, which only happen if it is dynamic, will be done
  // via sub-images.
  ImagePtr image(new(allocator) Image);
  image->Set(Image::kLuminance,
             static_cast<uint32>(width), static_cast<uint32>(height),
             base::DataContainer::CreateAndCopy<uint8>(
                 &data_buf[0], data_buf.size(), true, allocator));
  return image;
}

// Stores data for a 1-channel 8-bit luminance image from an SdfGrid. This
// assumes the Image has been set up correctly for the grid.
static void StoreGridInImage(const SdfGrid& grid, const ImagePtr& image) {
  const size_t width = grid.GetWidth();
  const size_t height = grid.GetHeight();

  // Access the data buffer in the Image.
  DCHECK(image->GetData().Get());
  DCHECK_EQ(image->GetDataSize(), width * height);
  uint8* data = image->GetData()->GetMutableData<uint8>();
  DCHECK(data);

  // Copy the grid data.
  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < width; ++x) {
      const double d = grid.Get(x, y);
      data[y * width + x] = static_cast<uint8>(d * 255.0);
    }
  }
}

// Returns a Range2f representing the rectangle of texture coordinates for a
// rectangle within an image.
static const Range2f ComputeTextureRectangle(
    const BinPacker::Rectangle& rect, const Vector2f& inverse_image_size) {
  const Point2f min_point(
      static_cast<float>(rect.bottom_left[0]) * inverse_image_size[0],
      static_cast<float>(rect.bottom_left[1]) * inverse_image_size[1]);
  const Vector2f size(static_cast<float>(rect.size[0]) * inverse_image_size[0],
                      static_cast<float>(rect.size[1]) * inverse_image_size[1]);
  return Range2f::BuildWithSize(min_point, size);
}

// Computes the texture coordinate rectangles based on the rectangles in a
// BinPacker and returns a TexRectMap containing all of them.
static const TexRectMap ComputeTextureRectangleMap(
    const Image& image, const BinPacker& bin_packer) {
  const Vector2f inverse_image_size(
      1.0f / static_cast<float>(image.GetWidth()),
      1.0f / static_cast<float>(image.GetHeight()));

  // Convert each packed rectangle to a texture coordinate rectangle.
  const std::vector<BinPacker::Rectangle>& rects = bin_packer.GetRectangles();
  const size_t num_rects = rects.size();
  TexRectMap texture_rectangle_map(image.GetAllocator());
  for (size_t i = 0; i < num_rects; ++i) {
    const BinPacker::Rectangle& rect = rects[i];
    texture_rectangle_map[static_cast<GlyphIndex>(rect.id)] =
        ComputeTextureRectangle(rect, inverse_image_size);
  }
  return texture_rectangle_map;
}

// Updates the image and texture rectangles in an ImageData based on the
// SdfGrids in grid_map and the packing information in bin_packer.
static void UpdateImageData(
    const SdfGridMap& grid_map, const BinPacker& bin_packer, uint32 image_size,
    size_t sdf_padding, FontImage::ImageData* image_data,
    const base::AllocatorPtr& allocator) {
  const SdfGrid packed_grid = CreatePackedGrid(
      grid_map, bin_packer, image_size, image_size, sdf_padding);
  DCHECK(packed_grid.GetSize());

  // Create an Image if there isn't already one in the ImageData's Texture.
  if (!image_data->texture->HasImage(0U))
    image_data->texture->SetImage(
        0U, CreateImage(packed_grid.GetWidth(), packed_grid.GetHeight(),
                        allocator));
  const ImagePtr& image = image_data->texture->GetImage(0U);

  // Store the SDF data from the packed grid into the Image.
  StoreGridInImage(packed_grid, image);

  // Compute per-glyph texture coordinate rectangles.
  image_data->texture_rectangle_map =
      ComputeTextureRectangleMap(*image, bin_packer);
}

// If updates is NULL, then adds SubImages to the passed texture for all of the
// passed grids using the Rectangles from bin_packer. If updates is non-NULL,
// then adds DeferredUpdates to the passed vector for all of the passed grids
// using the Rectangles from bin_packer. The allocator is used to allocate the
// Image data for each SubImage.
static void StoreSubImages(const SdfGridMap& grids, const BinPacker& bin_packer,
                           const base::AllocatorPtr& alloc,
                           const TexturePtr& texture,
                           base::AllocVector<DeferredUpdate>* updates) {
  const std::vector<BinPacker::Rectangle>& rects = bin_packer.GetRectangles();
  const size_t count = rects.size();
  for (size_t i = 0; i < count; ++i) {
    const BinPacker::Rectangle& rect = rects[i];
    const auto& it = grids.find(static_cast<GlyphIndex>(rect.id));
    // We only want the glyphs that are in the set of grids being added to the
    // image.
    if (it != grids.end()) {
      const SdfGrid& grid = it->second;
      ImagePtr image = CreateImage(grid.GetWidth(), grid.GetHeight(), alloc);
      StoreGridInImage(grid, image);
      if (updates)
        updates->push_back(
            DeferredUpdate(texture, 0U, rect.bottom_left, image));
      else
        texture->SetSubImage(0U, rect.bottom_left, image);
    }
  }
}

// Populates |*diff| with elements in |lhs| not in |rhs|.
static void SetDifference(const GlyphSet& lhs, const GlyphSet& rhs,
                          GlyphSet* diff) {
  std::set_difference(lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend(),
                      std::inserter(*diff, diff->end()));
}

}  // anonymous namespace

//-----------------------------------------------------------------------------
//
// FontImage::ImageData functions.
//
//-----------------------------------------------------------------------------

FontImage::ImageData::ImageData(const base::AllocatorPtr& allocator)
    : texture(CreateTexture(allocator)),
      glyph_set(allocator),
      texture_rectangle_map(allocator) {}

//-----------------------------------------------------------------------------
//
// FontImage functions.
//
//-----------------------------------------------------------------------------

FontImage::FontImage(Type type, const FontPtr& font, size_t max_image_size)
    : type_(type), font_(font), max_image_size_(max_image_size) {}

FontImage::~FontImage() {}

bool FontImage::GetTextureCoords(const ImageData& image_data,
                                 GlyphIndex glyph_index,
                                 math::Range2f* rectangle) {
  if (base::IsInvalidReference(image_data))
    return false;
  const ImageData::TexRectMap::const_iterator it =
      image_data.texture_rectangle_map.find(glyph_index);
  if (it == image_data.texture_rectangle_map.end())
    return false;
  *rectangle = it->second;
  return !rectangle->IsEmpty();
}

//-----------------------------------------------------------------------------
//
// StaticFontImage functions.
//
//-----------------------------------------------------------------------------

StaticFontImage::StaticFontImage(const FontPtr& font, size_t max_image_size,
                                 const GlyphSet& glyph_set)
    : FontImage(kStatic, font, max_image_size),
      image_data_(
          InitImageData(font.Get()
                            ? font->GetName() + "_" +
                                  base::ValueToString(font->GetSizeInPixels())
                            : "",
                        glyph_set)) {}

StaticFontImage::StaticFontImage(const FontPtr& font, size_t max_image_size,
                                 const ImageData& image_data)
    : FontImage(kStatic, font, max_image_size),
      image_data_(image_data) {}

StaticFontImage::~StaticFontImage() {}

const FontImage::ImageData& StaticFontImage::FindImageData(
    const GlyphSet& glyph_set) {
  // Since there is only one ImageData instance, return it.
  return image_data_;
}

const FontImage::ImageData StaticFontImage::InitImageData(
    const std::string& texture_name, const GlyphSet& glyph_set) {
  const base::AllocatorPtr& allocator = GetAllocator();
  ImageData image_data(allocator);
  image_data.texture->SetLabel(texture_name);
  Font* font = GetFont().Get();

  if (glyph_set.empty() || !font) {
    return image_data;
  }

  image_data.glyph_set = glyph_set;
  const base::AllocatorPtr& sta =
      allocator->GetAllocatorForLifetime(base::kShortTerm);

  // Make sure that all required glyphs have SDF grids cached.
  font->CacheSdfGrids(glyph_set);

  // Store SdfGrids in a map for all required glyphs.
  const SdfGridMap grid_map = BuildSdfGridMap(*font, glyph_set, sta);

  // Add the grids to a BinPacker.
  BinPacker bin_packer;
  AddGridsToBinPacker(grid_map, &bin_packer);

  // Try to pack them into a minimum-sized SdfGrid.
  const SdfGrid packed_grid = PackIntoMinimalGrid(
      grid_map, GetMaxImageSize(), font->GetSdfPadding(), &bin_packer);

  // If successful, store the results in the ImageData.
  if (packed_grid.GetSize()) {
    // Create an Image from the packed grid.
    ImagePtr image =
        CreateImage(packed_grid.GetWidth(), packed_grid.GetHeight(), allocator);
    image_data.texture->SetImage(0U, image);
    StoreGridInImage(packed_grid, image);

    // Compute per-glyph texture coordinate rectangles.
    image_data.texture_rectangle_map =
        ComputeTextureRectangleMap(*image, bin_packer);
  }

  return image_data;
}

//-----------------------------------------------------------------------------
//
// DynamicFontImage::Helper contains an ImageData along
// with other items that help choose the best ImageData to add to.
//
//-----------------------------------------------------------------------------

class DynamicFontImage::Helper : public Allocatable {
 public:
  Helper() : image_data_wrappers_(*this), deferred_updates_(*this) {}

  // Returns the vector of ImageDataWrappers.
  base::AllocVector<ImageDataWrapper>& GetImageDataWrappers() {
    return image_data_wrappers_;
  }

  // Returns the vector of DeferredUpdates.
  base::AllocVector<DeferredUpdate>& GetDeferredUpdates() {
    return deferred_updates_;
  }

 private:
  // Vector of ImageDataWrapper instances.
  base::AllocVector<ImageDataWrapper> image_data_wrappers_;

  // Vector of DeferredUpdate instances.
  base::AllocVector<DeferredUpdate> deferred_updates_;
};

//-----------------------------------------------------------------------------
//
// DynamicFontImage functions.
//
//-----------------------------------------------------------------------------

DynamicFontImage::DynamicFontImage(const FontPtr& font, size_t image_size)
    : FontImage(kDynamic, font, image_size),
      helper_(new(GetAllocator()) Helper()),
      updates_deferred_(false) {}

DynamicFontImage::~DynamicFontImage() {}

size_t DynamicFontImage::GetImageDataCount() const {
  return helper_->GetImageDataWrappers().size();
}

const FontImage::ImageData& DynamicFontImage::GetImageData(size_t index) const {
  const base::AllocVector<ImageDataWrapper>& wrappers =
      helper_->GetImageDataWrappers();
  return index < wrappers.size() ? wrappers[index].image_data
                                 : base::InvalidReference<ImageData>();
}

float DynamicFontImage::GetImageDataUsedAreaFraction(size_t index) const {
  const base::AllocVector<ImageDataWrapper>& wrappers =
      helper_->GetImageDataWrappers();
  return index < wrappers.size() ? wrappers[index].used_area_fraction : 0.f;
}

void DynamicFontImage::ProcessDeferredUpdates() {
  if (updates_deferred_) {
    base::AllocVector<DeferredUpdate>& updates = helper_->GetDeferredUpdates();
    base::ReadLock lock(&update_lock_);
    base::ReadGuard guard(&lock);
    const size_t count = updates.size();
    for (size_t i = 0; i < count; ++i) {
      const DeferredUpdate& di = updates[i];
      di.texture->SetSubImage(di.level, di.offset, di.image);
    }
    updates.clear();
  }
}

const FontImage::ImageData& DynamicFontImage::FindImageData(
    const GlyphSet& glyph_set) {
  return GetImageData(FindImageDataIndex(glyph_set));
}

size_t DynamicFontImage::FindImageDataIndex(
    const GlyphSet& unfiltered_glyph_set) {
  // This will be the index of the ImageData to return.
  size_t index = base::kInvalidIndex;

  if (!GetFont().Get()) {
    return index;
  }

  GlyphSet glyph_set(GetAllocator()->GetAllocatorForLifetime(base::kShortTerm),
                     unfiltered_glyph_set);
  GetFont()->FilterGlyphs(&glyph_set);

  if (glyph_set.empty() || (glyph_set.size() == 1 && !(*glyph_set.begin())))
    return index;

  // Make sure that all required glyphs have SDF grids cached.
  GetFont()->CacheSdfGrids(glyph_set);

  // NOTE: Possibly make this more efficient by combining work done in
  // the first 2 passes. Or do best-fit instead of first-fit.

  // See if there is an ImageData that already contains all of the glyphs.
  index = FindContainingImageDataIndexPrefiltered(glyph_set);

  // If that didn't work, find one that can have the glyphs added to it.
  if (index == base::kInvalidIndex) {
    index = FindImageDataThatFits(glyph_set);
  }

  // If that didn't work, try to create a new ImageData. If this doesn't work,
  // we're out of luck.
  if (index == base::kInvalidIndex) {
    index = AddImageData(glyph_set);
  }

  return index;
}

size_t DynamicFontImage::FindContainingImageDataIndex(
    const GlyphSet& unfiltered_glyph_set) {
  if (!GetFont().Get()) {
    return base::kInvalidIndex;
  }
  GlyphSet glyph_set(GetAllocator()->GetAllocatorForLifetime(base::kShortTerm),
                     unfiltered_glyph_set);
  GetFont()->FilterGlyphs(&glyph_set);
  return glyph_set.empty() ? base::kInvalidIndex
                           : FindContainingImageDataIndexPrefiltered(glyph_set);
}

size_t DynamicFontImage::FindContainingImageDataIndexPrefiltered(
    const GlyphSet& glyph_set) {
  const base::AllocVector<ImageDataWrapper>& wrappers =
      helper_->GetImageDataWrappers();
  const size_t num_wrappers = wrappers.size();
  for (size_t i = 0; i < num_wrappers; ++i) {
    if (HasAllGlyphs(wrappers[i].image_data, glyph_set)) return i;
    const auto& candidate = wrappers[i].image_data.glyph_set;
    if (std::includes(candidate.cbegin(), candidate.cend(), glyph_set.cbegin(),
                      glyph_set.cend())) {
      return i;
    }
  }
  return base::kInvalidIndex;
}

size_t DynamicFontImage::FindImageDataThatFits(const GlyphSet& glyph_set) {
  DCHECK(GetFont().Get());
  const Font& font = *GetFont();
  const uint32 image_size = static_cast<uint32>(GetMaxImageSize());
  const size_t max_area = math::Square(image_size);

  base::AllocVector<ImageDataWrapper>& wrappers =
      helper_->GetImageDataWrappers();
  const size_t num_wrappers = wrappers.size();
  for (size_t i = 0; i < num_wrappers; ++i) {
    ImageDataWrapper& wrapper = wrappers[i];
    ImageData& image_data = wrapper.image_data;
    const base::AllocatorPtr& allocator = GetAllocator();
    const base::AllocatorPtr& sta =
        allocator->GetAllocatorForLifetime(base::kShortTerm);

    // Create a GlyphSet containing just the missing glyphs and store
    // their SdfGrids in a map.
    GlyphSet missing_glyph_set(GetAllocator());
    SetDifference(glyph_set, image_data.glyph_set, &missing_glyph_set);
    DCHECK(!missing_glyph_set.empty());
    SdfGridMap missing_grid_map = BuildSdfGridMap(font, missing_glyph_set, sta);

    // Skip this ImageData if the added glyph area will exceed what remains in
    // the image.
    const size_t added_area = ComputeTotalGridArea(missing_grid_map);
    if (wrapper.packed_area + added_area > max_area)
      continue;

    // Create a copy of the current BinPacker for testing nondestructively and
    // add the grids to it.
    BinPacker test_bin_packer(wrapper.bin_packer);
    AddGridsToBinPacker(missing_grid_map, &test_bin_packer);

    // If all the glyphs will fit in this ImageData, update and return it.
    if (test_bin_packer.Pack(Vector2ui(image_size, image_size))) {
      // Update the GlyphSet.
      image_data.glyph_set.insert(missing_glyph_set.begin(),
                                  missing_glyph_set.end());

      // Save the BinPacker.
      wrapper.bin_packer = test_bin_packer;

      // Normalize the grids before creating sub-images.
      NormalizeGridsInMap(font.GetSdfPadding(), &missing_grid_map);
      // Generate sub images for the new glyphs added to the grid.
      if (updates_deferred_) {
        base::WriteLock lock(&update_lock_);
        base::WriteGuard guard(&lock);
        StoreSubImages(missing_grid_map, test_bin_packer, sta,
                       wrapper.image_data.texture,
                       &helper_->GetDeferredUpdates());
      } else {
        StoreSubImages(missing_grid_map, test_bin_packer, sta,
                       wrapper.image_data.texture, nullptr);
      }
      // Compute per-glyph texture coordinate rectangles.
      wrapper.image_data.texture_rectangle_map = ComputeTextureRectangleMap(
          *wrapper.image_data.texture->GetImage(0U), test_bin_packer);

      // Update the area values.
      wrapper.packed_area += added_area;
      wrapper.used_area_fraction +=
          static_cast<float>(added_area) / static_cast<float>(max_area);
      return i;
    }
  }

  // Could not fit the glyphs in any existing ImageData.
  return base::kInvalidIndex;
}

size_t DynamicFontImage::AddImageData(const GlyphSet& glyph_set) {
  const base::AllocatorPtr& allocator = GetAllocator();
  const base::AllocatorPtr& sta =
        allocator->GetAllocatorForLifetime(base::kShortTerm);
  DCHECK(GetFont().Get());
  const Font& font = *GetFont();

  // Create and add a new ImageDataWrapper.
  base::AllocVector<ImageDataWrapper>& wrappers =
      helper_->GetImageDataWrappers();
  const size_t index = wrappers.size();
  wrappers.push_back(ImageDataWrapper(allocator));
  ImageDataWrapper& wrapper = wrappers[index];
  ImageData& image_data = wrapper.image_data;
  const std::string texture_name = font.GetName() + "_" +
                                   base::ValueToString(font.GetSizeInPixels()) +
                                   "_" + base::ValueToString(index);
  image_data.texture->SetLabel(texture_name);

  // Store SdfGrids in a map for all glyphs.
  const SdfGridMap grid_map = BuildSdfGridMap(font, glyph_set, sta);

  // Add the grids to the BinPacker.
  AddGridsToBinPacker(grid_map, &wrapper.bin_packer);

  // Try to pack them into an SdfGrid of the proper size.
  const uint32 image_size = static_cast<uint32>(GetMaxImageSize());
  if (wrapper.bin_packer.Pack(Vector2ui(image_size, image_size))) {
    DCHECK(!wrapper.image_data.texture->HasImage(0U));
    UpdateImageData(grid_map, wrapper.bin_packer, image_size,
                    font.GetSdfPadding(), &wrapper.image_data, sta);

    // Fill in the GlyphSet.
    image_data.glyph_set = glyph_set;

    // Update the area values.
    wrapper.packed_area = ComputeTotalGridArea(grid_map);
    wrapper.used_area_fraction =
        static_cast<float>(wrapper.packed_area) /
        static_cast<float>(math::Square(image_size));

    return index;
  }

  // The grids didn't fit, so remove the wrapper.
  wrappers.pop_back();
  return base::kInvalidIndex;
}

}  // namespace text
}  // namespace ion
