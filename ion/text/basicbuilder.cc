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

#include "ion/text/basicbuilder.h"

#include "ion/base/datacontainer.h"
#include "ion/base/logging.h"
#include "ion/base/stlalloc/allocvector.h"
#include "ion/gfx/attribute.h"
#include "ion/gfx/attributearray.h"
#include "ion/gfx/bufferobject.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfx/texture.h"
#include "ion/gfx/uniform.h"
#include "ion/gfxutils/buffertoattributebinder.h"
#include "ion/math/vector.h"
#include "ion/text/font.h"
#include "ion/text/fontimage.h"
#include "ion/text/layout.h"

namespace ion {
namespace text {

namespace {

//-----------------------------------------------------------------------------
//
// Shader source strings.
//
//-----------------------------------------------------------------------------

static const char* kVertexShaderSource =
    "uniform mat4 uProjectionMatrix;\n"
    "uniform mat4 uModelviewMatrix;\n"
    "attribute vec3 aVertex;\n"
    "attribute vec2 aTexCoords;\n"
    "varying vec2 texture_coords;\n"
    "\n"
    "void main(void) {\n"
    "  texture_coords = aTexCoords;\n"
    "  gl_Position = uProjectionMatrix * uModelviewMatrix * vec4(aVertex, 1);\n"
    "}\n";

static const char* kFragmentShaderSource =
    "#ifdef GL_ES\n"
    "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
    "precision highp float;\n"
    "#else\n"
    "precision mediump float;\n"
    "#endif\n"
    "#endif\n"
    "\n"
    "varying vec2 texture_coords;\n"
    "uniform sampler2D uSdfSampler;\n"
    "uniform float uSdfPadding;\n"
    "uniform vec4 uTextColor;\n"
    "\n"
    "void main(void) {\n"
    "  float dist = texture2D(uSdfSampler, texture_coords).r;\n"
    "  float s = uSdfPadding == 0. ? 0.2 : 0.2 / uSdfPadding;\n"
    "  float d = 1.0 - smoothstep(-s, s, dist - 0.5);\n"
    "  if (dist > 0.5 + s)\n"
    "    discard;\n"
    "  gl_FragColor = d * uTextColor;\n"
    "}\n";

}  // anonymous namespace

//-----------------------------------------------------------------------------
//
// BasicBuilder functions.
//
//-----------------------------------------------------------------------------

BasicBuilder::BasicBuilder(const FontImagePtr& font_image,
                           const gfxutils::ShaderManagerPtr& shader_manager,
                           const base::AllocatorPtr& allocator)
    : Builder(font_image, shader_manager, allocator) {}

BasicBuilder::~BasicBuilder() {}

bool BasicBuilder::SetSdfPadding(float padding) {
  return GetNode().Get() && GetNode()->SetUniformByName("uSdfPadding", padding);
}

bool BasicBuilder::SetTextColor(const math::VectorBase4f& color) {
  return GetNode().Get() && GetNode()->SetUniformByName("uTextColor", color);
}

const gfx::ShaderInputRegistryPtr BasicBuilder::GetShaderInputRegistry() {
  gfx::ShaderInputRegistryPtr reg(new(GetAllocator()) gfx::ShaderInputRegistry);
  reg->IncludeGlobalRegistry();
  reg->Add(gfx::ShaderInputRegistry::UniformSpec(
      "uSdfPadding", gfx::kFloatUniform, "SDF padding amount"));
  reg->Add(gfx::ShaderInputRegistry::UniformSpec(
      "uSdfSampler", gfx::kTextureUniform, "SDF font texture sampler"));
  reg->Add(gfx::ShaderInputRegistry::UniformSpec(
      "uTextColor", gfx::kFloatVector4Uniform, "Text foreground color"));
  return reg;
}

void BasicBuilder::GetShaderStrings(std::string* id_string,
                                    std::string* vertex_source,
                                    std::string* fragment_source) {
  // If you copy code from here, and change the shaders. You must also change
  // the id_string. If you do not, you are likely to have strange failures.
  *id_string = "Basic Text Shader";
  *vertex_source = kVertexShaderSource;
  *fragment_source = kFragmentShaderSource;
}

void BasicBuilder::UpdateUniforms(const gfx::ShaderInputRegistryPtr& registry,
                                  gfx::Node* node) {
  const Font* font = GetFont().Get();
  const float sdf_padding =
      font ? static_cast<float>(font->GetSdfPadding()) : 0.f;
  // Add uniforms if not already there.
  if (node->GetUniforms().size() < 2U)
    node->ClearUniforms();
  if (node->GetUniforms().empty()) {
    node->AddUniform(registry->Create<gfx::Uniform>(
        "uSdfPadding", sdf_padding));
    node->AddUniform(registry->Create<gfx::Uniform>(
        "uSdfSampler", GetFontImageTexture()));
    node->AddUniform(registry->Create<gfx::Uniform>(
        "uTextColor", math::Point4f(1.f, 1.f, 1.f, 1.f)));
  } else {
    // Make sure the uniforms have the correct values. These are the only two
    // that can change from external sources.
    DCHECK_GE(node->GetUniforms().size(), 3U);
    node->SetUniformValue<float>(0U, sdf_padding);
    UpdateFontImageTextureUniform(1U, node);
  }
}

void BasicBuilder::BindAttributes(const gfx::AttributeArrayPtr& attr_array,
                                  const gfx::BufferObjectPtr& buffer_object) {
  Vertex v;
  gfxutils::BufferToAttributeBinder<Vertex>(v)
      .Bind(v.position, "aVertex")
      .Bind(v.texture_coords, "aTexCoords")
      .Apply(gfx::ShaderInputRegistry::GetGlobalRegistry(), attr_array,
             buffer_object);
}

base::AllocVector<char> BasicBuilder::BuildVertexData(const Layout& layout,
                                                      size_t* vertex_size,
                                                      size_t* num_vertices) {
  // There are 4 vertices per glyph.
  const size_t num_glyphs = layout.GetGlyphCount();
  *num_vertices = 4 * num_glyphs;
  *vertex_size = sizeof(Vertex);
  base::AllocVector<char> vertex_data(
      GetAllocator()->GetAllocatorForLifetime(base::kShortTerm));
  vertex_data.resize(sizeof(Vertex) * (*num_vertices));
  Vertex* vertices = reinterpret_cast<Vertex*>(&vertex_data[0]);
  math::Point3f positions[4];
  math::Point2f texture_coords[4];
  for (size_t i = 0; i < num_glyphs; ++i) {
    StoreGlyphVertices(layout, i, positions, texture_coords);
    for (int j = 0; j < 4; ++j)
      vertices[4 * i + j] = Vertex(positions[j], texture_coords[j]);
  }
  return vertex_data;
}


}  // namespace text
}  // namespace ion
