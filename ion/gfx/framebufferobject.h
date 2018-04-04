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

#ifndef ION_GFX_FRAMEBUFFEROBJECT_H_
#define ION_GFX_FRAMEBUFFEROBJECT_H_

#include <functional>
#include <initializer_list>

#include "base/integral_types.h"
#include "base/macros.h"
#include "ion/base/referent.h"
#include "ion/gfx/cubemaptexture.h"
#include "ion/gfx/image.h"
#include "ion/gfx/resourceholder.h"
#include "ion/gfx/texture.h"

namespace ion {
namespace gfx {

// As of 2016, all mainstream GPUs support at most 8 color attachments.
static const size_t kColorAttachmentSlotCount = 8;

// A FramebufferObject describes an off-screen framebuffer that can be drawn to
// and read from like a regular framebuffer. While the FramebufferObject is
// active, nothing is drawn to the screen; all draw commands draw into its
// Attachments.
//
// An Attachment can be in one of three states, unbound, bound to a
// renderbuffer, or bound to a texture. If an attachment is unbound any data
// written into it is discarded. For example, if a FramebufferObject has no
// depth attachment then there is effectively no depth buffer.
//
// If an Attachment is bound to a renderbuffer, Ion will allocate an internal
// data store (the renderbuffer) on the graphics hardware. The only way to get
// the data back after a draw call is through a ReadPixels command, though
// rendering may (depending on the platform) be faster than with a texture
// binding.
//
// If an Attachment is bound to a texture, then the passed TexturePtr is used
// as the target for all draw commands sent to the Attachment. The Texture must
// contain an Image (which may have NULL data) to specify the format of the
// framebuffer. The Texture can then be used, for example, as a Uniform input to
// a shader (a sampler). Note that not all platforms support binding textures to
// depth and stencil Attachments. If a particular texture format is unsupported,
// then the Attachment will be created as unbound.
class ION_API FramebufferObject : public ResourceHolder {
 public:
  // Changes that affect the resource.
  enum Changes {
    kColorAttachmentChanged = kNumBaseChanges,
    kDepthAttachmentChanged =
        kColorAttachmentChanged + kColorAttachmentSlotCount,
    kDimensionsChanged,
    kDrawBuffersChanged,
    kReadBufferChanged,
    kStencilAttachmentChanged,
    kNumChanges
  };

  // The type of binding for an Attachment.
  enum AttachmentBinding {
    kCubeMapTexture,
    kMultiview,
    kRenderbuffer,
    kTexture,
    kTextureLayer,
    kUnbound,
  };

  // An attachment represents a data store attached to one of the framebuffer's
  // targets. The AttachmentBinding of the attachment indicates its type,
  // unbound, renderbuffer, or texture. See the class comment for a description
  // of these different states.
  class Attachment {
   public:
    // Creates an unbound Attachment.
    Attachment();
    // Creates a renderbuffer Attachment of the specified format. The format
    // must be a supported format, or the Attachment will be set to an unbound
    // binding.
    explicit Attachment(Image::Format format_in);
    // Creates a texture Attachment using the passed TexturePtr. Note that the
    // Texture will be resized to match the FramebufferObject's dimensions, but
    // must contain an Image to specify the format to use. The format must be a
    // supported type for the current platform, or the Attachment will be set to
    // an unbound binding.
    explicit Attachment(const TexturePtr& texture_in, uint32 mip_level = 0U);
    // Similar to the constructor for a Texture, but uses the passed face of the
    // cubemap as the backing store of the attachment.
    Attachment(const CubeMapTexturePtr& cubemap_in,
               CubeMapTexture::CubeFace face, uint32 mip_level = 0U);

