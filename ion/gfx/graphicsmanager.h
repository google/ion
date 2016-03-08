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

#ifndef ION_GFX_GRAPHICSMANAGER_H_
#define ION_GFX_GRAPHICSMANAGER_H_

#include <bitset>
#include <cstring>
#include <iostream>  // NOLINT
#include <memory>
#include <sstream>
#include <string>

#include "ion/base/logging.h"
#include "ion/base/referent.h"
#include "ion/base/stlalloc/allocset.h"
#include "ion/base/stlalloc/allocvector.h"
#include "ion/gfx/graphicsmanagermacrodefs.h"
#include "ion/gfx/statetable.h"
#include "ion/gfx/tracinghelper.h"
#include "ion/math/range.h"
#include "ion/portgfx/glheaders.h"

namespace ion {
namespace gfx {

// GraphicsManager manages the graphics library for an application. It wraps
// calls to OpenGL functions, allowing extensions to be used easily, as well as
// allowing tracing of calls and error checking.
// GraphicsManager is generally thread agnostic, it can either be used
// consistenly from any single thread or protected by mutex from any thread.
// If tracing is enabled, thread safety of all OpenGL calls and capability
// queries becomes limited by the thread safety of the tracing stream, and
// furthermore is going to result in interleaving even if the tracing stream
// is thread-safe, which will make tracing difficult.
// Regardless of tracing, all OpenGL calls and capability checks are limited by
// the thread safety of the OpenGL subsystem.
// Extension/function/function group/glversion/glapi checks are always
// thread-safe.
class ION_API GraphicsManager : public base::Referent {
 public:
  // Information about shader precision, see below.
  struct ShaderPrecision {
    ShaderPrecision(const math::Range1i& range_in, const int precision_in)
        : range(range_in), precision(precision_in) {}
    // Returns true if the requested precision is unsupported.
    bool IsValid() const {
      return (range != math::Range1i(0, 0)) || precision != 0;
    }
    inline bool operator==(const ShaderPrecision& other) const {
      return range == other.range && precision == other.precision;
    }
    math::Range1i range;
    int precision;
  };

  // GraphicsManager supports queries on local GL platform capabilities, such as
  // the maximum texture size or viewport dimensions. The comments in the enum
  // indicate the canonical return type of GetCapabilityValue(). If a different
  // type is requested for the return value a warning message will be printed
  // and the return value will be invalid.
  enum Capability {
    kAliasedLineWidthRange,                      // math::Range1f
    kAliasedPointSizeRange,                      // math::Range1f
    kCompressedTextureFormats,                   // std::vector<int>
    kImplementationColorReadFormat,              // int
    kImplementationColorReadType,                // int
    kMaxColorAttachments,                        // int
    kMaxCombinedTextureImageUnits,               // int
    kMaxCubeMapTextureSize,                      // int
    kMaxDrawBuffers,                             // int
    kMaxFragmentUniformComponents,               // int
    kMaxFragmentUniformVectors,                  // int
    kMaxRenderbufferSize,                        // int
    kMaxSampleMaskWords,                         // int
    kMaxTextureImageUnits,                       // int
    kMaxTextureMaxAnisotropy,                    // float
    kMaxTextureSize,                             // int
    kMaxTransformFeedbackBuffers,                // int
    kMaxTransformFeedbackInterleavedComponents,  // int
    kMaxTransformFeedbackSeparateAttribs,        // int
    kMaxTransformFeedbackSeparateComponents,     // int
    kMaxVaryingVectors,                          // int
    kMaxVertexAttribs,                           // int
    kMaxVertexTextureImageUnits,                 // int
    kMaxVertexUniformComponents,                 // int
    kMaxVertexUniformVectors,                    // int
    kMaxViewportDims,                            // math::Range1i
    kShaderBinaryFormats,                        // std::vector<int>
    kTransformFeedbackVaryingMaxLength,          // int
    kMaxDebugLoggedMessages,                     // int
    kMaxDebugMessageLength,                      // int
    // The below are returned as a ShaderPrecision struct. A particular
    // precision is unsupported if the precision range is [0:0].
    kFragmentShaderHighFloatPrecisionFormat,
    kFragmentShaderHighIntPrecisionFormat,
    kFragmentShaderLowFloatPrecisionFormat,
    kFragmentShaderLowIntPrecisionFormat,
    kFragmentShaderMediumFloatPrecisionFormat,
    kFragmentShaderMediumIntPrecisionFormat,
    kVertexShaderHighFloatPrecisionFormat,
    kVertexShaderHighIntPrecisionFormat,
    kVertexShaderLowFloatPrecisionFormat,
    kVertexShaderLowIntPrecisionFormat,
    kVertexShaderMediumFloatPrecisionFormat,
    kVertexShaderMediumIntPrecisionFormat,
    kCapabilityCount,
  };

