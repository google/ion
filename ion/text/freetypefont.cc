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

#include "ion/text/freetypefont.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_MODULE_H
#include FT_SYSTEM_H
#include FT_TRUETYPE_TABLES_H

#include <algorithm>
#include <cstring>  // For memcpy().
#include <memory>
#include <mutex>  // NOLINT(build/c++11)
#include <unordered_map>
#include <vector>

#include "ion/base/allocator.h"
#include "ion/base/datacontainer.h"
#include "ion/base/logging.h"
#include "ion/base/staticsafedeclare.h"
#include "ion/port/fileutils.h"
#include "ion/port/memorymappedfile.h"
#include "ion/text/freetypefontutils.h"
#include "ion/text/layout.h"
#if defined(ION_USE_ICU)
#include "third_party/icu/icu4c/source/common/unicode/udata.h"
#include "third_party/iculehb/src/src/LEFontInstance.h"
#include "third_party/iculx_hb/include/layout/ParagraphLayout.h"
#endif  // ION_USE_ICU

namespace ion {
namespace text {

namespace {

using math::Vector2f;

// Convenience typedef for a vector of control points.
typedef std::vector<math::Point2f> ControlPoints;

// Returns true if a target size passed to a layout function is valid. To be
// valid, neither component can be negative.
static bool IsSizeValid(const Vector2f& target_size) {
  const float width = target_size[0];
  const float height = target_size[1];
  return (width >= 0.0f && height >= 0.0f);
}

//-----------------------------------------------------------------------------
//
// Each FreeTypeManager encapsulates a FT_Library instance, which uses the
// supplied Ion Allocator for memory management.
//
//-----------------------------------------------------------------------------

class FreeTypeManager {
 public:
  explicit FreeTypeManager(const base::AllocatorPtr& allocator);
  ~FreeTypeManager();

  // Returns the FreeTypeManager corresponding to |allocator| (or kLongTerm if
  // allocator is null).  If the FreeTypeManager does not exist, one is created.
  static FreeTypeManager* GetManagerForAllocator(
      const base::AllocatorPtr& allocator) {
    // Declare FreeTypeManagerMap as a mapping from allocators to managers, and
    // a mutex to synchronize access to it.
    typedef std::unordered_map<base::Allocator*,
                               std::unique_ptr<FreeTypeManager>>
        FreeTypeManagerMap;
    ION_DECLARE_SAFE_STATIC_POINTER(FreeTypeManagerMap, map);
    ION_DECLARE_SAFE_STATIC_POINTER(std::mutex, mutex);

    // Determine the allocator that will actually be used to look up the
    // FreeTypeManager in the map.
    base::AllocatorPtr allocator_to_use =
        allocator.Get()
            ? allocator
            : base::AllocationManager::GetDefaultAllocatorForLifetime(
                  base::kLongTerm);

    std::lock_guard<std::mutex> lock(*mutex);
    auto it = map->find(allocator_to_use.Get());
    if (it == map->end()) {
      auto man = new FreeTypeManager(allocator);
      (*map)[allocator_to_use.Get()] = std::unique_ptr<FreeTypeManager>(man);
      return man;
    }
    return it->second.get();
  }

  // Initializes and returns an FT_Face for a font represented by FreeType
  // data. If simulate_library_failure is true, this simulates a failure to
  // initialize the FreeType library. Returns nullptr on error.
  FT_Face InitFont(const void* data, size_t data_size,
                   bool simulate_library_failure);

  // Loads a glyph for a specific FT_Face font. Returns false on error.
  bool LoadGlyph(FT_Face face, uint32 glyph_index);

  // Frees up the memory used by a Font.
  void FreeFont(FT_Face face);

 private:
  // FreeType memory management functions. The FreeTypeManager instance is
  // passed as the "user" pointer in the FT_Memory structure.
  static void* Allocate(FT_Memory mem, long size);  // NOLINT
  static void Free(FT_Memory mem, void* ptr);
  static void* Realloc(FT_Memory mem, long cur_size, long new_size,  // NOLINT
                       void* ptr);

  // Returns the Allocator to use based on the "user" pointer in mem.
  static const base::AllocatorPtr& GetAllocator(FT_Memory mem) {
    DCHECK(mem);
    DCHECK(mem->user);
    FreeTypeManager* mgr = static_cast<FreeTypeManager*>(mem->user);
    DCHECK(!mgr->mutex_.try_lock());
    DCHECK(mgr->allocator_.Get());
    return mgr->allocator_;
  }

