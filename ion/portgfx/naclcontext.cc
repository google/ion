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

#include <stdint.h>

#include <cstring>

#include "ion/base/logging.h"
#include "ion/base/sharedptr.h"
#include "ion/portgfx/glcontext.h"
#include "ion/portgfx/glheaders.h"
#include "ppapi/cpp/graphics_3d.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/lib/gl/gles2/gl2ext_ppapi.h"

namespace ion {
namespace portgfx {
namespace {

struct GlFunctionInfo {
  const char* name;
  void* function;
};

#define BIND_GLES_FUNCTION(name) \
  { #name, reinterpret_cast < void* > (&name) }
// NaCl doesn't support any way to get function addresses, so we build a
// string-to-pointer table below.
static const GlFunctionInfo kEs2FunctionMap[] = {
    BIND_GLES_FUNCTION(glActiveTexture),
    BIND_GLES_FUNCTION(glAttachShader),
    BIND_GLES_FUNCTION(glBindAttribLocation),
    BIND_GLES_FUNCTION(glBindBuffer),
    BIND_GLES_FUNCTION(glBindFramebuffer),
    BIND_GLES_FUNCTION(glBindRenderbuffer),
    BIND_GLES_FUNCTION(glBindTexture),
    BIND_GLES_FUNCTION(glBindVertexArrayOES),
    BIND_GLES_FUNCTION(glBlendColor),
    BIND_GLES_FUNCTION(glBlendEquation),
    BIND_GLES_FUNCTION(glBlendEquationSeparate),
    BIND_GLES_FUNCTION(glBlendFunc),
    BIND_GLES_FUNCTION(glBlendFuncSeparate),
    BIND_GLES_FUNCTION(glBufferData),
    BIND_GLES_FUNCTION(glBufferSubData),
    BIND_GLES_FUNCTION(glCheckFramebufferStatus),
    BIND_GLES_FUNCTION(glClear),
    BIND_GLES_FUNCTION(glClearColor),
    BIND_GLES_FUNCTION(glClearDepthf),
    BIND_GLES_FUNCTION(glClearStencil),
    BIND_GLES_FUNCTION(glColorMask),
    BIND_GLES_FUNCTION(glCompileShader),
    BIND_GLES_FUNCTION(glCompressedTexImage2D),
    BIND_GLES_FUNCTION(glCompressedTexSubImage2D),
    BIND_GLES_FUNCTION(glCopyTexImage2D),
    BIND_GLES_FUNCTION(glCopyTexSubImage2D),
    BIND_GLES_FUNCTION(glCreateProgram),
    BIND_GLES_FUNCTION(glCreateShader),
    BIND_GLES_FUNCTION(glCullFace),
    BIND_GLES_FUNCTION(glDeleteBuffers),
    BIND_GLES_FUNCTION(glDeleteFramebuffers),
    BIND_GLES_FUNCTION(glDeleteProgram),
    BIND_GLES_FUNCTION(glDeleteRenderbuffers),
    BIND_GLES_FUNCTION(glDeleteShader),
    BIND_GLES_FUNCTION(glDeleteTextures),
    BIND_GLES_FUNCTION(glDeleteVertexArraysOES),
    BIND_GLES_FUNCTION(glDepthFunc),
    BIND_GLES_FUNCTION(glDepthMask),
    BIND_GLES_FUNCTION(glDepthRangef),
    BIND_GLES_FUNCTION(glDetachShader),
    BIND_GLES_FUNCTION(glDisable),
    BIND_GLES_FUNCTION(glDisableVertexAttribArray),
    BIND_GLES_FUNCTION(glDrawArrays),
    BIND_GLES_FUNCTION(glDrawElements),
    BIND_GLES_FUNCTION(glEnable),
    BIND_GLES_FUNCTION(glEnableVertexAttribArray),
    BIND_GLES_FUNCTION(glFinish),
    BIND_GLES_FUNCTION(glFlush),
    BIND_GLES_FUNCTION(glFramebufferRenderbuffer),
    BIND_GLES_FUNCTION(glFramebufferTexture2D),
    BIND_GLES_FUNCTION(glFrontFace),
    BIND_GLES_FUNCTION(glGenBuffers),
    BIND_GLES_FUNCTION(glGenerateMipmap),
    BIND_GLES_FUNCTION(glGenFramebuffers),
    BIND_GLES_FUNCTION(glGenRenderbuffers),
    BIND_GLES_FUNCTION(glGenTextures),
    BIND_GLES_FUNCTION(glGenVertexArraysOES),
    BIND_GLES_FUNCTION(glGetActiveAttrib),
    BIND_GLES_FUNCTION(glGetActiveUniform),
    BIND_GLES_FUNCTION(glGetAttachedShaders),
    BIND_GLES_FUNCTION(glGetAttribLocation),
    BIND_GLES_FUNCTION(glGetBooleanv),
    BIND_GLES_FUNCTION(glGetBufferParameteriv),
    BIND_GLES_FUNCTION(glGetError),
    BIND_GLES_FUNCTION(glGetFloatv),
    BIND_GLES_FUNCTION(glGetFramebufferAttachmentParameteriv),
    BIND_GLES_FUNCTION(glGetIntegerv),
    BIND_GLES_FUNCTION(glGetProgramInfoLog),
    BIND_GLES_FUNCTION(glGetProgramiv),
    BIND_GLES_FUNCTION(glGetRenderbufferParameteriv),
    BIND_GLES_FUNCTION(glGetShaderInfoLog),
    BIND_GLES_FUNCTION(glGetShaderiv),
    BIND_GLES_FUNCTION(glGetShaderPrecisionFormat),
    BIND_GLES_FUNCTION(glGetShaderSource),
    BIND_GLES_FUNCTION(glGetString),
    BIND_GLES_FUNCTION(glGetTexParameterfv),
    BIND_GLES_FUNCTION(glGetTexParameteriv),
    BIND_GLES_FUNCTION(glGetUniformfv),
    BIND_GLES_FUNCTION(glGetUniformiv),
    BIND_GLES_FUNCTION(glGetVertexAttribfv),
    BIND_GLES_FUNCTION(glGetVertexAttribiv),
    BIND_GLES_FUNCTION(glGetVertexAttribPointerv),
    BIND_GLES_FUNCTION(glGetUniformLocation),
    BIND_GLES_FUNCTION(glHint),
    BIND_GLES_FUNCTION(glIsBuffer),
    BIND_GLES_FUNCTION(glIsEnabled),
    BIND_GLES_FUNCTION(glIsFramebuffer),
    BIND_GLES_FUNCTION(glIsProgram),
    BIND_GLES_FUNCTION(glIsRenderbuffer),
    BIND_GLES_FUNCTION(glIsShader),
    BIND_GLES_FUNCTION(glIsTexture),
    BIND_GLES_FUNCTION(glIsVertexArrayOES),
    BIND_GLES_FUNCTION(glLineWidth),
    BIND_GLES_FUNCTION(glLinkProgram),
    BIND_GLES_FUNCTION(glPixelStorei),
    BIND_GLES_FUNCTION(glPolygonOffset),
    BIND_GLES_FUNCTION(glReadPixels),
    BIND_GLES_FUNCTION(glReleaseShaderCompiler),
    BIND_GLES_FUNCTION(glRenderbufferStorage),
    BIND_GLES_FUNCTION(glRenderbufferStorageMultisampleEXT),
    BIND_GLES_FUNCTION(glBlitFramebufferEXT),
    BIND_GLES_FUNCTION(glSampleCoverage),
    BIND_GLES_FUNCTION(glScissor),
    BIND_GLES_FUNCTION(glShaderBinary),
    BIND_GLES_FUNCTION(glShaderSource),
    BIND_GLES_FUNCTION(glStencilFunc),
    BIND_GLES_FUNCTION(glStencilFuncSeparate),
    BIND_GLES_FUNCTION(glStencilMask),
    BIND_GLES_FUNCTION(glStencilMaskSeparate),
    BIND_GLES_FUNCTION(glStencilOp),
    BIND_GLES_FUNCTION(glStencilOpSeparate),
    BIND_GLES_FUNCTION(glTexImage2D),
    BIND_GLES_FUNCTION(glTexParameterf),
    BIND_GLES_FUNCTION(glTexParameterfv),
    BIND_GLES_FUNCTION(glTexParameteri),
    BIND_GLES_FUNCTION(glTexParameteriv),
    BIND_GLES_FUNCTION(glTexSubImage2D),
    BIND_GLES_FUNCTION(glUniform1f),
    BIND_GLES_FUNCTION(glUniform1fv),
    BIND_GLES_FUNCTION(glUniform1i),
    BIND_GLES_FUNCTION(glUniform1iv),
    BIND_GLES_FUNCTION(glUniform2f),
    BIND_GLES_FUNCTION(glUniform2fv),
    BIND_GLES_FUNCTION(glUniform2i),
    BIND_GLES_FUNCTION(glUniform2iv),
    BIND_GLES_FUNCTION(glUniform3f),
    BIND_GLES_FUNCTION(glUniform3fv),
    BIND_GLES_FUNCTION(glUniform3i),
    BIND_GLES_FUNCTION(glUniform3iv),
    BIND_GLES_FUNCTION(glUniform4f),
    BIND_GLES_FUNCTION(glUniform4fv),
    BIND_GLES_FUNCTION(glUniform4i),
    BIND_GLES_FUNCTION(glUniform4iv),
    BIND_GLES_FUNCTION(glUniformMatrix2fv),
    BIND_GLES_FUNCTION(glUniformMatrix3fv),
    BIND_GLES_FUNCTION(glUniformMatrix4fv),
    BIND_GLES_FUNCTION(glUseProgram),
    BIND_GLES_FUNCTION(glValidateProgram),
    BIND_GLES_FUNCTION(glVertexAttrib1f),
    BIND_GLES_FUNCTION(glVertexAttrib1fv),
    BIND_GLES_FUNCTION(glVertexAttrib2f),
    BIND_GLES_FUNCTION(glVertexAttrib2fv),
    BIND_GLES_FUNCTION(glVertexAttrib3f),
    BIND_GLES_FUNCTION(glVertexAttrib3fv),
    BIND_GLES_FUNCTION(glVertexAttrib4f),
    BIND_GLES_FUNCTION(glVertexAttrib4fv),
    BIND_GLES_FUNCTION(glVertexAttribPointer),
    BIND_GLES_FUNCTION(glViewport),
};

#undef BIND_GLES_FUNCTION

// This class wraps a NaCl context in an ion::portgfx::GlContext implementation.
class NaclContext : public GlContext {
 public:
  explicit NaclContext(bool is_owned_context)
      : context_(0),
        is_owned_context_(is_owned_context),
        module_(nullptr),
        interface_(nullptr) {}
  ~NaclContext() override {}

  // GlContext implementation.
  bool IsValid() const override { return (context_ != 0); }
  void* GetProcAddress(const char* proc_name, uint32_t flags) const override {
    for (const char* suffix : {"", "OES", "EXT"}) {
      for (const auto& entry : kEs2FunctionMap) {
        const std::string full_name = std::string(proc_name) + suffix;
        if (entry.name == full_name) {
          return entry.function;
        }
      }
    }
    return nullptr;
  }
  void SwapBuffers() override {
    if (module_ && interface_ && context_) {
      interface_->SwapBuffers(context_, PP_BlockUntilComplete());
    }
  }
  bool MakeContextCurrentImpl() override {
    glSetCurrentContextPPAPI(context_);
    if (glGetCurrentContextPPAPI() != context_) {
      return false;
    }
    return true;
  }
  void ClearCurrentContextImpl() override { glSetCurrentContextPPAPI(0); }
  GlContextPtr CreateGlContextInShareGroupImpl(
      const GlContextSpec& spec) override {
    // Currently this platform only supports the default GlContextSpec.
    DCHECK(spec.backbuffer_width == 1 && spec.backbuffer_height == 1);
    base::SharedPtr<NaclContext> context(new NaclContext(true));
    if (!context->InitOwned(this)) {
      context.Reset();
    }
    return context;
  }
  bool IsOwned() const override { return is_owned_context_; }

  bool InitOwned(const NaclContext* shared_context);
  bool InitWrapped();

 private:
  // The (potentially) owned state.
  PP_Resource context_;

  // Whether the "owned state" is actually owned.
  const bool is_owned_context_;

  // The pepper module.
  pp::Module* module_;
  // The browser interface.
  const PPB_Graphics3D* interface_;
};

bool NaclContext::InitOwned(const NaclContext* shared_context) {
  DCHECK(is_owned_context_);

  module_ = pp::Module::Get();
  if (module_ == nullptr ||
      !glInitializePPAPI(module_->get_browser_interface())) {
    LOG(ERROR) << "Unable to initialize GL PPAPI.";
    return false;
  }

  interface_ = reinterpret_cast<const PPB_Graphics3D*>(
      module_->GetBrowserInterface(PPB_GRAPHICS_3D_INTERFACE));
  if (interface_ == nullptr) {
    LOG(ERROR) << "Unable to initialize PP Graphics3D interface.";
    return false;
  }

  static const int32_t kAttributes[] = {
      PP_GRAPHICS3DATTRIB_ALPHA_SIZE,   8, PP_GRAPHICS3DATTRIB_DEPTH_SIZE, 24,
      PP_GRAPHICS3DATTRIB_STENCIL_SIZE, 8, PP_GRAPHICS3DATTRIB_NONE,
  };

  const PP_Resource pp_context =
      (shared_context != nullptr ? shared_context->context_ : 0);

  const pp::Module::InstanceMap& instances = module_->current_instances();
  pp::Module::InstanceMap::const_iterator iter = instances.begin();
  while (iter != instances.end()) {
    // Choose the first instance.
    if (pp::Instance* instance = iter->second) {
      context_ = interface_->Create(instance->pp_instance(), pp_context,
                                    kAttributes);
      break;
    }
    ++iter;
  }
  if (iter == instances.end()) {
    LOG(ERROR) << "No PP module instance found.";
    return false;
  }

  SetIds(CreateId(),
         (shared_context != nullptr ? shared_context->GetShareGroupId()
                                    : CreateShareGroupId()),
         static_cast<uintptr_t>(context_));
  return true;
}

bool NaclContext::InitWrapped() {
  DCHECK(!is_owned_context_);

  context_ = glGetCurrentContextPPAPI();
  if (context_ == 0) {
    LOG(ERROR) << "No current context.";
    return false;
  }

  module_ = pp::Module::Get();
  if (module_) {
    interface_ = reinterpret_cast<const PPB_Graphics3D*>(
        module_->GetBrowserInterface(PPB_GRAPHICS_3D_INTERFACE));
  }

  SetIds(CreateId(), CreateShareGroupId(), static_cast<uintptr_t>(context_));
  return true;
}

}  // namespace

// static
GlContextPtr GlContext::CreateGlContext(const GlContextSpec& spec) {
  // Currently this platform only supports the default GlContextSpec.
  DCHECK(spec.backbuffer_width == 1 && spec.backbuffer_height == 1);
  base::SharedPtr<NaclContext> context(new NaclContext(true));
  if (!context->InitOwned(nullptr)) {
    context.Reset();
  }
  return context;
}

// static
GlContextPtr GlContext::CreateWrappingGlContext() {
  base::SharedPtr<NaclContext> context(new NaclContext(false));
  if (!context->InitWrapped()) {
    context.Reset();
  }
  return context;
}

// static
uintptr_t GlContext::GetCurrentGlContextId() {
  return static_cast<uintptr_t>(glGetCurrentContextPPAPI());
}

}  // namespace portgfx
}  // namespace ion
