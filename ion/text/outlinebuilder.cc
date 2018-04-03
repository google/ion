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

#include "ion/text/outlinebuilder.h"

#include "ion/base/datacontainer.h"
#include "ion/base/invalid.h"
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
// The fragment shader implements outlining as follows:
//
// Character edges occur where the SDF texture is 0.5. The inside of the
// character is where the field is less than 0.5.  The outline is uOutlineWidth
// pixels outside the edges.  Smoothing occurs across 2*uHalfSmoothWidth pixels
// on either side of the outline.
//
// Therefore, an edge generally looks like this:
//
//   Interior     Outline     Exterior
// ------------0+++++++++++|++++++++++++++    <--- SDF Values
//            aaaaaaa   bbbbbbb
//
// The b's represent the band over which the outline color is blended with the
// background, centered over the outside edge of the outline band.  The a's
// represent the band over which the text color is blended with the outline
// color; it is biased a little toward the exterior so that the text color
// predominates when the glyphs are small.
//-----------------------------------------------------------------------------

static const char* kVertexShaderSource =
    "uniform ivec2 uViewportSize;\n"
    "uniform mat4 uProjectionMatrix;\n"
    "uniform mat4 uModelviewMatrix;\n"
    "attribute vec3 aVertex;\n"
    "attribute vec3 aFontPixelVec;\n"
    "attribute vec2 aTexCoords;\n"
    "varying vec2 vTexCoords;\n"
    "varying vec2 vFontPixelSize;\n"
    "\n"
    "void main(void) {\n"
    "  vTexCoords = aTexCoords;\n"
    "  mat4 pmv = uProjectionMatrix * uModelviewMatrix;\n"
    "  vec4 v0 = pmv * vec4(aVertex, 1.0);\n"
    "  vec4 v1 = pmv * vec4(aVertex + aFontPixelVec, 1.0);\n"
    "  gl_Position = v0;\n"
    "  // Compute the size of a font pixel in screen pixels in X and Y.\n"
    "  vec4 v = (v1 / v1.w) - (v0 / v0.w);\n"
    "  vFontPixelSize = vec2(abs(v.x * float(uViewportSize.x)),\n"
    "                        abs(v.y * float(uViewportSize.y)));\n"
    "}\n";

static const char* kFragmentShaderSource =
    "#ifdef GL_ES\n"
    "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
    "precision highp float;\n"
    "#else\n"
    "precision mediump float;\n"
    "#endif\n"
    "#endif\n"
    "uniform sampler2D uSdfSampler;\n"
    "uniform float uSdfPadding;\n"
    "uniform vec4 uTextColor;\n"
    "uniform vec4 uOutlineColor;\n"
    "uniform float uHalfSmoothWidth;\n"
    "uniform float uOutlineWidth;\n"
    "varying vec2 vTexCoords;\n"
    "varying vec2 vFontPixelSize;\n"
    "\n"
    "void main(void) {\n"
    "  float half_smooth_width = uHalfSmoothWidth;\n"
    "  float outline_width = uOutlineWidth;\n"
    "\n"
    "  // Get the signed distance from the edge in font pixels, centered at\n"
    "  // 0, then convert to screen pixels.\n"
    "  float sdf = texture2D(uSdfSampler, vTexCoords).r;\n"
    "  float dist = uSdfPadding * 2.0 * (sdf - 0.5);\n"
    "  float pixel_scale = mix(vFontPixelSize.x, vFontPixelSize.y, 0.5);\n"
    "  dist *= pixel_scale;\n"
    "\n"
    "  // Ensure the outline blending does not exceed the maximum distance.\n"
    "  float max_dist = uSdfPadding * pixel_scale;\n"
    "  outline_width = min(outline_width, max_dist - half_smooth_width);\n"
    "\n"
    "  // Discard fragments completely outside the smoothed outline.\n"
    "  if (dist >= outline_width + half_smooth_width) {\n"
    "    discard;\n"
    "  } else {\n"
    "    vec4 color;\n"
    "    // Set to blended outline color.\n"
    "    float outline_min = outline_width - half_smooth_width;\n"
    "    float outline_max = outline_width + half_smooth_width;\n"
    "    float d1 = smoothstep(outline_min, outline_max, dist);\n"
    "    color = (1.0 - smoothstep(outline_min, outline_max, dist)) *\n"
    "            uOutlineColor;\n"
    "\n"
    "    // Blend in text color.\n"
    "    float interior_bias = 0.2;\n"
    "    float interior_min = -half_smooth_width + interior_bias;\n"
    "    float interior_max = half_smooth_width + interior_bias;\n"
    "    float d2 = smoothstep(interior_min, interior_max, dist);\n"
    "    color = mix(uTextColor, color,\n"
    "                smoothstep(interior_min, interior_max, dist));\n"
    "    gl_FragColor = vec4(color.rgb * color.a, color.a);\n"
    "  }\n"
    "}\n";

