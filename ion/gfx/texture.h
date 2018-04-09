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

#ifndef ION_GFX_TEXTURE_H_
#define ION_GFX_TEXTURE_H_

#include <bitset>

#include "base/integral_types.h"
#include "base/macros.h"
#include "ion/base/invalid.h"
#include "ion/base/referent.h"
#include "ion/base/stlalloc/allocvector.h"
#include "ion/gfx/image.h"
#include "ion/gfx/resourceholder.h"
#include "ion/gfx/sampler.h"
#include "ion/math/vector.h"

namespace ion {
namespace gfx {

// 16 mipmap levels supports a max texture size of 65536^2. As of early 2013 the
// newest hardware supports 16384^2 textures.
static const size_t kMipmapSlotCount = 16;

// This is an internal base class for all texture types.
class ION_API TextureBase : public ResourceHolder {
 public:
  // Wrapper around a sub-image, which is defined as an image, the xy offset
  // of where it should be placed in the texture, and a mipmap level. Subclasses
  // should use this to declare texture image data.
  struct SubImage {
    SubImage() : level(0U) {}

    SubImage(size_t level_in, const math::Point2ui& offset_in,
             const ImagePtr& image_in)
        : level(level_in),
          offset(offset_in[0], offset_in[1], 0U),
          image(image_in) {}
    SubImage(size_t level_in, const math::Point3ui& offset_in,
             const ImagePtr& image_in)
    : level(level_in), offset(offset_in), image(image_in) {}

    size_t level;
    math::Point3ui offset;
    ImagePtr image;
  };

  // Changes that affect this resource. Some changes are forwarded from Sampler.
  enum Changes {
    kBaseLevelChanged = kNumBaseChanges,
    kContentsImplicitlyChanged,
    kImmutableImageChanged,
    kMaxLevelChanged,
    kMultisampleChanged,
    kSamplerChanged,
    kSwizzleRedChanged,
    kSwizzleGreenChanged,
    kSwizzleBlueChanged,
    kSwizzleAlphaChanged,
    kNumChanges
  };

  enum Swizzle {
    kRed,
    kGreen,
    kBlue,
    kAlpha,
    kOne,
    kZero
  };

  enum TextureType {
    kCubeMapTexture,
    kTexture
  };

  // Sets/returns the Sampler to use for this. This is NULL by default.
  void SetSampler(const SamplerPtr& sampler);
  const SamplerPtr& GetSampler() const { return sampler_.Get(); }

  // Sets this texture to be fully allocated and made immutable by OpenGL, in
  // the sense that it cannot change size or its number of mipmap levels. The
  // actual image data of the texture is not immutable, and may be changed with
  // SetSubImage() (see below). This is equivalent to using a GL TexStorage()
  // function. The passed image specifies the dimensions of the base texture
  // face and the format to use (any image data stored in image is ignored),
  // while levels indicates the number of mipmap levels to allocate. Returns
  // whether the texture was successfully set as immutable.
  //
  // Note that after calling this function, calling SetImage() on any of this
  // texture's faces is an error. All face updates must be through
  // SetSubImage(). Calling SetImmutable() repeatedly is also an error; it may
  // only be called once.
  bool SetImmutableImage(const ImagePtr& image, size_t levels);
  const ImagePtr& GetImmutableImage() const { return immutable_image_.Get(); }
  // Returns the number of immutable mipmap levels used by this texture. Returns
  // 0 if there is no immutable image.
  size_t GetImmutableLevels() const { return immutable_levels_; }

  // Similar to SetImmutableImage(), but also marks the texture as protected,
  // meaning that the memory backing the texture should be allocated using
  // protected/trusted memory. Returns whether the texture was successfully set
  // as immutable and protected.
  bool SetProtectedImage(const ImagePtr& image, size_t levels);
  // Returns whether the texture contains protected content and is backed by
  // protected memory.
  bool IsProtected() const { return is_protected_; }

  // Sets/returns the index of the lowest mipmap level to use when rendering.
  // The default value is 0.
  void SetBaseLevel(int level) { base_level_.Set(level); }
  int GetBaseLevel() const { return base_level_.Get(); }