    // Creates a renderbuffer Attachment from the passed image, which must be of
    // type Image::kEgl or Image::kExternalEgl with format Image::kEglImage,
    // otherwise the Attachment will be set to an unbound binding.
    static Attachment CreateFromEglImage(const ImagePtr& image_in);
    // Creates an Attachment from a single layer of the passed texture. The
    // texture must have a three-dimensional image.
    static Attachment CreateFromLayer(const TexturePtr& texture_in,
                                      uint32 layer, uint32 mip_level = 0U);
    // Creates a renderbuffer attachment for multisampling.
    static Attachment CreateMultisampled(Image::Format format, uint32 samples);
    // Creates a multisampled attachment from a regular (non-multisampled)
    // texture. Requires support for the feature kImplicitMultisample
    // (OpenGL ES extension EXT_multisampled_render_to_texture). If you use this
    // type of attachment, all other attachments must be either implicitly
    // multisampled textures or multisampled renderbuffers. It is not permitted
    // to mix implicitly and explicitly multisampled texture attachments.
    static Attachment CreateImplicitlyMultisampled(const TexturePtr& texture_in,
                                                   uint32 samples,
                                                   uint32 mip_level = 0U);
    // Creates a multisampled attachment from a regular (non-multisampled) cube
    // map texture.
    static Attachment CreateImplicitlyMultisampled(
        const CubeMapTexturePtr& cube_map_in, CubeMapTexture::CubeFace face,
        uint32 samples, uint32 mip_level = 0U);
    // Creates a multiview attachment from an array texture. This will only work
    // if the kMultiview feature is available; otherwise, an error will be
    // reported when the framebuffer object is bound. num_views specifies the
    // number of views, which must be lower than the value of the kMaxViews
    // capability in GraphicsManager, while base_view_index specifies the offset
    // of the layer used as the output for the first view. The texture must have
    // at least base_view_index + num_views layers. At rendering time, the
    // vertex shader will be run num_views times for each vertex, each time with
    // a different value of the built-in variable gl_ViewID_OVR. Vertex shaders
    // must contain the following GLSL declaration:
    //
    //     layout(num_views=N) in;
    //
    // where N is the number of views that will be used by the shader. The
    // gl_ViewID_OVR variable is usually used to index into uniform arrays that
    // contain view-specific information.
    static Attachment CreateMultiview(const TexturePtr& texture_in,
                                      uint32 base_view_index, uint32 num_views,
                                      uint32 mip_level = 0U);
    // Creates an implicitly multisampled multiview attachment from an array
    // texture.
    static Attachment CreateImplicitlyMultisampledMultiview(
        const TexturePtr& texture_in, uint32 base_view_index, uint32 num_views,
        uint32 samples, uint32 mip_level = 0U);

    // Gets the format of the attachment, which is the texture format if it is a
    // texture attachment.
    Image::Format GetFormat() const;
    // Gets the binding of the attachment.
    AttachmentBinding GetBinding() const { return binding_; }
    // Gets the image of the attachment, if any.
    const ImagePtr& GetImage() const { return image_; }
    // Gets the texture of the attachment, if any.
    const TexturePtr& GetTexture() const { return texture_; }
    // Gets the cubemap of the attachment, if any.
    const CubeMapTexturePtr& GetCubeMapTexture() const { return cubemap_; }
    // Gets the cubemap face of the attachment.
    CubeMapTexture::CubeFace GetCubeMapFace() const { return face_; }
    // Gets the target layer of a texture layer attachment. This will be zero
    // for multiview attachments.
    uint32 GetLayer() const { return binding_ == kTextureLayer ? layer_ : 0U; }
    // Returns the mipmap level of a Texture or CubeMapTexture attachment.
    uint32 GetMipLevel() const { return mip_level_; }
    // Returns the number of samples for multisampling.
    uint32 GetSamples() const;
    // Returns the number of views for a multiview attachment and zero for
    // non-multiview attachments.
    uint32 GetNumViews() const { return num_views_; }
    // Returns the index of the texture layer where the first view will be
    // stored. For non-multiview attachments, this will always be zero.
    uint32 GetBaseViewIndex() const {
      return binding_ == kMultiview ? layer_ : 0;
    }
    // Checks whether the attachment is compatible with implicit multisampling
    // (EXT_multisampled_render_to_texture). This is true when the attachment
    // is an implicitly multisampled texture attachment or a renderbuffer.
    bool IsImplicitMultisamplingCompatible() const;

    // Needed for Field::Set().
    inline bool operator==(const Attachment& other) const {
      return binding_ == other.binding_ && format_ == other.format_ &&
             texture_.Get() == other.texture_.Get() &&
             image_.Get() == other.image_.Get() &&
             cubemap_.Get() == other.cubemap_.Get() && layer_ == other.layer_ &&
             mip_level_ == other.mip_level_ && samples_ == other.samples_;
    }
    inline bool operator !=(const Attachment& other) const {
      return !(*this == other);
    }

   private:
    void Construct(AttachmentBinding binding,
                   uint32 mip_level,
                   CubeMapTexture::CubeFace face);
    AttachmentBinding binding_;
    CubeMapTexture::CubeFace face_;
    CubeMapTexturePtr cubemap_;
    ImagePtr image_;
    TexturePtr texture_;
    Image::Format format_;
    // Target texture layer for layer attachments and the index of the first
    // layer of output for multiview attachments.
    uint32 layer_;
    uint32 num_views_;
    uint32 mip_level_;
    uint32 samples_;
  };

