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

#ifndef ION_GFX_COMPUTEPROGRAM_H_
#define ION_GFX_COMPUTEPROGRAM_H_

#include "ion/base/referent.h"
#include "ion/gfx/resourceholder.h"
#include "ion/gfx/shader.h"
#include "ion/gfx/shaderprogram.h"

namespace ion {
namespace gfx {

// Convenience typedefs for shared pointers to a ComputeProgram.
class ComputeProgram;
using ComputeProgramPtr = base::SharedPtr<ComputeProgram>;
typedef base::WeakReferentPtr<ComputeProgram> ComputeProgramWeakPtr;

// A ComputeProgram represents an OpenGL program that consists of a compute
// shader. It can be used to compute arbitrary data.
class ION_API ComputeProgram : public ProgramBase {
 public:
  // Changes that affect the resource.
  enum Changes {
    kComputeShaderChanged = kNumBaseChanges,
    kNumChanges
  };

  // A valid ShaderInputRegistryPtr must be passed to the constructor.
  explicit ComputeProgram(const ShaderInputRegistryPtr& registry);

  // Sets/returns the compute shader.
  void SetComputeShader(const ShaderPtr& shader) {
    if (Shader* old_shader = compute_shader_.Get().Get())
      old_shader->RemoveReceiver(this);
    compute_shader_.Set(shader);
    if (shader.Get())
      shader->AddReceiver(this);
  }
  const ShaderPtr& GetComputeShader() const {
    return compute_shader_.Get();
  }

  // Convenience function that builds and returns a new ComputeProgram instance
  // that uses the given ShaderInputRegistry and that points to a new compute
  // Shader instance whose source is specified as a string. The ComputeProgram's
  // label is set to id_string and the Shader labels are set to id_string +
  // " compute shader". The allocator is used for both the ComputeProgram and
  // the Shader.
  static ComputeProgramPtr BuildFromStrings(
      const std::string& id_string,
      const ShaderInputRegistryPtr& registry_ptr,
      const std::string& compute_shader_string,
      const base::AllocatorPtr& allocator);

 protected:
  // The destructor is protected because all base::Referent classes must have
  // protected or private destructors.
  ~ComputeProgram() override;

 private:
  // Called when one of the Shaders of this changes.
  void OnNotify(const base::Notifier* notifier) override;

  Field<ShaderPtr> compute_shader_;
};

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_COMPUTEPROGRAM_H_
