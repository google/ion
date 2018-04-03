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

#ifndef ION_GFX_CUBEMAPTEXTURE_H_
#define ION_GFX_CUBEMAPTEXTURE_H_

#include "ion/gfx/texture.h"

namespace ion {
namespace gfx {

// A CubeMapTexture object represents the image data and mipmaps associated with
// the six faces of a cube map.
class ION_API CubeMapTexture : public TextureBase {
 public:
  // Changes that affect this resource.
  enum Changes {
    kNegativeXSubImageChanged = TextureBase::kNumChanges,
    kNegativeYSubImageChanged,
    kNegativeZSubImageChanged,
    kPositiveXSubImageChanged,
    kPositiveYSubImageChanged,
    kPositiveZSubImageChanged,
    kNegativeXMipmapChanged,
    // Each set of mipmaps has an entry per level.
    kNegativeYMipmapChanged = kNegativeXMipmapChanged + kMipmapSlotCount,
    kNegativeZMipmapChanged = kNegativeYMipmapChanged + kMipmapSlotCount,
    kPositiveXMipmapChanged = kNegativeZMipmapChanged + kMipmapSlotCount,
    kPositiveYMipmapChanged = kPositiveXMipmapChanged + kMipmapSlotCount,
    kPositiveZMipmapChanged = kPositiveYMipmapChanged + kMipmapSlotCount,
    kNumChanges = kPositiveZMipmapChanged + kMipmapSlotCount
  };

  // The names of faces of the cube map.
  enum CubeFace {
    kNegativeX,
    kNegativeY,
    kNegativeZ,
    kPositiveX,
    kPositiveY,
    kPositiveZ,
  };

  CubeMapTexture();

  // See comments in TextureBase::Face.
  void SetImage(CubeFace face, size_t level, const ImagePtr& image) {
    if (GetImmutableImage().Get())
      LOG(ERROR) << "ION: SetImage() called on immutable texture \""
                 << GetLabel()
                 << "\".  Use SetSubImage() to update an immutable texture.";
    else
      faces_[face].SetImage(level, image, this);
  }
  bool HasImage(CubeFace face, size_t level) const {
    return level < GetImmutableLevels() || faces_[face].HasImage(level);
  }
  const ImagePtr GetImage(CubeFace face, size_t level) const {
    if (level < GetImmutableLevels()) {
      return GetImmutableImage();
    }
    return faces_[face].GetImage(level);
  }
  size_t GetImageCount(CubeFace face) const {
    return GetImmutableLevels() ? GetImmutableLevels()
                                : faces_[face].GetImageCount();
  }
  void SetSubImage(CubeFace face, size_t level, const math::Point2ui offset,
                   const ImagePtr& image) {
    faces_[face].SetSubImage(level, offset, image);
  }
  void SetSubImage(CubeFace face, size_t level, const math::Point3ui offset,
                   const ImagePtr& image) {
    faces_[face].SetSubImage(level, offset, image);
  }
  const base::AllocVector<SubImage>& GetSubImages(CubeFace face) const {
    return faces_[face].GetSubImages();
  }
  void ClearSubImages(CubeFace face) const {
    faces_[face].ClearSubImages();
  }

 protected:
  // The destructor is protected because all base::Referent classes must have
  // protected or private destructors.
  ~CubeMapTexture() override;

  void ClearNonImmutableImages() override;

 private:
  // Called when a Texture that this depends on changes.
  void OnNotify(const base::Notifier* notifier) override;

  // The cube map faces.
  Face* faces_;
  // The actual faces. We explicitly declare them here since we cannot array
  // initialize them directly without C++11.
  Face facenx_;
  Face faceny_;
  Face facenz_;
  Face facepx_;
  Face facepy_;
  Face facepz_;
};

// Convenience typedef for shared pointer to a CubeMapTexture.
using CubeMapTexturePtr = base::SharedPtr<CubeMapTexture>;

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_CUBEMAPTEXTURE_H_