  // Creates a FramebufferObject with the passed dimensions and unbound
  // attachments.
  FramebufferObject(uint32 width, uint32 height);

  // Resizes the FramebufferObject to the passed dimensions.
  void Resize(uint32 width, uint32 height);
  // Gets the width of the FramebufferObject and its attachments.
  uint32 GetWidth() const { return width_.Get(); }
  // Gets the height of the FramebufferObject and its attachments.
  uint32 GetHeight() const { return height_.Get(); }

  // Gets and sets the i-th color Attachment.
  const Attachment& GetColorAttachment(size_t i) const {
    return color_.Get(i);
  }
  void SetColorAttachment(size_t i, const Attachment& color);
  // Gets and sets the depth Attachment.
  const Attachment& GetDepthAttachment() const { return depth_.Get(); }
  void SetDepthAttachment(const Attachment& depth);
  // Gets and sets the stencil Attachment.
  const Attachment& GetStencilAttachment() const {
    return stencil_.Get();
  }
  void SetStencilAttachment(const Attachment& stencil);

  // Gets and sets the destination of a single shader output. |index| specifies
  // the index of the shader output, while |buffer| specifies the index of the
  // color attachment to which that output will be written. Note that it is an
  // error to write more than one shader output to a single attachment. The
  // value -1 indicates that the shader output should be discarded (GL_NONE).
  int32 GetDrawBuffer(size_t index) const;
  void SetDrawBuffer(size_t index, int32 buffer);

  // Sets the mapping between shader outputs and color attachments for this
  // framebuffer object. The default is to put the zeroth shader output in the
  // zeroth color attachment and ignore everything else.
  void SetDrawBuffers(const base::AllocVector<int32>& buffers);
  void SetDrawBuffers(const std::initializer_list<int32>& buffers);
  // Reverts draw buffers to the default, which is to write the i-th draw buffer
  // into the i-th attachment, as long as it's bound. For example, a framebuffer
  // with renderbuffers bound to color attachments 2 and 3 and all others
  // unbound will write draw buffers 2 and 3 into color attachments 2 and 3,
  // respectively, and discard all others.
  void ResetDrawBuffers();

  // Gets and sets the color attachment that should be used for reading pixels
  // from this framebuffer object. The value -1 indicates that the read buffer
  // is not set (GL_NONE), and reading from the framebuffer will fail.
  int32 GetReadBuffer() const;
  void SetReadBuffer(int32 buffer) { read_buffer_.Set(buffer); }
  // Reset the read buffer to the default, which is to read from the lowest
  // numbered bound attachment, or GL_NONE if there are no color attachments.
  void ResetReadBuffer();

  // Calls the specified function for each attachment slot. The first parameter
  // is a reference the attachment, while the second is the change bit
  // corresponding to the attachment. Note that the function will also be called
  // for attachments which are not bound.
  void ForEachAttachment(
      const std::function<void(const Attachment&, int)>& function) const;

  // Returns whether the passed GL formats are renderable.
  static bool IsColorRenderable(uint32 format);
  static bool IsDepthRenderable(uint32 format);
  static bool IsStencilRenderable(uint32 format);

 protected:
  // The destructor is protected because all base::Referent classes must have
  // protected or private destructors.
  ~FramebufferObject() override;

 private:
  // Called when a Texture or CubeMapTexture that this depends on changes.
  void OnNotify(const base::Notifier* notifier) override;

  // Sets the passed field Attachment to the passed attachment and updates
  // notification settings. The passed validator is used to ensure the
  // attachment has a valid format; if it does not, an error is logged and field
  // is not set.
  void SetAttachment(Field<Attachment>* field, bool (*validator)(Image::Format),
                     const Attachment& attachment,
                     const std::string& type_name);

  void SetColorAttachment(VectorField<Attachment>* field, size_t index,
                          const Attachment& attachment);

  // Sets the draw buffers from the passed iterator range. Draw buffers not
  // specified by the range will be set to -1 (equivalent to GL_NONE).
  template <typename Iterator>
  void SetDrawBuffers(Iterator first, Iterator last);

  Field<uint32> width_;
  Field<uint32> height_;
  VectorField<Attachment> color_;
  Field<Attachment> depth_;
  Field<Attachment> stencil_;
  Field<base::AllocVector<int32>> draw_buffers_;
  Field<int32> read_buffer_;
  bool use_default_draw_buffers_;

  DISALLOW_COPY_AND_ASSIGN(FramebufferObject);
};

// Convenience typedef for shared pointer to a FramebufferObject.
using FramebufferObjectPtr = base::SharedPtr<FramebufferObject>;

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_FRAMEBUFFEROBJECT_H_
