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

#include <OpenGLES/EAGL.h>
#include <dlfcn.h>
#include <stdint.h>

#include <memory>

#include "ion/base/logging.h"
#include "ion/base/sharedptr.h"
#include "ion/portgfx/visual.h"

namespace ion {
namespace portgfx {
namespace {

class VisualEagl : public Visual {
 public:
  explicit VisualEagl(bool is_owned_context)
      : weak_context_(nil),
        strong_context_(nil),
        is_owned_context_(is_owned_context) {}

  ~VisualEagl() override {}

  // Visual implementation.
  bool IsValid() const override { return (weak_context_ != nil); }
  void* GetProcAddress(const char* proc_name, bool is_core) const override {
    for (const char* suffix : { "", "OES", "APPLE", "ARB", "EXT", "KHR", "NV"} ) {
      const std::string full_name = std::string(proc_name) + suffix;
      void* func = reinterpret_cast<void*>(dlsym(RTLD_DEFAULT, full_name.c_str()));
      if (func != nullptr) {
        return func;
      }
    }
    return nullptr;
  }
  bool MakeContextCurrentImpl() override {
    EAGLContext* const strong_context = weak_context_;
    if (strong_context == nil) {
      LOG(ERROR) << "Unable to make Visual current.  "
                    "Unowned context has been released.";
      return false;
    }
    const BOOL success = [EAGLContext setCurrentContext:strong_context];
    if (success == NO) {
      LOG(ERROR) << "Unable to make Visual current.";
      return false;
    }
    return true;
  }
  void ClearCurrentContextImpl() override {
    [EAGLContext setCurrentContext:nil];
  }
  VisualPtr CreateVisualInShareGroupImpl(const VisualSpec& spec) override {
    // Currently this platform only supports the default VisualSpec.
    DCHECK(spec.backbuffer_width == 1 && spec.backbuffer_height == 1);
    base::SharedPtr<VisualEagl> visual(new VisualEagl(true));
    if (!visual->InitOwned(this)) {
      visual.Reset();
    }
    return visual;
  }
  bool IsOwned() const override { return is_owned_context_; }

  bool InitOwned(const VisualEagl* shared_visual);
  bool InitWrapped();

 private:
  // Use weak reference for unowned context, strong reference for owned context.
  __weak EAGLContext* weak_context_;
  EAGLContext* strong_context_;

  // Whether the "owned state" is actually owned.
  const bool is_owned_context_;
};

bool VisualEagl::InitOwned(const VisualEagl* shared_visual) {
  DCHECK(is_owned_context_);

  EAGLContext* const share_context =
    (shared_visual != nil ? shared_visual->weak_context_ : nil);
  EAGLSharegroup* const share_group =
    (share_context != nil ? share_context.sharegroup : nil);

  // Only store a strong reference to contexts we create and own.
  strong_context_ = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2
                                          sharegroup:share_group];
  if (strong_context_ == nil) {
    LOG(ERROR) << "Failed to create context.";
    return false;
  }
  weak_context_ = strong_context_;

  SetIds(CreateId(), reinterpret_cast<uintptr_t>(weak_context_.sharegroup),
         reinterpret_cast<uintptr_t>(weak_context_));
  return true;
}

bool VisualEagl::InitWrapped() {
  DCHECK(!is_owned_context_);

  weak_context_ = [EAGLContext currentContext];
  if (weak_context_ == nil) {
    LOG(ERROR) << "No current context.";
    return false;
  }

  SetIds(CreateId(), reinterpret_cast<uintptr_t>(weak_context_.sharegroup),
         reinterpret_cast<uintptr_t>(weak_context_));
  return true;
}

}  // namespace

// static
VisualPtr Visual::CreateVisual(const VisualSpec& spec) {
  // Currently this platform only supports the default VisualSpec.
  DCHECK(spec.backbuffer_width == 1 && spec.backbuffer_height == 1);
  base::SharedPtr<VisualEagl> visual(new VisualEagl(true));
  if (!visual->InitOwned(nil)) {
    visual.Reset();
  }
  return visual;
}

// static
VisualPtr Visual::CreateWrappingVisual() {
  base::SharedPtr<VisualEagl> visual(new VisualEagl(false));
  if (!visual->InitWrapped()) {
    visual.Reset();
  }
  return visual;
}

// static
uintptr_t Visual::GetCurrentGlContextId() {
  return reinterpret_cast<uintptr_t>([EAGLContext currentContext]);
}

// static
void Visual::CleanupThread() { MakeCurrent(VisualPtr()); }

}  // namespace portgfx
}  // namespace ion
