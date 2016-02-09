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

#ifndef ION_GFX_SHADERPROGRAM_H_
#define ION_GFX_SHADERPROGRAM_H_

#include "ion/base/referent.h"
#include "ion/gfx/resourceholder.h"
#include "ion/gfx/shader.h"

namespace ion {
namespace gfx {

class ShaderInputRegistry;
typedef base::ReferentPtr<ShaderInputRegistry>::Type ShaderInputRegistryPtr;

// Convenience typedefs for shared pointers to a ShaderProgram.
class ShaderProgram;
typedef base::ReferentPtr<ShaderProgram>::Type ShaderProgramPtr;
typedef base::WeakReferentPtr<ShaderProgram> ShaderProgramWeakPtr;

// A ShaderProgram represents an OpenGL shader program that can be applied to
// shapes. It contains vertex and fragments shaders.
class ION_API ShaderProgram : public ShaderBase {
 public:
  // Changes that affect the resource.
  enum Changes {
    kVertexShaderChanged = kNumBaseChanges,
    kFragmentShaderChanged,
    kNumChanges
  };

  // A valid ShaderInputRegistryPtr must be passed to the constructor.
  explicit ShaderProgram(const ShaderInputRegistryPtr& registry);

  // Returns the ShaderInputRegistry used for the instance.
  const ShaderInputRegistryPtr& GetRegistry() const { return registry_; }

  // Sets/returns the vertex shader stage.
  void SetVertexShader(const ShaderPtr& shader) {
    if (Shader* old_shader = vertex_shader_.Get().Get())
      old_shader->RemoveReceiver(this);
    vertex_shader_.Set(shader);
    if (shader.Get())
      shader->AddReceiver(this);
  }
  const ShaderPtr& GetVertexShader() const {
    return vertex_shader_.Get();
  }

  // Sets/returns the fragment shader stage.
  void SetFragmentShader(const ShaderPtr& shader) {
    if (Shader* old_shader = fragment_shader_.Get().Get())
      old_shader->RemoveReceiver(this);
    fragment_shader_.Set(shader);
    if (shader.Get())
      shader->AddReceiver(this);
  }
  const ShaderPtr& GetFragmentShader() const {
    return fragment_shader_.Get();
  }

  // Sets/returns whether this shader program should have per-thread state.
  // When this is enabled, it is possible to simultaneously set different
  // uniform values and attribute bindings in each thread, allowing one
  // to concurrently render the same scene from several different viewpoints
  // or render two different objects in two different threads using the same
  // shader program. However, a new OpenGL program object is created for each
  // rendering thread that uses this shader program, which consumes more
  // GPU memory.
  // After this function is called, subsequent calls cannot pass a different
  // value. By default, this setting is disabled, mirroring the behavior of
  // OpenGL.
  void SetConcurrent(bool value);
  bool IsConcurrent() const { return concurrent_; }

  // Convenience function that builds and returns a new ShaderProgram instance
  // that uses the given ShaderInputRegistry and that points to new vertex and
  // fragment Shader instances whose sources are specified as strings. The
  // ShaderProgram's label is set to id_string, the vertex Shader's label is
  // set to id_string + " vertex shader", and the fragment Shader's label is
  // set to id_string + " fragment shader". The allocator is used for the
  // ShaderProgram and both Shaders.
  static const ShaderProgramPtr BuildFromStrings(
    const std::string& id_string,
    const ShaderInputRegistryPtr& registry_ptr,
    const std::string& vertex_shader_string,
    const std::string& fragment_shader_string,
    const base::AllocatorPtr& allocator);

 protected:
  // The destructor is protected because all base::Referent classes must have
  // protected or private destructors.
  ~ShaderProgram() override;

 private:
  // Called when one of the Shaders of this changes.
  void OnNotify(const base::Notifier* notifier) override;

  Field<ShaderPtr> vertex_shader_;
  Field<ShaderPtr> fragment_shader_;
  ShaderInputRegistryPtr registry_;
  // True if each thread should have its own copy of this program object.
  bool concurrent_;
  // True if SetConcurrent was already called on this instance.
  bool concurrent_set_;
};

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_SHADERPROGRAM_H_
