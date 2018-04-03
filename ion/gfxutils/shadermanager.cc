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

#include "ion/gfxutils/shadermanager.h"

#include <mutex>  // NOLINT(build/c++11)

#include "ion/base/allocatable.h"
#include "ion/base/logging.h"
#include "ion/base/stlalloc/allocmap.h"
#include "ion/gfx/resourcemanager.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfx/shaderprogram.h"
#include "ion/gfxutils/resourcecallback.h"
#include "ion/gfxutils/shadersourcecomposer.h"

namespace ion {
namespace gfxutils {

namespace {

using gfx::GraphicsManagerPtr;
using gfx::Shader;
using gfx::ShaderProgram;
using gfx::ShaderProgramPtr;
using gfx::ShaderPtr;

}  // anonymous namespace


// Class that contains the details of ShaderManager's implementation.
class ShaderManager::ShaderManagerData : public base::Allocatable {
 public:
  // Helper struct that stores the latest info log for a shader program, a
  // WeakPtr to the program itself, and the composers used to construct the
  // program's shaders.
  struct ProgramInfo {
    ProgramInfo() : program(ShaderProgramPtr()) {}
    explicit ProgramInfo(const ShaderProgramPtr& program_in)
        : program(program_in) {}
    gfx::ShaderProgramWeakPtr program;
    ShaderSourceComposerPtr vertex_source_composer;
    ShaderSourceComposerPtr fragment_source_composer;
    ShaderSourceComposerPtr geometry_source_composer;
    ShaderSourceComposerPtr tess_control_source_composer;
    ShaderSourceComposerPtr tess_evaluation_source_composer;
  };
  typedef base::AllocMap<std::string, ProgramInfo> ProgramMap;

  explicit ShaderManagerData(const base::Allocatable& owner)
      : programs_(owner) {}
  ~ShaderManagerData() override {}

  // Adds a ProgramInfo to the map of infos.
  void AddProgramInfo(const std::string& name, const ProgramInfo& info) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (FindProgramInfo(name) != programs_.end()) {
      LOG(WARNING) << "ShaderManager: Overriding existing ShaderProgram named "
                   << name;
    }
    programs_[name] = info;
  }

  // This and the below functions follow the same interface as their
  // counterparts in ShaderManager. See shadermanager.h for detailed comments.
  const ShaderProgramPtr GetShaderProgram(const std::string& name) {
    std::lock_guard<std::mutex> guard(mutex_);
    ProgramMap::iterator it = FindProgramInfo(name);
    return GetProgramFromInfo(&it);
  }

  const std::vector<std::string> GetShaderProgramNames() {
    std::vector<std::string> names;
    names.reserve(programs_.size());
    for (ProgramMap::iterator it = programs_.begin(); it != programs_.end();) {
      if (GetProgramFromInfo(&it).Get()) {
        names.push_back(it->first);
        ++it;
      }
    }
    return names;
  }

  void GetShaderProgramComposers(const std::string& name,
                                 ShaderSourceComposerSet* set) {
    ShaderProgramPtr program;
    std::lock_guard<std::mutex> guard(mutex_);
    ProgramMap::iterator it = FindProgramInfo(name);
    if (it != programs_.end()) {
      set->vertex_source_composer = it->second.vertex_source_composer;
      set->tess_control_source_composer =
          it->second.tess_control_source_composer;
      set->tess_evaluation_source_composer =
          it->second.tess_evaluation_source_composer;
      set->geometry_source_composer = it->second.geometry_source_composer;
      set->fragment_source_composer = it->second.fragment_source_composer;
    } else {
      set->vertex_source_composer = nullptr;
      set->tess_control_source_composer = nullptr;
      set->tess_evaluation_source_composer = nullptr;
      set->geometry_source_composer = nullptr;
      set->fragment_source_composer = nullptr;
    }
  }

