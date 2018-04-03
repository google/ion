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

#include <AppKit/NSOpenGL.h>
#include <dlfcn.h>
#include <stdint.h>

#include <string>

#include "ion/base/sharedptr.h"
#include "ion/base/logging.h"
#include "ion/portgfx/glcontext.h"

#define GL_GLEXT_FUNCTION_POINTERS
#include "ion/portgfx/glheaders.h"

namespace ion {
namespace portgfx {
namespace {

class NsglContext : public GlContext {
 public:
  explicit NsglContext(bool is_owned_context)
      : weak_context_(nil),
        strong_context_(nil),
        is_owned_context_(is_owned_context) {}

  // GlContext implementation.
  bool IsValid() const override { return (weak_context_ != nil); }
  void* GetProcAddress(const char* proc_name, uint32_t flags) const override {
    const bool is_core = flags & kProcAddressCore;
    void* func = GetProcAddressImpl(proc_name, is_core);
    if (func != nil) {
      return func;
    }

    // These functions do not appear in core GL until 4.1.
    using std::strcmp;
    if (strcmp(proc_name, "glClearDepthf") == 0) {
      static const glClearDepthProcPtr kClearDepthFunc =
          reinterpret_cast<glClearDepthProcPtr>(GetProcAddressImpl("glClearDepth", is_core));
      if (kClearDepthFunc != nil) {
        static const auto kClearDepthLambda = [](GLfloat f){
          return kClearDepthFunc(static_cast<double>(f));
        };
        return reinterpret_cast<void*>(+kClearDepthLambda);
      }
    } else if (strcmp(proc_name, "glDepthRangef") == 0) {
      static const glDepthRangeProcPtr kDepthRangeFunc =
          reinterpret_cast<glDepthRangeProcPtr>(GetProcAddressImpl("glDepthRange", is_core));
      if (kDepthRangeFunc != nil) {
        static const auto kDepthRangeLambda = [](GLfloat n, GLfloat f){
          return kDepthRangeFunc(static_cast<double>(n), static_cast<double>(f));
        };
        return reinterpret_cast<void*>(+kDepthRangeLambda);
      }
    }
    return nil;
  }
  void SwapBuffers() override {
    if (weak_context_ != nil) {
      [weak_context_ flushBuffer];
    }
  }
  bool MakeContextCurrentImpl() override {
    NSOpenGLContext* const strong_context = weak_context_;
    if (strong_context == nil) {
      LOG(ERROR) << "Unable to make GlContext current.  Unowned context has been released.";
      return false;
    }
    [strong_context makeCurrentContext];
    if (strong_context != [NSOpenGLContext currentContext]) {
      LOG(ERROR) << "Unable to make GlContext current.";
      return false;
    }
    return true;
  }
  void ClearCurrentContextImpl() override {
    [NSOpenGLContext clearCurrentContext];
  }
  GlContextPtr CreateGlContextInShareGroupImpl(const GlContextSpec& spec) override {
    // Currently this platform only supports the default GlContextSpec.
    DCHECK(spec.backbuffer_width == 1 && spec.backbuffer_height == 1);
    base::SharedPtr<NsglContext> context(new NsglContext(true));
    if (!context->InitOwned(this)) {
      context.Reset();
    }
    return context;
  }
  bool IsOwned() const override { return is_owned_context_; }

  bool InitOwned(const NsglContext* shared_context);
  bool InitWrapped();

