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

#ifndef ION_GFXUTILS_COMMANDLISTGENERATOR_H_
#define ION_GFXUTILS_COMMANDLISTGENERATOR_H_

#include "ion/gfx/graphicsmanager.h"

#include <map>
#include <memory>

#include "base/macros.h"
#include "ion/base/allocatable.h"
#include "ion/base/calllist.h"
#include "ion/base/stlalloc/allocvector.h"

namespace ion {
namespace gfxutils {

template <typename ReturnType, typename... ArgsType>
class GraphicsManagerFunction : public base::FunctionCallBase {
 public:
  GraphicsManagerFunction(ReturnType (gfx::GraphicsManager::*func)(ArgsType...)) {

  }
  void operator()() const override = 0;
  ~GraphicsManagerFunction() override {}
};

class ION_API GraphicsManagerCallList : public base::Allocatable {
 public:
  GraphicsManagerCallList();
  ~GraphicsManagerCallList();

  GraphicsManagerCallList(GraphicsManagerCallList&& other);

  // Adds a function call to the list of calls to execute, overloaded for
  // non-member (free) functions. This version of Add() takes a free function
  // directly, which avoids the overhead of a std::function, but only works for
  // free functions.
  template <typename ReturnType, class... Args>
  void Add(ReturnType(gfx::GraphicsManager::*func)(Args...), Args&&... args) {
    calls_.push_back(std::unique_ptr<base::FunctionCallBase>(
        new (GetAllocator()) GraphicsManagerFunction<ReturnType(Args...)>(
            func, std::forward<Args>(args)...)));
  }

  // Executes the stored calls on the passed GraphicsManager.
  void Execute(const gfx::GraphicsManagerPtr& gm);

  // Returns the ith FunctionCall in this list. If the index is invalid or the
  // template arguments for the function signature are incorrect, this returns
  // NULL. This must be called with the function signature as the template
  // argument, e.g., GetCall<void(int)>().
  template <typename Func>
  GraphicsManagerFunction<Func>* GetCall(size_t i) {
    return i >= calls_.size()
               ? NULL
               : dynamic_cast<GraphicsManagerFunction<Func>*>(calls_[i].get());
  }

  // Clears all calls.
  void Reset() { calls_.clear(); }

 private:
  // The vector of function calls to make.
  base::AllocVector<std::unique_ptr<base::FunctionCallBase>> calls_;
};

// CommandListGenerator generates a sequence of GL commands that can be replayed
// multiple times.
class ION_API CommandListGenerator : public gfx::GraphicsManager {
 public:
  CommandListGenerator();

  // Starts a capture. Noop if a capture is already underway.
  void Begin();
  // Stops a capture and returns a list of calls to make.
  std::unique_ptr<GraphicsManagerCallList> End();

 private:
  // Redefines this to use internal versions of the OpenGL functions.  These
  // functions wrap the actual GL calls.
  void* Lookup(const char* name, bool is_core) override;

  // Mapping from string names to function pointers.
  std::map<std::string, void*> functions_;

  // Current in-progess list of calls.
  std::unique_ptr<GraphicsManagerCallList> call_list_;

#define ION_WRAP_GL_FUNC(group, name, return_type, typed_args, args, trace) \
  static return_type ION_APIENTRY List##name typed_args;

#include "ion/gfx/glfunctions.inc"

};

}  // namespace gfxutils
}  // namespace ion

#endif  // ION_GFXUTILS_COMMANDLISTGENERATOR_H_