  enum FunctionGroupId {
    kCore,  // Core OpenGL ES2 functions.
    kDebugLabel,
    kDebugMarker,
    kDebugOutput,
    kChooseBuffer,
    kFramebufferBlit,
    kFramebufferMultisample,
    // This is used for Apple devices running pre-es-3.0 device with the
    // APPLE_framebuffer_multisample extension.
    // See https://www.khronos.org/registry/gles/extensions/APPLE/
    // APPLE_framebuffer_multisample.txt.
    kMultisampleFramebufferResolve,
    // This covers both OES_EGL_image and OES_EGL_image_external.
    kEglImage,
    kGetString,
    // See https://www.opengl.org/registry/specs/EXT/gpu_shader4.txt.
    kGpuShader4,
    kMapBuffer,
    kMapBufferBase,
    kMapBufferRange,
    kCopyBufferSubData,
    kPointSize,
    kRaw,
    kSamplerObjects,
    kTexture3d,
    kTextureMultisample,
    kTextureStorage,
    kTextureStorageMultisample,
    kInstancedDrawing,
    kSync,
    kTransformFeedback,
    kVertexArrays,
    kNumFunctionGroupIds,
  };

  // OpenGL platform SDK standard.
  enum GlApi {
    kDesktop,
    kEs,
    kWeb,

    kNumApis
  };

  // OpenGL profile type.
  enum GlProfile {
    kCompatibilityProfile,
    kCoreProfile,
  };

 private:
  class FunctionGroup;

  // Base wrapper class for checking initialization results. This needs to be
  // defined first because the macros in the public section use it.
  class WrapperBase {
   public:
    WrapperBase(const char* func_name, FunctionGroupId group)
        : ptr_(NULL),
          func_name_(func_name),
          group_(group) {
      // Add this to the vector of all known wrappers.
      GraphicsManager::AddWrapper(this);
    }
    const char* GetFuncName() const { return func_name_; }
    bool Init(GraphicsManager* gm) {
      const std::string gl_name = "gl" + std::string(func_name_);
      ptr_ = gm->Lookup(gl_name.c_str(), group_ == kCore);
      // Add the function to its group.
      gm->AddFunctionToGroup(group_, func_name_, ptr_);
      return ptr_ != NULL;
    }
    void Reset() { ptr_ = NULL; }

   protected:
    void* ptr_;

   private:
    const char* func_name_;
    FunctionGroupId group_;
  };

 public:
  GraphicsManager();

  // Returns the value of the passed Capability. Note that capabilities are only
  // queried once; the value returned from GL is cached. The comments in the
  // Capability enum indicate the canonical return type of GetCapabilityValue().
  // If a different type is requested for the return value this function will do
  // nothing but print a warning message and return a default constructed T.
  // Mostly thread-safe, with the caveats mentioned in the class comments
  // regarding tracing.
  template <typename T> const T GetCapabilityValue(Capability cap);

  // Returns true if the named function is available. This is used primarily
  // for testing. Thread-safe.
  bool IsFunctionAvailable(const std::string& function_name) const {
    return wrapped_function_names_.count(function_name) > 0;
  }

  // Returns true if the named function group is available. Thread-safe.
  bool IsFunctionGroupAvailable(FunctionGroupId group) const;

  // Enables or disables a specific function group.
  void EnableFunctionGroup(FunctionGroupId group, bool enable);

