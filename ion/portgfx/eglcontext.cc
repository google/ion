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

#if !defined(ION_PLATFORM_WINDOWS)
#include <dlfcn.h>
#endif
#include <cstring>
#include <string>

#if defined(ION_PLATFORM_ANDROID)
#include <cstdio>
#include <sstream>
#endif

#include "ion/base/logging.h"
#include "ion/base/sharedptr.h"
#include "ion/base/stringutils.h"
#include "ion/portgfx/eglcontextbase.h"
#include "ion/portgfx/glcontext.h"

namespace ion {
namespace portgfx {
namespace {

#if defined(ION_PLATFORM_ANDROID)
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

#else
void* GetGlLibrary() {
#if defined(ION_PLATFORM_WINDOWS)
  return nullptr;
#else
  void* libGLESv2 = nullptr;
  libGLESv2 = dlopen("libGLESv2.so.2", RTLD_NOW);
  if (libGLESv2 == nullptr) {
    libGLESv2 = dlopen("libGLESv2.so", RTLD_NOW);
  }
  return libGLESv2;
#endif  // defined(ION_PLATFORM_WINDOWS)
}

bool IsDisabledFunction(const char* name) { return false; }
#endif

// This class wraps a standard EGL context in an ion::portgfx::GlContext
// implementation.  Almost all functionality is already implemented in
// EglContextBase, so there is not much to do here.
class EglContext : public EglContextBase {
 public:
  explicit EglContext(bool is_owned_context)
      : EglContextBase(is_owned_context) {}

  // GlContext implementation.
  void* GetProcAddress(const char* proc_name, uint32_t flags) const override {
    using std::strstr;
    // On a non-pure call to GetProcAddress(), we will need to interrogate the
    // hardware vendor's library directly.  Cache the library address using
    // a thread-safe static.
    static void* const libGLESv2 = GetGlLibrary();

    bool force_loading_from_so = false;
#if defined(ION_PLATFORM_ANDROID)
    if (!strcmp(proc_name, "glInsertEventMarker") ||
        !strcmp(proc_name, "glPushGroupMarker") ||
        !strcmp(proc_name, "glPopGroupMarker")) {
      // Android's EGL loader forces all GL_EXT_debug_label functions to be
      // noops, expecting them to be loaded by the now-defunct GLES_trace
      // library. Load them directly from the vendor implementation.
      LOG(INFO) << "Forcing non-pure loading for EXT_debug_label functions.";
      force_loading_from_so = true;
    }
#endif

    const bool is_core = flags & kProcAddressCore;
    const bool is_pure = flags & kProcAddressPure;

    for (const char* suffix : {"", "OES", "APPLE", "ARB", "EXT", "KHR", "NV"}) {
      const std::string proc_string(proc_name);
      const std::string full_name = proc_string + suffix;

#if defined(ION_PLATFORM_ANDROID)
      if (!is_pure) {
        // Log a warning, since we might disable non-pure loading in the future.
        static std::once_flag non_pure_log_flag;
        std::call_once(non_pure_log_flag, [proc_name]() {
          LOG_PROD(WARNING)
              << "Non-pure loading is deprecated on Android.  To enable pure "
              << "loading, pass kProcAddressPure to GetProcAddress. "
              << proc_name;
        });
      }
#endif
#if !defined(ION_PLATFORM_WINDOWS)
      if (!is_pure || force_loading_from_so) {
        DCHECK_NE(nullptr, libGLESv2) << "Unable to open graphics libraries.";

        // When non-pure loading, we cannot directly trust the return value of
        // eglGetProcAddress(), since some drivers return wrappers for
        // unimplemented functions.  To handle this, we only use the entry point
        // returned by eglGetProcAddress() if the entry point also exists in the
        // hardware vendor's library.
        void* const library_func =
            reinterpret_cast<void*>(dlsym(libGLESv2, full_name.c_str()));
        if (library_func) {
          // For core GL functions, we return the entry point as found in the
          // hardware vendor's library, if found.
          if (is_core) {
            return library_func;
          }
        } else {
          // For extension GL functions, we skip trying eglGetProcAddress() if
          // hardware vendor's library did not contain the entry point.  We make
          // an exception for EGL functions.
          if (!base::StartsWith(proc_string, "egl")) {
            continue;
          }
        }
      }
#endif  // !defined(ION_PLATFORM_WINDOWS)

      void* func =
          reinterpret_cast<void*>(eglGetProcAddress(full_name.c_str()));

#if !defined(ION_PLATFORM_WINDOWS)
      if (is_pure && func == nullptr && is_core) {
        // We will need to query the current executable image, so cache the
        // library address.
        static void* const exec_image = []() {
          void* const exec_image = dlopen(nullptr, RTLD_LAZY);
          DCHECK_NE(nullptr, exec_image)
              << "Unable to open current executable image.";
          return exec_image;
        }();

        // EGL 1.5 specifies that eglGetProcAddress() returns all client API
        // entry points, core or extension.  Unfortunately, EGL 1.4 specifies
        // that only extension entry points are returned.  Thus for pure
        // loading, fall back to dlsym() (on the current executable image) for
        // core entry points to emulate EGL 1.5 behavior.  We use the query
        // on the image, instead of RTLD_DEFAULT, to avoid a SIGFPE bug on
        // Android emulator 4.4.4 and earlier.
        func = reinterpret_cast<void*>(dlsym(exec_image, full_name.c_str()));
      }
#endif  // !defined(ION_PLATFORM_WINDOWS)

      if (func != nullptr) {
        return func;
      }
    }
    return nullptr;
  }

  GlContextPtr CreateGlContextInShareGroupImpl(
      const GlContextSpec& spec) override {
    base::SharedPtr<EglContext> context(new EglContext(true));
    if (!context->InitOwned(this, spec)) {
      context.Reset();
    }
    return context;
  }

  // EglContextBase implementation.
  EGLSurface EglCreateSurface(EGLDisplay display, EGLConfig config,
                              const GlContextSpec& spec) const override {
    if (spec.native_window) {
      auto window = reinterpret_cast<NativeWindowType>(spec.native_window);
      return eglCreateWindowSurface(display, config, window, nullptr);
    }

    const EGLint pbuffer_attributes[] = {
        EGL_WIDTH, spec.backbuffer_width, EGL_HEIGHT, spec.backbuffer_height,
        EGL_NONE,
    };
    return eglCreatePbufferSurface(display, config, pbuffer_attributes);
  }
};

}  // namespace

// static
GlContextPtr GlContext::CreateGlContext(const GlContextSpec& spec) {
  base::SharedPtr<EglContext> context(new EglContext(true));
  if (!context->InitOwned(nullptr, spec)) {
    context.Reset();
  }
  return context;
}

// static
GlContextPtr GlContext::CreateWrappingGlContext() {
  base::SharedPtr<EglContext> context(new EglContext(false));
  if (!context->InitWrapped()) {
    context.Reset();
  }
  return context;
}

// static
uintptr_t GlContext::GetCurrentGlContextId() {
  return reinterpret_cast<uintptr_t>(eglGetCurrentContext());
}

}  // namespace portgfx
}  // namespace ion
