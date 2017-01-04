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

#ifndef ION_IMAGE_NINEPATCH_H_
#define ION_IMAGE_NINEPATCH_H_

#include "base/integral_types.h"
#include "base/macros.h"
#include "ion/base/stlalloc/allocmap.h"
#include "ion/base/stlalloc/allocvector.h"
#include "ion/external/gtest/gunit_prod.h"
#include "ion/gfx/image.h"
#include "ion/math/range.h"
#include "ion/math/vector.h"

namespace ion {
namespace image {

// Represents a nine-patch image as described in the Android SDK reference.
// Valid nine-patches can be created using the draw9patch tool included with the
// Android SDK, or drawn manually as PNG files. This implementation of the
// nine-patch format supports arbitrary numbers of stretch regions along each
// dimension, but only one continuous content region. The interpolation of
// pixels regions is done in a nearest-neighbor fashion. All stretch regions are
// stretched proportional to their size.
class ION_API NinePatch : public base::Referent {
 public:
  // Sets the base image data of the NinePatch to the passed image, extracting
  // stretch and padding information from the image. If the image is NULL, has
  // no data, 0 dimensions, or is not of format Image::kRgba8888 then
  // BuildImage() will always return a blank image.
  explicit NinePatch(const gfx::ImagePtr& image);

  // Creates and returns an Image using the supplied Allocator and sets the
  // wipability of the image as requested. If the source image was invalid (see
  // above) then this function returns a blank image of the requested size.
  // Otherwise, the image contains the representation of this nine-patch at the
  // given size. If the size specified is smaller than the minimum size, the
  // image will be the minimum size instead. If the nine-patch has no stretch
  // regions along one or both dimensions, the returned image will be padded
  // with transparent pixels along the bottom and/or right edges.
  const gfx::ImagePtr BuildImage(uint32 width, uint32 height,
                                 const base::AllocatorPtr& alloc) const;

  // Sets whether images returned by BuildImage() are wipeable (see
  // base::DataContainer) or not. An image is wipeable if its data is deleted
  // after it is uploaded to OpenGL. Changing this option affects all future
  // images built by this. By default, all built images are wipeable.
  void SetBuildWipeable(bool wipable) { wipeable_ = wipable; }

  // Returns the minimum size at which this nine-patch can be drawn, i.e., the
  // size at which all stretch regions are removed.
  const math::Vector2ui GetMinimumSize() const;

  // Returns the padding box for this nine-patch for an image of the requested
  // size. If the source image of this instance did not specify a padding box
  // then the returned Range has the same size as the inputs.
  const math::Range2ui GetPaddingBox(uint32 width, uint32 height) const;

  // Returns the minimum size image required to fit the desired content inside
  // the "drawable area" (padding box). Note this is clamped to the NinePatch
  // image's minimum size.
  const math::Vector2ui GetSizeToFitContent(
      uint32 content_width, uint32 content_height) const;

 private:
  // Helper struct for tracking stretchable regions of the image.
  struct Region;

  // The destructor is private because all base::Referent classes must have
  // protected or private destructors.
  ~NinePatch() override;

  // Returns a vector of Regions that map source image areas to destination
  // image areas.
  const base::AllocVector<Region> GetRegionsForSize(uint32 width,
                                                    uint32 height) const;

  // Horizontal and vertical stretch regions.
  base::AllocMap<uint32, uint32> regions_h_;
  base::AllocMap<uint32, uint32> regions_v_;
  // Padding box.
  math::Range2ui padding_;
  // The source image to create other images from.
  gfx::ImagePtr image_;
  // Whether to set created images as wipeable (defaults to true).
  bool wipeable_;

  FRIEND_TEST(NinePatch, StretchRegions);
  FRIEND_TEST(NinePatch, PaddingBox);
  DISALLOW_COPY_AND_ASSIGN(NinePatch);
};

// Convenience typedef.
using NinePatchPtr = base::SharedPtr<NinePatch>;

}  // namespace image
}  // namespace ion

#endif  // ION_IMAGE_NINEPATCH_H_
