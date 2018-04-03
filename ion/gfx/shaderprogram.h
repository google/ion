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

#ifndef ION_GFX_SHADERPROGRAM_H_
#define ION_GFX_SHADERPROGRAM_H_

#include "ion/base/referent.h"
#include "ion/gfx/resourceholder.h"
#include "ion/gfx/shader.h"
#include "ion/gfx/shaderinputregistry.h"

namespace ion {
namespace gfx {

// Base class for ShaderProgram and ComputeProgram objects.
class ION_API ProgramBase : public ShaderBase {
 public:
  // Returns the ShaderInputRegistry used for the instance.
  const ShaderInputRegistryPtr& GetRegistry() const { return registry_; }

  // Sets/returns whether this program should have per-thread state.
  // When this is enabled, it is possible to simultaneously set different
  // uniform values and attribute bindings in each thread, allowing one
  // to concurrently render the same scene from several different viewpoints
  // or render two different objects in two different threads using the same
  // program. However, a new OpenGL program object is created for each thread
  // that uses this program, which consumes more GPU memory.
  // After this function is called, subsequent calls cannot pass a different
  // value. By default, this setting is disabled, mirroring the behavior of
  // OpenGL.
  void SetConcurrent(bool value);
  bool IsConcurrent() const { return concurrent_; }

 protected:
  // A valid ShaderInputRegistryPtr must be passed to the constructor.
  explicit ProgramBase(const ShaderInputRegistryPtr& registry);
  ~ProgramBase() override;

 private:
  ShaderInputRegistryPtr registry_;
  // True if each thread should have its own copy of this program object.
  bool concurrent_;
  // True if SetConcurrent was already called on this instance.
  bool concurrent_set_;
};

// Convenience typedefs for shared pointers to a ShaderProgram.
class ShaderProgram;
using ShaderProgramPtr = base::SharedPtr<ShaderProgram>;
typedef base::WeakReferentPtr<ShaderProgram> ShaderProgramWeakPtr;

// A ShaderProgram represents an OpenGL shader program that can be applied to
// shapes. It contains vertex, fragment, and geometry shaders.
class ION_API ShaderProgram : public ProgramBase {
 public:
  // Changes that affect the resource.
  enum Changes {
    kVertexShaderChanged = kNumBaseChanges,
    kGeometryShaderChanged,
    kFragmentShaderChanged,
    kTessControlShaderChanged,
    kTessEvaluationShaderChanged,
    kCapturedVaryingsChanged,
    kNumChanges
  };

  // A valid ShaderInputRegistryPtr must be passed to the constructor.
  explicit ShaderProgram(const ShaderInputRegistryPtr& registry);

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

  // Sets/returns the geometry shader stage.
  void SetGeometryShader(const ShaderPtr& shader) {
    if (Shader* old_shader = geometry_shader_.Get().Get())
      old_shader->RemoveReceiver(this);
    geometry_shader_.Set(shader);
    if (shader.Get())
      shader->AddReceiver(this);
  }
  const ShaderPtr& GetGeometryShader() const {
    return geometry_shader_.Get();
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

  void SetTessControlShader(const ShaderPtr& shader) {
    if (Shader* old_shader = tess_ctrl_shader_.Get().Get())
      old_shader->RemoveReceiver(this);
    tess_ctrl_shader_.Set(shader);
    CHECK(shader.Get());
    if (shader.Get())
      shader->AddReceiver(this);
  }
  const ShaderPtr& GetTessControlShader() const {
    return tess_ctrl_shader_.Get();
  }

  void SetTessEvalShader(const ShaderPtr& shader) {
    if (Shader* old_shader = tess_eval_shader_.Get().Get())
      old_shader->RemoveReceiver(this);
    tess_eval_shader_.Set(shader);
    if (shader.Get())
      shader->AddReceiver(this);
  }
  const ShaderPtr& GetTessEvalShader() const {
    return tess_eval_shader_.Get();
  }

  // Sets/returns the names of vertex shader outputs and geometry shader
  // outputs that should be captured when transform feedback is active.
  void SetCapturedVaryings(const std::vector<std::string>& varyings) {
    varyings_.Set(base::AllocVector<std::string>(*this, varyings));
  }
  const base::AllocVector<std::string>& GetCapturedVaryings() const {
    return varyings_.Get();
  }

  // Convenience function that builds and returns a new ShaderProgram instance
  // that uses the given ShaderInputRegistry and that points to new vertex,
  // fragment, geometry and tessellation Shader instances whose sources are
  // specified as strings. The
  // ShaderProgram's label is set to id_string and the Shader labels are set to
  // id_string + (type of shader) + " shader". The allocator is used for the
  // ShaderProgram and all Shaders.
  static ShaderProgramPtr BuildFromStrings(
      const std::string& id_string,
      const ShaderInputRegistryPtr& registry_ptr,
      const std::string& vertex_shader_string,
      const std::string& tess_ctrl_shader_string,
      const std::string& tess_eval_shader_string,
      const std::string& geometry_shader_string,
      const std::string& fragment_shader_string,
      const base::AllocatorPtr& allocator);
  // This variant omits tessellation shaders.
  static ShaderProgramPtr BuildFromStrings(
      const std::string& id_string,
      const ShaderInputRegistryPtr& registry_ptr,
      const std::string& vertex_shader_string,
      const std::string& geometry_shader_string,
      const std::string& fragment_shader_string,
      const base::AllocatorPtr& allocator);
  // This variant omits the geometry shader.
  static ShaderProgramPtr BuildFromStrings(
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
  Field<ShaderPtr> tess_ctrl_shader_;
  Field<ShaderPtr> tess_eval_shader_;
  Field<ShaderPtr> geometry_shader_;
  Field<ShaderPtr> fragment_shader_;

  Field<base::AllocVector<std::string>> varyings_;
};

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_SHADERPROGRAM_H_
