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
#include "ion/gfx/glfunctiontypes.h"
#include "ion/gfx/graphicsmanagermacrodefs.h"
#include "ion/gfx/statetable.h"
#include "ion/gfx/tracinghelper.h"
#include "ion/gfx/tracingstream.h"
#include "ion/math/range.h"
#include "ion/portgfx/glcontext.h"
#include "ion/portgfx/glheaders.h"

namespace ion {
namespace gfx {

// GraphicsManager manages the graphics library for an application. It wraps
// calls to OpenGL functions, allowing extensions to be used easily, as well as
// allowing tracing of calls and error checking.
// GraphicsManager is generally thread agnostic, it can either be used
// consistenly from any single thread or protected by mutex from any thread.
// GraphicsManager owns a multi-context tracing stream, which clients
// can start and stop as needed (the Ion demos do this using an
// instance of gfxutils::Frame that is associated with the main
// thread).
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

#define ION_WRAP_GL_VALUE(name, sname, gl_enum, Type, init) k##name,
#define ION_WRAP_GL_LIST(name, sname, gl_enum, gl_count_enum) k##name,
  // GraphicsManager supports queries of local GL platform constants, such as
  // the maximum texture size or viewport dimensions. The comments in the enum
  // indicate the canonical return type of GetCapabilityValue(). If a different
  // type is requested for the return value a warning message will be printed
  // and the return value will be invalid.
  enum Constant {
    // See glconstants.inc for canonical return types.
#include "ion/gfx/glconstants.inc"
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
    kConstantCount,
  };

  // OpenGL features. This includes both groups of functions that expose
  // related functionality and higher-level features.
  enum FeatureId {
    // BindBufferBase and BindBufferRange.
    kBindBufferIndexed,
    // GL_MIN and GL_MAX as blend equations.
    kBlendMinMax,
    // Writing to gl_ClipDistance in the vertex shader. At least eight clip
    // distances must be supported.
    kClipDistance,
    kComputeShader,
    kCopyBufferSubData,
    kCore,  // Core OpenGL ES2 functions.
    kDebugLabel,
    kDebugMarker,
    kDebugOutput,
    // Default tessellation levels only available in desktop OpenGL.
    // See the spec of EXT_tessellation_shader.
    kDefaultTessellationLevels,
    kDepthTexture,
    // See https://www.opengl.org/registry/specs/EXT/discard_framebuffer.txt
    kDiscardFramebuffer,
    kDrawBuffer,
    kDrawBuffers,
    kDrawInstanced,
    kEglImage,  // This covers both OES_EGL_image and OES_EGL_image_external.
    kElementIndex32Bit,
    kFramebufferBlit,
    kFramebufferFoveated,
    // DRAW_FRAMEBUFFER and READ_FRAMEBUFFER are valid FBO binding targets.
    kFramebufferTargets,
    kFramebufferTextureLayer,
    kGeometryShader,
    kGetString,  // glGetStringi
    // See https://www.opengl.org/registry/specs/EXT/gpu_shader4.txt.
    kGpuShader4,
    // Multisampled rendering to non-multisampled targets, as specified in the
    // extension EXT_multisampled_render_to_texture.
    kImplicitMultisample,
    kInstancedArrays,
    kInvalidateFramebuffer,
    kMapBuffer,
    kMapBufferBase,
    kMapBufferRange,
    // Framebuffers can use 4 or more color attachments.
    kMultipleColorAttachments,
    // Whether multisampling can be toggled in the state table.
    kMultisampleCapability,
    // This is used for Apple devices running pre-es-3.0 device with the
    // APPLE_framebuffer_multisample extension.
    // See https://www.khronos.org/registry/gles/extensions/APPLE/
    // APPLE_framebuffer_multisample.txt.
    kMultisampleFramebufferResolve,
    kMultiview,
    kMultiviewImplicitMultisample,
    kPointSize,
    kProtectedTextures,
    // Whether RASTERIZER_DISCARD can be toggled in the state table.
    kRasterizerDiscardCapability,
    kRaw,
    kReadBuffer,
    kRenderbufferMultisample,
    // 
    // formats, then remove this.
    kRgba8,
    kSamplerObjects,
    kSampleShading,
    // Fetch current framebuffer content using EXT_shader_framebuffer_fetch.
    kShaderFramebufferFetch,
    // sampler2DShadow and TEXTURE_COMPARE_* texture parameters.
    kShadowSamplers,
    kStandardDerivatives,
    kSync,
    kTessellationShader,
    kTexture3d,
    kTextureArray1d,
    kTextureArray2d,
    kTextureBarrier,
    kTextureCubeMapArray,
    kTextureFilterAnisotropic,
    kTextureFoveated,
    kTextureLod,
    kTextureMipmapRange,
    kTextureMultisample,
    kTextureStorage,
    kTextureStorageMultisample,
    kTextureSwizzle,
    kTiledRendering,
    kTransformFeedback,
    kVertexArrays,
    kNumFeatureIds,
  };

