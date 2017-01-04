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
#include <vector>

#include "ion/base/allocationmanager.h"
#include "ion/base/logging.h"
#include "ion/base/once.h"
#include "ion/base/stringutils.h"
#include "ion/base/threadlocalobject.h"
#include "ion/base/variant.h"
#include "ion/portgfx/isextensionsupported.h"

namespace ion {
namespace gfx {

//-----------------------------------------------------------------------------
//
// Capability queries.
//
//-----------------------------------------------------------------------------

class CapabilityValue {
 public:
  typedef base::Variant<GLint, GLfloat, math::Range1f, math::Range1i,
                        GraphicsManager::ShaderPrecision,
                        std::vector<GLint> > CapabilityVariant;
  typedef void(*Getter)(GraphicsManager* gm, CapabilityValue* cv);

  CapabilityValue() : uninitialized_(true) {}
  CapabilityValue(GLenum enum1_in, GLenum enum2_in, const Getter& getter_in)
      : enum1_(enum1_in),
        enum2_(enum2_in),
        getter_(getter_in),
        uninitialized_(false) {}

  CapabilityValue& operator=(const CapabilityValue& other) {
    // It is not valid to overwrite a capability value with another in case
    // GetValue might have been called, or may be called concurrently.
    // Therefore an assignment is only allowed to a default constructed
    // instance, which was never safe to call GetValue on anyway.
    DCHECK(uninitialized_ && !other.uninitialized_);
    enum1_ = other.enum1_;
    enum2_ = other.enum2_;
    getter_ = other.getter_;
    uninitialized_ = false;
    return *this;
  }

  // Returns the value of this, but only queries the passed GraphicsManager for
  // the value if it is not already set.
  const CapabilityVariant& GetValue(GraphicsManager* gm) {
    DCHECK(!uninitialized_);
    populated_flag_.CallOnce(std::bind(getter_, gm, this));
    return value_;
  }

  // The below functions are used to query particular capability values.
  static void GetIntVector(GraphicsManager* gm, CapabilityValue* cv) {
    GraphicsManager::ErrorSilencer silencer(gm);
    GLint count = 0;
    gm->GetIntegerv(cv->enum2_, &count);
    std::vector<GLint> values(count);
    if (count)
      gm->GetIntegerv(cv->enum1_, &values[0]);
    cv->value_.Set(values);
  }

  static void GetFloat(GraphicsManager* gm, CapabilityValue* cv) {
    GraphicsManager::ErrorSilencer silencer(gm);
    GLfloat val = 0;
    gm->GetFloatv(cv->enum1_, &val);
    cv->value_.Set(val);
  }

  static void GetFloatRange(GraphicsManager* gm, CapabilityValue* cv) {
    GraphicsManager::ErrorSilencer silencer(gm);
    GLfloat val[2];
    val[0] = val[1] = 0;
    gm->GetFloatv(cv->enum1_, val);
    cv->value_.Set(math::Range1f(val[0], val[1]));
  }

  static void GetInt(GraphicsManager* gm, CapabilityValue* cv) {
    GraphicsManager::ErrorSilencer silencer(gm);
    GLint val = 0;
    gm->GetIntegerv(cv->enum1_, &val);
    cv->value_.Set(val);
  }

  static void GetIntRange(GraphicsManager* gm, CapabilityValue* cv) {
    GraphicsManager::ErrorSilencer silencer(gm);
    GLint val[2];
    val[0] = val[1] = 0;
    gm->GetIntegerv(cv->enum1_, val);
    cv->value_.Set(math::Range1i(val[0], val[1]));
  }

  static void GetShaderPrecision(GraphicsManager* gm, CapabilityValue* cv) {
    GraphicsManager::ErrorSilencer silencer(gm);
    GLint range[2], precision = 0;
    range[0] = range[1] = 0;
    gm->GetShaderPrecisionFormat(cv->enum1_, cv->enum2_, range,
                                 &precision);
    cv->value_.Set(GraphicsManager::ShaderPrecision(
        math::Range1i(range[0], range[1]), precision));
  }