  // The Allocator for the FreeTypeManager and all its Fonts.
  base::AllocatorPtr allocator_;
  // Sets up FreeType to use an Ion Allocator to manage memory.
  FT_MemoryRec_ ft_mem_;
  // The shared FT_Library instance.
  FT_Library ft_lib_;
  // Protects shared access to the Allocator and FT_Library.
  std::mutex mutex_;
};

FreeTypeManager::FreeTypeManager(const base::AllocatorPtr& allocator)
    : allocator_(allocator), ft_lib_(nullptr) {
  ft_mem_.user = this;
  ft_mem_.alloc = Allocate;
  ft_mem_.free = Free;
  ft_mem_.realloc = Realloc;

  std::lock_guard<std::mutex> guard(mutex_);
  FT_New_Library(&ft_mem_, &ft_lib_);
  if (ft_lib_) FT_Add_Default_Modules(ft_lib_);
}

FreeTypeManager::~FreeTypeManager() {
  std::lock_guard<std::mutex> guard(mutex_);
  if (ft_lib_) FT_Done_Library(ft_lib_);
}

FT_Face FreeTypeManager::InitFont(const void* data, size_t data_size,
                                  bool simulate_library_failure) {
  FT_Face face = nullptr;
  std::lock_guard<std::mutex> guard(mutex_);
  if (FT_Library lib = simulate_library_failure ? nullptr : ft_lib_) {
    if (!FT_New_Memory_Face(lib, reinterpret_cast<const FT_Byte*>(data),
                            static_cast<FT_Long>(data_size), 0, &face)) {
      DCHECK(face);
    } else {
      LOG(ERROR) << "Could not read the FreeType font data";
      face = nullptr;
    }
  } else {
    LOG(ERROR) << "Could not initialize the FreeType library";
  }
  return face;
}

void FreeTypeManager::FreeFont(FT_Face face) {
  if (face) {
    std::lock_guard<std::mutex> guard(mutex_);
    FT_Done_Face(face);
  }
}

bool FreeTypeManager::LoadGlyph(FT_Face face, uint32 glyph_index) {
  std::lock_guard<std::mutex> guard(mutex_);
  const FT_Error result = FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER);
  return result == 0;
}

void* FreeTypeManager::Allocate(FT_Memory mem, long size) {  // NOLINT
  return GetAllocator(mem)->AllocateMemory(size);
}

void FreeTypeManager::Free(FT_Memory mem, void* ptr) {
  GetAllocator(mem)->DeallocateMemory(ptr);
}

void* FreeTypeManager::Realloc(FT_Memory mem, long cur_size,  // NOLINT
                               long new_size, void* ptr) {    // NOLINT
  const base::AllocatorPtr& allocator = GetAllocator(mem);
  void* new_ptr = allocator->AllocateMemory(new_size);
  memcpy(new_ptr, ptr, cur_size);
  allocator->DeallocateMemory(ptr);
  return new_ptr;
}

//-----------------------------------------------------------------------------
//
// Helper functions.
//
//-----------------------------------------------------------------------------

// Converts a FreeType value represented as 26.6 fixed-point to pixels.
static float ToPixels(FT_Pos v26_6) {
  static const float kToPixels = 1.0f / 64.0f;
  return kToPixels * static_cast<float>(v26_6);
}

// Extracts Font::GlyphMetrics from a FreeType glyph.
static const FreeTypeFont::GlyphMetrics GlyphToMetrics(
    const FT_GlyphSlot& glyph) {
  FreeTypeFont::GlyphMetrics metrics;
  // Size and advance values are in 26.6 fixed-point.
  metrics.size.Set(ToPixels(glyph->metrics.width),
                   ToPixels(glyph->metrics.height));
  metrics.bitmap_offset.Set(static_cast<float>(glyph->bitmap_left),
                            static_cast<float>(glyph->bitmap_top));
  metrics.advance.Set(ToPixels(glyph->advance.x), ToPixels(glyph->advance.y));
  return metrics;
}

// Extracts control points from a FreeType glyph.
static void GlyphToControlPoints(const FT_GlyphSlot& glyph,
                                 ControlPoints* control_points) {
  DCHECK_EQ(control_points->size(), 0U);
  if (glyph->format != FT_GLYPH_FORMAT_OUTLINE) return;
  const FT_Outline& outline = glyph->outline;
  control_points->reserve(outline.n_points);
  for (int16 i = 0; i < outline.n_points; ++i) {
    control_points->push_back(math::Point2f(ToPixels(outline.points[i].x),
                                            ToPixels(outline.points[i].y)));
  }
}

// Converts a FreeType glyph bitmap to a Font::GlyphGrid.
static const Font::GlyphGrid GlyphToGrid(const FT_GlyphSlot& glyph) {
  const FT_Bitmap bitmap = glyph->bitmap;
  const size_t width = bitmap.width;
  const size_t height = bitmap.rows;
  const size_t pitch = bitmap.pitch;

  Font::GlyphGrid grid(width, height);
  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < width; ++x) {
      grid.pixels.Set(
          x, y, static_cast<double>(bitmap.buffer[y * pitch + x]) / 255.0);
    }
  }
  return grid;
}