  // OpenGL API flavor.
  enum GlFlavor {
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

  // Silently discards GL errors from any calls executed while it remains in
  // scope, without altering the existing error status.
  class ErrorSilencer {
   public:
    explicit ErrorSilencer(GraphicsManager* gm);
    ~ErrorSilencer();

   private:
    GraphicsManager* gm_;
    bool error_checking_was_enabled_;
  };

 private:
  class Feature;

  // Wraps a GL entry point and checks initialization results. This needs to be
  // defined first because the macros in the public section use it.
  class WrapperBase {
   public:
    WrapperBase(const char* func_name, FeatureId feature)
        : ptr_(nullptr), func_name_(func_name), feature_(feature) {}
    // Returns the function name without the "gl" prefix.
    const char* GetFuncName() const { return func_name_; }
    // Queries the given GlContext for the actual function pointer, then adds
    // the function to the appropriate feature.  Returns false if a function
    // pointer could not be obtained.
    bool Init(GraphicsManager* gm, const portgfx::GlContextPtr& gl_context,
              bool use_pure_loading);

   protected:
    void* ptr_;

   private:
    const char* func_name_;
    FeatureId feature_;
  };

 public:
  // GraphicsManager requires an active GL context at construction time.
  //
  // On some platforms (e.g. Android), Ion supports a legacy approach of loading
  // directly from the graphics library (e.g. with dlopen and dlsym) during
  // GraphicsManager construction.  Clients may opt in to this behavior by
  // specifying false for use_pure_loading.
  explicit GraphicsManager(bool use_pure_loading = true);

  // Returns the value of the passed implementation constant. Note that
  // constants are only queried once; the value returned from GL is cached.
  // The types in glconstants.inc indicate the canonical return type of
  // GetConstant(). If a different type is requested for the return
  // value, this function will do nothing but print a warning message and return
  // a default constructed T. Mostly thread-safe, with the caveats mentioned in
  // the class comments regarding tracing.
  template <typename T> T GetConstant(Constant c);

  // Initialize all of the OpenGL constants queryable by GetConstant above. This
  // will prevent future GL calls for any constant.
  void PopulateConstantCache();

  // Checks if the given high-level feature is supported and enabled. Support is
  // determined by checking the OpenGL version, extensions, available functions
  // and implementation-specific blocking. The results are cached, so calling
  // this function is very fast. By default, all supported features are enabled.
  // Applications can explicitly disable some supported features and later
  // re-enable them. It is not possible to force-enable features which are not
  // supported.
  bool IsFeatureAvailable(FeatureId feature) const;

  // Enables or disables a specific feature. Note that you can only enable
  // features which are detected as supported.
  void EnableFeature(FeatureId feature, bool enable);

  // Returns a string containing the state of all features.
  std::string GetFeatureDebugString() const;

  // Checks if the passed extension is supported by this manager. Calling this
  // does not require a GL context to be bound, and is thread-safe. Passing
  // a name without a vendor prefix will match any vendor variant of the
  // extension, while passing a name with a vendor prefix will only match that
  // vendor. For example, passing NV_gpu_shader5 will match only the Nvidia
  // version, while passing gpu_shader5 will match both NV_gpu_shader5 and
  // ARB_gpu_shader5.
  bool IsExtensionSupported(const std::string& name) const;

  // Sets/returns a flag indicating whether glGetError() should be called after
  // every OpenGL call to check for errors. The default is false.
  void EnableErrorChecking(bool enable);
  bool IsErrorCheckingEnabled() const { return is_error_checking_enabled_; }

  // Gives access to all tracing functionality for this graphics manager.
  gfx::TracingStream& GetTracingStream() { return tracing_stream_; }

