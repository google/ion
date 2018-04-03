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

#ifndef ION_TEXT_OUTLINEBUILDER_H_
#define ION_TEXT_OUTLINEBUILDER_H_

#include "ion/base/stlalloc/allocvector.h"
#include "ion/math/vector.h"
#include "ion/text/builder.h"

namespace ion {
namespace text {

// OutlineBuilder is a derived Builder class that can render text with outlines.
//
// The Node returned by Builder::BuildNode() contains the following uniforms:
//   uSdfPadding       [float, derived from Font]
//     Number of pixels used to pad SDF images.
//   uSdfSampler       [sampler2D, derived from FontImage]
//     Sampler for the SDF texture.
//   uTextColor:       [VectorBase4f, default (1,1,1,1)]
//     Foreground color of the text.
//   uOutlineColor:    [VectorBase4f, default (0,0,0,0)]
//     Color of the text outline.
//   uOutlineWidth:    [float, default 2]
//     Outline width in font pixels, where 0 means no outlines.
//   uHalfSmoothWidth: [float, default 3]
//     Half the number of pixels over which edges are smoothed on each side of
//     outlines for antialiasing.
//
// The shaders in the returned node require the global registry's
// uViewportSize, uProjectionMatrix, and uModelviewMatrix uniforms to be set to
// the proper values.
class ION_API OutlineBuilder : public Builder {
 public:
  OutlineBuilder(const FontImagePtr& font_image,
                 const gfxutils::ShaderManagerPtr& shader_manager,
                 const base::AllocatorPtr& allocator);

  // These convenience functions can be used to modify uniform values in the
  // built Node returned by GetNode(). Each returns false if the Node is NULL
  // or the uniform does not exist in it.
  bool SetSdfPadding(float padding);
  bool SetTextColor(const math::VectorBase4f& color);
  bool SetOutlineColor(const math::VectorBase4f& color);
  bool SetOutlineWidth(float width);
  bool SetHalfSmoothWidth(float width);

 protected:
  // The destructor is protected because all base::Referent classes must have
  // protected or private destructors.
  ~OutlineBuilder() override;

  // Required Builder functions.
  const gfx::ShaderInputRegistryPtr GetShaderInputRegistry() override;
  void GetShaderStrings(std::string* id_string,
                        std::string* vertex_source,
                        std::string* fragment_source) override;
  void UpdateUniforms(const gfx::ShaderInputRegistryPtr& registry,
                      gfx::Node* node) override;
  void BindAttributes(const gfx::AttributeArrayPtr& attr_array,
                      const gfx::BufferObjectPtr& buffer_object) override;
  base::AllocVector<char> BuildVertexData(const Layout& layout,
                                          size_t* vertex_size,
                                          size_t* num_vertices) override;

 private:
  // A Vertex in the AttributeArray for the text.
  struct Vertex {
    Vertex() {}
    Vertex(const math::Point3f& position_in,
           const math::Point2f& texture_coords_in,
           const math::Vector3f& font_pixel_vec_in)
        : position(position_in),
          texture_coords(texture_coords_in),
          font_pixel_vec(font_pixel_vec_in) {}
    math::Point3f position;
    math::Point2f texture_coords;
    // Vector from the bottom-left to top-right corner of a font pixel. This
    // allows the shaders to convert from font pixels to screen pixels. This is
    // constant for most (flat) text layouts, but could vary for other layouts.
    math::Vector3f font_pixel_vec;
  };
};

// Convenience typedef for shared pointer to an OutlineBuilder.
using OutlineBuilderPtr = base::SharedPtr<OutlineBuilder>;

}  // namespace text
}  // namespace ion

#endif  // ION_TEXT_OUTLINEBUILDER_H_