// In order to avoid GlyphIndex collision between the main face and the fallback
// faces, we pack the FreeType glyph index and a face id into a uint64, with the
// face id in the high order 32 bits, and the glyph index in the low order 32
// bits.

// Extracts the FreeType glyph index from a GlyphIndex.
static uint32 GlyphIndexToGlyphId(const GlyphIndex glyph) {
  return static_cast<uint32>(glyph & 0xFFFFFFFF);
}

// Extracts the face id from a GlyphIndex.
static uint32 GlyphIndexToFaceId(const GlyphIndex glyph) {
  return static_cast<uint32>(glyph >> 32);
}

// Packs a freetype glyph index and a face id into a GlyphIndex.
static GlyphIndex BuildGlyphIndex(uint32 glyph, uint32 face) {
  return static_cast<GlyphIndex>(glyph) | (static_cast<GlyphIndex>(face) << 32);
}

}  // anonymous namespace

//-----------------------------------------------------------------------------
//
// FreeTypeFont::Helper class:
// - implements FreeType-specific aspects of Font;
// - owns FT_Face's lifecycle; and
// - presents the interface that ICU Paragraph Layout requires.
//
//-----------------------------------------------------------------------------

class FreeTypeFont::Helper
#if defined(ION_USE_ICU)
    : public icu::LEFontInstance