  // Special cases go here.
  static void GetMaxColorAttachments(GraphicsManager* gm, CapabilityValue* cv) {
    GLint val = 1;
    if (gm->IsFeatureAvailable(GraphicsManager::kMultipleColorAttachments)) {
      gm->GetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &val);
    }
    cv->value_.Set(std::max(1, val));
  }

 private:
  GLenum enum1_;
  GLenum enum2_;  // Second enum for shader precision, count enum for vectors.
  CapabilityVariant value_;
  Getter getter_;
  base::OnceFlag populated_flag_;
  bool uninitialized_;
};

class GraphicsManager::CapabilityHelper {
 public:
  CapabilityHelper() {
#define ION_SINGLE_CAP(index, enum, getter) \
  capabilities_[index] = CapabilityValue(enum, GL_NONE, CapabilityValue::getter)
#define ION_DOUBLE_CAP(index, enum1, enum2, getter) \
  capabilities_[index] = CapabilityValue(enum1, enum2, CapabilityValue::getter)

    ION_SINGLE_CAP(kAliasedLineWidthRange, GL_ALIASED_LINE_WIDTH_RANGE,
                   GetFloatRange);
    ION_SINGLE_CAP(kAliasedPointSizeRange, GL_ALIASED_POINT_SIZE_RANGE,
                   GetFloatRange);
    ION_DOUBLE_CAP(kCompressedTextureFormats, GL_COMPRESSED_TEXTURE_FORMATS,
                   GL_NUM_COMPRESSED_TEXTURE_FORMATS, GetIntVector);
    ION_SINGLE_CAP(kImplementationColorReadFormat,
                   GL_IMPLEMENTATION_COLOR_READ_FORMAT, GetInt);
    ION_SINGLE_CAP(kImplementationColorReadType,
                   GL_IMPLEMENTATION_COLOR_READ_TYPE, GetInt);
    ION_SINGLE_CAP(kMax3dTextureSize, GL_MAX_3D_TEXTURE_SIZE, GetInt);
    ION_SINGLE_CAP(kMaxArrayTextureLayers, GL_MAX_ARRAY_TEXTURE_LAYERS, GetInt);
    ION_SINGLE_CAP(kMaxClipDistances, GL_MAX_CLIP_DISTANCES, GetInt);
    ION_SINGLE_CAP(kMaxColorAttachments, GL_MAX_COLOR_ATTACHMENTS,
                   GetMaxColorAttachments);
    ION_SINGLE_CAP(kMaxCombinedTextureImageUnits,
                   GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, GetInt);
    ION_SINGLE_CAP(kMaxCubeMapTextureSize, GL_MAX_CUBE_MAP_TEXTURE_SIZE,
                   GetInt);
    ION_SINGLE_CAP(kMaxDebugLoggedMessages, GL_MAX_DEBUG_LOGGED_MESSAGES,
                   GetInt);
    ION_SINGLE_CAP(kMaxDebugMessageLength, GL_MAX_DEBUG_MESSAGE_LENGTH, GetInt);
    ION_SINGLE_CAP(kMaxDrawBuffers, GL_MAX_DRAW_BUFFERS, GetInt);
    ION_SINGLE_CAP(kMaxFragmentUniformComponents,
                   GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, GetInt);
    ION_SINGLE_CAP(kMaxFragmentUniformVectors, GL_MAX_FRAGMENT_UNIFORM_VECTORS,
                   GetInt);
    ION_SINGLE_CAP(kMaxRenderbufferSize, GL_MAX_RENDERBUFFER_SIZE, GetInt);
    ION_SINGLE_CAP(kMaxSampleMaskWords, GL_MAX_SAMPLE_MASK_WORDS, GetInt);
    ION_SINGLE_CAP(kMaxSamples, GL_MAX_SAMPLES, GetInt);
    ION_SINGLE_CAP(kMaxTextureImageUnits, GL_MAX_TEXTURE_IMAGE_UNITS, GetInt);
    ION_SINGLE_CAP(kMaxTextureMaxAnisotropy, GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT,
                   GetFloat);
    ION_SINGLE_CAP(kMaxTextureSize, GL_MAX_TEXTURE_SIZE, GetInt);
    ION_SINGLE_CAP(kMaxTransformFeedbackBuffers,
                   GL_MAX_TRANSFORM_FEEDBACK_BUFFERS, GetInt);
    ION_SINGLE_CAP(kMaxTransformFeedbackInterleavedComponents,
                   GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS, GetInt);
    ION_SINGLE_CAP(kMaxTransformFeedbackSeparateAttribs,
                   GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, GetInt);
    ION_SINGLE_CAP(kMaxTransformFeedbackSeparateComponents,
                   GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS, GetInt);
    ION_SINGLE_CAP(kMaxVaryingVectors, GL_MAX_VARYING_VECTORS, GetInt);
    ION_SINGLE_CAP(kMaxVertexAttribs, GL_MAX_VERTEX_ATTRIBS, GetInt);
    ION_SINGLE_CAP(kMaxVertexTextureImageUnits,
                   GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, GetInt);
    ION_SINGLE_CAP(kMaxVertexUniformComponents,
                   GL_MAX_VERTEX_UNIFORM_COMPONENTS, GetInt);
    ION_SINGLE_CAP(kMaxVertexUniformVectors, GL_MAX_VERTEX_UNIFORM_VECTORS,
                   GetInt);
    ION_SINGLE_CAP(kMaxViewportDims, GL_MAX_VIEWPORT_DIMS, GetIntRange);
    ION_SINGLE_CAP(kMaxViews, GL_MAX_VIEWS_OVR, GetInt);
    ION_DOUBLE_CAP(kShaderBinaryFormats, GL_SHADER_BINARY_FORMATS,
                   GL_NUM_SHADER_BINARY_FORMATS, GetIntVector);
    ION_SINGLE_CAP(kTransformFeedbackVaryingMaxLength,
                   GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH, GetInt);

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

  ~CapabilityHelper() {}
  const CapabilityValue::CapabilityVariant& GetCapabilityValue(
      GraphicsManager* gm, GraphicsManager::Capability cap) {
    return capabilities_[cap].GetValue(gm);
  }

 private:
  CapabilityValue capabilities_[GraphicsManager::kCapabilityCount];
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
  FEATURE_ENTRY(kClipDistance),
  FEATURE_ENTRY(kCopyBufferSubData),
  FEATURE_ENTRY(kCore),
  FEATURE_ENTRY(kDebugLabel),
  FEATURE_ENTRY(kDebugMarker),
  FEATURE_ENTRY(kDebugOutput),
  FEATURE_ENTRY(kDepthTexture),
  FEATURE_ENTRY(kDrawBuffer),
  FEATURE_ENTRY(kDrawBuffers),
  FEATURE_ENTRY(kDrawInstanced),
  FEATURE_ENTRY(kEglImage),
  FEATURE_ENTRY(kElementIndex32Bit),
  FEATURE_ENTRY(kFramebufferBlit),
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
  FEATURE_ENTRY(kSamplerObjects),
  FEATURE_ENTRY(kSampleShading),
  FEATURE_ENTRY(kShaderFramebufferFetch),
  FEATURE_ENTRY(kStandardDerivatives),
  FEATURE_ENTRY(kSync),
  FEATURE_ENTRY(kTexture3d),
  FEATURE_ENTRY(kTextureArray1d),
  FEATURE_ENTRY(kTextureArray2d),
  FEATURE_ENTRY(kTextureBarrier),
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
      DCHECK(std::lexicographical_compare(feature_names_[i-1].name.begin(),
                                          feature_names_[i-1].name.end(),
                                          feature_names_[i].name.begin(),
                                          feature_names_[i].name.end(),
                                          compare_fn))
          << "Enums must be in alphabetical order.";
    }
  }
#endif
}

