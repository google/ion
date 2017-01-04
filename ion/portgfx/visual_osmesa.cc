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

#include "ion/portgfx/visual.h"

namespace ion {
namespace portgfx {
namespace {

class VisualOsMesa : public Visual {
 public:
  explicit VisualOsMesa(bool is_owned_context)
      : context_(nullptr), is_owned_context_(is_owned_context) {}
  ~VisualOsMesa() override {
    if (is_owned_context_) {
      if (context_ != nullptr) {
        OSMesaDestroyContext(context_);
      }
    }
  }
  bool IsValid() const override { return (context_ != nullptr); }
  void* GetProcAddress(const char* proc_name, bool is_core) const override {
    return reinterpret_cast<void*>(OSMesaGetProcAddress(proc_name));
  }
  bool MakeContextCurrentImpl() override {
    return OSMesaMakeCurrent(context_, color_buffer_, GL_UNSIGNED_BYTE, width_,
                             height_);
  }
  void ClearCurrentContextImpl() override {
    OSMesaMakeCurrent(nullptr, nullptr, 0, 0, 0);
  }
  VisualPtr CreateVisualInShareGroupImpl(const VisualSpec& spec) override {
    base::SharedPtr<VisualOsMesa> visual(new VisualOsMesa(true));
    if (!visual->InitOwned(this, spec)) {
      visual.Reset();
    }
    return visual;
  }
  bool IsOwned() const override { return is_owned_context_; }
  bool InitOwned(const VisualOsMesa* shared_visual, const VisualSpec& spec);
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

bool VisualOsMesa::InitOwned(const VisualOsMesa* shared_visual,
                             const VisualSpec& spec) {
  DCHECK(is_owned_context_);
  const OSMesaContext shared_context =
      (shared_visual != nullptr ? shared_visual->context_ : nullptr);
  width_ = spec.backbuffer_width;
  height_ = spec.backbuffer_height;
  owned_color_buffer_.resize(4 * width_ * height_);
  color_buffer_ = owned_color_buffer_.data();
  format_ = OSMESA_RGBA;
  context_ = OSMesaCreateContextExt(format_, spec.depthbuffer_bit_depth, 0,
                                    0, shared_context);
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

bool VisualOsMesa::InitWrapped() {
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
VisualPtr Visual::CreateVisual(const VisualSpec& spec) {
  base::SharedPtr<VisualOsMesa> visual(new VisualOsMesa(true));
  if (!visual->InitOwned(nullptr, spec)) {
    visual.Reset();
  }
  return visual;
}

// static
VisualPtr Visual::CreateWrappingVisual() {
  base::SharedPtr<VisualOsMesa> visual(new VisualOsMesa(false));
  if (!visual->InitWrapped()) {
    visual.Reset();
  }
  return visual;
}

// static
uintptr_t Visual::GetCurrentGlContextId() {
  return reinterpret_cast<uintptr_t>(OSMesaGetCurrentContext());
}

// static
void Visual::CleanupThread() { MakeCurrent(VisualPtr()); }

}  // namespace portgfx
}  // namespace ion
