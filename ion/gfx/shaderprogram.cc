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

#include "ion/gfx/shaderprogram.h"

#include "ion/gfx/shaderinputregistry.h"

namespace ion {
namespace gfx {

ShaderProgram::ShaderProgram(const ShaderInputRegistryPtr& registry)
    : vertex_shader_(kVertexShaderChanged, ShaderPtr(), this),
      fragment_shader_(kFragmentShaderChanged, ShaderPtr(), this),
      registry_(registry),
      concurrent_(false),
      concurrent_set_(false) {
  DCHECK(registry_.Get());
}

ShaderProgram::~ShaderProgram() {
  if (Shader* shader = vertex_shader_.Get().Get())
    shader->RemoveReceiver(this);
  if (Shader* shader = fragment_shader_.Get().Get())
    shader->RemoveReceiver(this);
}

const ShaderProgramPtr ShaderProgram::BuildFromStrings(
    const std::string& id_string,
    const ShaderInputRegistryPtr& registry_ptr,
    const std::string& vertex_shader_string,
    const std::string& fragment_shader_string,
    const base::AllocatorPtr& allocator) {
  ShaderProgramPtr program(new(allocator) ShaderProgram(registry_ptr));
  program->SetLabel(id_string);
  program->SetVertexShader(
      ShaderPtr(new(allocator) Shader(vertex_shader_string)));
  program->GetVertexShader()->SetLabel(id_string + " vertex shader");
  program->SetFragmentShader(
      ShaderPtr(new(allocator) Shader(fragment_shader_string)));
  program->GetFragmentShader()->SetLabel(id_string + " fragment shader");
  return program;
}

void ShaderProgram::OnNotify(const base::Notifier* notifier) {
  if (GetResourceCount()) {
    if (notifier == vertex_shader_.Get().Get())
      OnChanged(kVertexShaderChanged);
    else if (notifier == fragment_shader_.Get().Get())
      OnChanged(kFragmentShaderChanged);
  }
}

void ShaderProgram::SetConcurrent(bool value) {
  if (concurrent_set_) {
    // Only emit warning when the value is actually different.
    if (value != concurrent_) {
      LOG(WARNING) << "Shader program resources already created"
                   << " - cannot change concurrency" << std::endl;
    }
  } else {
    concurrent_ = value;
    concurrent_set_ = true;
  }
}

}  // namespace gfx
}  // namespace ion