std::string GraphicsManager::GetFeatureDebugString() const {
#if !ION_PRODUCTION
  size_t max_length = 0;
  for (int i = 0; i < kNumFeatureIds; ++i)
    max_length = std::max(max_length, feature_names_[i].name.length());
  std::stringstream ss;
  for (int i = 0; i < kNumFeatureIds; ++i) {
    const std::string& name = feature_names_[i].name;
    const std::string padding(max_length - name.length(), ' ');
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

GraphicsManager::GraphicsManager()
    : gl_get_error_(nullptr),
      features_(GetAllocator(), kNumFeatureIds, Feature()),
      capability_helper_(new CapabilityHelper),
      wrapped_function_names_(*this),
      is_error_checking_enabled_(kErrorCheckingDefault),
      last_error_code_(GL_NO_ERROR),
      gl_version_(20),
      gl_api_standard_(kEs),
      gl_profile_type_(kCompatibilityProfile) {
  Init();
}

GraphicsManager::~GraphicsManager() {
}

void GraphicsManager::Init() {
  ValidateFeatureNames();
  InitFunctions();

#if !defined(ION_COVERAGE)  // COV_NF_START
  if (!features_[kCore].IsAvailable()) {
    LOG(ERROR) << "***ION: Some required OpenGL functions could not be "
               << "found. Either there is no valid OpenGL context, or the "
               << "following functions are missing from your OpenGL "
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
const T GraphicsManager::GetCapabilityValue(Capability cap) {
  const CapabilityValue::CapabilityVariant& value =
      capability_helper_->GetCapabilityValue(this, cap);
  const T& val = value.Get<T>();
  if (base::IsInvalidReference(val))
    LOG(WARNING) << "Invalid type requested for capability " << cap;
  return val;
}
// Specialize for supported types.
template const int GraphicsManager::GetCapabilityValue<int>(
    Capability cap);
template const float GraphicsManager::GetCapabilityValue<float>(
    Capability cap);
template const math::Range1f
    GraphicsManager::GetCapabilityValue<math::Range1f>(Capability cap);
template const math::Range1i
    GraphicsManager::GetCapabilityValue<math::Range1i>(Capability cap);
template const GraphicsManager::ShaderPrecision
    GraphicsManager::GetCapabilityValue<GraphicsManager::ShaderPrecision>(
        Capability cap);
template const std::vector<int>
    GraphicsManager::GetCapabilityValue<std::vector<int> >(Capability cap);

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

void GraphicsManager::AddFunctionToFeature(FeatureId group,
                                           const char* func_name,
                                           void* function) {
  features_[group].AddFunction(func_name, function);
}

void GraphicsManager::EnableFeature(FeatureId group, bool enable) {
  features_[group].SetEnabled(enable);

  // Turn on/off state table caps for function groups that implement the
  // corresponding capability.
  switch (group) {
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
}

void GraphicsManager::InitFunctions() {
  portgfx::VisualPtr visual = portgfx::Visual::GetCurrent();
  if (!visual) {
    return;
  }

#define ION_WRAP_GL_FUNC(group, name, return_type, typed_args, args, trace) \
  if (name##_wrapper_.Init(this, visual)) {                                 \
    AddWrappedFunctionName(name##_wrapper_.GetFuncName());                  \
  }
#define ION_WRAP_SKIP_GetError 1

#include "ion/gfx/glfunctions.inc"

#undef ION_WRAP_SKIP_GetError
#undef ION_WRAP_GL_FUNC

  gl_get_error_ =
      reinterpret_cast<GetErrorPtr>(visual->GetProcAddress("glGetError", true));
  InitGlInfo();
}

void GraphicsManager::InitGlInfo() {
  // Some calls here may generate errors. Do not let the application see them.
  ErrorSilencer silencer(this);

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
    if (version.find("WebGL") != std::string::npos) {
      version = "2.0";
      gl_api_standard_ = kWeb;
    } else if (version.find("GL ES") != std::string::npos ||
               version.find("GL/ES") != std::string::npos ||
               version.find("GL / ES") != std::string::npos) {
      gl_api_standard_ = kEs;
    } else {
      gl_api_standard_ = kDesktop;
    }
    const size_t dot_pos = version.find('.');
    if (dot_pos != std::string::npos && dot_pos > 0U) {
      int major = 0;
      int minor = 0;
      major = version[dot_pos - 1U] - '0';
      minor = version[dot_pos + 1U] - '0';
      gl_version_ = major * 10U + minor;
    }
  }

  if (const char* renderer_string =
      reinterpret_cast<const char*>(GetString(GL_RENDERER)))
    gl_renderer_ = renderer_string;

  // Query GL_CONTEXT_PROFILE_MASK to check OpenGL profile type.
  gl_profile_type_ = kCoreProfile;
  if (gl_api_standard_ == kDesktop) {
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
  SetFeatureSupportedIf(kClipDistance, GlVersions(31U, 0U, 0U),
                        "clip_distance", "");
  SetFeatureSupportedIf(kCopyBufferSubData, GlVersions(31U, 30U, 0U),
                        "copy_buffer", "");
  SetFeatureSupportedIf(kDebugLabel, GlVersions(0U, 0U, 0U),
                        "EXT_debug_label", "");
  SetFeatureSupportedIf(kDebugMarker, GlVersions(0U, 0U, 0U),
                        "EXT_debug_marker", "");
  SetFeatureSupportedIf(kDebugOutput, GlVersions(43U, 32U, 0U),
                        "ARB_debug_output,KHR_debug,WEBGL_debug", "");
  SetFeatureSupportedIf(kDepthTexture, GlVersions(14U, 0U, 0U),
                        "depth_texture", "");
  SetFeatureSupportedIf(kDrawBuffer, GlVersions(10U, 0U, 0U), "", "");
  SetFeatureSupportedIf(kDrawBuffers, GlVersions(30U, 30U, 20U),
                        "draw_buffers", "");
  SetFeatureSupportedIf(kEglImage, GlVersions(0U, 0U, 0U), "EGL_image", "");
  SetFeatureSupportedIf(kElementIndex32Bit, GlVersions(12U, 30U, 0U),
                        "element_index_uint", "");
  SetFeatureSupportedIf(kFramebufferBlit, GlVersions(20U, 30U, 20U),
                        "framebuffer_blit", "");
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
  SetFeatureSupportedIf(kRenderbufferMultisample, GlVersions(20U, 30U, 20U),
                        "framebuffer_multisample", "");
  SetFeatureSupportedIf(kSamplerObjects, GlVersions(33U, 30U, 20U),
                        "sampler_objects", "Mali ,Mali-");
  // The extension can be called ARB_sample_shading or OES_sample_shading. Both
  // are compatible with the core version.
  SetFeatureSupportedIf(kSampleShading, GlVersions(40U, 32U, 0U),
                        "sample_shading", "");
  SetFeatureSupportedIf(kShaderFramebufferFetch, GlVersions(0U, 0U, 0U),
                        "EXT_shader_framebuffer_fetch", "");
  SetFeatureSupportedIf(kStandardDerivatives, GlVersions(20U, 30U, 0U),
                        "OES_standard_derivatives", "");
  SetFeatureSupportedIf(kSync, GlVersions(32U, 30U, 20U), "sync", "");
  SetFeatureSupportedIf(kTexture3d, GlVersions(13U, 30U, 20U),
                        "texture_3d", "");
  SetFeatureSupportedIf(kTextureArray1d, GlVersions(30U, 0U, 0U),
                        "texture_array", "");
  SetFeatureSupportedIf(kTextureArray2d, GlVersions(30U, 30U, 20U),
                        "texture_array", "");
  SetFeatureSupportedIf(kTextureBarrier, GlVersions(45U, 0U, 0U),
                        "texture_barrier", "");
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

  GLint attachments = -1;
  GetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &attachments);
  // Check capability value for multiple color attachments. Four attachments
  // is the minimum legal value according to the spec.
  if (features_[kDrawBuffers].IsSupported() && attachments >= 4) {
    SetFeatureSupported(kMultipleColorAttachments, true);
  }

  // Additional sanity check for the number of clip distances.
  if (features_[kClipDistance].IsSupported()) {
    GLint distances = -1;
    GetIntegerv(GL_MAX_CLIP_DISTANCES, &distances);
    if (distances < 8) {
      SetFeatureSupported(kClipDistance, false);
    }
  }

  // Enable all supported features. This must come after all support checks.
  const size_t feature_count = features_.size();
  for (size_t i = 0; i < feature_count; ++i) {
    FeatureId id = static_cast<FeatureId>(i);
    EnableFeature(id, features_[i].IsSupported());
  }
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
  if (versions.versions[gl_api_standard_] &&
      gl_version_ >= versions.versions[gl_api_standard_]) return true;

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

}  // namespace gfx
}  // namespace ion