  // Returns the GL version as an integer obtained by calculating
  // 10 * major_version + minor_version. Thread-safe. Note that if you want to
  // trigger different code paths based on the capabilities of the
  // implementation, it is better to use feature checks instead.
  GLuint GetGlVersion() const { return gl_version_; }
  // Returns the GL version string. Thread-safe.
  const std::string& GetGlVersionString() const { return gl_version_string_; }
  // Returns the GL renderer string. Thread-safe.
  const std::string& GetGlRenderer() const { return gl_renderer_; }
  // Returns the GL API flavor (Desktop, ES or WebGL). Thread-safe.
  GlFlavor GetGlFlavor() const { return gl_flavor_; }
  // Returns the GL profile type (core or compatibility). Thread-safe.
  GlProfile GetGlProfileType() const { return gl_profile_type_; }

  // Returns whether the given state table capability is valid given the
  // capabilities of the local GL platform.
  bool IsValidStateTableCapability(const StateTable::Capability cap) const {
    return valid_statetable_caps_.test(cap);
  }

#include "ion/gfx/glfunctions.inc"

 public:
  // Special case wrapper for glGetError. This is in the header so that we have
  // access to the macro ION_PROFILE_GL_FUNC.
  GLenum GetError() {
    ION_PROFILE_GL_FUNC("GetError", "");
    tracing_stream_ << "GetError()\n";
    // Even when error checking is disabled, there might still be an unretrieved
    // error code in last_error_code_. This happens if we make a call that
    // generates an error while in error checking mode and then disable error
    // checking before calling GetError() in the app.
    if (last_error_code_ != GL_NO_ERROR) {
      GLenum error = last_error_code_;
      last_error_code_ = GL_NO_ERROR;
      return error;
    } else {
      return (*gl_get_error_)();
    }
  }

  // Returns a terse string description of an OpenGL error code.
  static const char* ErrorString(GLenum error_code);

 protected:
  // Simple wrapper for a set of GL versions. The order of the fields must match
  // the ordering in GlFlavor, above. This can be used, for example to pass the
  // set of minimum versions that contain a certain functionality (as is done by
  // EnableFunctionGroupIfAvailable()). Since GraphicsManager is used by all
  // Ion platfoms, this is a convenient way to let a single structure specify
  // information for all GL flavors.
  //
  // Version numbers in this structure are encoded as 10 * major_version +
  // minor_version, so OpenGL 3.3 is encoded as 33 and WebGL 2 is encoded
  // as 20.
  struct GlVersions {
    GlVersions(GLuint desktop, GLuint es, GLuint web) {
      versions[kDesktop] = desktop;
      versions[kEs] = es;
      versions[kWeb] = web;
    }
    GLuint operator[](GlFlavor i) const { return versions[i]; }
    GLuint versions[kNumApis];
  };

  // The destructor is protected because all base::Referent classes must have
  // protected or private destructors.
  ~GraphicsManager() override;

  // Initializes local GL information such as version and platform API standard.
  void InitGlInfo();

  // Returns true if the named function is available. This is used primarily
  // for testing. Thread-safe.
  bool IsFunctionAvailable(const std::string& function_name) const {
    return wrapped_function_names_.count(function_name) > 0 ||
        (function_name == "GetError" && gl_get_error_ != nullptr);
  }

  // Marks the specified feature as supported or unsupported.
  void SetFeatureSupported(FeatureId feature, bool supported);

  // Verifies that a feature is supported by checking if the GL version for the
  // current API standard is high enough, or if any of the passed
  // comma-delimited extensions are supported. No functional checks are
  // performed. A version of 0 for a given API standard specified in the
  // GlVersions structure means the version check will always fail for that API
  // standard.
  //
  // If an extension is referred to without a prefix, any prefixed
  // variant (i.e. ARB_, EXT_, KHR_, QCOM_, etc.) will match.
  // If the extension has the prefix explicitly specified, then only that
  // particular flavor of extension will match. For example: if we request
  // ARB_tessellation_shader, and EXT_tessellation_shader is available,
  // we won't get a match. OTOH, requesting simply "tesselation_shader"
  // will match if either EXT_- or ARB_-variant is available.
  //
  // The set of comma-delimited renderers in disabled_renderers will always have
  // the feature disabled; this is checked via a substring match. For example,
  // if |disabled_renderers| contains "Tribad, Polyworse" then if the current
  // GL_RENDERER string contains the strings "Tribad" or " Polyworse", the
  // feature will be disabled.
  void SetFeatureSupportedIf(FeatureId feature, const GlVersions& versions,
                             const std::string& extensions,
                             const std::string& disabled_renderers);

