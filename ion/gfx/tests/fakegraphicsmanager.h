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

#ifndef ION_GFX_TESTS_FAKEGRAPHICSMANAGER_H_
#define ION_GFX_TESTS_FAKEGRAPHICSMANAGER_H_

#include <memory>
#include <string>

#include "ion/gfx/graphicsmanager.h"
#include "ion/gfx/tests/fakeglcontext.h"

namespace ion {
namespace gfx {
namespace testing {

// FakeGraphicsManager is a version of GraphicsManager that makes no calls to
// OpenGL. Instead, it uses internal state that simulates those calls.
// NOTE: Not all functions are implemented yet.
class FakeGraphicsManager : public GraphicsManager {
 public:
  FakeGraphicsManager() {
    // VAOs are disabled on Android by default, but some tests rely on them,
    // so force-enable it here.
    EnableFeature(GraphicsManager::kVertexArrays, true);
  }

  // Returns the number of GL functions that have been called since the
  // construction of the FakeGraphicsManager or the last call to
  // ResetCallCount(). This is static because all MockGM calls are made
  // through a thread local static instance.
  static int64 GetCallCount();
  // Resets the call count to 0. This is static because all MockGM calls are
  // made through a thread local static instance.
  static void ResetCallCount();

  // Sets/returns a maximum size allowed for allocating any OpenGL buffer, such
  // as those created by the BufferData() and RenderbufferStorage() functions.
  // This is used primarily for testing out-of-memory errors. The default
  // maximum is 0, meaning that there is no limit.
  void SetMaxBufferSize(GLsizeiptr size_in_bytes);
  GLsizeiptr GetMaxBufferSize() const;

  // Forces a particular function to always fail. This is useful for testing the
  // handling of error cases. Any function set to fail will generate a
  // GL_INVALID_OPERATION and perform whatever action (e.g., do nothing or
  // set internal object state to a failure status) is appropriate.
  void SetForceFunctionFailure(const std::string& func_name, bool always_fails);

  // Sets whether to allow callers to set an invalid enum state, for example
  // setting the depth function to an invalid value. This is useful for testing
  // code that handles technically invalid returns from drivers.
  void EnableInvalidGlEnumState(bool enable);

  // Sets the current OpenGL error code. This is used solely to increase
  // coverage (testing unknown error conditions).
  void SetErrorCode(GLenum error_code);

  // Sets the extensions string of the manager to the passed string.
  void SetExtensionsString(const std::string& extensions);

  // Sets the vendor string of the manager to the passed string.
  void SetVendorString(const std::string& vendor);

  // Sets the renderer string of the manager to the passed string.
  void SetRendererString(const std::string& renderer);

  // Sets the version string of the manager to the passed string.
  void SetVersionString(const std::string& version);

  // Sets the context profile mask of the manager to the passed mask.
  void SetContextProfileMask(int mask);

  // Sets the context flags of the manager to the passed value.
  void SetContextFlags(int flags);

  // Rechecks for function groups and version.
  using GraphicsManager::InitGlInfo;

  // Global platform capability values.
#define ION_WRAP_GL_VALUE(name, sname, gl_enum, Type, init) \
  Type Get ## name() const; \
  void Set ## name(Type value);
#include "ion/gfx/glconstants.inc"

 protected:
  ~FakeGraphicsManager() override {}
};

// Convenience typedef for shared pointer to a GraphicsManager.
using FakeGraphicsManagerPtr = base::SharedPtr<FakeGraphicsManager>;

}  // namespace testing
}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_TESTS_FAKEGRAPHICSMANAGER_H_
