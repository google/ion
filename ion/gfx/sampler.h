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

#ifndef ION_GFX_SAMPLER_H_
#define ION_GFX_SAMPLER_H_

#include "ion/base/referent.h"
#include "ion/gfx/resourceholder.h"

namespace ion {
namespace gfx {

// A Sampler object represents texture parameters that control how texture data
// is accessed in shaders.
class ION_API Sampler : public ResourceHolder {
 public:
  // Changes that affect this resource.
  enum Changes {
    kAutoMipmappingChanged = kNumBaseChanges,
    kCompareFunctionChanged,
    kCompareModeChanged,
    kMagFilterChanged,
    kMaxAnisotropyChanged,
    kMaxLodChanged,
    kMinFilterChanged,
    kMinLodChanged,
    kWrapRChanged,
    kWrapSChanged,
    kWrapTChanged,
    kNumChanges
  };

  // Texture comparison functions for depth textures.
  enum CompareFunction {
    kAlways,
    kEqual,
    kGreater,
    kGreaterOrEqual,
    kLess,
    kLessOrEqual,
    kNever,
    kNotEqual
  };

  // Texture comparison modes for depth textures.
  enum CompareMode {
    kCompareToTexture,
    kNone
  };

  // Texture filter modes.
  enum FilterMode {
    kNearest,
    kLinear,
    // These are only usable with the minification filter.
    kNearestMipmapNearest,
    kNearestMipmapLinear,
    kLinearMipmapNearest,
    kLinearMipmapLinear
  };

  // Texture filter modes.
  enum WrapMode {
    kClampToEdge,
    kRepeat,
    kMirroredRepeat
  };

  Sampler();

  // Sets whether OpenGL should automatically generate mipmaps for this Sampler.
  // Any image set with SetMipmapImage() will override the automatically
  // generated images. Calling this function is not necessary if setting a
  // partial image pyramid.
  void SetAutogenerateMipmapsEnabled(bool enable) {
    auto_mipmapping_enabled_.Set(enable);
  }
  // Gets whether this Sampler should use mipmapping. The default is false.
  bool IsAutogenerateMipmapsEnabled() const {
    return auto_mipmapping_enabled_.Get();
  }

  // Sets/returns the comparison mode to use for the texture using this sampler.
  // The default mode is kNone.
  void SetCompareMode(CompareMode mode) {
    compare_mode_.Set(mode);
  }
  CompareMode GetCompareMode() const { return compare_mode_.Get(); }

  // Sets/returns the comparison function to use when texture comparison is
  // enabled, e.g., when this is used with a depth texture. The default
  // function is kLess.
  void SetCompareFunction(CompareFunction func) {
    compare_function_.Set(func);
  }
  CompareFunction GetCompareFunction() const { return compare_function_.Get(); }

  // Sets the maximum degree of anisotropy used when filtering textures.
  void SetMaxAnisotropy(float aniso) {
    max_anisotropy_.Set(aniso);
  }
  // Gets the maximum anisotropy parameter. The default is 1.
  float GetMaxAnisotropy() const { return max_anisotropy_.Get(); }

  // Sets the minification mode.
  void SetMinFilter(const FilterMode& mode) {
    min_filter_.Set(mode);
  }
  // Gets the minification mode. The default is kNearest.
  FilterMode GetMinFilter() const { return min_filter_.Get(); }

  // Sets the magnification mode.
  void SetMagFilter(const FilterMode& mode) {
    mag_filter_.Set(mode);
  }
  // Gets the magnification mode. The default is kNearest.
  FilterMode GetMagFilter() const { return mag_filter_.Get(); }

  // Sets the minimum level of detail parameter, which limits the
  // selection of the highest resolution (lowest level) mipmap.
  void SetMinLod(float lod) {
    min_lod_.Set(lod);
  }
  // Gets the minimum level of detail parameter. The default is -1000.
  float GetMinLod() const { return min_lod_.Get(); }

  // Sets the maximum level of detail parameter, which limits the
  // selection of the lowest resolution (highest level) mipmap.
  void SetMaxLod(float lod) {
    max_lod_.Set(lod);
  }
  // Gets the maximum level of detail parameter. The default is 1000.
  float GetMaxLod() const { return max_lod_.Get(); }

  // Sets the wrap along the r-coordinate (useful only for 3D textures).
  void SetWrapR(const WrapMode& mode) {
    wrap_r_.Set(mode);
  }
  // Gets the wrap along the r-coordinate. The default is kRepeat.
  WrapMode GetWrapR() const { return wrap_r_.Get(); }

  // Sets the wrap along the s-coordinate.
  void SetWrapS(const WrapMode& mode) {
    wrap_s_.Set(mode);
  }
  // Gets the wrap along the s-coordinate. The default is kRepeat.
  WrapMode GetWrapS() const { return wrap_s_.Get(); }

  // Sets the wrap along the t-coordinate.
  void SetWrapT(const WrapMode& mode) {
    wrap_t_.Set(mode);
  }
  // Gets the wrap along the t-coordinate. The default is kRepeat.
  WrapMode GetWrapT() const { return wrap_t_.Get(); }

 protected:
  // The destructor is protected because all base::Referent classes must have
  // protected or private destructors.
  ~Sampler() override;

 private:
  Field<bool> auto_mipmapping_enabled_;
  RangedField<float> max_anisotropy_;
  Field<float> min_lod_;
  Field<float> max_lod_;
  RangedField<CompareFunction> compare_function_;
  RangedField<CompareMode> compare_mode_;
  RangedField<FilterMode> min_filter_;
  RangedField<FilterMode> mag_filter_;
  RangedField<WrapMode> wrap_r_;
  RangedField<WrapMode> wrap_s_;
  RangedField<WrapMode> wrap_t_;
};

// Convenience typedef for shared pointer to a Sampler.
using SamplerPtr = base::SharedPtr<Sampler>;

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_SAMPLER_H_