#endif
{
 public:
  // This struct stores all information for a single glyph in the font.
  struct GlyphMetaData : public Allocatable {
    // Returns true if glyph x- *or* y-size is zero.
    bool IsZeroSize() const { return metrics.IsZeroSize(); }

    // Metrics for the glyph.
    GlyphMetrics metrics;
    // Control points for the glyph, if the font supports this.
    ControlPoints control_points;
  };

  // The constructor is passed an Allocator to use for FreeType structures.
  explicit Helper(FreeTypeFont* owning_font)
      : glyph_metadata_map_(owning_font->GetAllocator()),
        owning_font_(owning_font),
        allocator_(owning_font_->GetAllocator()),
#if defined(ION_USE_ICU)
        font_tables_(allocator_),
#endif  // ION_USE_ICU
        ft_face_(nullptr),
        manager_(FreeTypeManager::GetManagerForAllocator(allocator_)) {
  }
  ~Helper() { FreeFont(); }

  // Initializes the font from FreeType data. Logs messages and returns false
  // on error. If simulate_library_failure is true, this simulates a failure to
  // initialize the FreeType library. This should not be called more than once.
  bool Init(const void* data, size_t data_size, bool simulate_library_failure);

  // Loads a glyph from FreeType and fills in either or both of glyph_data and
  // glyph_grid, when not null.  size_in_pixels is used to scale the glyph.
  // Returns true if a glyph was loaded; will resort to fallback faces if the
  // glyph is not found in the main face.
  bool LoadGlyph(GlyphIndex glyph_index, GlyphMetaData* glyph_meta,
                 Font::GlyphGrid* glyph_grid) const;

  // Returns a FontMetrics for this font.
  const FontMetrics GetFontMetrics() const;

  // Returns the kerning amount in X and Y for a given character pair.
  const math::Vector2f GetKerning(CharIndex char_index0,
                                  CharIndex char_index1) const;

  GlyphIndex GetDefaultGlyphForChar(CharIndex char_index) const;

  // Returns the GlyphData for the indexed character. Returns an invalid
  // reference if the index does not refer to a glyph in the font or one of
  // the fallback fonts.
  const GlyphMetaData& GetGlyphMetaData(GlyphIndex glyph_index) const;

  // Frees up the FreeType font data structures.
  void FreeFont();

  // Adds a fallback face to this font.
  void AddFallbackFace(const std::weak_ptr<Helper>& fallback);

#if defined(ION_USE_ICU)
  // icu::LEFontInstance implementation.
  const void* getFontTable(LETag tableTag, size_t& length) const override;
  le_int32 getUnitsPerEM() const override;
  LEGlyphID mapCharToGlyph(LEUnicode32 ch) const override;
  void getGlyphAdvance(LEGlyphID glyph, LEPoint& advance) const override;
  le_bool getGlyphPoint(LEGlyphID glyph, le_int32 pointNumber,
                        LEPoint& point) const override;
  float getXPixelsPerEm() const override;
  float getYPixelsPerEm() const override;
  float getScaleFactorX() const override;
  float getScaleFactorY() const override;
  le_int32 getAscent() const override;
  le_int32 getDescent() const override;
  le_int32 getLeading() const override;

  const icu::LEFontInstance* GetFace(uint32 index) const;
  GlyphIndex GlyphIndexForICUFont(const icu::LEFontInstance* icu_font,
                                  int32 glyph_id) const;
#endif

 private:
  // Queries the owning font size, and correctly set the FreeType font size or
  // scale based upon that.
  void SetFontSizeLocked() const;

  // Queries for the FreeType glyph index assosiated with |char_index| in the
  // font ft_face_.
  uint32 GetGlyphForChar(CharIndex char_index) const;

  // If ft_face_ contains glyphs for at least one of |char_index0| or
  // |char_index1| this function sets the kerning value for the character pair
  // in |kern| and returns true. If ft_face_ contains glyphs for neither, this
  // function returns false without modifying |kern|.
  bool GetKerningNoFallback(CharIndex char_index0, CharIndex char_index1,
                            math::Vector2f* kern) const;

  // Queries FreeType for the kerning value associated to |char_index0| and
  // |char_index1| in ft_face_.
  const math::Vector2f GetKerningLocked(CharIndex char_index0,
                                        CharIndex char_index1) const;

  // As LoadGlyph above, but assumes mutex_ has been locked.
  bool LoadGlyphLocked(GlyphIndex glyph_index, GlyphMetaData* glyph_meta,
                       Font::GlyphGrid* glyph_grid) const;

  // As LoadGlyphLocked, but does not fallback on failure.
  bool LoadGlyphLockedNoFallback(GlyphIndex glyph_index,
                                 GlyphMetaData* glyph_meta,
                                 Font::GlyphGrid* glyph_grid) const;

  // Convenience typedef for the map storing GlyphData instances.
  typedef base::AllocMap<GlyphIndex, GlyphMetaData> GlyphMetaDataMap;

  // Grid for each glyph in the font, keyed by glyph index. Mutable so that it
  // can be changed from within GetGlyphMetaData, which must be const as it is
  // called from getGlyphAdvance, which is const because it is an override of
  // a const LEFontInstance method.
  mutable GlyphMetaDataMap glyph_metadata_map_;

  FreeTypeFont* owning_font_;
  base::AllocatorPtr allocator_;
#if defined(ION_USE_ICU)
  // pair::second is the size of pair::first.
  mutable base::AllocMap<LETag, std::pair<base::DataContainerPtr, size_t>>
      font_tables_;
#endif
  FT_Face ft_face_;
  std::vector<std::weak_ptr<Helper>> fallback_helpers_;
  FreeTypeManager* manager_;
  mutable std::mutex mutex_;
};

bool FreeTypeFont::Helper::Init(const void* data, size_t data_size,
                                bool simulate_library_failure) {
  DCHECK(!ft_face_);
  std::lock_guard<std::mutex> guard(mutex_);
  ft_face_ = manager_->InitFont(data, data_size, simulate_library_failure);
  if (!ft_face_) {
    LOG(ERROR) << "Could not read the FreeType font data.";
  }
  return ft_face_;
}

