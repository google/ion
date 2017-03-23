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

#include "ion/base/threadspawner.h"

#include <functional>

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
    : name_(name) {
  if (func_ptr) {
    thread_ = std::thread(std::bind(NamingThreadFunc, name, func_ptr));
  }
}

ThreadSpawner::ThreadSpawner(const std::string& name,
                             const port::ThreadStdFunc& func)
    : name_(name), thread_(std::bind(NamingThreadFunc, name, func)) {}

ThreadSpawner::~ThreadSpawner() {
  Join();
}

void ThreadSpawner::Join() {
  if (thread_.get_id() != std::thread::id()) thread_.join();
}

}  // namespace base
}  // namespace ion