  // Sets/returns the index of the highest mipmap level to use when rendering.
  // The default value is 1000.
  void SetMaxLevel(int level) { max_level_.Set(level); }
  int GetMaxLevel() const { return max_level_.Get(); }

  // Sets/returns the color component to use when the color channels of a
  // texture are used in a shader. These all default to themselves (e.g., red
  // swizzle is red).
  void SetSwizzleRed(Swizzle r) { swizzle_red_.Set(r); }
  Swizzle GetSwizzleRed() const { return swizzle_red_.Get(); }
  void SetSwizzleGreen(Swizzle g) { swizzle_green_.Set(g); }
  Swizzle GetSwizzleGreen() const { return swizzle_green_.Get(); }
  void SetSwizzleBlue(Swizzle b) { swizzle_blue_.Set(b); }
  Swizzle GetSwizzleBlue() const { return swizzle_blue_.Get(); }
  void SetSwizzleAlpha(Swizzle a) { swizzle_alpha_.Set(a); }
  Swizzle GetSwizzleAlpha() const { return swizzle_alpha_.Get(); }
  // Sets all swizzles at once.
  void SetSwizzles(Swizzle r, Swizzle g, Swizzle b, Swizzle a) {
    swizzle_red_.Set(r);
    swizzle_green_.Set(g);
    swizzle_blue_.Set(b);
    swizzle_alpha_.Set(a);
  }

  // Returns what type of texture that this is.
  TextureType GetTextureType() const { return texture_type_; }

  // Enables/disables and sets parameters for texture multisampling. If
  // multisampling is enabled, then both mipmapping and sub images are disabled.
  void SetMultisampling(int samples, bool fixed_sample_locations) {
    if (samples < 0) {
      LOG(WARNING) << "Ignoring bad number of samples: " << samples;
      return;
    }
    multisample_samples_.Set(samples);
    multisample_fixed_sample_locations_.Set(fixed_sample_locations);
  }
  int GetMultisampleSamples() const {
    return multisample_samples_.Get();
  }
  bool IsMultisampleFixedSampleLocations() const {
    return multisample_fixed_sample_locations_.Get();
  }

 protected:
  // Internal class that wraps texture data: a single image or a stack of
  // mipmaps, and any sub- or layered data.
  class Face {
   public:
    Face(TextureBase* texture, int sub_image_changed_bit,
         int mipmaps_changed_start_bit);

    // Sets the area of the texture starting from the passed xy offset and
    // having the dimensions of the passed image. If the Face is backed by a 3D
    // image, the z offset is set to 0.
    void SetSubImage(size_t level, const math::Point2ui offset,
                     const ImagePtr& image);

    // Sets the area of the texture starting from the passed xyz offset and
    // having the dimensions of the passed image.
    void SetSubImage(size_t level, const math::Point3ui offset,
                     const ImagePtr& image);

    // Returns the vector of sub-images; it may be empty.
    const base::AllocVector<SubImage>& GetSubImages() const {
      return sub_images_;
    }

    // Clears the vector of sub-images.
    void ClearSubImages() const {
      sub_images_.clear();
    }

    // Returns true if there is a mipmap image at the specified level stored
    // here.
    bool HasImage(size_t level) const {
      return level < mipmaps_set_.size() && mipmaps_set_.test(level);
    }

    // Returns the image at the specified mipmap level, or NULL if this is not
    // mipmapped.
    const ImagePtr GetImage(size_t level) const {
      if (HasImage(level))
        return mipmaps_.Get(level);
      else
        return ImagePtr();
    }

    // Gets the number of mipmap images that have been set.
    size_t GetImageCount() const {
      return mipmaps_set_.count();
    }

    // Sets the image to use for the texture at the specified mipmap level. If
    // the full image pyramid is not set and any mipmap other than the 0th level
    // is set, then the missing images will be autogenerated by OpenGL. Passing
    // a NULL ImagePtr will clear the client- supplied image for that level.
    //
    // Mipmap images must all share the same format, and have dimensions
    // consistent with a mipmap hierarchy. If these conditions are not met, an
    // error will be generated during rendering.
    void SetImage(size_t level, const ImagePtr& image_in, TextureBase* texture);