//-----------------------------------------------------------------------------
//
// Helper functions.
//
//-----------------------------------------------------------------------------

// Computes and returns the vector from the bottom-left to top-right corner of
// a font pixel for a glyph in a Layout. This vector is used as a per-vertex
// attribute to allow fixed-size outlines. This returns a zero vector if there
// is no such glyph or it has no area.
static const math::Vector3f ComputeFontPixelVec(
    const Font& font, const Layout& layout, size_t glyph_index) {
  const Layout::Glyph& glyph = layout.GetGlyph(glyph_index);
  const Font::GlyphGrid& grid = font.GetGlyphGrid(glyph.glyph_index);
  math::Vector3f vec(math::Vector3f::Zero());
  if (!base::IsInvalidReference(grid)) {
    const size_t width = grid.pixels.GetWidth();
    const size_t height = grid.pixels.GetHeight();
    if (width && height) {
      const float inv_width = 1.0f / static_cast<float>(width);
      const float inv_height = 1.0f / static_cast<float>(height);
      const math::Point3f& lower_left = glyph.quad.points[0];
      const math::Point3f& lower_right = glyph.quad.points[1];
      const math::Point3f& upper_left = glyph.quad.points[3];
      const math::Vector3f v_right = (lower_right - lower_left) * inv_width;
      const math::Vector3f v_up = (upper_left - lower_left) * inv_height;
      vec = v_right + v_up;
    }
  }
  return vec;
}

}  // anonymous namespace

//-----------------------------------------------------------------------------
//
// OutlineBuilder functions.
//
//-----------------------------------------------------------------------------

OutlineBuilder::OutlineBuilder(const FontImagePtr& font_image,
                               const gfxutils::ShaderManagerPtr& shader_manager,
                               const base::AllocatorPtr& allocator)
    : Builder(font_image, shader_manager, allocator) {}

OutlineBuilder::~OutlineBuilder() {}

bool OutlineBuilder::SetSdfPadding(float padding) {
  return GetNode().Get() && GetNode()->SetUniformByName("uSdfPadding", padding);
}

bool OutlineBuilder::SetTextColor(const math::VectorBase4f& color) {
  return GetNode().Get() && GetNode()->SetUniformByName("uTextColor", color);
}

bool OutlineBuilder::SetOutlineColor(const math::VectorBase4f& color) {
  return GetNode().Get() && GetNode()->SetUniformByName("uOutlineColor", color);
}

bool OutlineBuilder::SetOutlineWidth(float width) {
  return GetNode().Get() && GetNode()->SetUniformByName("uOutlineWidth", width);
}

bool OutlineBuilder::SetHalfSmoothWidth(float width) {
  return GetNode().Get() && GetNode()->SetUniformByName("uHalfSmoothWidth",
                                                        width);
}

const gfx::ShaderInputRegistryPtr OutlineBuilder::GetShaderInputRegistry() {
  gfx::ShaderInputRegistryPtr reg(new(GetAllocator()) gfx::ShaderInputRegistry);
  reg->IncludeGlobalRegistry();
  reg->Add(gfx::ShaderInputRegistry::UniformSpec(
      "uSdfPadding", gfx::kFloatUniform, "SDF padding amount"));
  reg->Add(gfx::ShaderInputRegistry::UniformSpec(
      "uSdfSampler", gfx::kTextureUniform, "SDF font texture sampler"));
  reg->Add(gfx::ShaderInputRegistry::UniformSpec(
      "uTextColor", gfx::kFloatVector4Uniform, "Text foreground color"));
  reg->Add(gfx::ShaderInputRegistry::UniformSpec(
      "uOutlineColor", gfx::kFloatVector4Uniform, "Text outline color"));
  reg->Add(gfx::ShaderInputRegistry::UniformSpec(
      "uOutlineWidth", gfx::kFloatUniform, "Text outline width in pixels"));
  reg->Add(gfx::ShaderInputRegistry::UniformSpec(
      "uHalfSmoothWidth", gfx::kFloatUniform,
      "Half of edge smoothing width in pixels"));
  return reg;
}