bool FreeTypeFont::Helper::LoadGlyph(GlyphIndex glyph_index,
                                     GlyphMetaData* glyph_meta,
                                     Font::GlyphGrid* glyph_grid) const {
  std::lock_guard<std::mutex> guard(mutex_);
  return LoadGlyphLocked(glyph_index, glyph_meta, glyph_grid);
}

bool FreeTypeFont::Helper::LoadGlyphLocked(GlyphIndex glyph_index,
                                           GlyphMetaData* glyph_meta,
                                           Font::GlyphGrid* glyph_grid) const {
  uint32 face_id = GlyphIndexToFaceId(glyph_index);
  if (face_id == 0) {
    return LoadGlyphLockedNoFallback(glyph_index, glyph_meta, glyph_grid);
  } else {
    if (auto helper = fallback_helpers_[face_id - 1].lock()) {
      std::lock_guard<std::mutex> guard(helper->mutex_);
      return helper->LoadGlyphLockedNoFallback(glyph_index, glyph_meta,
                                               glyph_grid);
    } else {
      return false;
    }
  }
}

bool FreeTypeFont::Helper::LoadGlyphLockedNoFallback(
    GlyphIndex glyph_index, GlyphMetaData* glyph_meta,
    Font::GlyphGrid* glyph_grid) const {
  DCHECK(ft_face_);

  // Indicate the proper size for the glyphs.
  SetFontSizeLocked();

  // Load FT glyph.
  if (!manager_->LoadGlyph(ft_face_, GlyphIndexToGlyphId(glyph_index))) {
    return false;
  }

  // Create GlyphData and add to Font.
  if (glyph_meta) {
    glyph_meta->metrics = GlyphToMetrics(ft_face_->glyph);
    GlyphToControlPoints(ft_face_->glyph, &glyph_meta->control_points);
  }
  if (glyph_grid) {
    *glyph_grid = GlyphToGrid(ft_face_->glyph);
  }
  return true;
}

void FreeTypeFont::Helper::SetFontSizeLocked() const {
  // C.f. the "Global glyph metrics" section of
  // http://www.freetype.org/freetype2/docs/tutorial/step2.html
  const size_t size_in_pixels = owning_font_->GetSizeInPixels();
  if (FT_IS_SCALABLE(ft_face_)) {
    FT_Set_Pixel_Sizes(ft_face_, static_cast<FT_UInt>(size_in_pixels),
                       static_cast<FT_UInt>(size_in_pixels));
  } else {  // Must be fixed size (bitmap) font.
    DCHECK(ft_face_->num_fixed_sizes);
    int closest_face = 0;  // Index of bitmap-strike closest to size_in_pixels.
    size_t closest_size = 0xFFFFFFFF;
    for (int i = 0; i < ft_face_->num_fixed_sizes; ++i) {
      FT_Select_Size(ft_face_, i);
      const size_t size_difference =
          abs(static_cast<int>(ft_face_->size->metrics.y_ppem) -
              static_cast<int>(size_in_pixels));
      if (size_difference < closest_size) {
        closest_size = size_difference;
        closest_face = i;
      }
    }
    FT_Select_Size(ft_face_, closest_face);
  }
}

const Font::FontMetrics FreeTypeFont::Helper::GetFontMetrics() const {
  std::lock_guard<std::mutex> guard(mutex_);
  FontMetrics metrics;
  SetFontSizeLocked();
  const size_t size_in_pixels = owning_font_->GetSizeInPixels();
  const float global_glyph_height = static_cast<float>(
      ft_face_->size->metrics.ascender - ft_face_->size->metrics.descender);
  metrics.line_advance_height =
      static_cast<float>(ft_face_->size->metrics.height / 64);
  // Some fonts do not contain the correct ascender or descender values, but
  // instead only the maximum and minimum y values, which will exceed the size.
  // To handle these cases, approximate the ascender with the ratio of ascender
  // to (ascender + descender) and scale by size.
  metrics.ascender = static_cast<float>(ft_face_->size->metrics.ascender *
                                        size_in_pixels / global_glyph_height);

  return metrics;
}

const math::Vector2f FreeTypeFont::Helper::GetKerning(
    CharIndex char_index0, CharIndex char_index1) const {
  math::Vector2f retval = math::Vector2f::Zero();
  if (GetKerningNoFallback(char_index0, char_index1, &retval)) {
    return retval;
  }

  for (auto& fallback_helper : fallback_helpers_) {
    auto fallback = fallback_helper.lock();
    if (fallback &&
        fallback->GetKerningNoFallback(char_index0, char_index1, &retval)) {
      return retval;
    }
  }
  return retval;
}

