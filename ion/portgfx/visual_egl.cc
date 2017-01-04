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

#include <dlfcn.h>
#include <cstring>
#include <string>

#if defined(ION_PLATFORM_ANDROID) || defined(ION_PLATFORM_GENERIC_ARM)
#include <cstdio>
#include <sstream>
#endif

#include "ion/base/logging.h"
#include "ion/base/sharedptr.h"
#include "ion/base/stringutils.h"
#include "ion/portgfx/visual.h"
#include "ion/portgfx/visual_egl_base.h"

namespace ion {
namespace portgfx {
namespace {

#if defined(ION_PLATFORM_ANDROID) || defined(ION_PLATFORM_GENERIC_ARM)
void* GetGlLibrary() {
  void* libGLESv2 = nullptr;
  if (!libGLESv2) {
    if (FILE* egl_config = std::fopen("/system/lib/egl/egl.cfg", "r")) {
      char line[256];
      while (std::fgets(line, 256, egl_config)) {
        int display;
        int implementation;
        char tag[256];
        std::istringstream s(line);
        s >> display >> implementation >> tag;
        if (!s.fail() && tag[0] != '\0') {
          // Use the first non-"android" library, which should be the hardware
          // manufacturer's version.
          if (std::strcmp(tag, "android")) {
            std::string library("libGLESv2_");
            library += tag;
            library += ".so";
            libGLESv2 = dlopen(library.c_str(), RTLD_NOW);
            break;
          }
        }
      }
      std::fclose(egl_config);
    }
    // Fallback to the android library.
    if (!libGLESv2) libGLESv2 = dlopen("libGLESv2_android.so", RTLD_NOW);
    // Fallback to the default library.
    if (!libGLESv2) libGLESv2 = dlopen("libGLESv2.so", RTLD_NOW);
  }
  return libGLESv2;
}

bool IsDisabledFunction(const char* name) {
  static const char* kDisabledFunctions[] = {
      // Disable vertex arrays on Android since there seems to be a buggy
      // implementation of them in the SDK.
      "glBindVertexArray", "glDeleteVertexArrays", "glGenVertexArrays",
      "glIsVertexArray",
  };

  for (const char* disabled_function : kDisabledFunctions) {
    if (!strcmp(name, disabled_function)) {
      LOG(INFO) << "disabling \"" << name << "\" for this EGL implementation";
      return true;
    }
  }
  return false;
}

#else
void* GetGlLibrary() {
  void* libGLESv2 = nullptr;
  libGLESv2 = dlopen("libGLESv2.so.2", RTLD_NOW);
  if (libGLESv2 == nullptr) {
    libGLESv2 = dlopen("libGLESv2.so", RTLD_NOW);
  }
  return libGLESv2;
}

bool IsDisabledFunction(const char* name) { return false; }
#endif

// This class wraps a standard EGL context in an ion::portgfx::Visual
// implementation.  Almost all functionality is already implemented in
// VisualEglBase, so there is not much to do here.
class VisualEgl : public VisualEglBase {
 public:
  explicit VisualEgl(bool is_owned_context) : VisualEglBase(is_owned_context) {}

  // Visual implementation.
  void* GetProcAddress(const char* proc_name, bool is_core) const override {
    using std::strstr;
    static void* libGLESv2 = GetGlLibrary();
    DCHECK_NE(libGLESv2, nullptr);

    if (IsDisabledFunction(proc_name)) {
      return nullptr;
    }

    // We cannot directly trust the return value of eglGetProcAddress() since it
    // may return a wrapper for an unimplemented function. To handle this, we
    // first check for the function in the hardware vendor's library, and only
    // query EGL for the functions we know are available. If the function is
    // actually an EGL extension-related function, however, we _must_ call
    // eglGetProcAddress().
    for (const char* suffix : {"", "OES", "APPLE", "ARB", "EXT", "KHR", "NV"}) {
      const std::string proc_string(proc_name);
      const std::string full_name = proc_string + suffix;
      void* func = reinterpret_cast<void*>(dlsym(libGLESv2, full_name.c_str()));
      if (func || base::StartsWith(proc_string, "egl")) {
        // Core ES2 functions must be used directly, only extensions should go
        // through EGL.
        if (!is_core) {
          func = reinterpret_cast<void*>(eglGetProcAddress(full_name.c_str()));
        }
      }
      if (func != nullptr) {
        return func;
      }
    }
    return nullptr;
  }

  VisualPtr CreateVisualInShareGroupImpl(const VisualSpec& spec) override {
    base::SharedPtr<VisualEgl> visual(new VisualEgl(true));
    if (!visual->InitOwned(this, spec)) {
      visual.Reset();
    }
    return visual;
  }

  // VisualEglBase implementation.
  EGLSurface EglCreateSurface(EGLDisplay display, EGLConfig config, int width,
                              int height) const override {
    const EGLint pbuffer_attributes[] = {
        EGL_WIDTH, width, EGL_HEIGHT, height, EGL_NONE,
    };
    return eglCreatePbufferSurface(display, config, pbuffer_attributes);
  }
};

}  // namespace

// static
VisualPtr Visual::CreateVisual(const VisualSpec& spec) {
  base::SharedPtr<VisualEgl> visual(new VisualEgl(true));
  if (!visual->InitOwned(nullptr, spec)) {
    visual.Reset();
  }
  return visual;
}

// static
VisualPtr Visual::CreateWrappingVisual() {
  base::SharedPtr<VisualEgl> visual(new VisualEgl(false));
  if (!visual->InitWrapped()) {
    visual.Reset();
  }
  return visual;
}

// static
uintptr_t Visual::GetCurrentGlContextId() {
  return reinterpret_cast<uintptr_t>(eglGetCurrentContext());
}

}  // namespace portgfx
}  // namespace ion
