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

#include "ion/gfx/graphicsmanager.h"

#include <string.h>  // For strcmp().

#include <algorithm>
#include <mutex>  // NOLINT(build/c++11)
#include <vector>

#include "ion/base/allocationmanager.h"
#include "ion/base/logging.h"
#include "ion/base/stringutils.h"
#include "ion/base/threadlocalobject.h"
#include "ion/base/variant.h"
#include "ion/portgfx/isextensionsupported.h"
#include "absl/memory/memory.h"

namespace ion {
namespace gfx {

//-----------------------------------------------------------------------------
//
// Constant queries.
//
//-----------------------------------------------------------------------------

class ConstantValue {
 public:
  typedef base::Variant<GLint, GLfloat, GLuint64, math::Range1f, math::Range1i,
                        math::Point2i, math::Vector3i,
                        GraphicsManager::ShaderPrecision,
                        std::vector<GLenum>> ConstantVariant;
  typedef void(*Getter)(GraphicsManager* gm, ConstantValue* cv);

  ConstantValue() : uninitialized_(true) {}
  ConstantValue(GLenum enum1_in, GLenum enum2_in, const Getter& getter_in)
      : enum1_(enum1_in),
        enum2_(enum2_in),
        getter_(getter_in),
        uninitialized_(false) {}

  ConstantValue& operator=(const ConstantValue& other) {
    enum1_ = other.enum1_;
    enum2_ = other.enum2_;
    getter_ = other.getter_;
    uninitialized_ = false;
    return *this;
  }

  void Initialize(GraphicsManager* gm) {
    DCHECK(!uninitialized_);
    std::call_once(populated_flag_, getter_, gm, this);
  }

  // Returns the value of this, but only queries the passed GraphicsManager for
  // the value if it is not already set.
  const ConstantVariant& GetValue(GraphicsManager* gm) {
    Initialize(gm);
    return value_;
  }

  template <typename T>
  void SetValue(T value) {
    std::call_once(populated_flag_, [this, value]() {
      value_.Set(value);
    });
  }

  // The below functions are used to query particular capability values.
  static void GetEnumVector(GraphicsManager* gm, ConstantValue* cv) {
    GraphicsManager::ErrorSilencer silencer(gm);
    GLint count = 0;
    gm->GetIntegerv(cv->enum2_, &count);
    std::vector<GLenum> values(count);
    if (count)
      gm->GetIntegerv(cv->enum1_, reinterpret_cast<GLint*>(values.data()));
    cv->value_.Set(values);
  }

  template <typename T>
  static void Query(GraphicsManager* gm, ConstantValue* cv) {}

  static void GetShaderPrecision(GraphicsManager* gm, ConstantValue* cv) {
    GraphicsManager::ErrorSilencer silencer(gm);
    GLint range[2], precision = 0;
    range[0] = range[1] = 0;
    gm->GetShaderPrecisionFormat(cv->enum1_, cv->enum2_, range,
                                 &precision);
    cv->value_.Set(GraphicsManager::ShaderPrecision(
        math::Range1i(range[0], range[1]), precision));
  }

