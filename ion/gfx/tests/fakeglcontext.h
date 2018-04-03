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

#ifndef ION_GFX_TESTS_FAKEGLCONTEXT_H_
#define ION_GFX_TESTS_FAKEGLCONTEXT_H_

#include <memory>

// Include graphics manager to get special gl typedefs needed for compatibility
// with FakeGraphicsManager's expectations.
#include "ion/base/sharedptr.h"
#include "ion/gfx/graphicsmanager.h"
#include "ion/port/atomic.h"
#include "ion/portgfx/glcontext.h"

namespace ion {
namespace gfx {
namespace testing {

// FakeGlContext is a replacement for GlContext which supports the use of
// FakeGraphicsManager in the same way GlContext supports the use of
// GraphicsManager.
class FakeGlContext : public portgfx::GlContext {
 public:
  // Shadows OpenGL state.
  class ShadowState;

  // Constructs a FakeGlContext that shares non-container OpenGL resources with
  // |share_context| (i.e. all resources except framebuffers, vertex arrays,
  // program pipelines, and transform feedbacks).  Note the following important
  // points:
  //   - Operations on FakeGlContext are not thread-safe.
  //   - The FakeGlContext is not set as current; that should be done on the
  //     thread it will be used on.
  //   - Both the original and new FakeGlContext will respond to IsValid() with
  //     the same result (unless one is later invalidated).
  static base::SharedPtr<FakeGlContext> CreateShared(
      const FakeGlContext& share_context);

  // Constructs a fake context.
  static base::SharedPtr<FakeGlContext> Create(int window_width,
                                            int window_height);
  ~FakeGlContext() override;

  // Context implementation.
  bool IsValid() const override { return is_valid_; }
  void* GetProcAddress(const char* proc_name, uint32_t flags) const override;
  void SwapBuffers() override;
  bool MakeContextCurrentImpl() override;
  void ClearCurrentContextImpl() override;
  portgfx::GlContextPtr CreateGlContextInShareGroupImpl(
      const portgfx::GlContextSpec&) override {
    // The returned FakeGlContext will be valid iff |*this| is valid.
    return CreateShared(*this);
  }
  bool IsOwned() const override { return true; }
  bool IsCurrentGlContext() const override;
  void MaybeCreateStamp() override {}
  bool CheckStamp() const override { return true; }

  // Sets the value that will subsequently be returned by IsValid().  This
  // allows testing that code work with both valid and invalid FakeGlContexts.
  void SetValid(bool valid) { is_valid_ = valid; }

  // Gets the current context, as a FakeGlContext.
  static base::SharedPtr<FakeGlContext> GetCurrent();

  // Gets the number of times an OpenGL function has been invoked on the
  // currently active FakeGlContext, since the last reset.
  static int64 GetCallCount() { return GetCurrent()->call_count_; }

  // Resets the call count of the currently active FakeGlContext to zero.
  static void ResetCallCount() { GetCurrent()->call_count_ = 0; }

  static ShadowState* IncrementAndCall(const char* name);

  // Sets the extensions string of the GL context to the passed string.
  void SetExtensionsString(const std::string& extensions);
  // Sets the vendor string of the GL context to the passed string for testing.
  void SetVendorString(const std::string& vendor);
  // Sets the renderer string of the GL context to the passed string.
  void SetRendererString(const std::string& renderer);
  // Sets the version string of the GL context to the passed string for testing.
  void SetVersionString(const std::string& version);
  // Sets the context profile mask of the GL context to the passed mask.
  void SetContextProfileMask(int mask);
  // Sets the context flags of the GL context to the passed value for testing.
  void SetContextFlags(int flags);

 private:
  friend class FakeGraphicsManager;

  FakeGlContext(std::unique_ptr<ShadowState> shadow_state, bool is_valid);

  // Sets/returns a maximum size allowed for allocating any OpenGL buffer.
  // This is used primarily for testing out-of-memory errors.
  void SetMaxBufferSize(GLsizeiptr size_in_bytes);
  GLsizeiptr GetMaxBufferSize() const;

  // Gets/sets the current OpenGL error code for testing.
  GLenum GetErrorCode() const;
  void SetErrorCode(GLenum error_code);

  // If always_fails is set to true, forces future calls of the referenced
  // function to fail with an invalid operation error. Calling with
  // always_fails false re-enables the function. This is useful for testing
  // that rendering code is robust to GL library failures or partial
  // implementations.
  void SetForceFunctionFailure(const std::string& func_name, bool always_fails);

  // Sets whether to allow callers to set an invalid enum state, for example
  // setting the depth function to an invalid value. This is useful for testing
  // code that handles technically invalid returns from drivers.
  void EnableInvalidGlEnumState(bool enable);

  // GL implementation-defined constants. FakeGlContext allows setting these
  // limits to arbitrary values in tests.
#define ION_WRAP_GL_VALUE(name, sname, gl_enum, Type, init) \
  Type Get##name() const;                                   \
  void Set##name(Type value);
#include "ion/gfx/glconstants.inc"

  const std::unique_ptr<ShadowState> shadow_state_;

  std::atomic<int32> call_count_;
  bool is_valid_;
};

}  // namespace testing
}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_TESTS_FAKEGLCONTEXT_H_
