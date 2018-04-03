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

#include "ion/gfx/texture.h"

#include <algorithm>

#include "ion/base/enumhelper.h"
#include "ion/base/static_assert.h"
#include "ion/math/utils.h"
#include "ion/portgfx/glheaders.h"
#include "absl/base/macros.h"

namespace ion {
namespace gfx {

//-----------------------------------------------------------------------------
//
// TextureBase::Face
//
//-----------------------------------------------------------------------------
TextureBase::Face::Face(TextureBase* texture, int sub_image_changed_bit,
                        int mipmaps_changed_start_bit)
    : sub_images_changed_(sub_image_changed_bit, false, texture),
      sub_images_(*texture),
      mipmaps_(mipmaps_changed_start_bit, kMipmapSlotCount, texture) {
  // Add entries for all mipmaps with no images by default.
  for (size_t i = 0; i < kMipmapSlotCount; ++i)
    mipmaps_.Add(ImagePtr());
}

// See comments for these functions in Texture.
void TextureBase::Face::SetSubImage(size_t level, const math::Point2ui offset,
                                    const ImagePtr& image) {
  SetSubImage(level, math::Point3ui(offset[0], offset[1], 0), image);
}

// See comments for these functions in Texture.
void TextureBase::Face::SetSubImage(size_t level, const math::Point3ui offset,
                                    const ImagePtr& image) {
  sub_images_.push_back(SubImage(level, offset, image));
  // Flip the bit twice to ensure that the resource bit is set.
  sub_images_changed_.Set(false);
  sub_images_changed_.Set(true);
}

void TextureBase::Face::SetImage(size_t level, const ImagePtr& image_in,
                                 TextureBase* texture) {
  if (level < kMipmapSlotCount) {
    if (Image* img = mipmaps_.Get(level).Get())
      img->RemoveReceiver(texture);
    mipmaps_.Set(level, image_in);
    if (Image* img = image_in.Get()) {
      img->AddReceiver(texture);
      mipmaps_set_.set(level);
    } else {
      mipmaps_set_.reset(level);
    }
  }
}

//-----------------------------------------------------------------------------
//
// TextureBase
//
//-----------------------------------------------------------------------------

TextureBase::TextureBase(TextureType type)
    : sampler_(kSamplerChanged, SamplerPtr(), this),
      base_level_(kBaseLevelChanged, 0, this),
      max_level_(kMaxLevelChanged, 1000, this),
      swizzle_red_(kSwizzleRedChanged, kRed, kRed, kZero, this),
      swizzle_green_(kSwizzleGreenChanged, kGreen, kRed, kZero, this),
      swizzle_blue_(kSwizzleBlueChanged, kBlue, kRed, kZero, this),
      swizzle_alpha_(kSwizzleAlphaChanged, kAlpha, kRed, kZero, this),
      texture_type_(type),
      immutable_image_(kImmutableImageChanged, ImagePtr(), this),
      immutable_levels_(0),
      is_protected_(false),
      multisample_samples_(kMultisampleChanged, 0, this),
      multisample_fixed_sample_locations_(kMultisampleChanged, true, this) {}

TextureBase::~TextureBase() {
  if (Sampler* sampler = sampler_.Get().Get())
    sampler->RemoveReceiver(this);
}

void TextureBase::SetSampler(const SamplerPtr& sampler) {
  if (Sampler* old_sampler = sampler_.Get().Get())
    old_sampler->RemoveReceiver(this);
  sampler_.Set(sampler);
  if (Sampler* new_sampler = sampler_.Get().Get())
    new_sampler->AddReceiver(this);
}

bool TextureBase::SetImmutableImage(const ImagePtr& image, size_t levels) {
  if (image.Get()) {
    if (immutable_image_.Get().Get()) {
      LOG(ERROR) << "ION: SetImmutableImage() called on an already immutable "
                    "texture; SetImmutableImage() can only be called once.";
    } else if (levels == 0) {
      LOG(ERROR) << "ION: SetImmutableImage() called with levels == 0. A "
                    "texture must have at least one level (the 0th level).";
    } else {
      immutable_levels_ = levels;
      immutable_image_.Set(image);
      ClearNonImmutableImages();
      return true;
    }
  }
  return false;
}

bool TextureBase::SetProtectedImage(const ImagePtr& image, size_t levels) {
  // Only change state if there was a valid call to SetImmutableImage().
  if (SetImmutableImage(image, levels)) {
    is_protected_ = true;
    return true;
  }
  return false;
}

//-----------------------------------------------------------------------------
//
// Texture
//
//-----------------------------------------------------------------------------

Texture::Texture()
    : TextureBase(kTexture),
      face_(this, kSubImageChanged, kMipmapChanged) {
}

bool Texture::ExpectedDimensionsForMipmap(const uint32 mipmap_width,
                                          const uint32 mipmap_height,
                                          const uint32 mipmap_level,
                                          const uint32 base_width,
                                          const uint32 base_height,
                                          uint32* expected_width,
                                          uint32* expected_height) {
  // Establish defaults for |expected_width| and |expected_height| in the event
  // of a premature "return false" condition.
  *expected_width = 0;
  *expected_height = 0;

  // Test for power-of-two.  NPOT mipmapping not supported.
  if (mipmap_width != 1 && (mipmap_width & (mipmap_width - 1))) {
    LOG(ERROR) << "Mipmap width: " << mipmap_width << " is not a power of 2.";
    return false;
  }
  if (mipmap_height != 1 && (mipmap_height & (mipmap_height - 1))) {
    LOG(ERROR) << "Mipmap height: " << mipmap_height << " is not a power of 2.";
    return false;
  }

  // Verify that the mipmap width and height are proportional to the base
  // width and height provided neither dimension is 1.
  if (mipmap_width != 1 && mipmap_height != 1) {
    const float base_ratio =
        static_cast<float>(base_width) / static_cast<float>(base_height);
    const float mipmap_ratio =
        static_cast<float>(mipmap_width) / static_cast<float>(mipmap_height);
    if (base_ratio != mipmap_ratio) {
      LOG(ERROR) << "Bad aspect ratio for mipmap.";
      return false;
    }
  }

  // Check max level against mipmap_level.
  const uint32 max_dimension = std::max(base_width, base_height);
  const uint32 max_level = static_cast<uint32>(math::Log2(max_dimension));
  if (mipmap_level > max_level) {
    LOG(ERROR) << "Mipmap level is: " << mipmap_level <<
                  " but maximum level is: " << max_level << ".";
    return false;
  }

  *expected_width = base_width >> mipmap_level;
  *expected_height = base_height >> mipmap_level;

  // If |base_width| and |base_height| are not equal, the smaller dimension
  // may get all bits shifted away leaving a bad dimension of 0 for the smaller
  // mipmap levels.
  if (base_width != base_height) {
    *expected_width = std::max(*expected_width, 1U);
    *expected_height = std::max(*expected_height, 1U);
  }

  if (!((mipmap_width == *expected_width) &&
        (mipmap_height == *expected_height))) {
    LOG(ERROR) << "***ION: Mipmap level " << mipmap_level << " has incorrect"
               << " dimensions [" << mipmap_width << "x"
               << mipmap_height << "], expected [" << *expected_width << "x"
               << *expected_height << "].  Base dimensions: ("
               << base_width << ", " << base_height << ").  Ignoring.\n";
    return false;
  }

  return true;
}

Texture::~Texture() {
  for (size_t i = 0; i < kMipmapSlotCount; ++i) {
    if (Image* image = face_.GetImage(i).Get()) image->RemoveReceiver(this);
  }
}

void Texture::OnNotify(const base::Notifier* notifier) {
  if (GetResourceCount()) {
    if (notifier == GetSampler().Get()) {
      OnChanged(kSamplerChanged);
    } else {
      for (size_t i = 0; i < kMipmapSlotCount; ++i) {
        if (notifier == face_.GetImage(i).Get())
          OnChanged(static_cast<int>(kMipmapChanged + i));
      }
    }
  }
}

void Texture::ClearNonImmutableImages() {
  face_.ClearMipmapImages();
}

}  // namespace gfx

namespace base {

using gfx::Texture;

// Specialize for Texture::Swizzle.
template <> ION_API
const EnumHelper::EnumData<Texture::Swizzle> EnumHelper::GetEnumData() {
  static const GLenum kValues[] =
      { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA, GL_ONE, GL_ZERO };
  static const char* kStrings[] =
      { "Red", "Green", "Blue", "Alpha", "One", "Zero" };
  ION_STATIC_ASSERT(ABSL_ARRAYSIZE(kValues) == ABSL_ARRAYSIZE(kStrings),
                    "EnumHelper size mismatch");
  return EnumData<Texture::Swizzle>(
      base::IndexMap<Texture::Swizzle, GLenum>(kValues,
                                               ABSL_ARRAYSIZE(kValues)),
      kStrings);
}

}  // namespace base

}  // namespace ion
