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

#include "ion/gfx/uniformholder.h"

#include "ion/gfx/shaderinputregistry.h"

namespace ion {
namespace gfx {

UniformHolder::UniformHolder(const base::AllocatorPtr& alloc)
    : is_enabled_(true), uniforms_(alloc) {}

UniformHolder::~UniformHolder() {}

size_t UniformHolder::GetUniformIndex(const std::string& name) const {
  const size_t uniform_count = uniforms_.size();
  for (size_t i = 0; i < uniform_count; ++i) {
    const Uniform& u = uniforms_[i];
    DCHECK(u.IsValid());
    if (name ==
        u.GetRegistry().GetSpecs<Uniform>()[u.GetIndexInRegistry()].name)
      return i;
  }
  return base::kInvalidIndex;
}

}  // namespace gfx
}  // namespace ion
