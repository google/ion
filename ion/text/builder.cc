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

#include "ion/text/builder.h"

#include "ion/base/allocationmanager.h"
#include "ion/base/datacontainer.h"
#include "ion/base/invalid.h"
#include "ion/base/logging.h"
#include "ion/gfx/indexbuffer.h"
#include "ion/gfx/statetable.h"
#include "ion/gfx/texture.h"
#include "ion/gfxutils/shadersourcecomposer.h"
#include "ion/math/range.h"
#include "ion/text/layout.h"

namespace ion {
namespace text {

//-----------------------------------------------------------------------------
//
// Helper functions.
//
//-----------------------------------------------------------------------------

namespace {

// Returns a GlyphSet containing indices of all glyphs used in a Layout
// and that uses the given Allocator.
static const GlyphSet GetGlyphSetFromLayout(
    const Layout& layout, const base::AllocatorPtr& allocator) {
  GlyphSet glyph_set(allocator);
  const size_t num_glyphs = layout.GetGlyphCount();
  for (size_t i = 0; i < num_glyphs; ++i)
    glyph_set.insert(layout.GetGlyph(i).glyph_index);
  return glyph_set;
}

// Creates and returns a StateTable suitable for text (back-face culling
// disabled, blending enabled).
static const gfx::StateTablePtr BuildStateTable(
    const base::AllocatorPtr& allocator) {
  gfx::StateTablePtr state_table(new(allocator) gfx::StateTable());
  state_table->Enable(gfx::StateTable::kCullFace, false);
  state_table->Enable(gfx::StateTable::kBlend, true);
  state_table->SetBlendEquations(gfx::StateTable::kAdd,
                                 gfx::StateTable::kAdd);
  state_table->SetBlendFunctions(gfx::StateTable::kOne,
                                 gfx::StateTable::kOneMinusSrcAlpha,
                                 gfx::StateTable::kOne,
                                 gfx::StateTable::kOneMinusSrcAlpha);
  return state_table;
}

// Creates and returns an IndexBuffer representing the indices of triangles
// representing text. The IndexBuffer will contain unsigned short indices. The
// buffer data will be marked as wipeable if the usage_mode is
// gfx::BufferObject::kStaticDraw. The allocator is used for the resulting
// buffer; if it is null, the default short-term allocator is used.
static const gfx::IndexBufferPtr BuildIndexBuffer(
    const Layout& layout, gfx::BufferObject::UsageMode usage_mode,
    const base::AllocatorPtr& allocator) {
  const base::AllocatorPtr& al =
      base::AllocationManager::GetNonNullAllocator(allocator);

  const size_t num_glyphs = layout.GetGlyphCount();
  base::AllocVector<uint16> indices(
      al->GetAllocatorForLifetime(base::kShortTerm));
  indices.reserve(6 * num_glyphs);  // 2 triangles per glyph.

  for (size_t i = 0; i < num_glyphs; ++i) {
    indices.push_back(static_cast<uint16>(4 * i + 0));
    indices.push_back(static_cast<uint16>(4 * i + 1));
    indices.push_back(static_cast<uint16>(4 * i + 2));
    indices.push_back(static_cast<uint16>(4 * i + 0));
    indices.push_back(static_cast<uint16>(4 * i + 2));
    indices.push_back(static_cast<uint16>(4 * i + 3));
  }

  gfx::IndexBufferPtr index_buffer(new(al) gfx::IndexBuffer);
  base::DataContainerPtr container =
      base::DataContainer::CreateAndCopy<uint16>(
          &indices[0], indices.size(),
          usage_mode == gfx::BufferObject::kStaticDraw, al);
  index_buffer->AddSpec(gfx::BufferObject::kUnsignedShort, 1, 0);
  index_buffer->SetData(container, sizeof(indices[0]), indices.size(),
                        usage_mode);
  return index_buffer;
}

static const gfx::BufferObjectPtr GetBufferObject(
    const gfx::AttributeArray& attr_array) {
  gfx::BufferObjectPtr bo;
  if (attr_array.GetBufferAttributeCount() >= 1U) {
    const gfx::Attribute& attr = attr_array.GetBufferAttribute(0);
    DCHECK_EQ(attr.GetType(), gfx::kBufferObjectElementAttribute);
    bo = attr.GetValue<gfx::BufferObjectElement>().buffer_object;
  }
  return bo;
}

static bool CanBufferObjectBeReused(const gfx::BufferObject& bo,
                                    size_t num_vertices) {
  return (bo.GetCount() == num_vertices &&
          bo.GetData().Get() &&
          bo.GetUsageMode() != gfx::BufferObject::kStaticDraw &&
          bo.GetData().Get() &&
          bo.GetData()->GetData());
}


}  // anonymous namespace

//-----------------------------------------------------------------------------
//
// Builder functions.
//
//-----------------------------------------------------------------------------

Builder::Builder(const FontImagePtr& font_image,
                 const gfxutils::ShaderManagerPtr& shader_manager,
                 const base::AllocatorPtr& allocator)
    : font_image_(font_image),
      shader_manager_(shader_manager),
      allocator_(base::AllocationManager::GetNonNullAllocator(allocator)),
      image_data_(nullptr) {}

Builder::~Builder() {}

bool Builder::Build(const Layout& layout,
                    gfx::BufferObject::UsageMode usage_mode) {
  // If the FontImage was not created or the Layout is empty, just return false.
  if (!font_image_.Get() || !layout.GetGlyphCount())
    return false;

  // Determine the FontImage::ImageData instance that contains all the
  // necessary glyphs. If successful, cache a pointer to it during
  // the call to Build(). Otherwise, stop.
  DCHECK(!image_data_);
  const GlyphSet glyph_set = GetGlyphSetFromLayout(
      layout, allocator_->GetAllocatorForLifetime(base::kShortTerm));
  const FontImage::ImageData& image_data =
      font_image_->FindImageData(glyph_set);
  if (base::IsInvalidReference(image_data))
    return false;
  image_data_ = &image_data;

  // Create a node if necessary.
  if (!node_.Get())
    node_.Reset(new (allocator_) gfx::Node);

  // Set up the ShaderProgram and StateTable if they don't already exist.
  // Neither needs to be updated when rebuilding.
  if (!node_->GetShaderProgram().Get())
    node_->SetShaderProgram(BuildShaderProgram());
  if (!node_->GetStateTable().Get())
    node_->SetStateTable(BuildStateTable(allocator_));

  // Let the derived class update the uniforms in the node. Always do this, as
  // they may need to change with a rebuild.
  DCHECK(registry_.Get());
  UpdateUniforms(registry_, node_.Get());

  // Create and add a Shape if necessary, and always update it.
  if (node_->GetShapes().empty())
    node_->AddShape(gfx::ShapePtr(new (allocator_) gfx::Shape()));
  UpdateShape(layout, usage_mode, node_->GetShapes()[0].Get());

  image_data_ = nullptr;
  return true;
}

const gfx::ShaderProgramPtr Builder::BuildShaderProgram() {
  // Get all the necessary items from the derived class.
  std::string id_string;
  std::string vertex_source;
  std::string fragment_source;
  GetShaderStrings(&id_string, &vertex_source, &fragment_source);

  gfx::ShaderProgramPtr program;
  if (shader_manager_.Get()) {
    // If a shader with the subclass-provided name already exists, use it.
    program = shader_manager_->GetShaderProgram(id_string);
    if (program.Get()) {
      registry_ = program->GetRegistry();
      return program;
    }
    // If there is a ShaderManager, use it to compose the program.
    if (!registry_)
      registry_ = GetShaderInputRegistry();
    program = shader_manager_->CreateShaderProgram(
        id_string, registry_,
        gfxutils::ShaderSourceComposerPtr(
            new(allocator_) gfxutils::StringComposer(
                id_string + " vertex shader", vertex_source)),
        gfxutils::ShaderSourceComposerPtr(
            new(allocator_) ion::gfxutils::StringComposer(
                id_string + " fragment shader", fragment_source)));
  } else {
    // Otherwise, build the shader program directly.
    if (!registry_)
      registry_ = GetShaderInputRegistry();
    program = new(allocator_) gfx::ShaderProgram(registry_);
    program->SetLabel(id_string);
    program->SetVertexShader(
        gfx::ShaderPtr(new(allocator_) gfx::Shader(vertex_source)));
    program->GetVertexShader()->SetLabel(id_string + " vertex shader");
    program->SetFragmentShader(
        gfx::ShaderPtr(new(allocator_) gfx::Shader(fragment_source)));
    program->GetFragmentShader()->SetLabel(id_string + " fragment shader");
  }
  return program;
}

const gfx::TexturePtr Builder::GetFontImageTexture() {
  gfx::TexturePtr texture;
  if (font_image_.Get()) {
    DCHECK(image_data_);
    texture = image_data_->texture;
  }
  return texture;
}

bool Builder::UpdateFontImageTextureUniform(size_t index, gfx::Node* node) {
  bool ok = false;
  if (node && index < node->GetUniforms().size()) {
    const gfx::Uniform& uniform = node->GetUniforms()[index];
    if (uniform.IsValid() && uniform.GetType() == gfx::kTextureUniform) {
      gfx::TexturePtr texture = GetFontImageTexture();
      if (uniform.GetValue<gfx::TexturePtr>() != texture)
        node->SetUniformValue<gfx::TexturePtr>(index, texture);
      ok = true;
    }
  }
  return ok;
}

void Builder::StoreGlyphVertices(const Layout& layout, size_t glyph_index,
                                 math::Point3f positions[4],
                                 math::Point2f texture_coords[4]) {
  const Layout::Glyph& glyph = layout.GetGlyph(glyph_index);
  DCHECK(!base::IsInvalidReference(glyph));

  math::Range2f texcoord_rect;
  DCHECK(image_data_);
  if (FontImage::GetTextureCoords(*image_data_, glyph.glyph_index,
                                  &texcoord_rect)) {
    for (int i = 0; i < 4; ++i) {
      positions[i] = glyph.quad.points[i];
      text_extents_.ExtendByPoint(positions[i]);
    }

    const float u_min = texcoord_rect.GetMinPoint()[0];
    const float u_max = texcoord_rect.GetMaxPoint()[0];
    // Invert v because OpenGL flips images vertically.
    const float v_min = texcoord_rect.GetMaxPoint()[1];
    const float v_max = texcoord_rect.GetMinPoint()[1];
    texture_coords[0].Set(u_min, v_min);
    texture_coords[1].Set(u_max, v_min);
    texture_coords[2].Set(u_max, v_max);
    texture_coords[3].Set(u_min, v_max);
  } else {
    // Use empty rectangles for glyphs that are not available in the font.
    for (int i = 0; i < 4; ++i) {
      positions[i] = math::Point3f::Zero();
      texture_coords[i] = math::Point2f::Zero();
    }
  }
}

bool Builder::UpdateAttributeArray(
    const Layout& layout, gfx::BufferObject::UsageMode usage_mode,
    const gfx::AttributeArrayPtr& attr_array) {
  DCHECK(attr_array.Get());
  const base::AllocatorPtr& allocator = GetAllocator();

  // Compute the vertices for the layout.
  size_t vertex_size = 0U;
  size_t num_vertices = 0U;
  text_extents_.MakeEmpty();
  base::AllocVector<char> vertex_data =
      BuildVertexData(layout, &vertex_size, &num_vertices);
  DCHECK_LT(0U, vertex_size);
  DCHECK_LT(0U, num_vertices);

  // Access the BufferObject from the AttributeArray. If there isn't one,
  // create one.
  gfx::BufferObjectPtr bo = GetBufferObject(*attr_array);
  bool reuse_buffer = false;
  if (!bo.Get()) {
    // If there isn't a BufferObject, create one.
    bo = new (allocator) gfx::BufferObject;
  } else {
    // If there is one, see if it can be reused.  This requires the same number
    // of vertices, a valid DataContainer, and a UsageMode that allows updates.
    reuse_buffer = CanBufferObjectBeReused(*bo, num_vertices);
  }

  if (reuse_buffer) {
    // Just overwrite the data in the BufferObject's DataContainer.
    base::DataContainer& container = *bo->GetData();
    char* old_vertex_data = container.GetMutableData<char>();
    DCHECK(old_vertex_data);
    memcpy(old_vertex_data, &vertex_data[0], vertex_size * num_vertices);
  } else {
    // Replace the BufferObject's DataContainer with one having the right size.
    base::DataContainerPtr container =
        base::DataContainer::CreateAndCopy<char>(
            &vertex_data[0], vertex_size * num_vertices,
            usage_mode == gfx::BufferObject::kStaticDraw, GetAllocator());
    bo->SetData(container, vertex_size, num_vertices, usage_mode);

    // Bind the BufferObject to the AttributeArray if it was just created.
    if (!attr_array->GetBufferAttributeCount()) {
      BindAttributes(attr_array, bo);
    }
  }
  return reuse_buffer;
}

void Builder::UpdateShape(
    const Layout& layout, gfx::BufferObject::UsageMode usage_mode,
    gfx::Shape* shape) {
  shape->SetPrimitiveType(gfx::Shape::kTriangles);

  // Create an AttributeArray if necessary.
  if (!shape->GetAttributeArray().Get())
    shape->SetAttributeArray(
        gfx::AttributeArrayPtr(new (allocator_) gfx::AttributeArray));

  // Let the derived class update the AttributeArray. It returns true if the
  // buffer in the AttributeArray did not need to be reallocated. If so, the
  // IndexBuffer in the Shape is ok to use as is.
  const bool buffer_reused =
      UpdateAttributeArray(layout, usage_mode, shape->GetAttributeArray());
  if (!shape->GetIndexBuffer().Get() || !buffer_reused) {
    shape->SetIndexBuffer(BuildIndexBuffer(
        layout, gfx::BufferObject::kStaticDraw, allocator_));
  }
}

}  // namespace text
}  // namespace ion
