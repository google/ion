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

#include <GL/osmesa.h>

#include <vector>

#include "ion/portgfx/glcontext.h"

namespace ion {
namespace portgfx {
namespace {

class OsMesaContext : public GlContext {
 public:
  explicit OsMesaContext(bool is_owned_context)
      : context_(nullptr), is_owned_context_(is_owned_context) {}
  ~OsMesaContext() override {
    if (is_owned_context_) {
      if (context_ != nullptr) {
        OSMesaDestroyContext(context_);
      }
    }
  }
  bool IsValid() const override { return (context_ != nullptr); }
  void* GetProcAddress(const char* proc_name, uint32_t flags) const override {
    return reinterpret_cast<void*>(OSMesaGetProcAddress(proc_name));
  }
  void SwapBuffers() override {}
  bool MakeContextCurrentImpl() override {
    return OSMesaMakeCurrent(context_, color_buffer_, GL_UNSIGNED_BYTE, width_,
                             height_);
  }
  void ClearCurrentContextImpl() override {
    OSMesaMakeCurrent(nullptr, nullptr, 0, 0, 0);
  }
  GlContextPtr CreateGlContextInShareGroupImpl(
      const GlContextSpec& spec) override {
    base::SharedPtr<OsMesaContext> visual(new OsMesaContext(true));
    if (!visual->InitOwned(this, spec)) {
      visual.Reset();
    }
    return visual;
  }
  bool IsOwned() const override { return is_owned_context_; }
  bool InitOwned(const OsMesaContext* shared_visual, const GlContextSpec& spec);
  bool InitWrapped();

 private:
  // The (potentially) owned state.
  OSMesaContext context_;

  // Whether the "owned state" is actually owned.
  const bool is_owned_context_;

  // When wrapping, this pointer is obtained via OSMesaGetColorBuffer.
  // For the owned case, it points to owned_color_buffer_.
  void* color_buffer_;

  GLint width_;
  GLint height_;
  GLint format_;
  std::vector<uint8> owned_color_buffer_;
};

bool OsMesaContext::InitOwned(const OsMesaContext* shared_visual,
                              const GlContextSpec& spec) {
  DCHECK(is_owned_context_);
  const OSMesaContext shared_context =
      (shared_visual != nullptr ? shared_visual->context_ : nullptr);
  width_ = spec.backbuffer_width;
  height_ = spec.backbuffer_height;
  owned_color_buffer_.resize(4 * width_ * height_);
  color_buffer_ = owned_color_buffer_.data();
  format_ = OSMESA_RGBA;
  context_ = OSMesaCreateContextExt(format_, spec.depthbuffer_bit_depth, 0, 0,
                                    shared_context);
  if (context_ == nullptr) {
    LOG(ERROR) << "Failed to create OSMesa context.";
    return false;
  }
  SetIds(CreateId(),
         (shared_visual != nullptr ? shared_visual->GetShareGroupId()
                                   : CreateShareGroupId()),
         reinterpret_cast<uintptr_t>(context_));
  return true;
}

bool OsMesaContext::InitWrapped() {
  DCHECK(!is_owned_context_);
  context_ = OSMesaGetCurrentContext();
  if (context_ == nullptr) {
    LOG(ERROR) << "No current context.";
    return false;
  }
  OSMesaGetColorBuffer(context_, &width_, &height_, &format_, &color_buffer_);
  SetIds(CreateId(), CreateShareGroupId(),
         reinterpret_cast<uintptr_t>(context_));
  return true;
}

}  // namespace

// static
GlContextPtr GlContext::CreateGlContext(const GlContextSpec& spec) {
  base::SharedPtr<OsMesaContext> visual(new OsMesaContext(true));
  if (!visual->InitOwned(nullptr, spec)) {
    visual.Reset();
  }
  return visual;
}

// static
GlContextPtr GlContext::CreateWrappingGlContext() {
  base::SharedPtr<OsMesaContext> visual(new OsMesaContext(false));
  if (!visual->InitWrapped()) {
    visual.Reset();
  }
  return visual;
}

// static
uintptr_t GlContext::GetCurrentGlContextId() {
  return reinterpret_cast<uintptr_t>(OSMesaGetCurrentContext());
}

}  // namespace portgfx
}  // namespace ion
