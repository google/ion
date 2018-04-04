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

#include "ion/image/ninepatch.h"

#include <algorithm>

#include "base/integral_types.h"
#include "ion/base/allocationmanager.h"
#include "ion/base/datacontainer.h"
#include "ion/base/logging.h"

namespace ion {
namespace image {

namespace {

using base::DataContainer;
using base::DataContainerPtr;
using gfx::Image;
using gfx::ImagePtr;
using math::Point2f;
using math::Point2ui;
using math::Range1ui;
using math::Range2f;
using math::Range2ui;
using math::Vector2f;
using math::Vector2ui;

typedef base::AllocMap<uint32, uint32> RegionMap;

// The value of a marked pixel.
static const uint32 kMarked = 0xff000000;

// Reads the stretch regions from a ninepatch image and adds a set of regions to
// the passed map.
static void ReadStretchRegions(const ImagePtr& image, RegionMap* regions,
                               uint32 length, uint32 stride) {
  bool in_stretch_region = false;
  uint32 stretch_region_start = 0;

  // The format must be kRgba8888.
  DCHECK_EQ(Image::kRgba8888, image->GetFormat());
  const uint32* data = image->GetData()->GetData<uint32>();
  // We start at 1 and end 1 pixel early since the corner pixels don't map to
  // any meaningful part of the image.
  for (uint32 i = 1U; i < length - 1U; ++i) {
    const bool marked = data[i * stride] == kMarked;
    if (marked && !in_stretch_region) {
      stretch_region_start = i;
      in_stretch_region = true;
    } else if (!marked && in_stretch_region) {
      (*regions)[stretch_region_start] = i - stretch_region_start;
      in_stretch_region = false;
    }
  }
  // If the last stretch region extends to the last pixel in the row or column
  // then we need to terminate it.
  if (in_stretch_region)
    (*regions)[stretch_region_start] = (length - 1) - stretch_region_start;
}

// Reads the padding box from a ninepatch image and returns its extents.
static Range2ui ReadPaddingBox(const ImagePtr& image) {
  Point2ui start;
  Point2ui end;
  Range2ui box;

  DCHECK_EQ(Image::kRgba8888, image->GetFormat());
  const uint32* data = image->GetData()->GetData<uint32>();
  const uint32 width = image->GetWidth();
  const uint32 height = image->GetHeight();
  // The last row of the image.
  const uint32* row = &data[width * (height - 1U)];
  // The start of the last column of the image.
  const uint32* col = &data[width - 1U];

  // We start at 1 and end 1 pixel early since the corner pixels don't map to
  // any meaningful part of the image.
  for (uint32 x = 1U; x < width - 1U; ++x) {
    if (row[x] == kMarked) {
      if (!start[0])
        start[0] = x;
      end[0] = x;
    }
  }
  for (uint32 y = 1U; y < height - 1U; ++y) {
    if (col[width * y] == kMarked) {
      if (!start[1])
        start[1] = y;
      end[1] = y;
    }
  }
  // Set the box if anything was set.
  if (start != Point2ui::Zero() || end != Point2ui::Zero()) {
    // Make range half inclusive [start, end).
    ++end[0];
    ++end[1];
    box.Set(start, end);
  }
  return box;
}

// Returns the pixel value at [x,y] in image.
static uint32 GetPixel(const ImagePtr& image, uint32 x, uint32 y) {
  if (x < image->GetWidth() && y < image->GetHeight()) {
    const uint32* pixels = image->GetData()->GetData<uint32>();
    return pixels[y * image->GetWidth() + x];
  }
  return 0U;
}

// Sets the pixel at [x,y] in image to value.
static void SetPixel(const ImagePtr& image, uint32 x, uint32 y, uint32 value) {
  if (x < image->GetWidth() && y < image->GetHeight()) {
    uint32* pixels = image->GetData()->GetMutableData<uint32>();
    pixels[y * image->GetWidth() + x] = value;
  }
}

// Draws pixels from the source image onto the destination image, using
// nearest-neighbor interpolation.
static void CopyRegion(const Range2ui& source_rect, const Range2f& dest_rect,
                       const ImagePtr& source, const ImagePtr& dest) {
  // The start of the source range, in floating-point.
  const Point2f source_start(static_cast<float>(source_rect.GetMinPoint()[0]),
                             static_cast<float>(source_rect.GetMinPoint()[1]));
  // The end of the source range.
  const Point2ui& source_end = source_rect.GetMaxPoint();
  // Floating-point size of the source
  const Vector2f source_size(
      static_cast<float>(source_end[0]) - source_start[0],
      static_cast<float>(source_end[1]) - source_start[1]);
  // Floating-point start and end of the dest range.
  const Point2f& dest_start_f = dest_rect.GetMinPoint();
  const Point2f& dest_end_f = dest_rect.GetMaxPoint();
  // Inverse size of the dest range.
  const Vector2f inv_dest_size(1.f / (dest_end_f[0] - dest_start_f[0]),
                               1.f / (dest_end_f[1] - dest_start_f[1]));
  // Start and end of the dest range.
  const Vector2ui dest_start(static_cast<uint32>(dest_start_f[0]),
                             static_cast<uint32>(dest_start_f[1]));
  const Vector2ui dest_end(static_cast<uint32>(dest_end_f[0]),
                           static_cast<uint32>(dest_end_f[1]));

  // Iterate over the destination pixels and determine what source pixels to
  // read from.
  for (uint32 y = dest_start[1]; y < dest_end[1]; ++y) {
    // Determine how far along this column we are.
    const float y_distance =
        static_cast<float>(y - dest_start[1]) * inv_dest_size[1];
    // Map this distance to the source region.
    const uint32 source_y =
        static_cast<uint32>(source_start[1] + y_distance * source_size[1]);
    DCHECK_LT(source_y, source_end[1]);
    for (uint32 x = dest_start[0]; x < dest_end[0]; ++x) {
      // Determine how far along this row we are.
      const float x_distance =
          static_cast<float>(x - dest_start[0]) * inv_dest_size[0];
      // Map this distance to the source region.
      const uint32 source_x =
          static_cast<uint32>(source_start[0] + x_distance * source_size[0]);
      DCHECK_LT(source_x, source_end[0]);
      const uint32 value = GetPixel(source, source_x, source_y);
      SetPixel(dest, x, y, value);
    }
  }
}

}  // anonymous namespace

struct NinePatch::Region {
  Region() : stretch_h(false), stretch_v(false) {}
  Region(const Point2ui& source_start, const Point2f& dest_start)
      : stretch_h(false), stretch_v(false) {
    source.SetMinPoint(source_start);
    dest.SetMinPoint(dest_start);
  }