  // Clears the cached values of constant queries.
  void ClearConstantCache();

 private:
  // Convenience typedef for the map of function groups..
  typedef base::AllocVector<Feature> FeatureVector;
  // Type of pointer for glGetError.
  typedef GLenum (*GetErrorPtr)();

  // An internal class that caches constant values queried from OpenGL.
  class ConstantCache;

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
          func_call_(func_call) {}
    ~ErrorChecker() {
      graphics_manager_->CheckForErrors(func_call_);
    }
   private:
    GraphicsManager* graphics_manager_;
    const std::string func_call_;
  };

#if !ION_PRODUCTION
  struct FeatureNameEntry {
    FeatureId feature;
    const char* name;
  };
#endif

  // Returns the GL version as an integer: major_version * 10 + minor_version.
  static int ParseGlVersionString(const std::string& version_string);

  // Adds a function into a function group.
  void AddFunctionToFeature(
      FeatureId feature, const char* func_name, void* function);

  // Initializes the graphics mananger and its function pointers from OpenGL.
  void Init(bool use_pure_loading);

  // Initializes all of the functions for the current platform. This logs an
  // error if any of the necessary functions is not available.
  void InitFunctions(bool use_pure_loading);

  // Adds to the set of wrapped function names.
  void AddWrappedFunctionName(const std::string& function_name) {
    wrapped_function_names_.insert(function_name);
  }

  // Calls glGetError() to check for errors, logging a message if one is found.
  void CheckForErrors(const std::string& func_call);

  // Performs the checks in SetFeatureSupportedIf(). Used for testing.
  bool CheckSupport(const GlVersions& versions, const std::string& extensions,
                    const std::string& disabled_renderers);

  // Perform consistancy checks on feature_names_;
  void ValidateFeatureNames() const;

  // Function pointer for glGetError.
  GetErrorPtr gl_get_error_;

  // Map of OpenGL features.
  FeatureVector features_;

#if !ION_PRODUCTION
  // Table of FeatureId names for debug string production.
  static const FeatureNameEntry feature_names_[];
#endif

  // Helper class for storing queried GL implementation limits.
  std::unique_ptr<ConstantCache> constant_cache_;

  // Maintains a set of the names of functions that are wrapped by the manager,
  // primarily for testing.
  base::AllocSet<std::string> wrapped_function_names_;

  // The set of extensions that are supported by this manager.
  std::string extensions_;

  // Set to true when checking for errors after all OpenGL calls.
  bool is_error_checking_enabled_;
  // Last error from glGetError().
  GLenum last_error_code_;

  // Output stream for tracing, disabled by default.
  gfx::TracingStream tracing_stream_;

  // Helper for printing values when tracing.
  TracingHelper tracing_helper_;

  // The name of the OpenGL renderer.
  std::string gl_renderer_;

  // The version string reported by OpenGL.
  std::string gl_version_string_;

  // The OpenGL version as an integer.
  GLuint gl_version_;

  // The OpenGL flavor (desktop, ES or Web).
  GlFlavor gl_flavor_;

  // The OpenGL profile type.
  GlProfile gl_profile_type_;

  // Stores whether state table capabilities are valid, based on capabilities
  // of local GL platform.
  std::bitset<StateTable::kNumCapabilities> valid_statetable_caps_;

  friend class ErrorSilencer;
  friend class GraphicsManagerTest;
  friend class ThreadedGraphicsManagerTest;
  FRIEND_TEST(GraphicsManagerTest, ParseGlVersionString);
};

// glconstants.inc defines some constants as unsigned. This specialization
// allows the calling of GetConstant with the Type defined there - it simply
// translates the unsigned queries to signed ones and casts the result.
template <> unsigned int GraphicsManager::GetConstant(Constant c);

// Convenience typedef for shared pointer to a GraphicsManager.
using GraphicsManagerPtr = base::SharedPtr<GraphicsManager>;

}  // namespace gfx
}  // namespace ion

#include "ion/gfx/graphicsmanagermacroundefs.h"

#endif  // ION_GFX_GRAPHICSMANAGER_H_
