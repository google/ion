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

#include "ion/image/renderutils.h"

#include <string>

#include "base/integral_types.h"
#include "ion/base/allocationmanager.h"
#include "ion/base/datacontainer.h"
#include "ion/gfx/framebufferobject.h"
#include "ion/gfx/node.h"
#include "ion/gfx/sampler.h"
#include "ion/gfx/shader.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfx/shaderprogram.h"
#include "ion/gfx/shape.h"
#include "ion/gfx/statetable.h"
#include "ion/gfx/texture.h"
#include "ion/gfxutils/shapeutils.h"
#include "ion/math/range.h"
#include "ion/math/vector.h"

namespace ion {
namespace image {

namespace {

//-----------------------------------------------------------------------------
//
// Shader strings.
//
//-----------------------------------------------------------------------------

static const char kVertexShaderString[] =
    "attribute vec3 aVertex;\n"
    "attribute vec2 aTexCoords;\n"
    "varying vec2 vTextureCoords;\n"
    "\n"
    "void main(void) {\n"
    "  vTextureCoords = aTexCoords;\n"
    "  gl_Position = vec4(aVertex, 1.);\n"
    "}\n";

static const char kTextureFragmentShaderString[] =
    "#ifdef GL_ES\n"
    "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
    "precision highp float;\n"
    "#else\n"
    "precision mediump float;\n"
    "#endif\n"
    "#endif\n"
    "\n"
    "uniform sampler2D uTexture;\n"
    "varying vec2 vTextureCoords;\n"
    "\n"
    "void main(void) {\n"
    "  gl_FragColor = texture2D(uTexture, vTextureCoords);\n"
    "}\n";

static const char kCubeMapFragmentShaderString[] =
    "#ifdef GL_ES\n"
    "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
    "precision highp float;\n"
    "#else\n"
    "precision mediump float;\n"
    "#endif\n"
    "#endif\n"
    "\n"
    "uniform int uCubeMapFace;\n"
    "uniform samplerCube uCubeMap;\n"
    "varying vec2 vTextureCoords;\n"
    "\n"
    "void main(void) {\n"
    "  /* Put coords in range (-1, 1). */\n"
    "  float s = -1. + 2. * vTextureCoords.s;\n"
    "  float t = -1. + 2. * vTextureCoords.t;\n"
    "  vec3 tc;\n"
    "  if (uCubeMapFace == 0) {         /* Left   */\n"
    "    tc = vec3(-1., t, s);\n"
    "  } else if (uCubeMapFace == 1) {  /* Bottom */\n"
    "    tc = vec3(s, -1., t);\n"
    "  } else if (uCubeMapFace == 2) {  /* Back   */\n"
    "    tc = vec3(s, -t, -1.);\n"
    "  } else if (uCubeMapFace == 3) {  /* Right  */\n"
    "    tc = vec3(1., t, -s);\n"
    "  } else if (uCubeMapFace == 4) {  /* Top    */\n"
    "    tc = vec3(s, 1., -t);\n"
    "  } else                        {  /* Front  */\n"
    "    tc = vec3(s, t, 1.);\n"
    "  }\n"
    "  gl_FragColor = textureCube(uCubeMap, tc);\n"
    "}\n";

//-----------------------------------------------------------------------------
//
// Helper functions.
//
//-----------------------------------------------------------------------------

// Returns a short-term allocator based on an Allocator.
static const base::AllocatorPtr& GetShortTermAllocator(
    const base::AllocatorPtr& allocator) {
  return allocator.Get() ?
      allocator->GetAllocatorForLifetime(base::kShortTerm) :
      base::AllocationManager::GetDefaultAllocatorForLifetime(
          base::kShortTerm);
}

// Builds a gfx::ShaderInputRegistry for BuildNode().
static const gfx::ShaderInputRegistryPtr BuildRegistry(
    bool is_cubemap, const base::AllocatorPtr& allocator) {
  gfx::ShaderInputRegistryPtr reg(new(allocator) gfx::ShaderInputRegistry);
  reg->IncludeGlobalRegistry();
  if (is_cubemap) {
    reg->Add(gfx::ShaderInputRegistry::UniformSpec(
        "uCubeMap", gfx::kCubeMapTextureUniform, "CubeMapTexture"));
    reg->Add(gfx::ShaderInputRegistry::UniformSpec(
        "uCubeMapFace", gfx::kIntUniform, "Face of cubemap"));
  } else {
    reg->Add(gfx::ShaderInputRegistry::UniformSpec(
        "uTexture", gfx::kTextureUniform, "Texture"));
  }
  return reg;
}

// Builds and returns a Node representing a Texture or CubeMapTexture to be
// rendered. If texture is not NULL, this uses it and sets the uCubeMapFace
// uniform to -1.  Otherwise, this uses cubemap and sets uCubeMapFace to the
// passed face. The viewport_size is used to set up a StateTable.
static const gfx::NodePtr BuildNode(
    const gfx::TexturePtr& texture,
    const gfx::CubeMapTexturePtr& cubemap, int face,
    const math::Vector2i& viewport_size, const base::AllocatorPtr& allocator) {
  gfx::NodePtr node(new(allocator) gfx::Node);

  const bool is_cubemap = cubemap.Get();

  // Add a StateTable which will be modified with the correct viewport setting
  // when rendering.
  gfx::StateTablePtr state_table(
      new(allocator) gfx::StateTable(viewport_size[0], viewport_size[1]));
  state_table->SetViewport(
      math::Range2i::BuildWithSize(math::Point2i::Zero(), viewport_size));
  node->SetStateTable(state_table);

  // Build and add a rectangle Shape.
  gfxutils::RectangleSpec rect_spec;
  rect_spec.allocator = allocator;
  rect_spec.size.Set(2.f, 2.f);  // -1 to +1.
  rect_spec.vertex_type = gfxutils::ShapeSpec::kPositionTexCoords;
  gfx::ShapePtr rect_shape = gfxutils::BuildRectangleShape(rect_spec);
  node->AddShape(rect_shape);

  // Set the shader registry and program.
  gfx::ShaderInputRegistryPtr reg = BuildRegistry(is_cubemap, allocator);
  node->SetShaderProgram(
      gfx::ShaderProgram::BuildFromStrings(
      "Ion image renderutils", reg, kVertexShaderString,
      is_cubemap ? kCubeMapFragmentShaderString : kTextureFragmentShaderString,
      allocator));

  // Add the Uniforms.
  if (is_cubemap) {
    node->AddUniform(reg->Create<gfx::Uniform>("uCubeMap", cubemap));
    node->AddUniform(reg->Create<gfx::Uniform>("uCubeMapFace",
                                               static_cast<int>(face)));
  } else {
    node->AddUniform(reg->Create<gfx::Uniform>("uTexture", texture));
  }

  return node;
}

// Renders a Node into a new Image, which is created with the given size using
// the Allocator and then returned.
static const gfx::ImagePtr RenderToImage(
    const gfx::RendererPtr& renderer, const gfx::NodePtr& node,
    const math::Vector2i& image_size, const base::AllocatorPtr& allocator) {
  DCHECK(renderer.Get());
  DCHECK(node.Get());
  DCHECK_GT(image_size[0], 0);
  DCHECK_GT(image_size[1], 0);

  // Determine the target format for rendering the image. ES2 supports only
  // RGB565.
  DCHECK(renderer->GetGraphicsManager().Get());
  const gfx::GraphicsManager& gm = *renderer->GetGraphicsManager();
  const gfx::Image::Format target_format =
      (gm.GetGlFlavor() == gfx::GraphicsManager::kEs &&
       gm.GetGlVersion() == 20) ? gfx::Image::kRgb565Byte : gfx::Image::kRgb8;

  // Create a temporary FrameBufferObject and render into it.
  const base::AllocatorPtr& st_alloc = GetShortTermAllocator(allocator);
  const gfx::FramebufferObjectPtr fbo(
      new (st_alloc) gfx::FramebufferObject(image_size[0], image_size[1]));
  fbo->SetColorAttachment(0U,
                          gfx::FramebufferObject::Attachment(target_format));
  renderer->BindFramebuffer(fbo);
  renderer->DrawScene(node);

  // Read the rendered result into an Image.
  gfx::ImagePtr image = renderer->ReadImage(
      math::Range2i::BuildWithSize(math::Point2i::Zero(), image_size),
      gfx::Image::kRgb888, allocator);

  // Bind the default framebuffer to avoid problems.
  renderer->BindFramebuffer(gfx::FramebufferObjectPtr());

  return image;
}

// Implements rendering a Texture or CubeMapTexture to an image. Either the
// texture or cubemap should be non-NULL for this to do anything.
static const gfx::ImagePtr RenderTextureOrCubeMapTextureToImage(
    const gfx::TexturePtr& texture,
    const gfx::CubeMapTexturePtr& cubemap, gfx::CubeMapTexture::CubeFace face,
    const math::Vector2i& size, const gfx::RendererPtr& renderer,
    const base::AllocatorPtr& allocator) {
  gfx::ImagePtr output_image;
  if ((texture.Get() || cubemap.Get()) &&
      renderer.Get() && size[0] && size[1]) {
    gfx::NodePtr node = BuildNode(texture, cubemap, face, size,
                                  GetShortTermAllocator(allocator));
    output_image = RenderToImage(renderer, node, size, allocator);
  }
  return output_image;
}

}  // anonymous namespace

//-----------------------------------------------------------------------------
//
// Public functions.
//
//-----------------------------------------------------------------------------

const gfx::ImagePtr RenderTextureImage(
    const gfx::TexturePtr& texture, uint32 width, uint32 height,
    const gfx::RendererPtr& renderer, const base::AllocatorPtr& allocator) {
  // Can pass any face - it will be ignored.
  return RenderTextureOrCubeMapTextureToImage(
      texture, gfx::CubeMapTexturePtr(), gfx::CubeMapTexture::kNegativeX,
      math::Vector2i(width, height), renderer, allocator);
}

const gfx::ImagePtr RenderCubeMapTextureFaceImage(
    const gfx::CubeMapTexturePtr& cubemap, gfx::CubeMapTexture::CubeFace face,
    uint32 width, uint32 height,
    const gfx::RendererPtr& renderer, const base::AllocatorPtr& allocator) {
  return RenderTextureOrCubeMapTextureToImage(
      gfx::TexturePtr(), cubemap, face,
      math::Vector2i(width, height), renderer, allocator);
}

}  // namespace image
}  // namespace ion
