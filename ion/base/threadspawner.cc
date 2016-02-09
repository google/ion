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

#include "ion/base/threadspawner.h"

namespace ion {
namespace base {

namespace {

// Only the current thread can be named, so this wrapper function invokes the
// user-supplied function after optionally naming the thread. This is the
// std::function that gets installed into each Thread instance.
static bool NamingThreadFunc(const std::string& name,
                             const port::ThreadStdFunc& user_func) {
  if (port::IsThreadNamingSupported())
    port::SetThreadName(name);
  return user_func();
}

}  // anonymous namespace

ThreadSpawner::ThreadSpawner(const std::string& name,
                             port::ThreadFuncPtr func_ptr)
    : name_(name),
      func_(func_ptr ? std::bind(NamingThreadFunc, name,
                                 port::ThreadStdFunc(func_ptr))
                     : port::ThreadStdFunc()),
      id_(Spawn()) {
}

ThreadSpawner::ThreadSpawner(const std::string& name,
                             const port::ThreadStdFunc& func)
    : name_(name),
      func_(std::bind(NamingThreadFunc, name, func)),
      id_(Spawn()) {
}

ThreadSpawner::~ThreadSpawner() {
  Join();
}

void ThreadSpawner::Join() {
  if (id_ != port::kInvalidThreadId) {
    port::ThreadId id = id_;
    id_ = port::kInvalidThreadId;
    port::JoinThread(id);
  }
}

port::ThreadId ThreadSpawner::Spawn() {
  if (func_)
    return port::SpawnThreadStd(&func_);
  else
    return port::kInvalidThreadId;
}


}  // namespace base
}  // namespace ion
