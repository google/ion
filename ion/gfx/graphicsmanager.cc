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

#include "ion/gfx/graphicsmanager.h"

#include <string.h>  // For strcmp().

#include <vector>

#include "ion/base/allocationmanager.h"
#include "ion/base/logging.h"
#include "ion/base/once.h"
#include "ion/base/staticsafedeclare.h"
#include "ion/base/stringutils.h"
#include "ion/base/threadlocalobject.h"
#include "ion/base/variant.h"
#include "ion/portgfx/getglprocaddress.h"
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
    GLint count = 0;
    gm->GetIntegerv(cv->enum2_, &count);
    std::vector<GLint> values(count);
    if (count)
      gm->GetIntegerv(cv->enum1_, &values[0]);
    cv->value_.Set(values);
  }

  static void GetFloat(GraphicsManager* gm, CapabilityValue* cv) {
    GLfloat val = 0;
    gm->GetFloatv(cv->enum1_, &val);
    cv->value_.Set(val);
  }

  static void GetFloatRange(GraphicsManager* gm, CapabilityValue* cv) {
    GLfloat val[2];
    val[0] = val[1] = 0;
    gm->GetFloatv(cv->enum1_, val);
    cv->value_.Set(math::Range1f(val[0], val[1]));
  }

  static void GetInt(GraphicsManager* gm, CapabilityValue* cv) {
    GLint val = 0;
    gm->GetIntegerv(cv->enum1_, &val);
    cv->value_.Set(val);
  }

  static void GetIntRange(GraphicsManager* gm, CapabilityValue* cv) {
    GLint val[2];
    val[0] = val[1] = 0;
    gm->GetIntegerv(cv->enum1_, val);
    cv->value_.Set(math::Range1i(val[0], val[1]));
  }

  static void GetShaderPrecision(GraphicsManager* gm, CapabilityValue* cv) {
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
    ION_SINGLE_CAP(kMaxColorAttachments,
                   GL_MAX_COLOR_ATTACHMENTS, GetInt);
    ION_SINGLE_CAP(kMaxCombinedTextureImageUnits,
                   GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, GetInt);
    ION_SINGLE_CAP(kMaxCubeMapTextureSize, GL_MAX_CUBE_MAP_TEXTURE_SIZE,
                   GetInt);
    ION_SINGLE_CAP(kMaxDrawBuffers, GL_MAX_DRAW_BUFFERS, GetInt);
    ION_SINGLE_CAP(kMaxFragmentUniformComponents,
                   GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, GetInt);
    ION_SINGLE_CAP(kMaxFragmentUniformVectors, GL_MAX_FRAGMENT_UNIFORM_VECTORS,
                   GetInt);
    ION_SINGLE_CAP(kMaxRenderbufferSize, GL_MAX_RENDERBUFFER_SIZE, GetInt);
    ION_SINGLE_CAP(kMaxSampleMaskWords, GL_MAX_SAMPLE_MASK_WORDS, GetInt);
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
    ION_DOUBLE_CAP(kShaderBinaryFormats, GL_SHADER_BINARY_FORMATS,
                   GL_NUM_SHADER_BINARY_FORMATS, GetIntVector);
    ION_SINGLE_CAP(kTransformFeedbackVaryingMaxLength,
                   GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH, GetInt);
    ION_SINGLE_CAP(kMaxDebugLoggedMessages, GL_MAX_DEBUG_LOGGED_MESSAGES,
                   GetInt);
    ION_SINGLE_CAP(kMaxDebugMessageLength, GL_MAX_DEBUG_MESSAGE_LENGTH, GetInt);
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
// GraphicsManager::FunctionGroup represents a group of OpenGL functions with
// related functionality. A group is "complete" if all of the functions in the
// group are available.
//
//-----------------------------------------------------------------------------

class GraphicsManager::FunctionGroup {
 public:
  FunctionGroup()
    : complete_(true),
      enabled_(true),
      available_functions_(),
      missing_functions_() {}
  ~FunctionGroup() {}
  void AddFunction(const std::string& name, void* function) {
#if defined(ION_COVERAGE)
    available_functions_.push_back(name);
#else  // COV_NF_START
    if (function == NULL)
      missing_functions_.push_back(name);
    else
      available_functions_.push_back(name);
#endif  // COV_NF_END
    complete_ = complete_ && (function != NULL);
  }
  bool IsComplete() const { return complete_ && enabled_; }
  bool IsEnabled() const { return enabled_; }
  void SetEnabled(bool enable) { enabled_ = enable; }
#if !defined(ION_COVERAGE)  // COV_NF_START
  const std::vector<std::string>& GetMissingFunctions() const {
    return missing_functions_;
  }
#endif  // COV_NF_END

 private:
  bool complete_;
  bool enabled_;
  std::vector<std::string> available_functions_;
  std::vector<std::string> missing_functions_;
};

//-----------------------------------------------------------------------------
//
// Thread local wrapper collection for GraphicsManager.
//
//-----------------------------------------------------------------------------

class GraphicsManager::WrapperVecHolder : public base::Allocatable {
 public:
  WrapperVecHolder() : wrappers_(GetAllocator()) {}
  WrapperVec& GetWrappers() { return wrappers_; }

 private:
  WrapperVec wrappers_;
};

GraphicsManager::WrapperVecHolder* GraphicsManager::GetWrapperVecHolder() {
  ION_DECLARE_SAFE_STATIC_POINTER(
      base::ThreadLocalObject<WrapperVecHolder>, s_helper);
  return s_helper->Get();
}

//-----------------------------------------------------------------------------
//
// GraphicsManager implementation.
//
//-----------------------------------------------------------------------------

GraphicsManager::GraphicsManager()
    : wrappers_(GetAllocator()),
      function_groups_(GetAllocator()),
      capability_helper_(new CapabilityHelper),
      wrapped_function_names_(*this),
      is_error_checking_enabled_(false),
      tracing_ostream_(NULL),
      gl_version_(20),
      gl_api_standard_(kEs),
      gl_profile_type_(kCompatibilityProfile) {
  Init(true);
}

GraphicsManager::GraphicsManager(GraphicsManager* gm)
    : wrappers_(GetAllocator()),
      function_groups_(GetAllocator()),
      capability_helper_(new CapabilityHelper),
      wrapped_function_names_(*this),
      is_error_checking_enabled_(false),
      tracing_ostream_(NULL),
      gl_version_(20),
      gl_api_standard_(kEs),
      gl_profile_type_(kCompatibilityProfile) {
  Init(false);
}

GraphicsManager::~GraphicsManager() {
}

void GraphicsManager::Init(bool init_functions_from_gl) {
  WrapperVecHolder* wrapper_holder = GetWrapperVecHolder();
  // The constructors for each of the wrapped functions should have been added
  // to the static vector.
  DCHECK(wrapper_holder != NULL);
  WrapperVec& thread_wrappers = wrapper_holder->GetWrappers();
  DCHECK(!thread_wrappers.empty());

  // TODO(user): Probably a good place to use std::move.
  wrappers_ = thread_wrappers;
  thread_wrappers.clear();
  thread_wrappers.reserve(0U);

  if (init_functions_from_gl) {
    InitFunctions();

#if !defined(ION_COVERAGE)  // COV_NF_START
    if (!function_groups_[kCore].IsComplete()) {
      LOG(ERROR) << "***ION: Some required OpenGL functions could not be "
                 << "found. Either there is no valid OpenGL context, or the "
                 << "following functions are missing from your OpenGL "
                 << "installation:";
      const std::vector<std::string>& missing =
          function_groups_[kCore].GetMissingFunctions();
      for (size_t i = 0; i < missing.size(); i++) {
        LOG(ERROR) << "  " << missing[i].c_str();
      }
    }
#endif  // COV_NF_END
  }
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

bool GraphicsManager::IsFunctionGroupAvailable(FunctionGroupId group) const {
  return function_groups_.size() > 0 && function_groups_[group].IsComplete();
}

bool GraphicsManager::IsExtensionSupported(const std::string& name) const {
  return portgfx::IsExtensionSupported(name, extensions_);
}

void GraphicsManager::AddWrapper(WrapperBase* wrapper) {
  WrapperVecHolder* holder = GetWrapperVecHolder();
  WrapperVec& thread_wrappers = holder->GetWrappers();
  if (thread_wrappers.capacity() < 64U) {
    thread_wrappers.reserve(64U);
  }
  thread_wrappers.push_back(wrapper);
}

void GraphicsManager::AddFunctionToGroup(FunctionGroupId group,
                                         const char* func_name,
                                         void* function) {
  if (function_groups_.size() == 0) {
    function_groups_.resize(kNumFunctionGroupIds);
  }
  function_groups_[group].AddFunction(func_name, function);
}

void GraphicsManager::EnableFunctionGroup(
    FunctionGroupId group, bool enable) {
  if (function_groups_.size() > 0)
    function_groups_[group].SetEnabled(enable);

  // Turn on/off state table caps for function groups that implement the
  // corresponding capability.
  switch (group) {
    case kDebugOutput:
      valid_statetable_caps_.set(StateTable::kDebugOutputSynchronous, enable);
      break;
    case kFramebufferMultisample:
      valid_statetable_caps_.set(StateTable::kMultisample, enable);
      break;
    default:
      break;
  }
}

void GraphicsManager::InitFunctions() {
  const size_t num_wrappers = wrappers_.size();
  for (size_t i = 0; i < num_wrappers; ++i) {
    if (wrappers_[i]->Init(this))
      AddWrappedFunctionName(wrappers_[i]->GetFuncName());
  }
  InitGlInfo();
}

void GraphicsManager::InitGlInfo() {
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

  GLint mask = 0;
  GetIntegerv(GL_CONTEXT_PROFILE_MASK, &mask);
  // The above call could generate an invalid enum if we are not called from a
  // core context. Eat the error.
  GetError();
  gl_profile_type_ = kCompatibilityProfile;
  if (mask & GL_CONTEXT_CORE_PROFILE_BIT)
    gl_profile_type_ = kCoreProfile;

  if (const char* extensions =
          reinterpret_cast<const char*>(GetString(GL_EXTENSIONS)))
    extensions_ = extensions;

  // State table capabilities default to valid.
  valid_statetable_caps_.reset();
  valid_statetable_caps_.flip();

  // Ensure that extension function groups are really supported.
  EnableFunctionGroupIfAvailable(kCopyBufferSubData, GlVersions(31U, 30U, 0U),
                                 "copy_buffer", "");
  EnableFunctionGroupIfAvailable(kDebugLabel, GlVersions(0U, 0U, 0U),
                                 "debug_label", "");
  EnableFunctionGroupIfAvailable(kDebugMarker, GlVersions(0U, 0U, 0U),
                                 "debug_marker", "");
  EnableFunctionGroupIfAvailable(kDebugOutput, GlVersions(0U, 0U, 0U),
                                 "debug_output,debug", "");
  EnableFunctionGroupIfAvailable(kFramebufferBlit, GlVersions(20U, 30U, 0U),
                                 "framebuffer_blit", "");
  EnableFunctionGroupIfAvailable(kFramebufferMultisample,
                                 GlVersions(20U, 30U, 0U),
                                 "framebuffer_multisample", "");
  EnableFunctionGroupIfAvailable(kGpuShader4, GlVersions(30U, 30U, 0U),
                                 "gpu_shader4", "");
  EnableFunctionGroupIfAvailable(kEglImage, GlVersions(0U, 0U, 0U), "EGL_image",
                                 "");
  EnableFunctionGroupIfAvailable(kGetString, GlVersions(30U, 30U, 0U), "", "");
  EnableFunctionGroupIfAvailable(kMapBuffer, GlVersions(15U, 30U, 0U),
                                 "mapbuffer,vertex_buffer_object",
                                 "Vivante GC1000,VideoCore IV HW");
  EnableFunctionGroupIfAvailable(kMapBufferRange, GlVersions(30U, 30U, 0U),
                                 "map_buffer_range",
                                 "Vivante GC1000,VideoCore IV HW");
  EnableFunctionGroupIfAvailable(kSamplerObjects, GlVersions(33U, 30U, 0U),
                                 "sampler_objects", "Mali ,Mali-");
  EnableFunctionGroupIfAvailable(kTexture3d, GlVersions(13U, 30U, 0U),
                                 "texture_3d", "");
  EnableFunctionGroupIfAvailable(kTextureMultisample, GlVersions(32U, 30U, 0U),
                                 "texture_multisample", "");
  EnableFunctionGroupIfAvailable(kTextureStorage, GlVersions(42U, 30U, 0U),
                                 "texture_storage", "");
  EnableFunctionGroupIfAvailable(kTextureStorageMultisample,
                                 GlVersions(42U, 30U, 0U),
                                 "texture_storage_multisample", "");
  EnableFunctionGroupIfAvailable(kVertexArrays, GlVersions(30U, 30U, 0U),
                                 "vertex_array_object", "Internet Explorer");
  EnableFunctionGroupIfAvailable(kInstancedDrawing, GlVersions(33U, 30U, 0U),
                                 "instanced_drawing", "");
  EnableFunctionGroupIfAvailable(kSync, GlVersions(32U, 30U, 2U), "sync", "");
  EnableFunctionGroupIfAvailable(kRaw, GlVersions(0U, 0U, 0U), "", "");
  EnableFunctionGroupIfAvailable(kTransformFeedback, GlVersions(40U, 30U, 0U),
                                 "transform_feedback", "");

  if (extensions_.empty() && IsFunctionGroupAvailable(kGetString)) {
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

  // On some platforms vertex arrays are improperly advertised. Ensure
  // GenVertexArrays succeeds. We can only perform this test, however, if a
  // valid function pointer exists and is enabled.
  if (IsFunctionGroupAvailable(kVertexArrays)) {
    // Just to be safe check that the most basic functionality works.
    GLuint id = 0U;
    GenVertexArrays(1U, &id);
    // Delete the array if it's valid.
    if (id)
      DeleteVertexArrays(1U, &id);
    else
      EnableFunctionGroup(kVertexArrays, false);
  }

  // Clear any errors by initial Gets.
  GetError();
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

void GraphicsManager::ReinitFunctions() {
  const size_t num_wrappers = wrappers_.size();
  // Reset the function pointers to NULL.
  for (size_t i = 0; i < num_wrappers; ++i)
    wrappers_[i]->Reset();
  wrapped_function_names_.clear();
  if (function_groups_.size() > 0) {
    function_groups_.clear();
    function_groups_.resize(kNumFunctionGroupIds);
  }
  InitFunctions();
}

void GraphicsManager::CheckForErrors(const char* when,
                                     const std::string& func_call) {
  // We cannot call glGetError if func_name is GetError, or we infinitely
  // recurse.
  if (func_call.find("GetError") == std::string::npos) {
    GLenum error = this->GetError();
    if (error != GL_NO_ERROR) {
      if (tracing_ostream_) {
        *tracing_ostream_ << "*** GL error " << when << " call to "
                          << func_call << ": " << ErrorString(error) << "\n";
      }
      LOG(ERROR) << "*** GL error " << when << " call to "
                 << func_call << ": " << ErrorString(error) << "\n";
    }
  }
}

void* GraphicsManager::Lookup(const char* name, bool is_core) {
  return portgfx::GetGlProcAddress(name, is_core);
}

void GraphicsManager::EnableFunctionGroupIfAvailable(
    GraphicsManager::FunctionGroupId group, const GlVersions& versions,
    const std::string& extensions, const std::string& disabled_renderers) {
  EnableFunctionGroup(group, true);
  if (IsFunctionGroupAvailable(group)) {
    const std::vector<std::string> renderers =
        base::SplitString(disabled_renderers, ",");
    const size_t renderer_count = renderers.size();
    // Disable the group if platform's renderer is in the disabled set.
    for (size_t i = 0; i < renderer_count; ++i) {
      if (gl_renderer_.find(renderers[i]) != std::string::npos) {
        EnableFunctionGroup(group, false);
        return;
      }
    }

    const std::vector<std::string> names = base::SplitString(extensions, ",");
    const size_t count = names.size();
    // Disable the group if any of the needed extensions are known to be
    // incomplete.
    for (size_t i = 0; i < count; ++i) {
      if (portgfx::IsExtensionIncomplete(names[i].c_str())) {
        EnableFunctionGroup(group, false);
        return;
      }
    }
    // If the GL version is high enough then we don't need to check extensions.
    if (versions[gl_api_standard_] && gl_version_ >= versions[gl_api_standard_])
      return;

    // Check extensions.
    for (size_t i = 0; i < count; ++i) {
      if (IsExtensionSupported(names[i]))
        return;
    }
  }
  EnableFunctionGroup(group, false);
}

}  // namespace gfx
}  // namespace ion