 private:
  // Internal implementation details for GetProcAddress().
  void* GetProcAddressImpl(const char* proc_name, bool is_core) const {
    static const glGetIntegervProcPtr kGetIntegervFunc =
        reinterpret_cast<glGetIntegervProcPtr>(dlsym(RTLD_DEFAULT, "glGetIntegerv"));
    static const glGetErrorProcPtr kGetErrorFunc =
        reinterpret_cast<glGetErrorProcPtr>(dlsym(RTLD_DEFAULT, "glGetError"));
    if (kGetIntegervFunc == nil || kGetErrorFunc == nil) {
      LOG(ERROR) << "Unable to get glGetIntegerv() and glGetError() entry points.";
      return nil;
    }

    // Mac has special APPLE functions that must be used when using a
    // compatibility profile, but must _not_ be used when using a core profile.
    static const char* kCompatSuffixes[] = {"APPLE", "", "ARB", "EXT", "KHR", "NV", nil};
    static const char* kCoreSuffixes[] = {"", "APPLE", "ARB", "EXT", "KHR", "NV", nil};
    GLint mask = 0;
    (*kGetIntegervFunc)(GL_CONTEXT_PROFILE_MASK, &mask);
    // The above call could generate an invalid enum if we are not called from a
    // core context. Eat the error.
    (*kGetErrorFunc)();

    const bool is_core_profile = mask & GL_CONTEXT_CORE_PROFILE_BIT;
    const char** suffixes = is_core_profile ? kCoreSuffixes : kCompatSuffixes;
    for (const char** suffix = suffixes; *suffix != nil; ++suffix) {
      const std::string full_name = std::string(proc_name) + *suffix;
      void* func = reinterpret_cast<void*>(dlsym(RTLD_DEFAULT, full_name.c_str()));
      if (func != nil) {
        return func;
      }
    }
    return nil;
  }

  // Use weak reference for unowned context, strong reference for owned context.
  __weak NSOpenGLContext* weak_context_;
  NSOpenGLContext* strong_context_;

  // Whether the "owned state" is actually owned.
  const bool is_owned_context_;
};

bool NsglContext::InitOwned(const NsglContext* shared_context) {
  DCHECK(is_owned_context_);

  static const NSOpenGLPixelFormatAttribute kPixelAttributes[] = {
    NSOpenGLPFAColorSize, 24,
    NSOpenGLPFAAlphaSize, 8,
    NSOpenGLPFADepthSize, 24,
    NSOpenGLPFAStencilSize, 8,
    NSOpenGLPFASampleBuffers, 0,
    0,
  };

  NSOpenGLPixelFormat* const pixel_format =
    [[NSOpenGLPixelFormat alloc] initWithAttributes:kPixelAttributes];

  NSOpenGLContext* const share_context =
    (shared_context != nil ? shared_context->weak_context_ : nil);

  // Only store a strong reference to contexts we create and own.
  strong_context_ = [[NSOpenGLContext alloc] initWithFormat:pixel_format
                                               shareContext:share_context];
  weak_context_ = strong_context_;

  SetIds(CreateId(),
         (shared_context != nil ? shared_context->GetShareGroupId()
                                : CreateShareGroupId()),
         reinterpret_cast<uintptr_t>(strong_context_));
  return true;
}

bool NsglContext::InitWrapped() {
  DCHECK(!is_owned_context_);

  weak_context_ = [NSOpenGLContext currentContext];
  if (weak_context_ == nil) {
    LOG(ERROR) << "No current context.";
    return false;
  }

  SetIds(CreateId(), CreateShareGroupId(),
         reinterpret_cast<uintptr_t>(weak_context_));
  return true;
}

}  // namespace

// static
GlContextPtr GlContext::CreateGlContext(const GlContextSpec& spec) {
  // Currently this platform only supports the default GlContextSpec.
  DCHECK(spec.backbuffer_width == 1 && spec.backbuffer_height == 1);
  base::SharedPtr<NsglContext> context(new NsglContext(true));
  if (!context->InitOwned(nil)) {
    context.Reset();
  }
  return context;
}

// static
GlContextPtr GlContext::CreateWrappingGlContext() {
  base::SharedPtr<NsglContext> context(new NsglContext(false));
  if (!context->InitWrapped()) {
    context.Reset();
  }
  return context;
}

// static
uintptr_t GlContext::GetCurrentGlContextId() {
  return reinterpret_cast<uintptr_t>([NSOpenGLContext currentContext]);
}

}  // namespace portgfx
}  // namespace ion
