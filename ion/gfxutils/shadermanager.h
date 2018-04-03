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

#ifndef ION_GFXUTILS_SHADERMANAGER_H_
#define ION_GFXUTILS_SHADERMANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "ion/base/referent.h"
#include "ion/gfx/renderer.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfx/shaderprogram.h"
#include "ion/gfxutils/shadersourcecomposer.h"

namespace ion {
namespace gfxutils {

// ShaderManager contains an association between shader programs, their names,
// and any source dependencies they have.
class ION_API ShaderManager : public base::Referent {
 public:
  struct ShaderSourceComposerSet {
    ShaderSourceComposerPtr vertex_source_composer;
    ShaderSourceComposerPtr tess_control_source_composer;
    ShaderSourceComposerPtr tess_evaluation_source_composer;
    ShaderSourceComposerPtr geometry_source_composer;
    ShaderSourceComposerPtr fragment_source_composer;
  };

  ShaderManager();

  // Creates and returns a ShaderProgram with the passed name using the passed
  // composers and registry.
  const gfx::ShaderProgramPtr CreateShaderProgram(
      const std::string& name, const ion::gfx::ShaderInputRegistryPtr& registry,
      const ShaderSourceComposerSet& set);

  // DEPRECTATED.
  // Use the ShaderSourceComposerSet version instead.
  const gfx::ShaderProgramPtr CreateShaderProgram(
      const std::string& name, const ion::gfx::ShaderInputRegistryPtr& registry,
      const ShaderSourceComposerPtr& vertex_source_composer,
      const ShaderSourceComposerPtr& fragment_source_composer,
      const ShaderSourceComposerPtr& geometry_source_composer =
        ShaderSourceComposerPtr());

  // Returns a ReferentPtr to a ShaderProgram that has the passed name. If no
  // program with the passed name exists, returns a NULL ShaderProgramPtr.
  const gfx::ShaderProgramPtr GetShaderProgram(const std::string& name);

  // Gets a vector of the names of the shader programs created through the
  // manager.
  const std::vector<std::string> GetShaderProgramNames();

  // Gets the composers used to construct the named program's shaders. Either
  // of the passed pointers may be NULL. If the named program does not exist
  // then the passed composers will be set to NULL.
  void GetShaderProgramComposers(const std::string& name,
                                 ShaderSourceComposerSet* set);

  // DEPRECATED.
  // Use the ShaderSourceComposerSet version instead.
  void GetShaderProgramComposers(
      const std::string& name,
      ShaderSourceComposerPtr* vertex_source_composer,
      ShaderSourceComposerPtr* fragment_source_composer,
      ShaderSourceComposerPtr* geometry_source_composer = nullptr);

  // Reconstructs all shaders from their composers.
  void RecreateAllShaderPrograms();

  // Reconstructs all shaders that depend on the named dependency. The passed
  // dependency name could be a filename or some other identifier that a
  // ShaderSourceComposer will recognize.
  void RecreateShaderProgramsThatDependOn(const std::string& dependency);

 private:
  // Internal class that holds the ShaderManager's implementation.
  class ShaderManagerData;

  // The destructor is private because this is derived from base::Referent.
  ~ShaderManager() override;

  std::unique_ptr<ShaderManagerData> data_;

  DISALLOW_COPY_AND_ASSIGN(ShaderManager);
};

using ShaderManagerPtr = base::SharedPtr<ShaderManager>;

}  // namespace gfxutils
}  // namespace ion

#endif  // ION_GFXUTILS_SHADERMANAGER_H_
