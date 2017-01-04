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

#ifndef ION_GFX_TESTS_MOCKVISUAL_H_
#define ION_GFX_TESTS_MOCKVISUAL_H_

#include <memory>

// Include graphics manager to get special gl typedefs needed for compatibility
// with MockGraphicsManager's expectations.
#include "ion/base/sharedptr.h"
#include "ion/gfx/graphicsmanager.h"
#include "ion/port/atomic.h"
#include "ion/portgfx/visual.h"

namespace ion {
namespace gfx {
namespace testing {

// MockVisual is a replacement for Visual which supports the use of
// MockGraphicsManager in the same way Visual supports the use of
// GraphicsManager.
class MockVisual : public portgfx::Visual {
 public:
  // Shadows OpenGL state.
  class ShadowState;

  // Constructs a MockVisual that shares non-container OpenGL resources with
  // |share_visual| (i.e. all resources except framebuffers, vertex arrays,
  // program pipelines, and transform feedbacks).  Note the following important
  // points:
  //   - Operations on MockVisual are not thread-safe.
  //   - The MockVisual is not set as current; that should be done on the thread
  //     it will be used on.
  //   - Both the original and new MockVisual will respond to IsValid() with the
  //     same result (unless one is later invalidated).
  static base::SharedPtr<MockVisual> CreateShared(
      const MockVisual& share_visual);

  // Constructs a mock visual.
  static base::SharedPtr<MockVisual> Create(int window_width,
                                            int window_height);
  ~MockVisual() override;

  // Visual implementation.
  bool IsValid() const override { return is_valid_; }
  void* GetProcAddress(const char* proc_name, bool is_core) const override;
  bool MakeContextCurrentImpl() override { return true; }
  void ClearCurrentContextImpl() override {}
  portgfx::VisualPtr CreateVisualInShareGroupImpl(
      const portgfx::VisualSpec&) override {
    // The returned MockVisual will be valid iff |*this| is valid.
    return CreateShared(*this);
  }
  bool IsOwned() const override { return true; }

  // Sets the value that will subsequently be returned by IsValid().  This
  // allows testing that code work with both valid and invalid MockVisuals.
  void SetValid(bool valid) { is_valid_ = valid; }

  // Gets the current Visual, as a MockVisual.
  static base::SharedPtr<MockVisual> GetCurrent();

  // Gets the number of times an OpenGL function has been invoked on the
  // currently active MockVisual, since the last reset.
  static int64 GetCallCount() { return GetCurrent()->call_count_; }

  // Resets the call count of the currently active MockVisual to zero.
  static void ResetCallCount() { GetCurrent()->call_count_ = 0; }

  static ShadowState* IncrementAndCall(const char* name);

 private:
  friend class MockGraphicsManager;

  MockVisual(std::unique_ptr<ShadowState> shadow_state, bool is_valid);

  // Sets/returns a maximum size allowed for allocating any OpenGL buffer.
  // This is used primarily for testing out-of-memory errors.
  void SetMaxBufferSize(GLsizeiptr size_in_bytes);
  GLsizeiptr GetMaxBufferSize() const;

  // Gets/sets the current OpenGL error code for testing.
  GLenum GetErrorCode() const;
  void SetErrorCode(GLenum error_code);

  // Sets the extensions string of the manager to the passed string for testing.
  void SetExtensionsString(const std::string& extensions);
  // Sets the vendor string of the manager to the passed string for testing.
  void SetVendorString(const std::string& vendor);
  // Sets the renderer string of the manager to the passed string for testing.
  void SetRendererString(const std::string& renderer);
  // Sets the version string of the manager to the passed string for testing.
  void SetVersionString(const std::string& version);
  // Sets the context profile mask of the manager to the passed mask.
  void SetContextProfileMask(int mask);

  // If always_fails is set to true, forces future calls of the referenced
  // function to fail with an invalid operation error. Calling with
  // always_fails false re-enables the function. This is useful for testing
  // that rendering code is robust to GL library failures or partial
  // implementations.
  void SetForceFunctionFailure(const std::string& func_name, bool always_fails);

// Global platform capability values.
#define ION_PLATFORM_CAP(type, name) \
  type Get##name() const;            \
  void Set##name(type value);
#include "ion/gfx/tests/glplatformcaps.inc"

  const std::unique_ptr<ShadowState> shadow_state_;

  std::atomic<int32> call_count_;
  bool is_valid_;
};

}  // namespace testing
}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_TESTS_MOCKVISUAL_H_
