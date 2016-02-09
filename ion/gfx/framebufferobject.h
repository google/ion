/**
Copyright 2016 Google Inc. All Rights Reserved.

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

#include "base/macros.h"
#include "ion/base/referent.h"
#include "ion/gfx/cubemaptexture.h"
#include "ion/gfx/image.h"
#include "ion/gfx/resourceholder.h"
#include "ion/gfx/texture.h"

namespace ion {
namespace gfx {

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
    kDepthAttachmentChanged,
    kDimensionsChanged,
    kStencilAttachmentChanged,
    kNumChanges
  };

  // The type of binding for an Attachment.
  enum AttachmentBinding {
    kCubeMapTexture,
    kRenderbuffer,
    kTexture,
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
    // Creates a render buffer attachment for multisampling.
    Attachment(Image::Format format, size_t samples);
    // Creates a renderbuffer Attachment of the specified format. The format
    // must be a supported format, or the Attachment will be set to an unbound
    // binding.
    explicit Attachment(Image::Format format_in);
    // Creates a renderbuffer Attachment from the passed image, which must be of
    // type Image::kEgl or Image::kExternalEgl with format Image::kEglImage,
    // otherwise the Attachment will be set to an unbound binding.
    explicit Attachment(const ImagePtr& image_in);
    // Creates a texture Attachment using the passed TexturePtr. Note that the
    // Texture will be resized to match the FramebufferObject's dimensions, but
    // must contain an Image to specify the format to use. The format must be a
    // supported type for the current platform, or the Attachment will be set to
    // an unbound binding.
    explicit Attachment(const TexturePtr& texture_in);
    // As above, but with specified mipmap level of the texture.
    Attachment(const TexturePtr& texture_in, size_t mip_level);
    // Similar to the constructor for a Texture, but uses the passed face of the
    // cubemap as the backing store of the attachment.
    Attachment(const CubeMapTexturePtr& texture_in,
               CubeMapTexture::CubeFace face);
    // As above, but with specified mipmap level of the cubemap texture.
    Attachment(const CubeMapTexturePtr& texture_in,
               CubeMapTexture::CubeFace face,
               size_t mip_level);

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
    // Returns the mipmap level of a Texture or CubeMapTexture attachment.
    size_t GetMipLevel() const { return mip_level_; }
    // Returns the number of samples for multisampling.
    size_t GetSamples() const { return samples_; }

    // Needed for Field::Set().
    inline bool operator !=(const Attachment& other) const {
      return binding_ != other.binding_ || format_ != other.format_ ||
             texture_.Get() != other.texture_.Get() ||
             image_.Get() != other.image_.Get() ||
             cubemap_.Get() != other.cubemap_.Get() ||
             mip_level_ != other.mip_level_ || samples_ != other.samples_;
    }

   private:
    void Construct(AttachmentBinding binding,
                   size_t mip_level,
                   CubeMapTexture::CubeFace face);
    AttachmentBinding binding_;
    CubeMapTexture::CubeFace face_;
    CubeMapTexturePtr cubemap_;
    ImagePtr image_;
    TexturePtr texture_;
    Image::Format format_;
    size_t mip_level_;
    size_t samples_;
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

  // Gets and sets the i-th color Attachment. For now, only a single color
  // attachment is supported.
  // TODO(user): Support multiple color attachments.
  const Attachment& GetColorAttachment(size_t i) const {
    DCHECK_EQ(0U, i) << "***ION: Only a single color attachment is supported";
    return color0_.Get();
  }
  void SetColorAttachment(size_t i, const Attachment& attachment);
  // Gets and sets the depth Attachment.
  const Attachment& GetDepthAttachment() const { return depth_.Get(); }
  void SetDepthAttachment(const Attachment& attachment);
  // Gets and sets the stencil Attachment.
  const Attachment& GetStencilAttachment() const {
    return stencil_.Get();
  }
  void SetStencilAttachment(const Attachment& attachment);

  // Returns whether the passed GL formats are renderable.
  static bool IsColorRenderable(uint32 gl_format);
  static bool IsDepthRenderable(uint32 gl_format);
  static bool IsStencilRenderable(uint32 gl_format);

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

  Field<uint32> width_;
  Field<uint32> height_;
  Field<Attachment> color0_;
  Field<Attachment> depth_;
  Field<Attachment> stencil_;

  DISALLOW_COPY_AND_ASSIGN(FramebufferObject);
};

// Convenience typedef for shared pointer to a FramebufferObject.
typedef base::ReferentPtr<FramebufferObject>::Type FramebufferObjectPtr;

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_FRAMEBUFFEROBJECT_H_
