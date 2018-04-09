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

#ifndef ION_DEMOS_UTILS_H_
#define ION_DEMOS_UTILS_H_

#include <cstring>
#include <string>

#include "ion/base/invalid.h"
#include "ion/base/logging.h"
#include "ion/base/zipassetmanager.h"
#include "ion/gfx/graphicsmanager.h"
#include "ion/gfx/node.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfx/uniform.h"
#include "ion/gfxutils/shadermanager.h"
#include "ion/gfxutils/shadersourcecomposer.h"
#include "ion/gfxutils/shapeutils.h"
#include "ion/image/conversionutils.h"
#include "ion/text/fontmanager.h"

namespace demoutils {

//-----------------------------------------------------------------------------
//
// Uniform utilities.
//
//-----------------------------------------------------------------------------

template <typename ValueType>
static inline size_t AddUniformToNode(
    const ion::gfx::ShaderInputRegistryPtr& registry,
    const std::string& name, const ValueType& value,
    const ion::gfx::NodePtr& node) {
  const ion::gfx::Uniform u = registry->Create<ion::gfx::Uniform>(name, value);
  if (u.IsValid()) {
    return node->AddUniform(u);
  } else {
    std::cerr << "Error adding uniform '" << name << "' to node\n";
    return ion::base::kInvalidIndex;
  }
}

template <typename ValueType>
static bool SetUniformInNode(
    size_t index, const ValueType& value, const ion::gfx::NodePtr& node) {
  if (!node->SetUniformValue<ValueType>(index, value)) {
    std::cerr << "Error setting uniform with index '" << index << "' in node\n";
    return false;
  }
  return true;
}

//-----------------------------------------------------------------------------
//
// Loading resources from assets.
//
//-----------------------------------------------------------------------------

// Loads a model from a zipped asset. The passed ExternalShapeSpec specifies
// additional information, such as the transforms to apply to the vertex
// data after loading. The radius of the model, defined as half of the diameter
// of the model's bounding box, is stored in the "radius" pointer.
ion::gfx::ShapePtr LoadShapeAsset(
    const std::string& asset_name,
    const ion::gfxutils::ExternalShapeSpec &spec,
    float* radius = nullptr);

// Loads a texture from a zipped asset. The asset with the specified name
// must contain image data that is understood by the function
// ion::image::ConvertFromExternalImageData().
ion::gfx::TexturePtr LoadTextureAsset(const std::string& asset_name);

// Loads a cube map texture from six zipped image assets. The asset names
// are constructed by inserting the following strings between the specified
// prefix and suffix:
// - "_left" for negative X side
// - "_bottom" for negative Y side
// - "_back" for negative Z side
// - "_right" for positive X side
// - "_top" for positive Y side
// - "_front" for positive Z side
// All assets must contain image data that is understood by the function
// ion::image::ConvertFromExternalImageData().
ion::gfx::CubeMapTexturePtr LoadCubeMapAsset(const std::string& prefix,
                                             const std::string& suffix);

// Loads a complete shader program from assets. The vertex and fragment shader
// sources are loaded from asset names constructed by appending ".vp" of ".fp"
// to the specified prefix, respectively.
ion::gfx::ShaderProgramPtr LoadShaderProgramAsset(
    const ion::gfxutils::ShaderManagerPtr& shader_manager,
    const std::string& label,
    const ion::gfx::ShaderInputRegistryPtr& input_registry,
    const std::string& asset_prefix);

// Loads a complete shader program from assets. This version allows one to
// explicitly specify the vertex and fragment shader source asset names.
ion::gfx::ShaderProgramPtr LoadShaderProgramAsset(
    const ion::gfxutils::ShaderManagerPtr& shader_manager,
    const std::string& label,
    const ion::gfx::ShaderInputRegistryPtr& input_registry,
    const std::string& vertex_shader_asset,
    const std::string& fragment_shader_asset);

//-----------------------------------------------------------------------------
//
// Font management.
//
//-----------------------------------------------------------------------------

// Uses the given FontManager to initialize a font. The name, size, and SDF
// padding values are passed to the FontManager to specify the Font.  If any
// errors occur, this logs a message and returns a NULL pointer.
//
// On Mac and iOS, the font name is passed to CoreText, which will use the
// system font with that name if it exists, otherwise will fall back to the
// default system font (Helvetica Neue). On other platforms, the font is loaded
// from data in a file managed by the ZipAssetManager. The file name is created
// by appending ".ttf" to the name of the font. If the font has already been
// initialized by the FontManager, this just returns it.
static inline ion::text::FontPtr InitFont(
    const ion::text::FontManagerPtr& font_manager,
    const std::string& font_name, size_t size_in_pixels, size_t sdf_padding) {
  // See if the font is already initialized.
  DCHECK(font_manager.Get());
  ion::text::FontPtr font =
      font_manager->FindFont(font_name, size_in_pixels, sdf_padding);
  if (!font.Get()) {
    // Read the font data.
    const std::string& data =
        ion::base::ZipAssetManager::GetFileData(font_name + ".ttf");
    if (ion::base::IsInvalidReference(data) || data.empty()) {
      LOG(ERROR) << "Unable to read data for font \"" << font_name << "\"";
    } else {
      font = font_manager->AddFont(
          font_name, size_in_pixels, sdf_padding, &data[0], data.size());
    }
  }
  return font;
}

//-----------------------------------------------------------------------------
//
// GraphicsManager queries.
//
//-----------------------------------------------------------------------------

// Returns true if the graphics renderer supports the GL_RGB16F format for
// framebuffers.
static inline bool RendererSupportsRgb16fHalf(
    const ion::gfx::GraphicsManagerPtr& gm) {
  // Only Mesa renderers cannot deal properly with this format.
  const char* s = reinterpret_cast<const char*>(gm->GetString(GL_RENDERER));
  return !strstr(s, "Mesa");
}

}  // namespace demoutils

#endif  // ION_DEMOS_UTILS_H_
