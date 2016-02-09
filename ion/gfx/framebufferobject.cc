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

#include "ion/gfx/framebufferobject.h"

#include "ion/base/invalid.h"
#include "ion/base/static_assert.h"
#include "ion/gfx/image.h"
#include "ion/portgfx/glheaders.h"

namespace ion {
namespace gfx {

namespace {

static bool IsValid(Image::Format format) {
  return static_cast<uint32>(format) < Image::kNumFormats;
}

static bool IsColorFormatRenderable(Image::Format format) {
  return IsValid(format)
             ? FramebufferObject::IsColorRenderable(
                   Image::GetPixelFormat(format).internal_format) ||
                   format == Image::kEglImage
             : false;
}

static bool IsDepthFormatRenderable(Image::Format format) {
  return IsValid(format)
             ? FramebufferObject::IsDepthRenderable(
                   Image::GetPixelFormat(format).internal_format) ||
                   format == Image::kEglImage
             : false;
}

static bool IsStencilFormatRenderable(Image::Format format) {
  return IsValid(format)
             ? FramebufferObject::IsStencilRenderable(
                   Image::GetPixelFormat(format).internal_format) ||
                   format == Image::kEglImage
             : false;
}

// Returns whether the passed notifer is one of the objects in the passed
// Attachment.
static bool IsAttachmentNotifier(
    const base::Notifier* notifier,
    const FramebufferObject::Attachment& attachment) {
  return notifier == attachment.GetTexture().Get() ||
      notifier == attachment.GetCubeMapTexture().Get() ||
      notifier == attachment.GetImage().Get();
}

static CubeMapTexture::CubeFace kInvalidFace =
    static_cast<CubeMapTexture::CubeFace>(base::kInvalidIndex);

}  // anonymous namespace

ION_API FramebufferObject::Attachment::Attachment() {
  Construct(kUnbound, 0, kInvalidFace);
}

ION_API FramebufferObject::Attachment::Attachment(
    Image::Format format, size_t samples) {
  Construct(kRenderbuffer, 0, kInvalidFace);
  format_ = format;
  samples_ = samples;
}

ION_API FramebufferObject::Attachment::Attachment(Image::Format format_in) {
  Construct(kRenderbuffer, 0, kInvalidFace);
  format_ = format_in;
}

ION_API FramebufferObject::Attachment::Attachment(
    const ImagePtr& image_in) : image_(image_in) {
  Construct(image_.Get() && image_->GetFormat() == Image::kEglImage &&
                    (image_->GetType() == Image::kEgl ||
                     image_->GetType() == Image::kExternalEgl)
                ? kRenderbuffer
                : kUnbound,
            0, kInvalidFace);
}

ION_API FramebufferObject::Attachment::Attachment(
    const TexturePtr& texture_in) : texture_(texture_in) {
  Construct(texture_.Get() ? kTexture : kUnbound, 0, kInvalidFace);
}

ION_API FramebufferObject::Attachment::Attachment(
    const TexturePtr& texture_in, size_t mip_level)
    : texture_(texture_in) {
  Construct(texture_.Get() ? kTexture : kUnbound, mip_level, kInvalidFace);
}

ION_API FramebufferObject::Attachment::Attachment(
    const CubeMapTexturePtr& cubemap_in, CubeMapTexture::CubeFace face)
    : cubemap_(cubemap_in) {
  Construct(cubemap_.Get() ? kCubeMapTexture : kUnbound, 0, face);
}

ION_API FramebufferObject::Attachment::Attachment(
    const CubeMapTexturePtr& cubemap_in,
    CubeMapTexture::CubeFace face,
    size_t mip_level) : cubemap_(cubemap_in) {
  Construct(cubemap_.Get() ? kCubeMapTexture : kUnbound, mip_level, face);
}

ION_API void FramebufferObject::Attachment::Construct(
    AttachmentBinding binding,
    size_t mip_level,
    CubeMapTexture::CubeFace face) {
  binding_ = binding;
  mip_level_ = mip_level;
  face_ = face;
  format_ = static_cast<Image::Format>(base::kInvalidIndex);
  samples_ = 0;
}

ION_API Image::Format FramebufferObject::Attachment::GetFormat() const {
  Image::Format format = format_;
  if (Texture* tex = texture_.Get()) {
    if (tex->HasImage(0))
      format = tex->GetImage(0)->GetFormat();
    else
      format = Image::kRgba8888;
  } else if (CubeMapTexture* tex = cubemap_.Get()) {
    if (tex->HasImage(face_, 0))
      format = tex->GetImage(face_, 0)->GetFormat();
    else
      format = Image::kRgba8888;
  } else if (image_.Get()) {
    format = image_->GetFormat();
  }
  return format;
}

FramebufferObject::FramebufferObject(uint32 width, uint32 height)
    : width_(kDimensionsChanged, width, this),
      height_(kDimensionsChanged, height, this),
      color0_(kColorAttachmentChanged, Attachment(), this),
      depth_(kDepthAttachmentChanged, Attachment(), this),
      stencil_(kStencilAttachmentChanged, Attachment(), this) {
  if (width_.Get() == 0 || height_.Get() == 0)
    LOG(ERROR) << "Framebuffer created with zero width or height; it will be"
               << " ignored if used for rendering.";
}

FramebufferObject::~FramebufferObject() {
  SetAttachment(&color0_, IsColorFormatRenderable, Attachment(), "color");
  SetAttachment(&depth_, IsDepthFormatRenderable, Attachment(), "depth");
  SetAttachment(&stencil_, IsStencilFormatRenderable, Attachment(), "stencil");
}

void FramebufferObject::Resize(uint32 width, uint32 height) {
  width_.Set(width);
  height_.Set(height);
}

void FramebufferObject::SetColorAttachment(size_t i, const Attachment& color) {
  DCHECK_EQ(0U, i) << "***ION: Only a single color attachment is supported";
  SetAttachment(&color0_, IsColorFormatRenderable, color, "color");
}

void FramebufferObject::SetDepthAttachment(const Attachment& depth) {
  SetAttachment(&depth_, IsDepthFormatRenderable, depth, "depth");
}

void FramebufferObject::SetStencilAttachment(const Attachment& stencil) {
  SetAttachment(&stencil_, IsStencilFormatRenderable, stencil, "stencil");
}

bool FramebufferObject::IsColorRenderable(uint32 format) {
  bool renderable = false;
  switch (format) {
    case GL_RGB16F: case GL_RGB32F: case GL_RGBA16F: case GL_RGBA32F:
    case GL_RGB: case GL_RGBA: case GL_R8: case GL_R8UI: case GL_R8I:
    case GL_R16UI: case GL_R16I: case GL_R32UI: case GL_R32I: case GL_RG8:
    case GL_RG8UI: case GL_RG8I: case GL_RG16UI: case GL_RG16I:
    case GL_RG32UI: case GL_RG32I: case GL_RGB8: case GL_RGB565:
    case GL_RGB5_A1: case GL_RGBA4: case GL_RGB10_A2: case GL_RGB10_A2UI:
    case GL_RGBA8: case GL_SRGB8_ALPHA8: case GL_RGBA8UI: case GL_RGBA8I:
    case GL_RGBA16UI: case GL_RGBA16I: case GL_RGBA32I: case GL_RGBA32UI:
      renderable = true;
      break;

    default:
      break;
  }
  return renderable;
}

bool FramebufferObject::IsDepthRenderable(uint32 format) {
  return format == GL_DEPTH_COMPONENT || format == GL_DEPTH_COMPONENT16 ||
         format == GL_DEPTH_COMPONENT24 || format == GL_DEPTH_COMPONENT32F ||
         format == GL_DEPTH24_STENCIL8 || format == GL_DEPTH32F_STENCIL8;
}

bool FramebufferObject::IsStencilRenderable(uint32 format) {
  return format == GL_DEPTH24_STENCIL8 || format == GL_DEPTH32F_STENCIL8 ||
         format == GL_STENCIL_INDEX8;
}

void FramebufferObject::OnNotify(const base::Notifier* notifier) {
  if (GetResourceCount()) {
    if (IsAttachmentNotifier(notifier, color0_.Get())) {
      OnChanged(kColorAttachmentChanged);
    } else if (IsAttachmentNotifier(notifier, depth_.Get())) {
      OnChanged(kDepthAttachmentChanged);
    } else if (IsAttachmentNotifier(notifier, stencil_.Get())) {
      OnChanged(kStencilAttachmentChanged);
    }
  }
}

void FramebufferObject::SetAttachment(Field<Attachment>* field,
                                      bool (*validator)(Image::Format),
                                      const Attachment& attachment,
                                      const std::string& type_name) {
  // Update notifications.
  if (Texture* tex = field->Get().GetTexture().Get())
    tex->RemoveReceiver(this);
  else if (CubeMapTexture* tex = field->Get().GetCubeMapTexture().Get())
    tex->RemoveReceiver(this);
  else if (Image* img = field->Get().GetImage().Get())
    img->RemoveReceiver(this);

  if (attachment.GetBinding() != kUnbound &&
      !validator(attachment.GetFormat())) {
    LOG(ERROR) << "Invalid " << type_name << " attachment format "
               << Image::GetFormatString(attachment.GetFormat());
    field->Set(Attachment());
  } else {
    if (Texture* tex = attachment.GetTexture().Get())
      tex->AddReceiver(this);
    else if (CubeMapTexture* tex = attachment.GetCubeMapTexture().Get())
      tex->AddReceiver(this);
    else if (Image* img = attachment.GetImage().Get())
      img->AddReceiver(this);
    field->Set(attachment);
  }
}

}  // namespace gfx
}  // namespace ion
