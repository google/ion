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

#include "ion/demos/utils.h"

#include "ion/base/logging.h"
#include "ion/gfxutils/shadermanager.h"
#include "ion/gfxutils/shadersourcecomposer.h"
#include "ion/gfxutils/shapeutils.h"
#include "ion/math/vector.h"

using ion::math::Point3f;

namespace {

ion::gfx::ImagePtr LoadImageAsset(
    const std::string& asset_name,
    const ion::base::AllocatorPtr& allocator,
    bool flip_vertically) {
  const std::string& image_string =
      ion::base::ZipAssetManager::GetFileData(asset_name);
  DCHECK(!ion::base::IsInvalidReference(image_string));

  ion::gfx::ImagePtr image = ion::image::ConvertFromExternalImageData(
      image_string.data(), image_string.size(), flip_vertically, false,
      allocator);
  return image;
}

}  // end anonymous namespace

namespace demoutils {

ion::gfx::ShapePtr LoadShapeAsset(
    const std::string& asset_name,
    const ion::gfxutils::ExternalShapeSpec &spec,
    float* radius) {
  LOG(INFO) << "Loading " << asset_name;

  ion::gfxutils::ExternalShapeSpec real_spec(spec);
  if (real_spec.format == ion::gfxutils::ExternalShapeSpec::kUnknown) {
    // Default to OBJ format.
    real_spec.format = ion::gfxutils::ExternalShapeSpec::kObj;
  }
  const std::string& shape_string =
      ion::base::ZipAssetManager::GetFileData(asset_name);
  DCHECK(!ion::base::IsInvalidReference(shape_string));
  std::istringstream str(shape_string);
  ion::gfx::ShapePtr shape = ion::gfxutils::LoadExternalShape(real_spec, str);

  if (radius != nullptr) {
    // Get position attribute.
    const ion::gfx::AttributeArrayPtr& attrs = shape->GetAttributeArray();
    const size_t position_idx = attrs->GetAttributeIndexByName("aVertex");
    const ion::gfx::Attribute& a = attrs->GetAttribute(position_idx);
    DCHECK(a.IsValid());

    // Get the vertex buffer.
    const ion::gfx::BufferObjectElement& element =
        a.GetValue<ion::gfx::BufferObjectElement>();
    DCHECK(!ion::base::IsInvalidReference(element));
    const ion::gfx::BufferObjectPtr& vb = element.buffer_object;
    const ion::base::DataContainerPtr& container = vb->GetData();
    // Get a generic pointer to the data.
    // 
    // in the data.
    uint8* data = container->GetMutableData<uint8>();
    const size_t num_verts = vb->GetCount();
    const size_t vertex_size = vb->GetStructSize();

    // Actual bounding box computation.
    Point3f pmin = *reinterpret_cast<const Point3f*>(data);
    Point3f pmax = pmin;
    for (size_t i = 1; i < num_verts; ++i) {
      const Point3f& v =
          *reinterpret_cast<const Point3f*>(data + i * vertex_size);
      for (int j = 0; j < 3; ++j) {
        pmin[j] = std::min(pmin[j], v[j]);
        pmax[j] = std::max(pmax[j], v[j]);
      }
    }
    *radius = 0.5f * ion::math::Length(pmax - pmin);
  }

  return shape;
}

ion::gfx::TexturePtr LoadTextureAsset(const std::string& asset_name) {
  ion::gfx::TexturePtr texture(new ion::gfx::Texture);
  texture->SetImage(0U, LoadImageAsset(
      asset_name, texture->GetAllocator(), true));
  return texture;
}

ion::gfx::CubeMapTexturePtr LoadCubeMapAsset(
    const std::string& prefix,
    const std::string& suffix) {
  ion::gfx::CubeMapTexturePtr cube_map(new ion::gfx::CubeMapTexture);
  ion::base::AllocatorPtr alloc = cube_map->GetAllocator();
  cube_map->SetImage(ion::gfx::CubeMapTexture::kNegativeX, 0U,
                     LoadImageAsset(prefix + "_left" + suffix, alloc, false));
  cube_map->SetImage(ion::gfx::CubeMapTexture::kNegativeY, 0U,
                     LoadImageAsset(prefix + "_bottom" + suffix, alloc, false));
  cube_map->SetImage(ion::gfx::CubeMapTexture::kNegativeZ, 0U,
                     LoadImageAsset(prefix + "_back" + suffix, alloc, false));
  cube_map->SetImage(ion::gfx::CubeMapTexture::kPositiveX, 0U,
                     LoadImageAsset(prefix + "_right" + suffix, alloc, false));
  cube_map->SetImage(ion::gfx::CubeMapTexture::kPositiveY, 0U,
                     LoadImageAsset(prefix + "_top" + suffix, alloc, false));
  cube_map->SetImage(ion::gfx::CubeMapTexture::kPositiveZ, 0U,
                     LoadImageAsset(prefix + "_front" + suffix, alloc, false));
  return cube_map;
}

ion::gfx::ShaderProgramPtr LoadShaderProgramAsset(
    const ion::gfxutils::ShaderManagerPtr& shader_manager,
    const std::string& label,
    const ion::gfx::ShaderInputRegistryPtr& input_registry,
    const std::string& asset_prefix) {
  ion::gfx::ShaderProgramPtr shader = LoadShaderProgramAsset(shader_manager,
      label, input_registry, asset_prefix + ".vp", asset_prefix + ".fp");
  return shader;
}

// Loads a complete shader program from assets. This version allows one to
// explicitly specify the vertex and fragment shader source asset names.
ion::gfx::ShaderProgramPtr LoadShaderProgramAsset(
    const ion::gfxutils::ShaderManagerPtr& shader_manager,
    const std::string& label,
    const ion::gfx::ShaderInputRegistryPtr& input_registry,
    const std::string& vertex_shader_asset,
    const std::string& fragment_shader_asset) {
  ion::gfx::ShaderProgramPtr shader = shader_manager->CreateShaderProgram(
        label,
        input_registry,
        ion::gfxutils::ShaderSourceComposerPtr(
            new ion::gfxutils::ZipAssetComposer(vertex_shader_asset,
                                                false)),
        ion::gfxutils::ShaderSourceComposerPtr(
            new ion::gfxutils::ZipAssetComposer(fragment_shader_asset,
                                                false)));
  return shader;
}

}  // namespace demoutils
