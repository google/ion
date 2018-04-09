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

#include "ion/gfx/shaderprogram.h"

#include "ion/gfx/shaderinputregistry.h"

namespace ion {
namespace gfx {

ProgramBase::ProgramBase(const ShaderInputRegistryPtr& registry)
    : registry_(registry), concurrent_(false), concurrent_set_(false) {}

ProgramBase::~ProgramBase() {}

void ProgramBase::SetConcurrent(bool value) {
  if (concurrent_set_) {
    // Only emit warning when the value is actually different.
    if (value != concurrent_) {
      LOG(WARNING) << "Program resources already created"
                   << " - cannot change concurrency" << std::endl;
    }
  } else {
    concurrent_ = value;
    concurrent_set_ = true;
  }
}

ShaderProgram::ShaderProgram(const ShaderInputRegistryPtr& registry)
    : ProgramBase(registry),
      vertex_shader_(kVertexShaderChanged, ShaderPtr(), this),
      tess_ctrl_shader_(kTessControlShaderChanged, ShaderPtr(), this),
      tess_eval_shader_(kTessEvaluationShaderChanged, ShaderPtr(), this),
      geometry_shader_(kGeometryShaderChanged, ShaderPtr(), this),
      fragment_shader_(kFragmentShaderChanged, ShaderPtr(), this),
      varyings_(kCapturedVaryingsChanged, base::AllocVector<std::string>(*this),
                this) {
  DCHECK(registry.Get());
}

ShaderProgram::~ShaderProgram() {
  if (Shader* shader = vertex_shader_.Get().Get())
    shader->RemoveReceiver(this);
  if (Shader* shader = geometry_shader_.Get().Get())
    shader->RemoveReceiver(this);
  if (Shader* shader = fragment_shader_.Get().Get())
    shader->RemoveReceiver(this);
  if (Shader *shader = tess_ctrl_shader_.Get().Get())
    shader->RemoveReceiver(this);
  if (Shader *shader = tess_eval_shader_.Get().Get())
    shader->RemoveReceiver(this);
}

ShaderProgramPtr ShaderProgram::BuildFromStrings(
    const std::string& id_string,
    const ShaderInputRegistryPtr& registry_ptr,
    const std::string& vertex_shader_string,
    const std::string& fragment_shader_string,
    const base::AllocatorPtr& allocator) {
  return BuildFromStrings(id_string, registry_ptr, vertex_shader_string,
                          "", "", "", fragment_shader_string, allocator);
}

ShaderProgramPtr ShaderProgram::BuildFromStrings(
    const std::string& id_string,
    const ShaderInputRegistryPtr& registry_ptr,
    const std::string& vertex_shader_string,
    const std::string& tess_ctrl_shader_string,
    const std::string& tess_eval_shader_string,
    const std::string& geometry_shader_string,
    const std::string& fragment_shader_string,
    const base::AllocatorPtr& allocator) {
  ShaderProgramPtr program(new(allocator) ShaderProgram(registry_ptr));
  program->SetLabel(id_string);
  program->SetVertexShader(
      ShaderPtr(new(allocator) Shader(vertex_shader_string)));
  program->GetVertexShader()->SetLabel(id_string + " vertex shader");
  if (!geometry_shader_string.empty()) {
    program->SetGeometryShader(
          ShaderPtr(new(allocator) Shader(geometry_shader_string)));
    program->GetGeometryShader()->SetLabel(id_string + " geometry shader");
  }
  program->SetFragmentShader(
      ShaderPtr(new(allocator) Shader(fragment_shader_string)));
  program->GetFragmentShader()->SetLabel(id_string + " fragment shader");

  if (!tess_ctrl_shader_string.empty()) {
    program->SetTessControlShader(
        ShaderPtr(new(allocator) Shader(tess_ctrl_shader_string)));
    program->GetTessControlShader()->SetLabel(
        id_string + " tessellation control shader");
  }

  if (!tess_eval_shader_string.empty()) {
    program->SetTessEvalShader(
        ShaderPtr(new(allocator) Shader(tess_eval_shader_string)));
    program->GetTessEvalShader()->SetLabel(
        id_string + " tessellation evaluation shader");
  }
  return program;
}

ShaderProgramPtr ShaderProgram::BuildFromStrings(
    const std::string& id_string,
    const ShaderInputRegistryPtr& registry_ptr,
    const std::string& vertex_shader_string,
    const std::string& geometry_shader_string,
    const std::string& fragment_shader_string,
    const base::AllocatorPtr& allocator) {
  return BuildFromStrings(id_string, registry_ptr, vertex_shader_string,
                          "", "", geometry_shader_string,
                          fragment_shader_string, allocator);
}

void ShaderProgram::OnNotify(const base::Notifier* notifier) {
  if (GetResourceCount()) {
    if (notifier == vertex_shader_.Get().Get()) {
      OnChanged(kVertexShaderChanged);
    } else if (notifier == geometry_shader_.Get().Get()) {
      OnChanged(kGeometryShaderChanged);
    } else if (notifier == fragment_shader_.Get().Get()) {
      OnChanged(kFragmentShaderChanged);
    } else if (notifier == tess_ctrl_shader_.Get().Get()) {
      OnChanged(kTessControlShaderChanged);
    } else if (notifier == tess_eval_shader_.Get().Get()) {
      OnChanged(kTessEvaluationShaderChanged);
    }
  }
}

}  // namespace gfx
}  // namespace ion