  bool stretch_h;
  bool stretch_v;
  // This is integral because the source data is pixels.
  Range2ui source;
  // This must be floating-point to avoid round-off error with non-integer
  // stretch ratios.
  Range2f dest;
};

NinePatch::NinePatch(const ImagePtr& image)
    : regions_h_(*this), regions_v_(*this), wipeable_(true) {
  if (image.Get() && image->GetWidth() && image->GetHeight() &&
      image->GetFormat() == Image::kRgba8888 && image->GetData().Get() &&
      image->GetData()->GetData()) {
    image_ = image;
    // Read the metadata from the image.
    ReadStretchRegions(image_, &regions_v_, image_->GetWidth(), 1U);
    ReadStretchRegions(image_, &regions_h_, image_->GetHeight(),
                       image_->GetWidth());
    padding_ = ReadPaddingBox(image_);
  }
}

NinePatch::~NinePatch() {}

const ImagePtr NinePatch::BuildImage(uint32 width, uint32 height,
                                     const base::AllocatorPtr& alloc) const {
  base::AllocatorPtr allocator =
      base::AllocationManager::GetNonNullAllocator(alloc);

  // Create the image and set it completely transparent.
  ImagePtr output(new (allocator) Image);
  const uint32 length = 4U * width * height;
  uint32* pixels = reinterpret_cast<uint32*>(allocator->AllocateMemory(length));
  output->Set(Image::kRgba8888, width, height,
             DataContainer::Create<uint32>(
                 pixels, std::bind(DataContainer::AllocatorDeleter, allocator,
                                   std::placeholders::_1),
                                   wipeable_, allocator));
  memset(pixels, 0, length);

  if (image_.Get()) {
    // Copy the regions from the source to dest image.
    const base::AllocVector<Region> regions = GetRegionsForSize(width, height);
    const size_t count = regions.size();
    for (size_t i = 0; i < count; ++i)
      CopyRegion(regions[i].source, regions[i].dest, image_, output);
  }
  return output;
}

const Vector2ui NinePatch::GetMinimumSize() const {
  // 
  // size.  On Android, the minimum size is the same as the natural size.
  // Stretch regions are never shrunk.
  Vector2ui min_size(2U, 2U);
  if (image_.Get()) {
    min_size.Set(image_->GetWidth(), image_->GetHeight());
    for (RegionMap::const_iterator it = regions_v_.begin();
         it != regions_v_.end(); ++it)
      min_size[0] -= it->second;
    for (RegionMap::const_iterator it = regions_h_.begin();
         it != regions_h_.end(); ++it)
      min_size[1] -= it->second;

    // Ensure the size is at least (2, 2).
    min_size[0] = std::max(2U, min_size[0]);
    min_size[1] = std::max(2U, min_size[1]);
  }
  // Since the pixels of the first and last rows and columns are just the image
  // metadata, subtract those if the source image isn't completely empty.
  return min_size - Vector2ui(2U, 2U);
}

const Range2ui NinePatch::GetPaddingBox(uint32 width, uint32 height) const {
  if (padding_.IsEmpty() || !image_.Get()) {
    // No padding box was specified in the source image (or the image is empty),
    // so return the full requested size.
    // 
    // the padding instead.
    return Range2ui(Point2ui::Zero(), Point2ui(width, height));
  }
  // The minimum sized image this instance can build.
  const Vector2ui min_size = GetMinimumSize();
  // The requested dimensions cannot be smaller than the minimum image size.
  const Vector2ui clamped_size(std::max(min_size[0], width),
                               std::max(min_size[1], height));
  // The natural size of the image is the source size without the ninepatch
  // borders.
  const Vector2ui natural_size(image_->GetWidth() - 2U,
                               image_->GetHeight() - 2U);
  // The padding box at natural size accounts for the 1 pixel offset from
  // removing the ninepatch borders.
  Range2ui box(padding_.GetMinPoint() - Vector2ui(1U, 1U),
               padding_.GetMaxPoint() - Vector2ui(1U, 1U));
  // Adjust the box with the target size. Note that the offset can potentially
  // be negative, but this is fine, as uint32s have well-defined overflow
  // behavior.
  const Vector2ui offset(clamped_size[0] - natural_size[0],
                         clamped_size[1] - natural_size[1]);
  box.SetMaxPoint(box.GetMaxPoint() + offset);
  return box;
}

const Vector2ui NinePatch::GetSizeToFitContent(
    uint32 content_width, uint32 content_height) const {
  if (padding_.IsEmpty() || !image_.Get()) {
    // No padding box was specified in the source image (or the image is empty),
    // so return the full requested size.
    // 
    // the padding instead.
    return Vector2ui(content_width, content_height);
  }
  // The natural size of the image is the source size without the ninepatch
  // borders.
  const Vector2ui natural_size(image_->GetWidth() - 2U,
                               image_->GetHeight() - 2U);
  const Vector2ui natural_pad_size = padding_.GetSize();

  // Adjust the natural size with the content size.
  // This will always be positive because the naturalpad size cannot be larger
  // than the natural_size.
  const Vector2ui size(natural_size[0] + content_width - natural_pad_size[0],
                       natural_size[1] + content_height - natural_pad_size[1]);

  // The minimum sized image this instance can build.
  const Vector2ui min_size = GetMinimumSize();
  const Vector2ui clamped_size(std::max(min_size[0], size[0]),
                               std::max(min_size[1], size[1]));
  return clamped_size;
}

const base::AllocVector<NinePatch::Region> NinePatch::GetRegionsForSize(
    uint32 width, uint32 height) const {
  DCHECK(image_.Get());
  base::AllocVector<Region> regions(*this);

  const Vector2ui min_size = GetMinimumSize();
  const Vector2ui natural_size(image_->GetWidth(), image_->GetHeight());

  // Ensure there is no division by zero below. This can only occur of there are
  // no stretch regions.
  if (natural_size[0] == 2U + min_size[0] ||
      natural_size[1] == 2U + min_size[1])
    return regions;

  // Converts from source to destination sizes.
  const Point2f stretch_ratio(
      static_cast<float>(width - min_size[0]) /
          static_cast<float>(natural_size[0] - 2U - min_size[0]),
      static_cast<float>(height - min_size[1]) /
          static_cast<float>(natural_size[1] - 2U - min_size[1]));
  Point2ui next_stretch(0U, 0U);
  Point2f next_dest(0.f, 0.f);

  // Ignore the corner pixels of the image.
  uint32 region_start_y = 1;
  while (region_start_y < natural_size[1] - 1U) {
    uint32 region_start_x = 1;
    RegionMap::const_iterator y_iter = regions_h_.lower_bound(region_start_y);
    if (y_iter == regions_h_.end()) {
      // The current static region is at the end of the column, close it.
      next_stretch[1] = natural_size[1] - 1U;
    } else {
      next_stretch[1] = y_iter->first;
    }
    while (region_start_x < natural_size[0] - 1U) {
      RegionMap::const_iterator x_iter = regions_v_.lower_bound(region_start_x);
      if (x_iter == regions_v_.end()) {
        // The current static region is at the end of the row, close it.
        next_stretch[0] = natural_size[0] - 1U;
      } else {
        next_stretch[0] = x_iter->first;
      }

      // Determine if we're in a static region.
      const bool is_static_region_x = region_start_x < next_stretch[0];
      const bool is_static_region_y = region_start_y < next_stretch[1];

      Vector2ui source_size;
      Vector2f dest_size;
      Region region(Point2ui(region_start_x, region_start_y),
                    Point2f(next_dest[0], next_dest[1]));
      if (is_static_region_x) {
        // This is a static region, so the destination width and source width
        // are both equal to the distance from here to the next stretch region,
        // regardless of destination size.
        region.stretch_h = false;
        source_size[0] = next_stretch[0] - region_start_x;
        dest_size[0] = static_cast<float>(source_size[0]);
        // Move our counter to point at the beginning of the next (stretch)
        // region (or end-of-row if there isn't one).
        region_start_x = next_stretch[0];
      } else {
        // This is a stretch region, so the source width is in the stretch
        // region map, and the destination width is determined by the stretch
        // ratio.
        region.stretch_h = true;
        source_size[0] = regions_v_.find(region_start_x)->second;
        dest_size[0] = static_cast<float>(source_size[0]) * stretch_ratio[0];
        // Move our counter to point at the beginning of the next (static)
        // region (or end-of-row if there isn't one).
        region_start_x += regions_v_.find(region_start_x)->second;
      }
      // Calculate the heights for the vertical dimension. Note that all regions
      // in this row will have the same height.
      if (is_static_region_y) {
        region.stretch_v = false;
        source_size[1] = next_stretch[1] - region_start_y;
        dest_size[1] = static_cast<float>(source_size[1]);
      } else {
        region.stretch_v = true;
        source_size[1] = regions_h_.find(region_start_y)->second;
        dest_size[1] = static_cast<float>(source_size[1]) * stretch_ratio[1];
      }

      // Set the endpoints of the ranges.
      region.source.SetMaxPoint(region.source.GetMinPoint() + source_size);
      region.dest.SetMaxPoint(region.dest.GetMinPoint() + dest_size);

      // Now that we know how wide the region will be in the destination, we can
      // increment this to figure out the next one.
      next_dest[0] += dest_size[0];

      regions.push_back(region);
    }
    // Reset the destination range since we're on a new row, adding the height
    // of the last region (which will be the same as the height of any other
    // region in the last row) to the destination y location for the new row.
    next_dest[0] = 0.0;
    next_dest[1] += regions.back().dest.GetSize()[1];
    const bool is_static_region_y = region_start_y < next_stretch[1];
    // Same as above, but only needed once per row instead of for every region
    // because every region in a given row will share the same start y value.
    if (is_static_region_y)
      region_start_y = next_stretch[1];
    else
      region_start_y += regions_h_.find(region_start_y)->second;
  }
  return regions;
}

}  // namespace image
}  // namespace ion
