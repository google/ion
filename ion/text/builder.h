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

#ifndef ION_TEXT_BUILDER_H_
#define ION_TEXT_BUILDER_H_

#include <string>

#include "ion/base/referent.h"
#include "ion/base/stlalloc/allocvector.h"
#include "ion/gfx/attributearray.h"
#include "ion/gfx/bufferobject.h"
#include "ion/gfx/node.h"
#include "ion/gfx/sampler.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfxutils/shadermanager.h"
#include "ion/math/vector.h"
#include "ion/text/font.h"
#include "ion/text/fontimage.h"

namespace ion {
namespace gfx {
class Shape;
}

namespace text {
class Layout;

// Builder is an abstract base class for building graphics objects used to
// render text.
class ION_API Builder : public base::Referent {
 public:
  // Returns the FontImage passed to the constructor.
  const FontImagePtr& GetFontImage() const { return font_image_; }

  // Modifies the Builder to use a different FontImage in subsequent calls to
  // Build().
  void SetFontImage(const FontImagePtr& font_image) {
    font_image_ = font_image;
  }

  // Returns the Font from the FontImage. This may be a NULL pointer.
  const FontPtr GetFont() const {
    return font_image_.Get() ? font_image_->GetFont() : FontPtr();
  }

  // Builds an Ion Node representing the text string defined by a Layout, using
  // the FontImage passed to the constructor. The usage_mode is used for buffer
  // objects in the shape, allowing Ion to make certain optimizations.  (In
  // general, buffer data will be marked as wipeable if the usage_mode is
  // gfx::BufferObject::kStaticDraw.)  If anything goes wrong, this logs an
  // error and returns false. Otherwise, the node can be accessed with
  // GetNode(). If this is called more than once, the Builder will attempt to
  // reuse objects from the previous call, meaning that any changes to them made
  // from outside the Builder may persist.  The node will contain a StateTable
  // that disables face culling, enables alpha blending, sets the blend
  // equations to kAdd, and sets both blend functions to kOne,
  // kOneMinusSrcAlpha, so colors must be premultiplied by their alpha values.
  bool Build(const Layout& layout, gfx::BufferObject::UsageMode usage_mode);

  // Returns the Node set up by the last successful call to Build().
  const gfx::NodePtr& GetNode() const { return node_; }

  // Returns the canonical 3D extents of the last generated geometry.
  math::Range3f GetExtents() const { return text_extents_; }

 protected:
  // The constructor is protected because this is an abstract class. The
  // FontImage is used to set up the texture image and texture coordinates. The
  // ShaderManager, if it is not NULL, is used to create ShaderProgram
  // instances to enable remote editing of shader source. The Allocator is used
  // when building; if it is NULL, the default allocator is used.
  Builder(const FontImagePtr& font_image,
          const gfxutils::ShaderManagerPtr& shader_manager,
          const base::AllocatorPtr& allocator);

  // The destructor is protected because all base::Referent classes must have
  // protected or private destructors.
  ~Builder() override;

  // Returns the Allocator passed to the constructor. This should never be
  // NULL.
  const base::AllocatorPtr& GetAllocator() { return allocator_; }

  //----------------------------------------------------------------------------
  // Functions that derived classes must implement.

  // Returns the ShaderInputRegistry for the Builder's shaders.
  virtual const gfx::ShaderInputRegistryPtr GetShaderInputRegistry() = 0;

  // Returns the strings needed for shader definition.
  virtual void GetShaderStrings(std::string* id_string,
                                std::string* vertex_source,
                                std::string* fragment_source) = 0;

  // Adds or updates uniforms for the shaders in the node. The registry
  // returned by GetShaderInputRegistry() is passed in.
  virtual void UpdateUniforms(const gfx::ShaderInputRegistryPtr& registry,
                              gfx::Node* node) = 0;

  // Binds attributes for the Builder's shader program.
  virtual void BindAttributes(const gfx::AttributeArrayPtr& attr_array,
                              const gfx::BufferObjectPtr& buffer_object) = 0;

  // Returns a vector of data that represents vertex data, the size of a vertex,
  // the number of vertices, and the pixel size of the generated geometry.
  virtual base::AllocVector<char> BuildVertexData(const Layout& layout,
                                                  size_t* vertex_size,
                                                  size_t* num_vertices) = 0;

  //----------------------------------------------------------------------------
  // Convenience functions for derived classes.

  // Returns a pointer to the FontImage::ImageData that specifies an image
  // containing all glyphs necessary for representing the characters in the
  // Layout being built. This will be NULL if this is called when Build() is
  // not currently in operation.
  const FontImage::ImageData* GetImageData() const { return image_data_; }

  // Returns a Texture that contains the FontImage image. This returns a NULL
  // pointer if there is no valid FontImage.
  const gfx::TexturePtr GetFontImageTexture();

  // Modifies the indexed Texture uniform in the node if necessary to contain
  // the current FontImage image. Returns false if the node is NULL, the index
  // is invalid, or the indexed uniform is invalid or is of the wrong type.
  bool UpdateFontImageTextureUniform(size_t index, gfx::Node* node);

  // Fills in the position and texture_coords for the 4 vertices of the indexed
  // Layout glyph quad, using the current FontImage for texture coordinates.
  void StoreGlyphVertices(const Layout& layout, size_t glyph_index,
                          math::Point3f positions[4],
                          math::Point2f texture_coords[4]);

 private:
  // Builds and returns a ShaderProgram, calling virtual functions to set up
  // the various parts.
  const gfx::ShaderProgramPtr BuildShaderProgram();

  // Builds or updates an AttributeArray representing the vertices for the
  // Layout and stores it in the shape. Returns true if there was already a
  // BufferObject in the AttributeArray and it was reused for the new Layout.
  // Calls virtual functions to bind attributes and get vertex data.
  bool UpdateAttributeArray(
      const Layout& layout, gfx::BufferObject::UsageMode usage_mode,
      const gfx::AttributeArrayPtr& attr_array);

  // Updates the Shape with the correct data based on the Layout.
  void UpdateShape(const Layout& layout,
                   gfx::BufferObject::UsageMode usage_mode, gfx::Shape* shape);

  // FontImage passed to the constructor. It is used to set up the texture
  // image and texture coordinates.
  FontImagePtr font_image_;
  // ShaderManager used for creating shaders.
  gfxutils::ShaderManagerPtr shader_manager_;
  // Allocator used for building everything.
  base::AllocatorPtr allocator_;
  // Node resulting from the last call to Build().
  gfx::NodePtr node_;
  // ShaderInputRegistry returned by BuildShaderInputRegistry(). It is cached
  // here to ensure it has a lifetime beyond those of the Uniforms subclasses of
  // this create.
  gfx::ShaderInputRegistryPtr registry_;
  // Sampler used by FontImage texture uniform.
  gfx::SamplerPtr sampler_;
  // The extents of the last generated text geometry.
  math::Range3f text_extents_;
  // During a call to Build(), this caches the FontImage::ImageData that
  // specifies the image with the character glyphs. It is NULL all other times.
  const FontImage::ImageData* image_data_;
};

// Convenience typedef for shared pointer to a Builder.
using BuilderPtr = base::SharedPtr<Builder>;

}  // namespace text
}  // namespace ion

#endif  // ION_TEXT_BUILDER_H_