bool FreeTypeFont::Helper::GetKerningNoFallback(CharIndex char_index0,
                                                CharIndex char_index1,
                                                math::Vector2f* kern) const {
  const uint32 idx0 = GetGlyphForChar(char_index0);
  const uint32 idx1 = GetGlyphForChar(char_index1);
  std::lock_guard<std::mutex> guard(mutex_);
  if (idx0 != 0 && idx1 != 0) {
    // Both will be rendered with this font.
    *kern = GetKerningLocked(char_index0, char_index1);
    return true;
  } else if (idx0 != 0 || idx1 != 0) {
    // One will, but the other won't.
    *kern = math::Vector2f::Zero();
    return true;
  }
  return false;
}

const math::Vector2f FreeTypeFont::Helper::GetKerningLocked(
    CharIndex char_index0, CharIndex char_index1) const {
  math::Vector2f kerning(0.f, 0.f);
  FT_Vector ft_kerning;
  if (ft_face_ && FT_HAS_KERNING(ft_face_) &&
      !FT_Get_Kerning(ft_face_, static_cast<FT_UInt>(char_index0),
                      static_cast<FT_UInt>(char_index1), FT_KERNING_DEFAULT,
                      &ft_kerning)) {
    // Kerning values are in 26.6 fixed-point.
    kerning.Set(ToPixels(ft_kerning.x), ToPixels(ft_kerning.y));
  }
  return kerning;
}

uint32 FreeTypeFont::Helper::GetGlyphForChar(CharIndex char_index) const {
  std::lock_guard<std::mutex> guard(mutex_);
  return FT_Get_Char_Index(ft_face_, char_index);
}

GlyphIndex FreeTypeFont::Helper::GetDefaultGlyphForChar(
    CharIndex char_index) const {
  uint32 idx = GetGlyphForChar(char_index);
  if (idx != 0) {
    return BuildGlyphIndex(idx, 0);
  }
  for (uint32 i = 0; i < fallback_helpers_.size(); ++i) {
    if (auto helper = fallback_helpers_[i].lock()) {
      idx = helper->GetGlyphForChar(char_index);
      if (idx != 0) {
        return BuildGlyphIndex(idx, i + 1);
      }
    }
  }
  // If we didn't get a valid glyph, replace with the Unicode replacement
  // character: https://en.wikipedia.org/wiki/Specials_(Unicode_block).
  return BuildGlyphIndex(GetGlyphForChar(static_cast<CharIndex>(0xfffd)), 0);
}

const FreeTypeFont::Helper::GlyphMetaData&
FreeTypeFont::Helper::GetGlyphMetaData(GlyphIndex glyph_index) const {
  std::lock_guard<std::mutex> guard(mutex_);
  if (!glyph_index) {
    return base::InvalidReference<GlyphMetaData>();
  }
  const auto& it = glyph_metadata_map_.find(glyph_index);
  if (it == glyph_metadata_map_.end()) {
    GlyphMetaData glyph_meta;
    if (LoadGlyphLocked(glyph_index, &glyph_meta, nullptr)) {
      return glyph_metadata_map_[glyph_index] = glyph_meta;
    }
    return base::InvalidReference<GlyphMetaData>();
  }
  return it->second;
}

void FreeTypeFont::Helper::FreeFont() {
  std::lock_guard<std::mutex> guard(mutex_);
  if (ft_face_) {
    manager_->FreeFont(ft_face_);
    ft_face_ = nullptr;
  }
}

void FreeTypeFont::Helper::AddFallbackFace(
    const std::weak_ptr<Helper>& fallback) {
  auto locked_fallback = fallback.lock();
  if (!locked_fallback || locked_fallback.get() == this) {
    return;
  }
  std::lock_guard<std::mutex> guard(mutex_);
  fallback_helpers_.push_back(fallback);
}