    // Removes all mipmap images from this Face.
    void ClearMipmapImages() {
      mipmaps_.Clear();
    }

   private:
    // Whether any sub-images have been added. This is a Field rather than the
    // below vector so that sub-images can be cleared without triggering a
    // change bit.
    Field<bool> sub_images_changed_;
    // This is mutable so that it can be cleared in const instances.
    mutable base::AllocVector<TextureBase::SubImage> sub_images_;
    VectorField<ImagePtr> mipmaps_;
    std::bitset<kMipmapSlotCount> mipmaps_set_;
  };

  // The constructor is protected since this is a base class.
  explicit TextureBase(TextureType type);

  // The destructor is protected because all base::Referent classes must have
  // protected or private destructors.
  ~TextureBase() override;

  // Clears any non-immutable mipmap images.
  virtual void ClearNonImmutableImages() = 0;

 private:
  Field<SamplerPtr> sampler_;
  Field<int> base_level_;
  Field<int> max_level_;
  RangedField<Swizzle> swizzle_red_;
  RangedField<Swizzle> swizzle_green_;
  RangedField<Swizzle> swizzle_blue_;
  RangedField<Swizzle> swizzle_alpha_;

  // The type of this (must be set by subclasses).
  TextureType texture_type_;

  // The immutable image.
  Field<ImagePtr> immutable_image_;
  // The number of immutable levels.
  size_t immutable_levels_;

  // Whether this texture is a protected immutable image.
  bool is_protected_;

  // Multisampling.
  Field<int> multisample_samples_;
  Field<bool> multisample_fixed_sample_locations_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(TextureBase);
};

// A Texture object represents the image data and mipmaps associated with a
// single texture.
class ION_API Texture : public TextureBase {
 public:
  // Changes that affect this resource.
  enum Changes {
    kSubImageChanged = TextureBase::kNumChanges,
    // kMipmapChanged must be last since it is a range of slots.
    kMipmapChanged,
    kNumChanges = kMipmapChanged + kMipmapSlotCount
  };

  Texture();

  // See comments in TextureBase::Face.
  void SetImage(size_t level, const ImagePtr& image) {
    if (GetImmutableImage().Get())
      LOG(ERROR) << "ION: SetImage() called on immutable texture \""
                 << GetLabel()
                 << "\".  Use SetSubImage() to update an immutable texture.";
    else
      face_.SetImage(level, image, this);
  }
  bool HasImage(size_t level) const {
    return level < GetImmutableLevels() || face_.HasImage(level);
  }
  const ImagePtr GetImage(size_t level) const {
    if (level < GetImmutableLevels()) {
      return GetImmutableImage();
    }
    return face_.GetImage(level);
  }
  size_t GetImageCount() const {
    return GetImmutableLevels() ? GetImmutableLevels() : face_.GetImageCount();
  }
  void SetSubImage(size_t level, const math::Point2ui offset,
                   const ImagePtr& image) {
    SetSubImage(level, math::Point3ui(offset[0], offset[1], 0), image);
  }
  void SetSubImage(size_t level, const math::Point3ui offset,
                   const ImagePtr& image) {
    face_.SetSubImage(level, offset, image);
  }
  const base::AllocVector<SubImage>& GetSubImages() const {
    return face_.GetSubImages();
  }
  void ClearSubImages() const {
    face_.ClearSubImages();
  }

  // Tests mipmap dimensions to see that they are proportional and in range
  // with respect to |base_width| and |base_height|.
  static bool ExpectedDimensionsForMipmap(const uint32 mipmap_width,
                                          const uint32 mipmap_height,
                                          const uint32 mipmap_level,
                                          const uint32 base_width,
                                          const uint32 base_height,
                                          uint32* expected_width,
                                          uint32* expected_height);

 protected:
  // The destructor is protected because all base::Referent classes must have
  // protected or private destructors.
  ~Texture() override;

  void ClearNonImmutableImages() override;

 private:
  // Called when an Image or DataContainer that this depends on changes.
  void OnNotify(const base::Notifier* notifier) override;

  // The single face of the Texture.
  Face face_;
};

// Convenience typedef for shared pointer to a Texture.
using TexturePtr = base::SharedPtr<Texture>;

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_TEXTURE_H_