  // Checks if the passed extension is supported by this manager. Calling this
  // does not require a GL context to be bound, and is thread-safe.
  bool IsExtensionSupported(const std::string& name) const;

  // Sets/returns a flag indicating whether glGetError() should be called after
  // every OpenGL call to check for errors. The default is false.
  void EnableErrorChecking(bool enable) { is_error_checking_enabled_ = enable; }
  bool IsErrorCheckingEnabled() const { return is_error_checking_enabled_; }

  // Sets an output stream to use for tracing OpenGL calls. If the stream is
  // non-NULL, tracing is enabled, and a message is printed to the stream for
  // each OpenGL call. The default is a NULL stream.
  void SetTracingStream(std::ostream* s) { tracing_ostream_ = s; }
  std::ostream* GetTracingStream() const { return tracing_ostream_; }

  // Sets a prefix to be printed when tracing OpenGL calls.
  void SetTracingPrefix(const std::string& s) { tracing_prefix_ = s; }
  const std::string& GetTracingPrefix() const { return tracing_prefix_; }

  // Returns the GL version as an integer. Thread-safe.
  GLuint GetGlVersion() const { return gl_version_; }
  // Returns the GL version string. Thread-safe.
  const std::string& GetGlVersionString() const { return gl_version_string_; }
  // Returns the GL renderer string. Thread-safe.
  const std::string& GetGlRenderer() const { return gl_renderer_; }
  // Returns the GL API standard. Thread-safe.
  GlApi GetGlApiStandard() const { return gl_api_standard_; }
  // Returns the GL profile type. Thread-safe.
  GlProfile GetGlProfileType() const { return gl_profile_type_; }

  // Returns whether the given state table capability is valid given the
  // capabilities of the local GL platform.
  bool IsValidStateTableCapability(const StateTable::Capability cap) const {
    return valid_statetable_caps_.test(cap);
  }

#include "ion/gfx/glfunctions.inc"

 public:
  // Returns a terse string description of an OpenGL error code.
  static const char* ErrorString(GLenum error_code);

 protected:
  // Simple wrapper for a set of GL versions. The order of the fields must match
  // the ordering in GlApi, above. This can be used, for example to pass the set
  // of minimum versions that contain a certain functionality (as is done by
  // EnableFunctionGroupIfAvailable()). Since GraphicsManager is used by all
  // Ion platfoms, this is a convenient way to let a single structure specify
  // information for all GL flavors.
  struct GlVersions {
    GlVersions(GLuint desktop, GLuint es, GLuint web) {
      versions[kDesktop] = desktop;
      versions[kEs] = es;
      versions[kWeb] = web;
    }
    GLuint operator[](GlApi i) const { return versions[i]; }
    GLuint versions[kNumApis];
  };

  // Special version of the constructor that does not try to init GL functions.
  // It is protected so that only subclasses can use it.
  explicit GraphicsManager(GraphicsManager* gm);

  // The destructor is protected because all base::Referent classes must have
  // protected or private destructors.
  ~GraphicsManager() override;

  // This is used by MockGraphicsManager to install its own versions of the
  // OpenGL functions.
  void ReinitFunctions();

  // Initializes local GL information such as version and platform api standard.
  void InitGlInfo();

  // Verifies that a function group is available by checking if the GL version
  // for the current API is high enough, or if any of the passed comma-delimited
  // extensions are supported. Passing a 0 version means that it will not be
  // checked.
  //
  // The set of comma-delimited renderers in disabled_renderers will always have
  // the extension disabled; this is checked via a substring match. For example,
  // if |disabled_renderers| contains "Tribad, Polyworse" then if the current
  // GL_RENDERER string contains the strings "Tribad" or "Polyworse", the
  // function group will be disabled.
  virtual void EnableFunctionGroupIfAvailable(
      FunctionGroupId group, const GlVersions& versions,
      const std::string& extensions, const std::string& disabled_renderers);

