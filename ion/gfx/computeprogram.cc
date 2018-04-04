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

#include "ion/gfx/computeprogram.h"

#include "ion/gfx/shaderinputregistry.h"

namespace ion {
namespace gfx {

ComputeProgram::ComputeProgram(const ShaderInputRegistryPtr& registry)
    : ProgramBase(registry),
      compute_shader_(kComputeShaderChanged, ShaderPtr(), this) {
  DCHECK(registry.Get());
}

ComputeProgram::~ComputeProgram() {
  if (Shader* shader = compute_shader_.Get().Get())
    shader->RemoveReceiver(this);
}

ComputeProgramPtr ComputeProgram::BuildFromStrings(
    const std::string& id_string,
    const ShaderInputRegistryPtr& registry_ptr,
    const std::string& compute_shader_string,
    const base::AllocatorPtr& allocator) {
  ComputeProgramPtr program(new(allocator) ComputeProgram(registry_ptr));
  program->SetLabel(id_string);
  program->SetComputeShader(
      ShaderPtr(new(allocator) Shader(compute_shader_string)));
  program->GetComputeShader()->SetLabel(id_string + " compute shader");
  return program;
}

void ComputeProgram::OnNotify(const base::Notifier* notifier) {
  if (notifier == compute_shader_.Get().Get())
    OnChanged(kComputeShaderChanged);
}

}  // namespace gfx
}  // namespace ion
