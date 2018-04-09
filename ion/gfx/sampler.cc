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

#include "ion/gfx/sampler.h"

#include "ion/base/enumhelper.h"
#include "ion/base/static_assert.h"
#include "ion/portgfx/glheaders.h"
#include "absl/base/macros.h"

namespace ion {
namespace gfx {

Sampler::Sampler()
    : auto_mipmapping_enabled_(kAutoMipmappingChanged, false, this),
      max_anisotropy_(kMaxAnisotropyChanged, 1.f, 1.f, 32.f, this),
      min_lod_(kMinLodChanged, -1000.f, this),
      max_lod_(kMaxLodChanged, 1000.f, this),
      compare_function_(
          kCompareFunctionChanged, kLess, kAlways, kNotEqual, this),
      compare_mode_(kCompareModeChanged, kNone, kCompareToTexture, kNone, this),
      min_filter_(kMinFilterChanged, kNearest, kNearest, kLinearMipmapLinear,
                  this),
      mag_filter_(kMagFilterChanged, kNearest, kNearest, kLinear, this),
      wrap_r_(kWrapRChanged, kRepeat, kClampToEdge, kMirroredRepeat, this),
      wrap_s_(kWrapSChanged, kRepeat, kClampToEdge, kMirroredRepeat, this),
      wrap_t_(kWrapTChanged, kRepeat, kClampToEdge, kMirroredRepeat, this) {}

Sampler::~Sampler() {
}

}  // namespace gfx

namespace base {

using gfx::Sampler;

// Specialize for Sampler::CompareFunction.
template <> ION_API const EnumHelper::EnumData<Sampler::CompareFunction>
EnumHelper::GetEnumData() {
  static const GLenum kValues[] = {
    GL_ALWAYS, GL_EQUAL, GL_GREATER, GL_GEQUAL, GL_LESS, GL_LEQUAL, GL_NEVER,
    GL_NOTEQUAL
  };
  static const char* kStrings[] = {
    "Always", "Equal", "Greater", "GreaterOrEqual", "Less", "LessOrEqual",
    "Never", "NotEqual"
  };
  ION_STATIC_ASSERT(ABSL_ARRAYSIZE(kValues) == ABSL_ARRAYSIZE(kStrings),
                    "EnumHelper size mismatch");
  return EnumData<Sampler::CompareFunction>(
      base::IndexMap<Sampler::CompareFunction, GLenum>(kValues,
                                                       ABSL_ARRAYSIZE(kValues)),
      kStrings);
}

// Specialize for Sampler::CompareMode.
template <> ION_API const EnumHelper::EnumData<Sampler::CompareMode>
EnumHelper::GetEnumData() {
  static const GLenum kValues[] = { GL_COMPARE_REF_TO_TEXTURE, GL_NONE };
  static const char* kStrings[] = { "CompareToTexture", "None" };
  ION_STATIC_ASSERT(ABSL_ARRAYSIZE(kValues) == ABSL_ARRAYSIZE(kStrings),
                    "EnumHelper size mismatch");
  return EnumData<Sampler::CompareMode>(
      base::IndexMap<Sampler::CompareMode, GLenum>(kValues,
                                                   ABSL_ARRAYSIZE(kValues)),
      kStrings);
}

// Specialize for Sampler::FilterMode.
template <> ION_API const EnumHelper::EnumData<Sampler::FilterMode>
EnumHelper::GetEnumData() {
  static const GLenum kValues[] = {
    GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_LINEAR,
    GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_LINEAR
  };
  static const char* kStrings[] = {
    "Nearest", "Linear", "NearestMipmapNearest", "NearestMipmapLinear",
    "LinearMipmapNearest", "LinearMipmapLinear"
  };
  ION_STATIC_ASSERT(ABSL_ARRAYSIZE(kValues) == ABSL_ARRAYSIZE(kStrings),
                    "EnumHelper size mismatch");
  return EnumData<Sampler::FilterMode>(
      base::IndexMap<Sampler::FilterMode, GLenum>(kValues,
                                                  ABSL_ARRAYSIZE(kValues)),
      kStrings);
}

// Specialize for Sampler::WrapMode.
template <> ION_API const EnumHelper::EnumData<Sampler::WrapMode>
EnumHelper::GetEnumData() {
  static const GLenum kValues[] = {
    GL_CLAMP_TO_EDGE, GL_REPEAT, GL_MIRRORED_REPEAT
  };
  static const char* kStrings[] = { "ClampToEdge", "Repeat", "MirroredRepeat" };
  ION_STATIC_ASSERT(ABSL_ARRAYSIZE(kValues) == ABSL_ARRAYSIZE(kStrings),
                    "EnumHelper size mismatch");
  return EnumData<Sampler::WrapMode>(
      base::IndexMap<Sampler::WrapMode, GLenum>(kValues,
                                                ABSL_ARRAYSIZE(kValues)),
      kStrings);
}

}  // namespace base

}  // namespace ion
