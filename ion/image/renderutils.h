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

#ifndef ION_IMAGE_RENDERUTILS_H_
#define ION_IMAGE_RENDERUTILS_H_

#include "base/integral_types.h"
#include "ion/base/allocator.h"
#include "ion/gfx/cubemaptexture.h"
#include "ion/gfx/image.h"
#include "ion/gfx/renderer.h"
#include "ion/gfx/texture.h"

namespace ion {
namespace image {

// Creates and returns an Image representing a Texture. This uses the Renderer
// to render the Texture into a new Image that is created using allocator. The
// new Image will have dimensions width x height, which do not have to be the
// same as the dimensions of the texture. The Renderer must be the same one
// that was used previously to render a shape using the Texture.
ION_API const gfx::ImagePtr RenderTextureImage(
    const gfx::TexturePtr& texture, uint32 width, uint32 height,
    const gfx::RendererPtr& renderer, const base::AllocatorPtr& allocator);

// This is similar to RenderTextureImage(), but instead operates on one face of
// a CubeMapTexture.
ION_API const gfx::ImagePtr RenderCubeMapTextureFaceImage(
    const gfx::CubeMapTexturePtr& cubemap, gfx::CubeMapTexture::CubeFace face,
    uint32 width, uint32 height,
    const gfx::RendererPtr& renderer, const base::AllocatorPtr& allocator);

}  // namespace image
}  // namespace ion

#endif  // ION_IMAGE_RENDERUTILS_H_
