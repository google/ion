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

#ifndef ION_DEMOS_DEMOBASE_H_
#define ION_DEMOS_DEMOBASE_H_

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>

#include "ion/gfx/graphicsmanager.h"
#include "ion/gfx/image.h"

//-----------------------------------------------------------------------------
//
// The DemoBase class abstracts out a demo program's responses to events in a
// platform-independent way. It also wraps the ownership of a Remote server.
//
//-----------------------------------------------------------------------------

namespace ion {
namespace remote {
class RemoteServer;
}  // namespace remote
}  // namespace ion

class DemoBase {
 public:
  virtual ~DemoBase();
  virtual void Resize(int width, int height) = 0;
  virtual void Update() = 0;
  virtual void Render() = 0;
  virtual void Keyboard(int key, int x, int y, bool is_press) = 0;
  virtual void ProcessMotion(float x, float y, bool is_press) = 0;
  virtual void ProcessScale(float scale) = 0;

  // NOTE: we can't use a less verbose function name such as GetClassName,
  // because it would cause build failures on Windows. When the <windows.h>
  // Win32 API header is included, GetClassName is a #define to either
  // GetClassNameA or GetClassNameW depending on whether the macro UNICODE
  // is defined. This means we can't use the names of any Windows functions
  // that take or return strings as an identifier in ANY namespace.
  virtual std::string GetDemoClassName() const = 0;
  virtual std::string GetDemoAppName() const {
    std::string name = GetDemoClassName();
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    return name;
  }

#if !ION_PRODUCTION
  static ion::remote::RemoteServer* GetRemoteServer() {
    return remote_.get();
  }

 protected:
  static std::unique_ptr<ion::remote::RemoteServer> remote_;
#endif
};

// Define this in the demo file.
// It should simply construct and return an instance of the derived demo class.
// |width|, |height| specify the dimensions of the window that was created by
// platform-specific setup code before this function is run.
std::unique_ptr<DemoBase> CreateDemo(int width, int height);

// Rewrite the shader to be compatible with the given GL version.
// 
const std::string RewriteShader(const std::string& source,
                                ion::gfx::GraphicsManager::GlFlavor gl_flavor,
                                unsigned int version, bool is_fragment_shader);

#endif  // ION_DEMOS_DEMOBASE_H_
