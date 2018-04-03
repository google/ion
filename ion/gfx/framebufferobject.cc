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

#include "ion/gfx/framebufferobject.h"

#include <limits>

#include "ion/base/invalid.h"
#include "ion/base/static_assert.h"
#include "ion/gfx/image.h"
#include "ion/portgfx/glheaders.h"

namespace ion {
namespace gfx {

namespace {

static const int32 kDefaultBufferNumber = std::numeric_limits<int32>::min();

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
  if (attachment.GetBinding() == FramebufferObject::kUnbound) return false;
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

ION_API FramebufferObject::Attachment::Attachment(Image::Format format_in) {
  Construct(kRenderbuffer, 0, kInvalidFace);
  format_ = format_in;
}

ION_API FramebufferObject::Attachment::Attachment(const TexturePtr& texture_in,
                                                  uint32 mip_level)
    : texture_(texture_in) {
  DCHECK(texture_.Get());
  DCHECK(texture_->GetImageCount()) << "Texture " << texture_->GetLabel()
      << " has no image";
  Construct(kTexture, mip_level, kInvalidFace);
}

ION_API FramebufferObject::Attachment::Attachment(
    const CubeMapTexturePtr& cubemap_in,
    CubeMapTexture::CubeFace face,
    uint32 mip_level) : cubemap_(cubemap_in) {
  DCHECK(cubemap_.Get());
  DCHECK(cubemap_->GetImageCount(face)) << "Cube map "
      << cubemap_->GetLabel() << " has no image";
  Construct(kCubeMapTexture, mip_level, face);
}

ION_API FramebufferObject::Attachment
FramebufferObject::Attachment::CreateFromEglImage(const ImagePtr& image_in) {
  DCHECK(image_in.Get() && image_in->GetFormat() == Image::kEglImage &&
         (image_in->GetType() == Image::kEgl ||
          image_in->GetType() == Image::kExternalEgl))
      << "Attachment::CreateFromEglImage(): passed image is "
      << (image_in.Get() ? "not an EGL image" : "null");
  Attachment result;
  result.image_ = image_in;
  result.binding_ = kRenderbuffer;
  return result;
}

ION_API FramebufferObject::Attachment
FramebufferObject::Attachment::CreateFromLayer(const TexturePtr& texture_in,
                                               uint32 layer, uint32 mip_level) {
  Attachment result(texture_in, mip_level);
  const uint32 texture_depth = texture_in->GetImage(mip_level)->GetDepth();
  DCHECK_GT(texture_depth, layer) << "Layer out of bounds: layer " << layer
      << " in texture " << texture_in->GetLabel() << ", which has "
      << texture_depth << " layers";
  result.binding_ = kTextureLayer;
  result.layer_ = layer;
  return result;
}

ION_API FramebufferObject::Attachment
FramebufferObject::Attachment::CreateMultisampled(Image::Format format,
                                                  uint32 samples) {
  Attachment result(format);
  result.samples_ = samples;
  return result;
}

ION_API FramebufferObject::Attachment
FramebufferObject::Attachment::CreateImplicitlyMultisampled(
    const TexturePtr& texture_in, uint32 samples, uint32 mip_level) {
  Attachment result(texture_in, mip_level);
  DCHECK_EQ(0, texture_in->GetMultisampleSamples())
      << "Cannot create an implicitly multisampled attachment from "
         "an explicitly multisampled texture " << texture_in->GetLabel();
  result.samples_ = samples;
  return result;
}

ION_API FramebufferObject::Attachment
FramebufferObject::Attachment::CreateImplicitlyMultisampled(
    const CubeMapTexturePtr& cube_map_in, CubeMapTexture::CubeFace face,
    uint32 samples, uint32 mip_level) {
  Attachment result(cube_map_in, face, mip_level);
  result.samples_ = samples;
  return result;
}

ION_API FramebufferObject::Attachment
FramebufferObject::Attachment::CreateMultiview(const TexturePtr& texture_in,
                                               uint32 base_view_index,
                                               uint32 num_views,
                                               uint32 mip_level) {
  Attachment result(texture_in, mip_level);
  DCHECK_LT(0U, num_views) << "Multiview attachment cannot have zero views";
  const ImagePtr& img = texture_in->GetImage(mip_level);
  DCHECK(img) << "Multiview texture must have image";
  DCHECK_EQ(Image::k3d, img->GetDimensions())
    << "Multiview image must be an array";
  // Make sure that the requested sequence of layers is within bounds, unless
  // this is an EglImage that does not have width/height/depth metadata.
  if (img->GetFormat() != Image::kEglImage) {
    const uint32 texture_depth = img->GetDepth();
    if (base_view_index + num_views > texture_depth) {
      LOG_PROD(DFATAL) << "Multiview layer out of bounds: " << num_views
                       << " views starting "
                       << "at layer " << base_view_index << " in texture "
                       << texture_in->GetLabel() << ", which has "
                       << texture_depth << " layers";
    }
  }
  result.binding_ = kMultiview;
  result.layer_ = base_view_index;
  result.num_views_ = num_views;
  return result;
}

ION_API FramebufferObject::Attachment
FramebufferObject::Attachment::CreateImplicitlyMultisampledMultiview(
    const TexturePtr& texture_in, uint32 base_view_index, uint32 num_views,
    uint32 samples, uint32 mip_level) {
  Attachment result = CreateMultiview(texture_in, base_view_index, num_views,
                                      mip_level);
  result.samples_ = samples;
  return result;
}

uint32 FramebufferObject::Attachment::GetSamples() const {
  if (texture_.Get()) {
    const int tex_samples = texture_->GetMultisampleSamples();
    if (tex_samples > 0) return tex_samples;
  }
  return samples_;
}

bool FramebufferObject::Attachment::IsImplicitMultisamplingCompatible() const {
  if (binding_ == kTexture || binding_ == kMultiview) {
    return texture_.Get() && texture_->GetMultisampleSamples() == 0 &&
        samples_ > 0;
  }
  if (binding_ == kCubeMapTexture || binding_ == kRenderbuffer) {
    return samples_ > 0;
  }
  if (binding_ == kUnbound) return true;
  return false;
}

ION_API void FramebufferObject::Attachment::Construct(
    AttachmentBinding binding,
    uint32 mip_level,
    CubeMapTexture::CubeFace face) {
  binding_ = binding;
  layer_ = 0;
  num_views_ = 0;
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
      color_(kColorAttachmentChanged, kColorAttachmentSlotCount, this),
      depth_(kDepthAttachmentChanged, Attachment(), this),
      stencil_(kStencilAttachmentChanged, Attachment(), this),
      draw_buffers_(kDrawBuffersChanged, base::AllocVector<int32>(*this), this),
      read_buffer_(kReadBufferChanged, kDefaultBufferNumber, this),
      use_default_draw_buffers_(true) {
  if (width_.Get() == 0 || height_.Get() == 0)
    LOG(ERROR) << "Framebuffer created with zero width or height; it will be"
               << " ignored if used for rendering.";
  auto buffers = draw_buffers_.GetMutable();
  buffers->resize(kColorAttachmentSlotCount, -1);

  for (size_t i = 0; i < kColorAttachmentSlotCount; ++i) {
    color_.Add(Attachment());
  }
}

FramebufferObject::~FramebufferObject() {
  for (size_t i = 0; i < kColorAttachmentSlotCount; ++i) {
    SetColorAttachment(&color_, i, Attachment());
  }
  SetAttachment(&depth_, IsDepthFormatRenderable, Attachment(), "depth");
  SetAttachment(&stencil_, IsStencilFormatRenderable, Attachment(), "stencil");
}

void FramebufferObject::Resize(uint32 width, uint32 height) {
  width_.Set(width);
  height_.Set(height);
}

void FramebufferObject::SetColorAttachment(size_t i, const Attachment& color) {
  SetColorAttachment(&color_, i, color);
}

void FramebufferObject::SetDepthAttachment(const Attachment& depth) {
  SetAttachment(&depth_, IsDepthFormatRenderable, depth, "depth");
}

void FramebufferObject::SetStencilAttachment(const Attachment& stencil) {
  SetAttachment(&stencil_, IsStencilFormatRenderable, stencil, "stencil");
}

void FramebufferObject::SetDrawBuffer(size_t index, int32 buffer) {
  DCHECK_EQ(kColorAttachmentSlotCount, draw_buffers_.Get().size());

  // If using default draw buffers, write those defaults to the internal vector.
  if (use_default_draw_buffers_) {
    for (size_t i = 0; i < kColorAttachmentSlotCount; ++i)
      (*draw_buffers_.GetMutable())[i] = GetDrawBuffer(i);
    use_default_draw_buffers_ = false;
  }

  // Check index bounds.
  if (index >= kColorAttachmentSlotCount) {
    LOG(ERROR) << "Out of bounds index " << index
               << " when setting a draw buffer";
    return;
  }
  // Check buffer number bounds. If the number is out of bounds, set a value
  // corresponding to GL_NONE instead.
  if (buffer < -1 || buffer >= int32{kColorAttachmentSlotCount}) {
    LOG(ERROR) << "Out of bounds buffer number " << buffer
               << " when setting draw buffer " << index;
    if (draw_buffers_.Get()[index] != -1)
      (*draw_buffers_.GetMutable())[index] = -1;
    return;
  }

  // Actually set the specified draw buffer.
  if (draw_buffers_.Get()[index] != buffer)
    (*draw_buffers_.GetMutable())[index] = buffer;
}

int32 FramebufferObject::GetDrawBuffer(size_t index) const {
  DCHECK_EQ(kColorAttachmentSlotCount, draw_buffers_.Get().size());
  if (index > kColorAttachmentSlotCount) return -1;
  if (use_default_draw_buffers_) {
    if (color_.Get(index).GetBinding() == kUnbound) return -1;
    return static_cast<int32>(index);
  }
  return draw_buffers_.Get()[index];
}

void FramebufferObject::SetDrawBuffers(
    const base::AllocVector<int32>& buffers) {
  SetDrawBuffers(buffers.begin(), buffers.end());
}

void FramebufferObject::SetDrawBuffers(
    const std::initializer_list<int32>& buffers) {
  SetDrawBuffers(buffers.begin(), buffers.end());
}

void FramebufferObject::ResetDrawBuffers() {
  use_default_draw_buffers_ = true;
  // Trigger change notification.
  draw_buffers_.GetMutable();
}

int32 FramebufferObject::GetReadBuffer() const {
  if (read_buffer_.Get() != kDefaultBufferNumber) return read_buffer_.Get();
  for (size_t i = 0; i < kColorAttachmentSlotCount; ++i) {
    if (color_.Get(i).GetBinding() != kUnbound) return static_cast<int32>(i);
  }
  return -1;
}

void FramebufferObject::ResetReadBuffer() {
  read_buffer_.Set(kDefaultBufferNumber);
}

void FramebufferObject::ForEachAttachment(
      const std::function<void(const Attachment&, int)>& function) const {
  for (uint32 i = 0; i < kColorAttachmentSlotCount; ++i) {
    const int change_bit = static_cast<int>(
        FramebufferObject::kColorAttachmentChanged + i);
    function(GetColorAttachment(size_t{i}), change_bit);
  }
  function(GetDepthAttachment(),
           static_cast<int>(FramebufferObject::kDepthAttachmentChanged));
  function(GetStencilAttachment(),
           static_cast<int>(FramebufferObject::kStencilAttachmentChanged));
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
    case GL_R32F: case GL_RG32F: case GL_RG16F: case GL_R16F:
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
         format == GL_DEPTH_STENCIL || format == GL_DEPTH24_STENCIL8 ||
         format == GL_DEPTH32F_STENCIL8;
}

bool FramebufferObject::IsStencilRenderable(uint32 format) {
  return format == GL_DEPTH_STENCIL || format == GL_DEPTH24_STENCIL8 ||
         format == GL_DEPTH32F_STENCIL8 || format == GL_STENCIL_INDEX8;
}

void FramebufferObject::OnNotify(const base::Notifier* notifier) {
  if (GetResourceCount()) {
    for (size_t i = 0; i < kColorAttachmentSlotCount; ++i) {
      if (IsAttachmentNotifier(notifier, color_.Get(i))) {
        OnChanged(kColorAttachmentChanged + static_cast<uint32>(i));
        return;
      }
    }
    if (IsAttachmentNotifier(notifier, depth_.Get())) {
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
    return;
  }
  if (Texture* tex = attachment.GetTexture().Get())
    tex->AddReceiver(this);
  else if (CubeMapTexture* tex = attachment.GetCubeMapTexture().Get())
    tex->AddReceiver(this);
  else if (Image* img = attachment.GetImage().Get())
    img->AddReceiver(this);
  field->Set(attachment);
}

void FramebufferObject::SetColorAttachment(VectorField<Attachment>* field,
                                           size_t index,
                                           const Attachment& attachment) {
  // Update notifications.
  if (Texture* tex = field->Get(index).GetTexture().Get())
    tex->RemoveReceiver(this);
  else if (CubeMapTexture* tex = field->Get(index).GetCubeMapTexture().Get())
    tex->RemoveReceiver(this);
  else if (Image* img = field->Get(index).GetImage().Get())
    img->RemoveReceiver(this);

  if (attachment.GetBinding() != kUnbound &&
      !IsColorFormatRenderable(attachment.GetFormat())) {
    LOG(ERROR) << "Invalid color attachment format "
               << Image::GetFormatString(attachment.GetFormat());
    field->Set(index, Attachment());
    return;
  }
  if (Texture* tex = attachment.GetTexture().Get())
    tex->AddReceiver(this);
  else if (CubeMapTexture* tex = attachment.GetCubeMapTexture().Get())
    tex->AddReceiver(this);
  else if (Image* img = attachment.GetImage().Get())
    img->AddReceiver(this);
  field->Set(index, attachment);
  // Default draw buffers and read buffer depend on the attachments,
  // so trigger updates.
  if (use_default_draw_buffers_) draw_buffers_.GetMutable();
  if (read_buffer_.Get() == kDefaultBufferNumber) read_buffer_.GetMutable();
}

template <typename Iterator>
void FramebufferObject::SetDrawBuffers(Iterator first, Iterator last) {
  DCHECK_EQ(kColorAttachmentSlotCount, draw_buffers_.Get().size());
  use_default_draw_buffers_ = false;
  const size_t bufcount = static_cast<size_t>(std::distance(first, last));
  if (bufcount > kColorAttachmentSlotCount) {
    LOG(WARNING) << "Trying to set more than " << kColorAttachmentSlotCount
                 << " draw buffers";
  }

  base::AllocVector<int32>* buffer_vector = draw_buffers_.GetMutable();
  size_t i = 0;
  for (; first != last && i < kColorAttachmentSlotCount; ++first, ++i) {
    int32 buffer = *first;
    if (buffer < -1 || buffer >= int32{kColorAttachmentSlotCount}) {
      LOG(ERROR) << "Out of bounds buffer number " << buffer
                 << " when setting draw buffer " << i;
      (*buffer_vector)[i] = -1;
    } else {
      (*buffer_vector)[i] = *first;
    }
  }
  // If the range was not long enough to cover all draw buffers, set the
  // remainder to discarded (-1).
  for (; i < kColorAttachmentSlotCount; ++i) {
    (*buffer_vector)[i] = -1;
  }
}

}  // namespace gfx
}  // namespace ion
