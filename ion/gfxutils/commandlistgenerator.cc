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

#include "ion/gfxutils/commandlistgenerator.h"

#include "ion/base/staticsafedeclare.h"
#include "ion/base/threadlocalobject.h"
#include "ion/portgfx/visual.h"

namespace ion {
namespace gfxutils {

namespace {

using portgfx::Visual;
using portgfx::VisualPtr;

class CommandGeneratorVisual;
using CommandGeneratorVisualPtr = base::SharedPtr<CommandGeneratorVisual>;

struct CommandGeneratorInfo {
  CommandGeneratorInfo() {}
  void Reset() {
    generator = nullptr;
    visual.Reset();
    old_visual = nullptr;
  }
  CommandListGenerator* generator;
  CommandGeneratorVisualPtr visual;
  VisualPtr old_visual;
};

static CommandGeneratorInfo* GetGeneratorInfo() {
  ION_DECLARE_SAFE_STATIC_POINTER(base::ThreadLocalObject<CommandGeneratorInfo>,
                                  s_info);
  return s_info->Get();
}

class CommandGeneratorVisual : public Visual {
 public:
  CommandGeneratorVisual() {
    SetId(GetVisualId());
  }

  ~CommandGeneratorVisual() override {
    if (GetCurrent().Get() == this) {
      Visual::MakeCurrent(VisualPtr());
    }
  }

 protected:
  uint32 GetVisualId() {
    static std::atomic<uint32> counter(0);
    // Use a 1-based counter.
    return ++counter;
  }

  // Override Visual::CreateVisualInShareGroup() in order to return a
  // CommandGeneratorVisual instead of a Visual.
  Visual* CreateVisualInShareGroup() const override {
    return new CommandGeneratorVisual();
  }

  // No-op, so that a CommandGeneratorVisual can be made current.
  bool MakeCurrent() const override { return true; }
  void UpdateId() override {
    SetId(GetVisualId());
  }
};

}  // anonymous namespace

GraphicsManagerCallList::GraphicsManagerCallList() : calls_(*this) {}

GraphicsManagerCallList::~GraphicsManagerCallList() {}

GraphicsManagerCallList::GraphicsManagerCallList(
    GraphicsManagerCallList&& other) : calls_(*this) {
  calls_.reserve(other.calls_.size());
  for (auto& call : other.calls_) {
    calls_.push_back(std::move(call));
  }
}

void GraphicsManagerCallList::Execute(const gfx::GraphicsManagerPtr& gm) {

}

CommandListGenerator::CommandListGenerator() {
#define ION_WRAP_GL_FUNC(group, name, return_type, typed_args, args, trace) \
  functions_["gl" #name] =                                                  \
      reinterpret_cast<void*>(&CommandListGenerator::List##name);

#include "ion/gfx/glfunctions.inc"

  // Install our versions of wrapped OpenGL functions.
  ReinitFunctions();
}

void CommandListGenerator::Begin() {
  CommandGeneratorInfo* info = GetGeneratorInfo();
  if (info->generator || info->visual) {
    LOG(ERROR) << "Call End() before calling Begin() again.";
    return;
  }

  // Create a fake visual and make it current so that GL state is not cached.
  info->old_visual = Visual::GetCurrent();
  info->generator = this;
  info->visual.Reset(new CommandGeneratorVisual);
  Visual::MakeCurrent(info->visual);
  call_list_.reset();
}

std::unique_ptr<GraphicsManagerCallList> CommandListGenerator::End() {
  CommandGeneratorInfo* info = GetGeneratorInfo();
  if (!info->generator || !info->visual) {
    LOG(ERROR) << "Call Begin() before calling End().";
    std::unique_ptr<GraphicsManagerCallList>();
  }
  Visual::MakeCurrent(info->old_visual);
  info->Reset();
  return std::move(call_list_);
}

void* CommandListGenerator::Lookup(const char* name, bool is_core) {
  return functions_[name];
}

#define ION_WRAP_GL_FUNC(group, name, return_type, typed_args, args, trace) \
  return_type CommandListGenerator::List##name typed_args {                 \
    typedef return_type(*FuncType) typed_args;                              \
    static FuncType gl_function =                                           \
        portgfx::GetGlProcAddress(#name, group == kCore);                   \
    return gl_function args;                                                \
  }

#include "ion/gfx/glfunctions.inc"

}  // namespace gfxutils
}  // namespace ion