#if defined(ION_USE_ICU)
const void* FreeTypeFont::Helper::getFontTable(LETag tableTag,
                                               size_t& length) const {
  auto it = font_tables_.find(tableTag);
  if (it == font_tables_.end()) {
    FT_ULong table_size = 0;
    FT_Error error =
        FT_Load_Sfnt_Table(ft_face_, tableTag, 0, nullptr, &table_size);
    // It's legit for a font table to be missing.
    if (!error && error != FT_Err_Table_Missing) {
      base::DataContainerPtr table =
          base::DataContainer::CreateOverAllocated<FT_Byte>(table_size, nullptr,
                                                            allocator_);
      error = FT_Load_Sfnt_Table(ft_face_, tableTag, 0,
                                 table->GetMutableData<FT_Byte>(), &table_size);
      DCHECK_EQ(error, 0);
      auto inserted = font_tables_.insert(
          std::make_pair(tableTag, std::make_pair(table, table_size)));
      DCHECK(inserted.second);
      it = inserted.first;
    }
  }
  if (it == font_tables_.end()) {
    length = 0;
    return nullptr;
  }
  length = it->second.second;
  return it->second.first->GetMutableData<uint8>();
}

le_int32 FreeTypeFont::Helper::getUnitsPerEM() const {
  return ft_face_->units_per_EM;
}

LEGlyphID FreeTypeFont::Helper::mapCharToGlyph(LEUnicode32 ch) const {
  return FT_Get_Char_Index(ft_face_, ch);
}

static void SetLEPointToZero(LEPoint* point) {
  point->fX = 0.f;
  point->fY = 0.f;
}

void FreeTypeFont::Helper::getGlyphAdvance(LEGlyphID glyph,
                                           LEPoint& advance) const {
  const GlyphMetaData& data = GetGlyphMetaData(glyph);
  if (base::IsInvalidReference(data)) return SetLEPointToZero(&advance);
  const FreeTypeFont::GlyphMetrics& metrics = data.metrics;
  advance.fX = metrics.advance[0];
  advance.fY = metrics.advance[1];
}

le_bool FreeTypeFont::Helper::getGlyphPoint(LEGlyphID glyph,
                                            le_int32 pointNumber,
                                            LEPoint& point) const {
  const GlyphMetaData& data = GetGlyphMetaData(glyph);
  if (base::IsInvalidReference(data)) {
    SetLEPointToZero(&point);
    return false;
  }
  const ControlPoints& control_points = data.control_points;
  if (static_cast<size_t>(pointNumber) < control_points.size()) {
    point.fX = control_points[pointNumber][0];
    point.fY = control_points[pointNumber][1];
    return true;
  }
  return false;
}

float FreeTypeFont::Helper::getXPixelsPerEm() const {
  return ft_face_->size->metrics.x_ppem;
}

float FreeTypeFont::Helper::getYPixelsPerEm() const {
  return ft_face_->size->metrics.y_ppem;
}

float FreeTypeFont::Helper::getScaleFactorX() const {
  // FreeType stores the x_scale as a 16.16 fixed point value
  static const float k16_16ToFloat = 1.0f / 65536.0f;
  return static_cast<float>(ft_face_->size->metrics.x_scale) * k16_16ToFloat;
}

float FreeTypeFont::Helper::getScaleFactorY() const {
  // FreeType stores the y_scale as a 16.16 fixed point value
  static const float k16_16ToFloat = 1.0f / 65536.0f;
  return static_cast<float>(ft_face_->size->metrics.y_scale) * k16_16ToFloat;
}

le_int32 FreeTypeFont::Helper::getAscent() const {
  return static_cast<le_int32>(
      FT_MulFix(ft_face_->ascender,
                static_cast<FT_Int32>(ft_face_->size->metrics.y_scale)) /
      64);
}

le_int32 FreeTypeFont::Helper::getDescent() const {
  return -static_cast<le_int32>(
      FT_MulFix(ft_face_->descender, ft_face_->size->metrics.y_scale) / 64);
}

le_int32 FreeTypeFont::Helper::getLeading() const {
  return static_cast<le_int32>(
      FT_MulFix(ft_face_->height,
                static_cast<FT_Int32>(ft_face_->size->metrics.y_scale)) /
      64);
}

const icu::LEFontInstance* FreeTypeFont::Helper::GetFace(uint32 index) const {
  if (index == 0) {
    return this;
  }
  auto helper = fallback_helpers_[index - 1].lock();
  if (helper) {
    return helper.get();
  }
  return nullptr;
}