  void RecreateAllShaderPrograms() {
    std::lock_guard<std::mutex> guard(mutex_);
    for (ProgramMap::iterator it = programs_.begin(); it != programs_.end();) {
      const ShaderProgramPtr& program = GetProgramFromInfo(&it);
      if (program.Get()) {
        const ProgramInfo& info = it->second;
        DCHECK(program.Get());

        if (Shader* shader = program->GetVertexShader().Get())
          shader->SetSource(info.vertex_source_composer->GetSource());
        if (Shader* shader = program->GetTessControlShader().Get())
          shader->SetSource(info.tess_control_source_composer->GetSource());
        if (Shader* shader = program->GetTessEvalShader().Get())
          shader->SetSource(info.tess_evaluation_source_composer->GetSource());
        if (Shader* shader = program->GetGeometryShader().Get())
          shader->SetSource(info.geometry_source_composer->GetSource());
        if (Shader* shader = program->GetFragmentShader().Get())
          shader->SetSource(info.fragment_source_composer->GetSource());
        ++it;
      }
    }
  }

  void RecreateShaderProgramsThatDependOn(const std::string& dependency) {
    std::lock_guard<std::mutex> guard(mutex_);
    for (ProgramMap::iterator it = programs_.begin(); it != programs_.end();) {
      const ShaderProgramPtr& program = GetProgramFromInfo(&it);
      if (program.Get()) {
        const ProgramInfo& info = it->second;
        if (info.vertex_source_composer->DependsOn(dependency)) {
          if (Shader* shader = program->GetVertexShader().Get())
            shader->SetSource(info.vertex_source_composer->GetSource());
        }
        if (info.tess_control_source_composer.Get() != nullptr) {
          if (info.tess_control_source_composer->DependsOn(dependency)) {
            if (Shader* shader = program->GetTessControlShader().Get())
              shader->SetSource(info.tess_control_source_composer->GetSource());
          }
        }
        if (info.tess_evaluation_source_composer.Get() != nullptr) {
          if (info.tess_evaluation_source_composer->DependsOn(dependency)) {
            if (Shader* shader = program->GetTessEvalShader().Get())
              shader->SetSource(
                info.tess_evaluation_source_composer->GetSource());
          }
        }
        if (info.geometry_source_composer.Get() != nullptr) {
          if (info.geometry_source_composer->DependsOn(dependency)) {
            if (Shader* shader = program->GetGeometryShader().Get()) {
              shader->SetSource(info.geometry_source_composer->GetSource());
            }
          }
        }
        if (info.fragment_source_composer.Get() != nullptr) {
          if (info.fragment_source_composer->DependsOn(dependency)) {
            if (Shader* shader = program->GetFragmentShader().Get())
              shader->SetSource(info.fragment_source_composer->GetSource());
          }
        }
        ++it;
      }
    }
  }

 private:
  // Gets a ProgramInfo::iterator if the named program exists in the map. If it
  // does not or the program has been deleted then the returned iterator will
  // point to the end of the map.
  ProgramMap::iterator FindProgramInfo(const std::string& name) {
    ProgramMap::iterator it = programs_.find(name);
    if (!GetProgramFromInfo(&it).Get())
      it = programs_.end();
    return it;
  }

  // Returns whether the ProgramInfo pointed to by the passed iterator contains
  // a program that is still active (that is, that the WeakPtr can Acquire() a
  // ReferentPtr successfully). The iterator is passed as a pointer because it
  // might be erase()d.
  const ShaderProgramPtr GetProgramFromInfo(ProgramMap::iterator* it) {
    ShaderProgramPtr program;
    if ((*it) != programs_.end()) {
      program = (*it)->second.program.Acquire();
      if (!program.Get()) {
        // The program has been destroyed if we cannot Acquire() it. Remove it
        // from the map if that is the case.
        *it = programs_.erase(*it);
      }
    }
    return program;
  }

  // All shader programs registered with the manager.
  ProgramMap programs_;

