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

#include "ion/gfx/tests/mockgraphicsmanager.h"

#include <algorithm>
#include <bitset>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "ion/base/allocator.h"
#include "ion/base/logging.h"
#include "ion/base/stringutils.h"
#include "ion/gfx/framebufferobject.h"
#include "ion/gfx/openglobjects.h"
#include "ion/gfx/tests/mockvisual.h"
#include "ion/math/range.h"
#include "ion/math/utils.h"
#include "ion/math/vector.h"

namespace ion {
namespace gfx {
namespace testing {

//-----------------------------------------------------------------------------
//
// MockGraphicsManager class functions.
//
//-----------------------------------------------------------------------------

MockGraphicsManager::MockGraphicsManager() : GraphicsManager(this) {
  DCHECK(MockVisual::GetCurrent());
  InitMockFunctions();
}

void MockGraphicsManager::InitMockFunctions() {
#define ION_WRAP_GL_FUNC(group, name, return_type, typed_args, args, trace) \
  functions_["gl" #name] = reinterpret_cast<void*>(&MockVisual::Wrapped##name)

#include "ion/gfx/glfunctions.inc"

  // Install our versions of wrapped OpenGL functions.
  ReinitFunctions();
}

int64 MockGraphicsManager::GetCallCount() {
  return MockVisual::GetCallCount();
}

void MockGraphicsManager::ResetCallCount() {
  MockVisual::ResetCallCount();
}

void MockGraphicsManager::SetMaxBufferSize(GLsizeiptr size_in_bytes) {
  MockVisual::GetCurrent()->SetMaxBufferSize(size_in_bytes);
}

GLsizeiptr MockGraphicsManager::GetMaxBufferSize() const {
  return MockVisual::GetCurrent()->GetMaxBufferSize();
}

void MockGraphicsManager::SetForceFunctionFailure(
    const std::string& func_name, bool always_fails) {
  MockVisual::GetCurrent()->SetForceFunctionFailure(func_name, always_fails);
}

void MockGraphicsManager::SetErrorCode(GLenum error_code) {
  MockVisual::GetCurrent()->SetErrorCode(error_code);
}

void MockGraphicsManager::SetExtensionsString(const std::string& extensions) {
  MockVisual::GetCurrent()->SetExtensionsString(extensions);
  InitGlInfo();
}

void MockGraphicsManager::SetVendorString(const std::string& vendor) {
  MockVisual::GetCurrent()->SetVendorString(vendor);
  InitGlInfo();
}

void MockGraphicsManager::SetRendererString(const std::string& renderer) {
  MockVisual::GetCurrent()->SetRendererString(renderer);
  InitGlInfo();
}

void MockGraphicsManager::SetVersionString(const std::string& version) {
  MockVisual::GetCurrent()->SetVersionString(version);
  InitGlInfo();
}

void MockGraphicsManager::SetContextProfileMask(int mask) {
  MockVisual::GetCurrent()->SetContextProfileMask(mask);
  InitGlInfo();
}

// Global platform capability values.
#define ION_PLATFORM_CAP(type, name) \
  type MockGraphicsManager::Get ## name() const { \
    return MockVisual::GetCurrent()->Get ## name(); \
  } \
  void MockGraphicsManager::Set ## name(type value) { \
    MockVisual::GetCurrent()->Set ## name(value); \
  }
#include "ion/gfx/tests/glplatformcaps.inc"

void* MockGraphicsManager::Lookup(const char* name, bool is_core) {
  return functions_[name];
}

void MockGraphicsManager::EnableFunctionGroupIfAvailable(
    GraphicsManager::FunctionGroupId group, const GlVersions& versions,
    const std::string& extensions, const std::string& disabled_renderers) {
  GraphicsManager::EnableFunctionGroupIfAvailable(group, versions, extensions,
                                                  disabled_renderers);

  // If the group was disabled by the parent class, override here so that we can
  // have deterministic testing of all platforms.
  if (!IsFunctionGroupAvailable(group)) {
    EnableFunctionGroup(group, true);
    const std::vector<std::string> names = base::SplitString(extensions, ",");
    const size_t count = names.size();

    // If the GL version is high enough then we don't need to check extensions.
    if (versions[GetGlApiStandard()] &&
        GetGlVersion() >= versions[GetGlApiStandard()])
      return;

    // Check extensions.
    for (size_t i = 0; i < count; ++i) {
      if (IsExtensionSupported(names[i]))
        return;
    }
    EnableFunctionGroup(group, false);
  }
}

}  // namespace testing
}  // namespace gfx
}  // namespace ion