 private:
  // Convenience typedef for the vector of initialized wrappers.
  typedef base::AllocVector<WrapperBase*> WrapperVec;
  // Convenience typedef for the map of function groups..
  typedef base::AllocVector<FunctionGroup> FunctionGroupVector;

  // Internal class for holding a WrapperVec in thread local storage.
  class WrapperVecHolder;

  // An internal class that helps track capability values.
  class CapabilityHelper;

  // This nested class is used to check for errors after invoking an OpenGL
  // function. The check is called from the destructor, so instance should be
  // created in the scope in which the OpenGL call is made. It is necessary to
  // do it this way rather than a straight function call because the return
  // value of the OpenGL call (which may be void) needs to be returned to the
  // caller after the error check.
  class ErrorChecker {
   public:
    ErrorChecker(GraphicsManager* gm, const std::string& func_call)
        : graphics_manager_(gm),
          func_call_(func_call) {
      graphics_manager_->CheckForErrors("before", func_call_);
    }
    ~ErrorChecker() {
      graphics_manager_->CheckForErrors("after", func_call_);
    }
   private:
    GraphicsManager* graphics_manager_;
    const std::string func_call_;
  };

  // Gets the thread local WrapperVecHolder to capture wrappers created during
  // construction.
  static WrapperVecHolder* GetWrapperVecHolder();

  // Adds a wrapper to check during initialization. This is called from the
  // constructor of each of the specific function wrappers (in the macros).
  static void AddWrapper(WrapperBase* wrapper);

  // Adds a function into a function group.
  void AddFunctionToGroup(
      FunctionGroupId group, const char* func_name, void* function);

  // Initializes the graphics mananger and optionally inits function pointers
  // from OpenGL.
  void Init(bool init_functions_from_gl);

  // Initializes all of the functions for the current platform. This logs an
  // error if any of the necessary functions is not available.
  void InitFunctions();

  // Adds to the set of wrapped function names.
  void AddWrappedFunctionName(const std::string& function_name) {
    wrapped_function_names_.insert(function_name);
  }

  // Calls glGetError() to check for errors, logging a message if one is found.
  void CheckForErrors(const char* when, const std::string& func_call);

  // Looks up a function by name and whether the function is core. Returns NULL
  // if not found. This is virtual so it can be redefined in
  // MockGraphicsManager.
  virtual void* Lookup(const char* name, bool is_core);

  // Vector storing all function wrappers. This is a copy of the wrappers
  // acquired by thread local during construction for use in ReinitFunctions.
  WrapperVec wrappers_;

  // Map of groups of OpenGL functions.
  FunctionGroupVector function_groups_;

  // Helper class for tracking capability values.
  std::unique_ptr<CapabilityHelper> capability_helper_;

  // Maintains a set of the names of functions that are wrapped by the manager,
  // primarily for testing.
  base::AllocSet<std::string> wrapped_function_names_;

  // The set of extensions that are supported by this manager.
  std::string extensions_;

  // Set to true when checking for errors after all OpenGL calls.
  bool is_error_checking_enabled_;

  // Output stream for tracing. NULL when tracing is disabled.
  std::ostream* tracing_ostream_;

  // A prefix that is printed out in front of all tracing messages.
  std::string tracing_prefix_;

  // Helper for printing values when tracing.
  TracingHelper tracing_helper_;

  // The name of the OpenGL renderer.
  std::string gl_renderer_;

  // The version string reported by OpenGL.
  std::string gl_version_string_;

  // The OpenGL version as an integer.
  GLuint gl_version_;

  // The OpenGL API standard.
  GlApi gl_api_standard_;

  // The OpenGL profile type.
  GlProfile gl_profile_type_;

  // Stores whether state table capabilities are valid, based on capabilities
  // of local GL platform.
  std::bitset<StateTable::kNumCapabilities> valid_statetable_caps_;
};

// Convenience typedef for shared pointer to a GraphicsManager.
typedef base::ReferentPtr<GraphicsManager>::Type GraphicsManagerPtr;

}  // namespace gfx
}  // namespace ion

#include "ion/gfx/graphicsmanagermacroundefs.h"

#endif  // ION_GFX_GRAPHICSMANAGER_H_