  // For locking access to programs_.
  std::mutex mutex_;
};

ShaderManager::ShaderManager()
    : data_(new(GetAllocator()) ShaderManagerData(*this)) {}

ShaderManager::~ShaderManager() {}

const ShaderProgramPtr ShaderManager::CreateShaderProgram(
    const std::string& name, const ion::gfx::ShaderInputRegistryPtr& registry,
    const ShaderSourceComposerSet& set) {
  // Create ProgramInfo and add it to the map.
  ShaderProgramPtr program(new(GetAllocatorForLifetime(base::kMediumTerm))
                           ion::gfx::ShaderProgram(registry));
  ShaderManagerData::ProgramInfo info(program);
  program->SetLabel(name);
  program->SetVertexShader(
      ShaderPtr(new(GetAllocatorForLifetime(base::kMediumTerm))
                Shader(set.vertex_source_composer->GetSource())));
  program->GetVertexShader()->SetLabel(name + " vertex shader");

  if (set.tess_control_source_composer.Get() != nullptr) {
    program->SetTessControlShader(
        ShaderPtr(new(GetAllocatorForLifetime(base::kMediumTerm))
                  Shader(set.tess_control_source_composer->GetSource())));
    program->GetTessControlShader()->SetLabel(
        name + " tessellation control shader");
  }

  if (set.tess_evaluation_source_composer.Get() != nullptr) {
    program->SetTessEvalShader(
        ShaderPtr(new(GetAllocatorForLifetime(base::kMediumTerm))
                  Shader(set.tess_evaluation_source_composer->GetSource())));
    program->GetTessEvalShader()->SetLabel(
        name + " tessellation evaluation shader");
  }

  if (set.geometry_source_composer.Get() != nullptr) {
    program->SetGeometryShader(
        ShaderPtr(new(GetAllocatorForLifetime(base::kMediumTerm))
                  Shader(set.geometry_source_composer->GetSource())));
    program->GetGeometryShader()->SetLabel(name + " geometry shader");
  }

  if (set.fragment_source_composer.Get() != nullptr) {
    program->SetFragmentShader(ShaderPtr(new (GetAllocatorForLifetime(
        base::kMediumTerm)) Shader(set.fragment_source_composer->GetSource())));
    program->GetFragmentShader()->SetLabel(name + " fragment shader");
  }

  info.vertex_source_composer = set.vertex_source_composer;
  info.fragment_source_composer = set.fragment_source_composer;
  info.geometry_source_composer = set.geometry_source_composer;
  info.tess_control_source_composer = set.tess_control_source_composer;
  info.tess_evaluation_source_composer = set.tess_evaluation_source_composer;
  data_->AddProgramInfo(name, info);

  return program;
}

const ShaderProgramPtr ShaderManager::CreateShaderProgram(
    const std::string& name, const ion::gfx::ShaderInputRegistryPtr& registry,
    const ShaderSourceComposerPtr& vertex_source_composer,
    const ShaderSourceComposerPtr& fragment_source_composer,
    const ShaderSourceComposerPtr& geometry_source_composer) {
  ShaderSourceComposerSet set;
  set.vertex_source_composer = vertex_source_composer;
  set.fragment_source_composer = fragment_source_composer;
  set.geometry_source_composer = geometry_source_composer;
  return CreateShaderProgram(name, registry, set);
}

const ShaderProgramPtr ShaderManager::GetShaderProgram(
    const std::string& name) {
  return data_->GetShaderProgram(name);
}

const std::vector<std::string> ShaderManager::GetShaderProgramNames() {
  return data_->GetShaderProgramNames();
}

void ShaderManager::GetShaderProgramComposers(
    const std::string& name,
    ShaderSourceComposerPtr* vertex_source_composer,
    ShaderSourceComposerPtr* fragment_source_composer,
    ShaderSourceComposerPtr* geometry_source_composer) {
  ShaderSourceComposerSet set;
  data_->GetShaderProgramComposers(name, &set);
  if (vertex_source_composer) {
    *vertex_source_composer = set.vertex_source_composer;
  }
  if (fragment_source_composer) {
    *fragment_source_composer = set.fragment_source_composer;
  }
  if (geometry_source_composer) {
    *geometry_source_composer = set.geometry_source_composer;
  }
}

void ShaderManager::GetShaderProgramComposers(
    const std::string& name,
    ShaderManager::ShaderSourceComposerSet* set) {
  data_->GetShaderProgramComposers(name, set);
}

void ShaderManager::RecreateAllShaderPrograms() {
  data_->RecreateAllShaderPrograms();
}

void ShaderManager::RecreateShaderProgramsThatDependOn(
    const std::string& dependency) {
  data_->RecreateShaderProgramsThatDependOn(dependency);
}

}  // namespace gfxutils
}  // namespace ion