void OutlineBuilder::GetShaderStrings(std::string* id_string,
                                    std::string* vertex_source,
                                    std::string* fragment_source) {
  *id_string = "Outline Text Shader";
  *vertex_source = kVertexShaderSource;
  *fragment_source = kFragmentShaderSource;
}

void OutlineBuilder::UpdateUniforms(const gfx::ShaderInputRegistryPtr& registry,
                                  gfx::Node* node) {
  // Add uniforms if not already there.
  DCHECK(GetFontImage().Get());
  const Font* font = GetFontImage()->GetFont().Get();
  DCHECK(font);
  const float sdf_padding =
      font ? static_cast<float>(font->GetSdfPadding()) : 0.f;
  if (node->GetUniforms().size() < 6U)
    node->ClearUniforms();
  if (node->GetUniforms().empty()) {
    node->AddUniform(registry->Create<gfx::Uniform>(
        "uSdfPadding", sdf_padding));
    node->AddUniform(registry->Create<gfx::Uniform>(
        "uSdfSampler", GetFontImageTexture()));
    node->AddUniform(registry->Create<gfx::Uniform>(
        "uTextColor", math::Point4f(1.f, 1.f, 1.f, 1.f)));
    node->AddUniform(registry->Create<gfx::Uniform>(
        "uOutlineColor", math::Point4f(0.f, 0.f, 0.f, 0.f)));
    node->AddUniform(registry->Create<gfx::Uniform>("uOutlineWidth", 2.f));
    node->AddUniform(registry->Create<gfx::Uniform>("uHalfSmoothWidth", 3.f));
  } else {
    // Make sure the uniforms have the correct values. These are the only two
    // that can change from external sources.
    DCHECK_GE(node->GetUniforms().size(), 6U);
    node->SetUniformValue<float>(0U, sdf_padding);
    UpdateFontImageTextureUniform(1U, node);
  }
}

void OutlineBuilder::BindAttributes(const gfx::AttributeArrayPtr& attr_array,
                                    const gfx::BufferObjectPtr& buffer_object) {
  Vertex v;
  gfxutils::BufferToAttributeBinder<Vertex>(v)
      .Bind(v.position, "aVertex")
      .Bind(v.texture_coords, "aTexCoords")
      .Bind(v.font_pixel_vec, "aFontPixelVec")
      .Apply(gfx::ShaderInputRegistry::GetGlobalRegistry(), attr_array,
             buffer_object);
}

base::AllocVector<char> OutlineBuilder::BuildVertexData(const Layout& layout,
                                                        size_t* vertex_size,
                                                        size_t* num_vertices) {
  // There are 4 vertices per glyph.
  const size_t num_glyphs = layout.GetGlyphCount();
  *vertex_size = sizeof(Vertex);
  *num_vertices = 4 * num_glyphs;
  base::AllocVector<char> vertex_data(
      GetAllocator()->GetAllocatorForLifetime(base::kShortTerm));
  vertex_data.resize(sizeof(Vertex) * (*num_vertices));
  Vertex* vertices = reinterpret_cast<Vertex*>(&vertex_data[0]);
  math::Point3f positions[4];
  math::Point2f texture_coords[4];
  DCHECK(GetFont().Get());
  const Font& font = *GetFont();
  for (size_t i = 0; i < num_glyphs; ++i) {
    StoreGlyphVertices(layout, i, positions, texture_coords);
    const math::Vector3f font_pixel_vec = ComputeFontPixelVec(font, layout, i);
    for (int j = 0; j < 4; ++j)
      vertices[4 * i + j] =
          Vertex(positions[j], texture_coords[j], font_pixel_vec);
  }
  return vertex_data;
}

}  // namespace text
}  // namespace ion