 private:
  GLenum enum1_;
  GLenum enum2_;  // Second enum for shader precision, count enum for vectors.
  ConstantVariant value_;
  Getter getter_;
  std::once_flag populated_flag_;
  bool uninitialized_;
};

template <>
void ConstantValue::Query<GLint>(GraphicsManager* gm, ConstantValue* cv) {
  GraphicsManager::ErrorSilencer silencer(gm);
  GLint value = 0;
  gm->GetIntegerv(cv->enum1_, &value);
  cv->value_.Set(value);
}

// Treat unsigned integral limits as signed, since there are no query functions
// for unsigned integers in OpenGL.
template <>
void ConstantValue::Query<GLuint>(GraphicsManager* gm, ConstantValue* cv) {
  GraphicsManager::ErrorSilencer silencer(gm);
  GLint value = 0;
  gm->GetIntegerv(cv->enum1_, &value);
  cv->value_.Set(value);
}

template <>
void ConstantValue::Query<GLuint64>(GraphicsManager* gm, ConstantValue* cv) {
  GraphicsManager::ErrorSilencer silencer(gm);
  GLuint64 value = 0;
  gm->GetInteger64v(cv->enum1_, reinterpret_cast<GLint64*>(&value));
  cv->value_.Set(value);
}

template <>
void ConstantValue::Query<GLfloat>(GraphicsManager* gm, ConstantValue* cv) {
  GraphicsManager::ErrorSilencer silencer(gm);
  GLfloat val = 0;
  gm->GetFloatv(cv->enum1_, &val);
  cv->value_.Set(val);
}

template <>
void ConstantValue::Query<math::Range1f>(GraphicsManager* gm,
                                         ConstantValue* cv) {
  GraphicsManager::ErrorSilencer silencer(gm);
  GLfloat val[2];
  val[0] = val[1] = 0;
  gm->GetFloatv(cv->enum1_, val);
  cv->value_.Set(math::Range1f(val[0], val[1]));
}

template <>
void ConstantValue::Query<math::Point2i>(GraphicsManager* gm,
                                         ConstantValue* cv) {
  GraphicsManager::ErrorSilencer silencer(gm);
  GLint val[2];
  val[0] = val[1] = 0;
  gm->GetIntegerv(cv->enum1_, val);
  cv->value_.Set(math::Point2i(val[0], val[1]));
}

template <>
void ConstantValue::Query<math::Vector3i>(GraphicsManager* gm,
                                          ConstantValue* cv) {
  GraphicsManager::ErrorSilencer silencer(gm);
  math::Vector3i vector;
  for (uint32_t i = 0; i < 3U; ++i) {
    gm->GetIntegeri_v(cv->enum1_, i, &vector[i]);
  }
  cv->value_.Set(vector);
}

class GraphicsManager::ConstantCache {
 public:
  ConstantCache() {
    // Generate most entries using gllimits.inc.
#define ION_WRAP_GL_VALUE(name, s, gl_enum, Type, i) \
  constants_[k##name] = \
      ConstantValue(gl_enum, GL_NONE, ConstantValue::Query<Type>);
#define ION_WRAP_GL_LIST(name, s, gl_enum, gl_count_enum) \
  constants_[k##name] = \
      ConstantValue(gl_enum, gl_count_enum, ConstantValue::GetEnumVector);
#include "ion/gfx/glconstants.inc"

    // Handle shader precision formats separately.
#define ION_DOUBLE_CAP(index, enum1, enum2, getter) \
  constants_[index] = ConstantValue(enum1, enum2, ConstantValue::getter)

    ION_DOUBLE_CAP(kFragmentShaderHighFloatPrecisionFormat, GL_FRAGMENT_SHADER,
                   GL_HIGH_FLOAT, GetShaderPrecision);
    ION_DOUBLE_CAP(kFragmentShaderHighIntPrecisionFormat, GL_FRAGMENT_SHADER,
                   GL_HIGH_INT, GetShaderPrecision);
    ION_DOUBLE_CAP(kFragmentShaderLowFloatPrecisionFormat, GL_FRAGMENT_SHADER,
                   GL_LOW_FLOAT, GetShaderPrecision);
    ION_DOUBLE_CAP(kFragmentShaderLowIntPrecisionFormat, GL_FRAGMENT_SHADER,
                   GL_LOW_INT, GetShaderPrecision);
    ION_DOUBLE_CAP(kFragmentShaderMediumFloatPrecisionFormat,
                   GL_FRAGMENT_SHADER, GL_MEDIUM_FLOAT, GetShaderPrecision);
    ION_DOUBLE_CAP(kFragmentShaderMediumIntPrecisionFormat, GL_FRAGMENT_SHADER,
                   GL_MEDIUM_INT, GetShaderPrecision);
    ION_DOUBLE_CAP(kVertexShaderHighFloatPrecisionFormat, GL_VERTEX_SHADER,
                   GL_HIGH_FLOAT, GetShaderPrecision);
    ION_DOUBLE_CAP(kVertexShaderHighIntPrecisionFormat, GL_VERTEX_SHADER,
                   GL_HIGH_INT, GetShaderPrecision);
    ION_DOUBLE_CAP(kVertexShaderLowFloatPrecisionFormat, GL_VERTEX_SHADER,
                   GL_LOW_FLOAT, GetShaderPrecision);
    ION_DOUBLE_CAP(kVertexShaderLowIntPrecisionFormat, GL_VERTEX_SHADER,
                   GL_LOW_INT, GetShaderPrecision);
    ION_DOUBLE_CAP(kVertexShaderMediumFloatPrecisionFormat, GL_VERTEX_SHADER,
                   GL_MEDIUM_FLOAT, GetShaderPrecision);
    ION_DOUBLE_CAP(kVertexShaderMediumIntPrecisionFormat, GL_VERTEX_SHADER,
                   GL_MEDIUM_INT, GetShaderPrecision);

#undef ION_SINGLE_CAP
#undef ION_DOUBLE_CAP
  }

  void InitializeAll(GraphicsManager* gm) {
    for (auto& constant : constants_) {
      constant.Initialize(gm);
    }
  }

  // Gets the constant value using a query.
  const ConstantValue::ConstantVariant& GetValue(
      GraphicsManager* gm, GraphicsManager::Constant cap) {
    return constants_[cap].GetValue(gm);
  }
  // Sets a fixed value and prevents the OpenGL query from being executed.
  template <typename T>
  void SetValue(GraphicsManager::Constant cname, T value) {
    constants_[cname].SetValue(value);
  }

 private:
  ConstantValue constants_[GraphicsManager::kConstantCount];
};

//-----------------------------------------------------------------------------
//
// GraphicsManager::Feature represents a subset of OpenGL functionality that
// includes zero or more functions. A feature is complete when all functions
// that are part of it exist in the implementation.
//
//-----------------------------------------------------------------------------

class GraphicsManager::Feature {
 public:
  Feature()
    : supported_(false),
      enabled_(false),
      available_functions_(),
      missing_functions_() {}
  ~Feature() {}
  void AddFunction(const std::string& name, void* function) {
#if defined(ION_COVERAGE)
    available_functions_.push_back(name);
#else  // COV_NF_START
    if (function == nullptr)
      missing_functions_.push_back(name);
    else
      available_functions_.push_back(name);
#endif  // COV_NF_END
  }
  bool IsAvailable() const { return IsSupported() && IsEnabled(); }
  bool IsEnabled() const { return enabled_; }
  void SetEnabled(bool enable) { enabled_ = enable; }
  bool IsSupported() const { return supported_ && missing_functions_.empty(); }
  void SetSupported(bool supported) { supported_ = supported; }
#if !defined(ION_COVERAGE)  // COV_NF_START
  const std::vector<std::string>& GetMissingFunctions() const {
    return missing_functions_;
  }
#endif  // COV_NF_END

 private:
  // The result of support checks done in InitGlInfo() that does not take
  // into account whether any functions are missing.
  bool supported_;
  // Enable status set by the GraphicsManager or the application. By default,
  // all supported features are enabled. Enabling an unsupported feature has
  // no effect.
  bool enabled_;
  std::vector<std::string> available_functions_;
  std::vector<std::string> missing_functions_;
};

#if !ION_PRODUCTION
#define FEATURE_ENTRY(id) { id, #id }
const GraphicsManager::FeatureNameEntry
    GraphicsManager::feature_names_[kNumFeatureIds] = {
        FEATURE_ENTRY(kBindBufferIndexed),
        FEATURE_ENTRY(kBlendMinMax),
        FEATURE_ENTRY(kClipDistance),
        FEATURE_ENTRY(kComputeShader),
        FEATURE_ENTRY(kCopyBufferSubData),
        FEATURE_ENTRY(kCore),
        FEATURE_ENTRY(kDebugLabel),
        FEATURE_ENTRY(kDebugMarker),
        FEATURE_ENTRY(kDebugOutput),
        FEATURE_ENTRY(kDefaultTessellationLevels),
        FEATURE_ENTRY(kDepthTexture),
        FEATURE_ENTRY(kDiscardFramebuffer),
        FEATURE_ENTRY(kDrawBuffer),
        FEATURE_ENTRY(kDrawBuffers),
        FEATURE_ENTRY(kDrawInstanced),
        FEATURE_ENTRY(kEglImage),
        FEATURE_ENTRY(kElementIndex32Bit),
        FEATURE_ENTRY(kFramebufferBlit),
        FEATURE_ENTRY(kFramebufferFoveated),
        FEATURE_ENTRY(kFramebufferTargets),
        FEATURE_ENTRY(kFramebufferTextureLayer),
        FEATURE_ENTRY(kGeometryShader),
        FEATURE_ENTRY(kGetString),
        FEATURE_ENTRY(kGpuShader4),
        FEATURE_ENTRY(kImplicitMultisample),
        FEATURE_ENTRY(kInstancedArrays),
        FEATURE_ENTRY(kInvalidateFramebuffer),
        FEATURE_ENTRY(kMapBuffer),
        FEATURE_ENTRY(kMapBufferBase),
        FEATURE_ENTRY(kMapBufferRange),
        FEATURE_ENTRY(kMultipleColorAttachments),
        FEATURE_ENTRY(kMultisampleCapability),
        FEATURE_ENTRY(kMultisampleFramebufferResolve),
        FEATURE_ENTRY(kMultiview),
        FEATURE_ENTRY(kMultiviewImplicitMultisample),
        FEATURE_ENTRY(kPointSize),
        FEATURE_ENTRY(kProtectedTextures),
        FEATURE_ENTRY(kRasterizerDiscardCapability),
        FEATURE_ENTRY(kRaw),
        FEATURE_ENTRY(kReadBuffer),
        FEATURE_ENTRY(kRenderbufferMultisample),
        FEATURE_ENTRY(kRgba8),
        FEATURE_ENTRY(kSamplerObjects),
        FEATURE_ENTRY(kSampleShading),
        FEATURE_ENTRY(kShaderFramebufferFetch),
        FEATURE_ENTRY(kShadowSamplers),
        FEATURE_ENTRY(kStandardDerivatives),
        FEATURE_ENTRY(kSync),
        FEATURE_ENTRY(kTessellationShader),
        FEATURE_ENTRY(kTexture3d),
        FEATURE_ENTRY(kTextureArray1d),
        FEATURE_ENTRY(kTextureArray2d),
        FEATURE_ENTRY(kTextureBarrier),
        FEATURE_ENTRY(kTextureCubeMapArray),
        FEATURE_ENTRY(kTextureFilterAnisotropic),
        FEATURE_ENTRY(kTextureFoveated),
        FEATURE_ENTRY(kTextureLod),
        FEATURE_ENTRY(kTextureMipmapRange),
        FEATURE_ENTRY(kTextureMultisample),
        FEATURE_ENTRY(kTextureStorage),
        FEATURE_ENTRY(kTextureStorageMultisample),
        FEATURE_ENTRY(kTextureSwizzle),
        FEATURE_ENTRY(kTiledRendering),
        FEATURE_ENTRY(kTransformFeedback),
        FEATURE_ENTRY(kVertexArrays),
};
#endif

void GraphicsManager::ValidateFeatureNames() const {
#if !ION_PRODUCTION
  for (int i = 0; i < kNumFeatureIds; ++i) {
    DCHECK_EQ(i, feature_names_[i].feature);
    if (i > 0) {
      // Not using std::tolower here to avoid any possible dependency on the
      // currently set locale.
      auto compare_fn = [](char a, char b) -> bool {
        char lower_offset = 'a' - 'A';
        int lower_a = (a >= 'A' && a <= 'Z') ? a + lower_offset : a;
        int lower_b = (b >= 'A' && b <= 'Z') ? b + lower_offset : b;
        return lower_a < lower_b;
      };
      const char* previous = feature_names_[i-1].name;
      const char* current = feature_names_[i].name;
      DCHECK(std::lexicographical_compare(
          previous, previous + std::strlen(previous), current,
          current + std::strlen(current), compare_fn))
          << "Enums must be in alphabetical order.";
    }
  }
#endif
}

std::string GraphicsManager::GetFeatureDebugString() const {
#if !ION_PRODUCTION
  size_t max_length = 0;
  for (int i = 0; i < kNumFeatureIds; ++i)
    max_length = std::max(max_length, std::strlen(feature_names_[i].name));
  std::stringstream ss;
  for (int i = 0; i < kNumFeatureIds; ++i) {
    const char* name = feature_names_[i].name;
    const std::string padding(max_length - std::strlen(name), ' ');
    ss << padding << name << ": "
       << "available: " << (features_[i].IsAvailable() ? "yes" : " no")
       << ", supported: " << (features_[i].IsSupported() ? "yes" : " no")
       << ", enabled: " << (features_[i].IsEnabled() ?  "yes" : " no")
       << "\n";
  }
  return ss.str();
#else
  return std::string();
#endif
}

// Specifies whether GL error checking should be turned on or off by default.
// This is behind a separate compile-time option, since error checking may
// serialize draw calls, leading to severe performance degradation.
#if ION_CHECK_GL_ERRORS
static const bool kErrorCheckingDefault = true;
#else
static const bool kErrorCheckingDefault = false;
#endif

//-----------------------------------------------------------------------------
//
// GraphicsManager::ErrorSilencer temporarily silences any GL errors that may
// be generated by calls made while it is in scope. This is useful when the
// GraphicsManager needs to do some GL calls internally and their errors should
// not be visible to the application.
//
//-----------------------------------------------------------------------------
GraphicsManager::ErrorSilencer::ErrorSilencer(GraphicsManager* gm)
    : gm_(gm),
      error_checking_was_enabled_(gm->IsErrorCheckingEnabled()) {
  if (gm_->last_error_code_ == GL_NO_ERROR)
    gm_->last_error_code_ = (*gm_->gl_get_error_)();
  if (error_checking_was_enabled_)
    gm_->EnableErrorChecking(false);
}

GraphicsManager::ErrorSilencer::~ErrorSilencer() {
  (*gm_->gl_get_error_)();
  if (error_checking_was_enabled_)
    gm_->EnableErrorChecking(true);
}

//-----------------------------------------------------------------------------
//
// GraphicsManager implementation.
//
//-----------------------------------------------------------------------------

GraphicsManager::GraphicsManager(bool use_pure_loading)
    : gl_get_error_(nullptr),
      features_(GetAllocator(), kNumFeatureIds, Feature()),
      wrapped_function_names_(*this),
      is_error_checking_enabled_(kErrorCheckingDefault),
      last_error_code_(GL_NO_ERROR),
      gl_version_(20),
      gl_flavor_(kEs),
      gl_profile_type_(kCompatibilityProfile) {
  Init(use_pure_loading);
  DCHECK(constant_cache_);
}

GraphicsManager::~GraphicsManager() {
}

void GraphicsManager::Init(bool use_pure_loading) {
  ValidateFeatureNames();
  InitFunctions(use_pure_loading);

#if !defined(ION_COVERAGE)  // COV_NF_START
  if (!features_[kCore].IsAvailable()) {
    LOG(ERROR) << "***ION: Some required OpenGL functions could not be found. "
               << "The following functions are missing from your OpenGL "
               << "installation:";
    const std::vector<std::string>& missing =
        features_[kCore].GetMissingFunctions();
    for (size_t i = 0; i < missing.size(); i++) {
      LOG(ERROR) << "  " << missing[i].c_str();
    }
  }
#endif  // COV_NF_END
}

template <typename T>
T GraphicsManager::GetConstant(Constant c) {
  DCHECK(constant_cache_);
  const ConstantValue::ConstantVariant& value =
      constant_cache_->GetValue(this, c);
  const T& val = value.Get<T>();
  if (base::IsInvalidReference(val))
    LOG(WARNING) << "Invalid type requested for constant " << c;
  return val;
}
template <>
unsigned int GraphicsManager::GetConstant(Constant c) {
  return static_cast<unsigned int>(GetConstant<int>(c));
}

// Instantiate for supported types.
template int GraphicsManager::GetConstant<int>(Constant c);
template uint64_t GraphicsManager::GetConstant<uint64_t>(Constant c);
template float GraphicsManager::GetConstant<float>(Constant c);
template math::Range1f
    GraphicsManager::GetConstant<math::Range1f>(Constant c);
template math::Point2i
    GraphicsManager::GetConstant<math::Point2i>(Constant c);
template math::Vector3i
    GraphicsManager::GetConstant<math::Vector3i>(Constant c);
template GraphicsManager::ShaderPrecision
    GraphicsManager::GetConstant<GraphicsManager::ShaderPrecision>(
        Constant c);
template std::vector<GLenum>
    GraphicsManager::GetConstant<std::vector<GLenum>>(Constant c);

void GraphicsManager::PopulateConstantCache() {
  constant_cache_->InitializeAll(this);
}

bool GraphicsManager::IsFeatureAvailable(FeatureId feature) const {
  return features_[feature].IsAvailable();
}

bool GraphicsManager::IsExtensionSupported(const std::string& name) const {
  return portgfx::IsExtensionSupported(name, extensions_);
}

void GraphicsManager::EnableErrorChecking(bool enable) {
  is_error_checking_enabled_ = enable;
#if !ION_PRODUCTION
  // Invariants:
  // When error checking is enabled, the next error code to be returned to the
  // app is always in last_error_code_.
  // When error checking is disabled, error codes are returned directly from
  // glGetError(), unless there is an error code unretrieved by the app that
  // was stored when we had error checking enabled.
  if (enable) {
    // If there is already an error code stored, we have to discard the current
    // code from OpenGL, since the spec says that further errors are not
    // recorded until the last error code is retrieved.
    if (last_error_code_ == GL_NO_ERROR) {
      last_error_code_ = (*gl_get_error_)();
    } else {
      (*gl_get_error_)();
    }
  }
#endif
}

void GraphicsManager::AddFunctionToFeature(FeatureId feature,
                                           const char* func_name,
                                           void* function) {
  features_[feature].AddFunction(func_name, function);
}

void GraphicsManager::EnableFeature(FeatureId feature, bool enable) {
  features_[feature].SetEnabled(enable);

  // Turn on/off state table caps for function groups that implement the
  // corresponding capability.
  switch (feature) {
    case kClipDistance:
      for (size_t i = StateTable::kClipDistance0;
           i < StateTable::kClipDistance0 + StateTable::kClipDistanceCount;
           ++i) {
        valid_statetable_caps_.set(static_cast<StateTable::Capability>(i),
                                   enable);
      }
      break;
    case kDebugOutput:
      valid_statetable_caps_.set(StateTable::kDebugOutputSynchronous, enable);
      break;
    case kMultisampleCapability:
      valid_statetable_caps_.set(StateTable::kMultisample, enable);
      break;
    case kSampleShading:
      valid_statetable_caps_.set(StateTable::kSampleShading, enable);
      break;
    case kRasterizerDiscardCapability:
      valid_statetable_caps_.set(StateTable::kRasterizerDiscard, enable);
      break;
    default:
      break;
  }

  // The set of enabled features may affect which constant queries are actually
  // made, so recreate the constant cache. This is inside an if, so that when we
  // call EnableFeature() to enable all supported features in InitGlInfo(), it
  // doesn't recreate the cache for every call.
  if (constant_cache_) {
    ClearConstantCache();
  }
}

void GraphicsManager::InitFunctions(bool use_pure_loading) {
  portgfx::GlContextPtr gl_context = portgfx::GlContext::GetCurrent();
  CHECK(gl_context) << "GraphicsManager created without a valid GL context "
                    << "or FakeGlContext";

#define ION_WRAP_GL_FUNC(group, name, return_type, typed_args, args, trace) \
  if (name##_wrapper_.Init(this, gl_context, use_pure_loading)) {           \
    AddWrappedFunctionName(name##_wrapper_.GetFuncName());                  \
  }
#define ION_WRAP_SKIP_GetError 1

#include "ion/gfx/glfunctions.inc"

#undef ION_WRAP_SKIP_GetError
#undef ION_WRAP_GL_FUNC

  uint32_t flags = portgfx::GlContext::kProcAddressCore;
  if (use_pure_loading) {
    flags |= portgfx::GlContext::kProcAddressPure;
  }
  gl_get_error_ = reinterpret_cast<GetErrorPtr>(
      gl_context->GetProcAddress("glGetError", flags));
  DCHECK(gl_get_error_) << "Unable to obtain the glGetError proc address.  Try "
                           "toggling use_pure_loading on your platform.";
  InitGlInfo();
}

void GraphicsManager::InitGlInfo() {
  // Some calls here may generate errors. Do not let the application see them.
  ErrorSilencer silencer(this);
  constant_cache_.reset();

  // glGetIntegerv(GL_MAJOR_VERSION) is (surprisingly) not supported on all
  // platforms (e.g. mac), so we use the GL_VERSION string instead.
  //
  // Try to get the local OpenGL version by looking for major.minor in the
  // version string.
  const char* version_string =
      reinterpret_cast<const char*>(GetString(GL_VERSION));
  if (version_string) {
    std::string version(version_string);
    gl_version_string_ = version;
    std::size_t webgl_pos = version.find("WebGL");
    if (webgl_pos != std::string::npos) {
      // asm.js modifies the version string by putting it in parentheses and
      // prepending the matching OpenGL ES version, which gives us the wrong
      // version number.
      gl_flavor_ = kWeb;
      version = version.substr(webgl_pos, std::string::npos);
    } else if (version.find("GL ES") != std::string::npos ||
               version.find("GL/ES") != std::string::npos ||
               version.find("GL / ES") != std::string::npos) {
      gl_flavor_ = kEs;
    } else {
      gl_flavor_ = kDesktop;
    }
    gl_version_ = ParseGlVersionString(version);
    // If parsing fails, fall back to 20 as a reasonable default.
    gl_version_ = (gl_version_ == 0) ? 20 : gl_version_;
  }

  if (const char* renderer_string =
      reinterpret_cast<const char*>(GetString(GL_RENDERER)))
    gl_renderer_ = renderer_string;

  // Query GL_CONTEXT_PROFILE_MASK to check OpenGL profile type.
  gl_profile_type_ = kCoreProfile;
  if (gl_flavor_ == kDesktop) {
    gl_profile_type_ = kCompatibilityProfile;
    GLint mask = 0;
    GetIntegerv(GL_CONTEXT_PROFILE_MASK, &mask);
    if (mask & GL_CONTEXT_CORE_PROFILE_BIT)
      gl_profile_type_ = kCoreProfile;
  }

  // Check this here, since we need it to retrieve extensions.
  SetFeatureSupportedIf(kGetString, GlVersions(30U, 30U, 0U), "", "");

  if (const char* extensions =
          reinterpret_cast<const char*>(GetString(GL_EXTENSIONS)))
    extensions_ = extensions;

  if (extensions_.empty() && features_[kGetString].IsSupported()) {
    GLint count = 0;
    GetIntegerv(GL_NUM_EXTENSIONS, &count);
    std::vector<std::string> extension_list;
    for (GLint i = 0; i < count; ++i) {
      if (const char* extension =
              reinterpret_cast<const char*>(GetStringi(GL_EXTENSIONS, i))) {
        extension_list.push_back(std::string(extension));
      }
    }
    extensions_ = base::JoinStrings(extension_list, " ");
  }

  // State table capabilities default to valid.
  valid_statetable_caps_.reset();
  valid_statetable_caps_.flip();

  // At this point, all features except kGetString are marked as unsupported.
  // Detect support for features based on GL version checks, extension presence
  // and renderer blacklists. Additional functional checks go after that.
  SetFeatureSupportedIf(kCore, GlVersions(10U, 20U, 10U), "", "");

  SetFeatureSupportedIf(kBlendMinMax, GlVersions(14U, 30U, 20U),
                        "EXT_blend_minmax", "");
  SetFeatureSupportedIf(kClipDistance, GlVersions(31U, 0U, 0U),
                        "clip_distance,EXT_clip_cull_distance", "");
  SetFeatureSupportedIf(kComputeShader, GlVersions(43U, 31U, 0U),
                        "ARB_compute_shader", "");
  SetFeatureSupportedIf(kCopyBufferSubData, GlVersions(31U, 30U, 0U),
                        "copy_buffer", "");
  SetFeatureSupportedIf(kDebugLabel, GlVersions(0U, 0U, 0U),
                        "EXT_debug_label", "");
  SetFeatureSupportedIf(kDebugMarker, GlVersions(0U, 0U, 0U),
                        "EXT_debug_marker", "");
  SetFeatureSupportedIf(kDebugOutput, GlVersions(43U, 32U, 0U),
                        "ARB_debug_output,KHR_debug,WEBGL_debug", "");
  SetFeatureSupportedIf(kDefaultTessellationLevels, GlVersions(40U, 0U, 0U),
                        "ARB_tessellation_shader", "");
  SetFeatureSupportedIf(kDepthTexture, GlVersions(14U, 0U, 0U),
                        "depth_texture", "");
  SetFeatureSupportedIf(kDiscardFramebuffer, GlVersions(0U, 0U, 0U),
                        "EXT_discard_framebuffer", "");
  SetFeatureSupportedIf(kDrawBuffer, GlVersions(10U, 0U, 0U), "", "");
  SetFeatureSupportedIf(kDrawBuffers, GlVersions(30U, 30U, 20U),
                        "draw_buffers", "");
  SetFeatureSupportedIf(kEglImage, GlVersions(0U, 0U, 0U), "EGL_image", "");
  SetFeatureSupportedIf(kElementIndex32Bit, GlVersions(12U, 30U, 0U),
                        "element_index_uint", "");
  // NaCl includes glBlitFramebuffer in its CHROMIUM_framebuffer_multisample
  // extension, which is unspecified and apparently does not match either the
  // EXT or ANGLE variant, because those do not include the function and mention
  // that EXT/ANGLE_framebuffer_blit is required.
  SetFeatureSupportedIf(kFramebufferBlit, GlVersions(20U, 30U, 20U),
                        "framebuffer_blit,CHROMIUM_framebuffer_multisample",
                        "");
  SetFeatureSupportedIf(kFramebufferFoveated, GlVersions(0U, 0U, 0U),
                        "QCOM_framebuffer_foveated", "");
  SetFeatureSupportedIf(kFramebufferTargets, GlVersions(31U, 30U, 20U), "", "");
  SetFeatureSupportedIf(kFramebufferTextureLayer, GlVersions(30U, 30U, 20U),
                        "geometry_shader4,geometry_program4", "");
  // The EXT version of the geometry shader extension is incompatible with the
  // core feature; we only support the core/ARB variant.
  SetFeatureSupportedIf(kGeometryShader, GlVersions(32U, 32U, 0U),
                        "ARB_geometry_shader4", "");
  SetFeatureSupportedIf(kGpuShader4, GlVersions(30U, 30U, 0U),
                        "gpu_shader4", "");
  // The IMG variant of this extension uses incompatible enum values.
  SetFeatureSupportedIf(kImplicitMultisample, GlVersions(0U, 0U, 0U),
                        "EXT_multisampled_render_to_texture", "");
  // The draw_instanced functions are also defined by instanced_arrays.
  SetFeatureSupportedIf(kDrawInstanced, GlVersions(33U, 30U, 20U),
                        "draw_instanced,instanced_arrays", "");
  SetFeatureSupportedIf(kInstancedArrays, GlVersions(33U, 30U, 20U),
                        "instanced_arrays", "");
  SetFeatureSupportedIf(kInvalidateFramebuffer, GlVersions(43U, 30U, 0U), "",
                        "");
  // Core OpenGL ES 3.0 has glMapBufferRange, glUnmapBuffer and
  // glGetBufferPointerv, but is not guaranteed to have glMapBuffer, so we
  // split out this function into a separate group.
  SetFeatureSupportedIf(kMapBuffer, GlVersions(15U, 0U, 0U),
                        "mapbuffer,vertex_buffer_object",
                        "Vivante GC1000,VideoCore IV HW");
  SetFeatureSupportedIf(kMapBufferBase, GlVersions(15U, 30U, 0U),
                        "mapbuffer,vertex_buffer_object",
                        "Vivante GC1000,VideoCore IV HW");
  SetFeatureSupportedIf(kMapBufferRange, GlVersions(30U, 30U, 0U),
                        "map_buffer_range",
                        "Vivante GC1000,VideoCore IV HW");
  SetFeatureSupportedIf(kMultipleColorAttachments, GlVersions(31U, 30U, 20U),
                        "NV_fbo_color_attachments", "");
  // GL_MULTISAMPLE was available in OpenGL ES 1.1, but was removed in ES 2.0.
  SetFeatureSupportedIf(kMultisampleCapability, GlVersions(13U, 0U, 0U),
                        "ARB_multisample,EXT_multisample_compatibility", "");
  SetFeatureSupportedIf(kMultisampleFramebufferResolve, GlVersions(0U, 0U, 0U),
                        "APPLE_framebuffer_multisample", "");
  SetFeatureSupportedIf(kMultiview, GlVersions(0U, 0U, 0U), "multiview2", "");
  SetFeatureSupportedIf(kMultiviewImplicitMultisample, GlVersions(0U, 0U, 0U),
                        "multiview_multisampled_render_to_texture", "");
  SetFeatureSupportedIf(kPointSize, GlVersions(10U, 0U, 0U), "", "");
  SetFeatureSupportedIf(kProtectedTextures, GlVersions(0U, 0U, 0U),
                        "protected_textures", "");
  SetFeatureSupportedIf(kReadBuffer, GlVersions(10U, 30U, 20U), "", "");
  SetFeatureSupportedIf(kRenderbufferMultisample, GlVersions(30U, 30U, 20U),
                        "framebuffer_multisample", "");
  SetFeatureSupportedIf(kRgba8, GlVersions(20U, 30U, 20U), "OES_rgb8_rgba8",
                        "");
  SetFeatureSupportedIf(kSamplerObjects, GlVersions(33U, 30U, 20U),
                        "sampler_objects", "Mali ,Mali-,SwiftShader");
  // The extension can be called ARB_sample_shading or OES_sample_shading. Both
  // are compatible with the core version.
  SetFeatureSupportedIf(kSampleShading, GlVersions(40U, 32U, 0U),
                        "sample_shading", "");
  SetFeatureSupportedIf(kShaderFramebufferFetch, GlVersions(0U, 0U, 0U),
                        "EXT_shader_framebuffer_fetch", "");
  SetFeatureSupportedIf(kShadowSamplers, GlVersions(14U, 30U, 20U),
                        "EXT_shadow_samplers", "");
  SetFeatureSupportedIf(kStandardDerivatives, GlVersions(20U, 30U, 0U),
                        "OES_standard_derivatives", "");
  SetFeatureSupportedIf(kSync, GlVersions(32U, 30U, 20U), "sync", "");
  SetFeatureSupportedIf(kTessellationShader, GlVersions(40U, 32U, 0U),
                        "tessellation_shader", "");
  SetFeatureSupportedIf(kTexture3d, GlVersions(13U, 30U, 20U),
                        "texture_3d", "");
  SetFeatureSupportedIf(kTextureArray1d, GlVersions(30U, 0U, 0U),
                        "texture_array", "");
  SetFeatureSupportedIf(kTextureArray2d, GlVersions(30U, 30U, 20U),
                        "texture_array", "");
  SetFeatureSupportedIf(kTextureBarrier, GlVersions(45U, 0U, 0U),
                        "texture_barrier", "");
  SetFeatureSupportedIf(kTextureCubeMapArray, GlVersions(40U, 32U, 0U),
                        "texture_cube_map_array", "");
  SetFeatureSupportedIf(kTextureFilterAnisotropic, GlVersions(46U, 0U, 0U),
                        "EXT_texture_filter_anisotropic", "");
  SetFeatureSupportedIf(kTextureFoveated, GlVersions(0U, 0U, 0U),
                        "QCOM_texture_foveated", "");
  SetFeatureSupportedIf(kTextureLod, GlVersions(12U, 30U, 20U), "", "");
  SetFeatureSupportedIf(kTextureMipmapRange, GlVersions(32U, 30U, 20U), "", "");
  SetFeatureSupportedIf(kTextureMultisample, GlVersions(32U, 31U, 0U),
                        "texture_multisample", "");
  SetFeatureSupportedIf(kTextureStorage, GlVersions(42U, 30U, 20U),
                        "texture_storage", "");
  SetFeatureSupportedIf(kTextureStorageMultisample, GlVersions(42U, 31U, 0U),
                        "texture_storage_multisample", "");
  SetFeatureSupportedIf(kTextureSwizzle, GlVersions(33U, 30U, 0U),
                        "texture_swizzle", "");
  SetFeatureSupportedIf(kTiledRendering, GlVersions(0U, 0U, 0U),
                        "QCOM_tiled_rendering", "");
  SetFeatureSupportedIf(kTransformFeedback, GlVersions(30U, 30U, 0U),
                        "transform_feedback", "");
  SetFeatureSupportedIf(kRasterizerDiscardCapability, GlVersions(30U, 30U, 0U),
                        "transform_feedback", "");
  SetFeatureSupportedIf(kVertexArrays, GlVersions(30U, 30U, 20U),
                        "vertex_array_object", "Internet Explorer");

  // This feature contains functions that are wrapped in GraphicsManager,
  // but have no further support in Ion. It is never enabled.
  SetFeatureSupportedIf(kRaw, GlVersions(0U, 0U, 0U), "", "");

  // On some platforms vertex arrays are improperly advertised. Ensure
  // GenVertexArrays succeeds. We can only perform this test, however, if a
  // valid function pointer exists and is enabled.
  if (features_[kVertexArrays].IsSupported()) {
    // Just to be safe check that the most basic functionality works.
    GLuint id = 0U;
    GenVertexArrays(1U, &id);
    // Delete the array if it's valid.
    if (id)
      DeleteVertexArrays(1U, &id);
    else
      SetFeatureSupported(kVertexArrays, false);
  }

  // Enable all supported features. This must come after all support checks.
  const size_t feature_count = features_.size();
  for (size_t i = 0; i < feature_count; ++i) {
    FeatureId id = static_cast<FeatureId>(i);
    EnableFeature(id, features_[i].IsSupported());
  }
#if defined(__ANDROID__)
  // Disable VAOs on Android by default.
  // See b/29642897 and b/29391940.
  EnableFeature(kVertexArrays, false);
#endif
  ClearConstantCache();
}

const char* GraphicsManager::ErrorString(GLenum error_code) {
  switch (error_code) {
    case GL_INVALID_ENUM: return "invalid enumerant";
    case GL_INVALID_VALUE: return "invalid value";
    case GL_INVALID_OPERATION: return "invalid operation";
    case GL_OUT_OF_MEMORY: return "out of memory";
    case GL_INVALID_FRAMEBUFFER_OPERATION:
      return "invalid framebuffer operation";
    default: return "unknown error";
  }
}

void GraphicsManager::CheckForErrors(const std::string& func_call) {
  const GLenum error = (*gl_get_error_)();
  if (error != GL_NO_ERROR) {
    TracingHelper helper;
    tracing_stream_ << "GetError() returned "
                    << helper.ToString("GLenum", error) << "\n";
    if (!tracing_stream_.IsLogging()) {
      LOG(ERROR) << "*** GL error after call to "
                 << func_call << ": " << ErrorString(error) << "\n";
    }
    if (last_error_code_ == GL_NO_ERROR)
      last_error_code_ = error;
  }
}

void GraphicsManager::SetFeatureSupported(FeatureId feature, bool supported) {
  features_[feature].SetSupported(supported);
}

bool GraphicsManager::CheckSupport(const GlVersions& versions,
                                   const std::string& extensions,
                                   const std::string& disabled_renderers) {
  for (const auto& renderer : base::SplitString(disabled_renderers, ",")) {
    if (gl_renderer_.find(renderer) != std::string::npos) {
      return false;
    }
  }

  // If the GL version is high enough, we don't need to check extensions.
  if (versions.versions[gl_flavor_] &&
      gl_version_ >= versions.versions[gl_flavor_]) return true;

  // Check extensions.
  std::vector<std::string> extension_names = base::SplitString(extensions, ",");
  for (const auto& extension_name : extension_names) {
    if (IsExtensionSupported(extension_name)) return true;
  }
  return false;
}

void GraphicsManager::SetFeatureSupportedIf(
    GraphicsManager::FeatureId feature, const GlVersions& versions,
    const std::string& extensions, const std::string& disabled_renderers) {
  SetFeatureSupported(feature, CheckSupport(versions, extensions,
                                            disabled_renderers));
}

void GraphicsManager::ClearConstantCache() {
  using math::Vector3i;
  constant_cache_ = absl::make_unique<ConstantCache>();
  // Set some constants which we know to be zero due to missing or disabled
  // features.
  if (!IsFeatureAvailable(kClipDistance)) {
    constant_cache_->SetValue(kMaxClipDistances, 0);
  }
  if (!IsFeatureAvailable(kComputeShader)) {
    constant_cache_->SetValue(kMaxCombinedComputeUniformComponents, 0);
    constant_cache_->SetValue(kMaxComputeImageUniforms, 0);
    constant_cache_->SetValue(kMaxComputeSharedMemorySize, 0);
    constant_cache_->SetValue(kMaxComputeTextureImageUnits, 0);
    constant_cache_->SetValue(kMaxComputeUniformBlocks, 0);
    constant_cache_->SetValue(kMaxComputeUniformComponents, 0);
    constant_cache_->SetValue(kMaxComputeWorkGroupCount, Vector3i::Zero());
    constant_cache_->SetValue(kMaxComputeWorkGroupInvocations, 0);
    constant_cache_->SetValue(kMaxComputeWorkGroupSize, Vector3i::Zero());
  }
  if (!IsFeatureAvailable(kDebugOutput)) {
    constant_cache_->SetValue(kMaxDebugLoggedMessages, 0);
    constant_cache_->SetValue(kMaxDebugMessageLength, 0);
  }
  if (!IsFeatureAvailable(kDrawBuffers)) {
    constant_cache_->SetValue(kMaxDrawBuffers, 0);
  }
  if (!IsFeatureAvailable(kMultipleColorAttachments)) {
    // A single color attachment is always supported.
    constant_cache_->SetValue(kMaxColorAttachments, 1);
  }
  if (!IsFeatureAvailable(kMultiview)) {
    constant_cache_->SetValue(kMaxViews, 0);
  }
  if (!IsFeatureAvailable(kTexture3d)) {
    constant_cache_->SetValue(kMax3dTextureSize, 0);
  }
  if (!IsFeatureAvailable(kTextureArray2d)) {
    constant_cache_->SetValue(kMaxArrayTextureLayers, 0);
  }
  if (!IsFeatureAvailable(kTextureFilterAnisotropic)) {
    constant_cache_->SetValue(kMaxTextureMaxAnisotropy, 0);
  }
  if (!IsFeatureAvailable(kTransformFeedback)) {
    constant_cache_->SetValue(kMaxTransformFeedbackInterleavedComponents, 0);
    constant_cache_->SetValue(kMaxTransformFeedbackSeparateAttribs, 0);
    constant_cache_->SetValue(kMaxTransformFeedbackSeparateComponents, 0);
    constant_cache_->SetValue(kTransformFeedbackVaryingMaxLength, 0);
  }
}

// Unfortunately, GL_MAJOR_VERSION and GL_MINOR_VERSION queries are not
// available until OpenGL 3.0, so we need to do some manual parsing. If a
// tertiary "patch" version number exists, it is ignored.
// static
int GraphicsManager::ParseGlVersionString(const std::string& version_string) {
  const size_t dot_pos = version_string.find('.');
  if (dot_pos != std::string::npos && dot_pos > 0U) {
    int major = 0;
    int minor = 0;
    major = version_string[dot_pos - 1U] - '0';
    minor = version_string[dot_pos + 1U] - '0';
    return major * 10U + minor;
  }
  return 0;
}

bool GraphicsManager::WrapperBase::Init(GraphicsManager* gm,
                                        const portgfx::GlContextPtr& gl_context,
                                        bool use_pure_loading) {
  const std::string gl_name = "gl" + std::string(func_name_);
  uint32_t flags = 0;
  if (feature_ == kCore) {
    flags |= portgfx::GlContext::kProcAddressCore;
  }
  if (use_pure_loading) {
    flags |= portgfx::GlContext::kProcAddressPure;
  }
  ptr_ = gl_context->GetProcAddress(gl_name.c_str(), flags);
  // Add the function to its group.
  gm->AddFunctionToFeature(feature_, func_name_, ptr_);
  return ptr_ != nullptr;
}

}  // namespace gfx
}  // namespace ion
