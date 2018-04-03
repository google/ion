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

#include "ion/gfx/cubemaptexture.h"

#include "ion/base/enumhelper.h"
#include "ion/base/static_assert.h"
#include "ion/portgfx/glheaders.h"
#include "absl/base/macros.h"

namespace ion {
namespace gfx {

CubeMapTexture::CubeMapTexture()
    : TextureBase(kCubeMapTexture),
      // This is because we cannot yet use C++11 initializer lists. Instead of
      // using an array, we have an extra pointer where each index in the
      // pointer is one of the faces.
      faces_(&facenx_),
      facenx_(this, kNegativeXSubImageChanged, kNegativeXMipmapChanged),
      faceny_(this, kNegativeYSubImageChanged, kNegativeYMipmapChanged),
      facenz_(this, kNegativeZSubImageChanged, kNegativeZMipmapChanged),
      facepx_(this, kPositiveXSubImageChanged, kPositiveXMipmapChanged),
      facepy_(this, kPositiveYSubImageChanged, kPositiveYMipmapChanged),
      facepz_(this, kPositiveZSubImageChanged, kPositiveZMipmapChanged) {
  DCHECK_EQ(&facenx_, &faces_[0]);
  DCHECK_EQ(&faceny_, &faces_[1]);
  DCHECK_EQ(&facenz_, &faces_[2]);
  DCHECK_EQ(&facepx_, &faces_[3]);
  DCHECK_EQ(&facepy_, &faces_[4]);
  DCHECK_EQ(&facepz_, &faces_[5]);
}

CubeMapTexture::~CubeMapTexture() {
  for (size_t j = 0; j < 6; ++j) {
    const Face& face = faces_[j];
    for (size_t i = 0; i < kMipmapSlotCount; ++i) {
      if (Image* image = face.GetImage(i).Get())
        image->RemoveReceiver(this);
    }
  }
}

void CubeMapTexture::OnNotify(const base::Notifier* notifier) {
  if (GetResourceCount()) {
    if (notifier == GetSampler().Get()) {
      OnChanged(kSamplerChanged);
    } else {
      for (size_t j = 0; j < 6; ++j) {
        const Face& face = faces_[j];
        int mipmap_changed_bit = static_cast<int>(
            kNegativeXMipmapChanged + j * kMipmapSlotCount);
        for (size_t i = 0; i < kMipmapSlotCount; ++i) {
          if (notifier == face.GetImage(i).Get())
            OnChanged(static_cast<int>(mipmap_changed_bit + i));
        }
      }
    }
  }
}

void CubeMapTexture::ClearNonImmutableImages() {
  for (size_t j = 0; j < 6; ++j) {
    Face& face = faces_[j];
    face.ClearMipmapImages();
  }
}

}  // namespace gfx

namespace base {

using gfx::CubeMapTexture;

// Specialize for CubeMapTexture::Face.
template <> ION_API
const EnumHelper::EnumData<CubeMapTexture::CubeFace> EnumHelper::GetEnumData() {
  static const GLenum kValues[] = {
    GL_TEXTURE_CUBE_MAP_NEGATIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
    GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, GL_TEXTURE_CUBE_MAP_POSITIVE_X,
    GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
  };
  static const char* kStrings[] = {
    "Negative X", "Negative Y", "Negative Z",
    "Positive X", "Positive Y", "Positive Z"
  };
  ION_STATIC_ASSERT(ABSL_ARRAYSIZE(kValues) == ABSL_ARRAYSIZE(kStrings),
                    "EnumHelper size mismatch");
  return EnumData<CubeMapTexture::CubeFace>(
      base::IndexMap<CubeMapTexture::CubeFace, GLenum>(kValues,
                                                       ABSL_ARRAYSIZE(kValues)),
      kStrings);
}

}  // namespace base
}  // namespace ion
