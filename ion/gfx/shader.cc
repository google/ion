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

#include "ion/gfx/shader.h"

#include "ion/gfx/shaderinputregistry.h"

namespace ion {
namespace gfx {

ShaderBase::ShaderBase() {}

ShaderBase::~ShaderBase() {}

Shader::Shader() : source_(kSourceChanged, std::string(), this) {}

Shader::Shader(const std::string& source)
    :  source_(kSourceChanged, source, this) {}

Shader::~Shader() {}

}  // namespace gfx
}  // namespace ion