GlyphIndex FreeTypeFont::Helper::GlyphIndexForICUFont(
    const icu::LEFontInstance* icu_font, int32 glyph_id) const {
  for (uint32 i = 0, n = static_cast<uint32>(fallback_helpers_.size()); i <= n;
       ++i) {
    if (icu_font == GetFace(i)) {
      return BuildGlyphIndex(glyph_id, i);
    }
  }
  return BuildGlyphIndex(glyph_id, 0);
}

#endif  // ION_USE_ICU

//-----------------------------------------------------------------------------
//
// FreeTypeFont functions.
//
//-----------------------------------------------------------------------------

FreeTypeFont::FreeTypeFont(const std::string& name, size_t size_in_pixels,
                           size_t sdf_padding, const void* data,
                           size_t data_size)
    : Font(name, size_in_pixels, sdf_padding), helper_(new Helper(this)) {
  // Initialize the Freetype font and set the font metrics.
  if (helper_->Init(data, data_size, false)) {
    SetFontMetrics(helper_->GetFontMetrics());
  }
}

FreeTypeFont::FreeTypeFont(const std::string& name, size_t size_in_pixels,
                           size_t sdf_padding)
    : Font(name, size_in_pixels, sdf_padding), helper_(new Helper(this)) {
  // Simulate library initialization failure.
  helper_->Init(nullptr, 0U, true);
}

FreeTypeFont::~FreeTypeFont() {}

bool FreeTypeFont::LoadGlyphGrid(GlyphIndex glyph_index,
                                 GlyphGrid* glyph_grid) const {
  return helper_->LoadGlyph(glyph_index, nullptr, glyph_grid);
}

const math::Vector2f FreeTypeFont::GetKerning(CharIndex char_index0,
                                              CharIndex char_index1) const {
  return helper_->GetKerning(char_index0, char_index1);
}

#if defined(ION_USE_ICU)
void FreeTypeFont::GetFontRunsForText(icu::UnicodeString chars,
                                      iculx::FontRuns* runs) const {
  uint32 current_face = GlyphIndexToFaceId(GetDefaultGlyphForChar(chars[0]));
  for (int i = 1; i < chars.length(); ++i) {
    uint32 this_face = GlyphIndexToFaceId(GetDefaultGlyphForChar(chars[i]));
    if (this_face != current_face) {
      runs->add(helper_->GetFace(current_face), i);
      current_face = this_face;
    }
  }
  runs->add(helper_->GetFace(current_face), chars.length());
  return;
}

GlyphIndex FreeTypeFont::GlyphIndexForICUFont(
    const icu::LEFontInstance* icu_font, int32 glyph_id) const {
  return helper_->GlyphIndexForICUFont(icu_font, glyph_id);
}
#endif  // ION_USE_ICU

GlyphIndex FreeTypeFont::GetDefaultGlyphForChar(CharIndex char_index) const {
  return helper_->GetDefaultGlyphForChar(char_index);
}

const Layout FreeTypeFont::BuildLayout(const std::string& text,
                                       const LayoutOptions& options) const {
  if (text.empty() || !IsSizeValid(options.target_size)) return Layout();

  const Lines lines = base::SplitString(text, "\n");

  // Determine the size of the text.
  const TextSize text_size = ComputeTextSize(*this, options, lines);

  // Determine how to convert pixel-based glyph rectangles to world-space
  // rectangles in the XY-plane.
  const FreeTypeFontTransformData transform_data =
      ComputeTransformData(*this, options, text_size);

// Lay out the text using all the data.
#if defined(ION_USE_ICU)
  bool use_icu = true;
#else   // ION_USE_ICU
  bool use_icu = false;
#endif  // ION_USE_ICU

  return LayOutText(*this, use_icu, lines, transform_data);
}

void FreeTypeFont::AddFallbackFont(const FontPtr& fallback) {
  helper_->AddFallbackFace(std::weak_ptr<Helper>(
      reinterpret_cast<FreeTypeFont*>(fallback.Get())->helper_));
}

const FreeTypeFont::GlyphMetrics& FreeTypeFont::GetGlyphMetrics(
    GlyphIndex glyph_index) const {
  if (!glyph_index) {
    return base::InvalidReference<FreeTypeFont::GlyphMetrics>();
  }
  const auto& meta = helper_->GetGlyphMetaData(glyph_index);
  if (base::IsInvalidReference(meta)) {
    return base::InvalidReference<FreeTypeFont::GlyphMetrics>();
  }
  return meta.metrics;
}

}  // namespace text
}  // namespace ion
