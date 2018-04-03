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

// These tests rely on trace streams, which are disabled in production builds.
#include "absl/memory/memory.h"
#if !ION_PRODUCTION

#include "ion/gfx/updatestatetable.h"

#include <algorithm>
#include <string>
#include <vector>

#include "ion/base/stringutils.h"
#include "ion/gfx/statetable.h"
#include "ion/gfx/tests/fakegraphicsmanager.h"
#include "ion/gfx/tests/traceverifier.h"
#include "ion/math/range.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfx {

using math::Point2i;
using math::Range1f;
using math::Range2i;
using math::Vector2i;
using testing::FakeGraphicsManager;
using testing::FakeGraphicsManagerPtr;
using testing::FakeGlContext;

// NOTE: The UpdateStateTable() function is tested via the public
// Renderer::UpdateStateFromOpenGL() function, so there is no test for it
// here.

class UpdateStateTableTest : public ::testing::Test {
 protected:
  void SetUp() override {
    st0_.Reset(new StateTable(kWidth, kHeight));
    st1_.Reset(new StateTable(kWidth, kHeight));
    gl_context_ = FakeGlContext::Create(kWidth, kHeight);
    portgfx::GlContext::MakeCurrent(gl_context_);
    gm_.Reset(new FakeGraphicsManager());

    trace_verifier_ = absl::make_unique<testing::TraceVerifier>(gm_.Get());
  }

  // Resets call counts and the trace verifier.
  void Reset() {
    FakeGraphicsManager::ResetCallCount();
    trace_verifier_->Reset();
  }

  // Resets the call count and invokes the ClearFromStateTable() call.
  void ResetAndClear() {
    Reset();
    ClearFromStateTable(*st1_, st0_.Get(), gm_.Get());
  }

  // Resets the call count and invokes the UpdateFromStateTable() call.
  void ResetAndUpdate() {
    Reset();
    UpdateFromStateTable(*st1_, st0_.Get(), gm_.Get());
  }

  // Resets the call count and invokes the UpdateSettingsInStateTable() call.
  void ResetAndUpdateSet() {
    Reset();
    UpdateSettingsInStateTable(st1_.Get(), gm_.Get());
  }

  // Tests a change to a single capability at a time.
  void TestOneCapability(const StateTable::Capability cap,
                         bool new_value, const std::string& gl_cap_string) {
    SCOPED_TRACE(gl_cap_string);
    const bool old_value = st1_->IsEnabled(cap);
    st1_->Enable(cap, new_value);
    ResetAndUpdate();
    EXPECT_EQ(1, FakeGraphicsManager::GetCallCount());
    EXPECT_TRUE(trace_verifier_->VerifyOneCall(
        std::string(new_value ? "Enable(" : "Disable(") + gl_cap_string + ")"));
    // Restore state for next test.
    st1_->Enable(cap, old_value);
  }

  // Tests a change to a single capability at a time.
  void TestOneSetCapability(const StateTable::Capability cap, bool new_value,
                            const std::string& gl_cap_string) {
    SCOPED_TRACE(gl_cap_string);
    const bool old_value = st1_->IsEnabled(cap);
    st1_->Enable(cap, new_value);
    ResetAndUpdateSet();
    EXPECT_EQ(1, FakeGraphicsManager::GetCallCount());
    EXPECT_TRUE(trace_verifier_->VerifyOneCall("IsEnabled(" + gl_cap_string));
    st1_->Enable(cap, old_value);
    st1_->Reset();
  }

  // Calls VerifySortedCalls() on the TraceVerifier after defining a
  // SCOPED_TRACE that helps pinpoint errors.
  void VerifySortedCalls(const std::string& where,
                         const std::vector<std::string>& strings) {
    SCOPED_TRACE(where);
    EXPECT_TRUE(trace_verifier_->VerifySortedCalls(strings));
  }

  // Returns the TraceVerifier for calling other verification functions.
  const testing::TraceVerifier& GetTraceVerifier() { return *trace_verifier_; }
  GraphicsManager* GetGraphicsManager() { return gm_.Get(); }

  StateTablePtr st0_;
  StateTablePtr st1_;

  FakeGraphicsManagerPtr gm_;

 private:
  static const int kWidth = 400;
  static const int kHeight = 300;

  portgfx::GlContextPtr gl_context_;
  std::unique_ptr<testing::TraceVerifier> trace_verifier_;
};

TEST_F(UpdateStateTableTest, UpdateFromStateTableNoOp) {
  // This should cause no calls to OpenGL.
  ResetAndUpdate();
  EXPECT_EQ(0, FakeGraphicsManager::GetCallCount());
}

TEST_F(UpdateStateTableTest, UpdateFromStateTableCapability) {
  // Change one capability at a time. Only the corresponding OpenGL call should
  // be made.
  TestOneCapability(StateTable::kBlend, true, "GL_BLEND");
  TestOneCapability(StateTable::kCullFace, true, "GL_CULL_FACE");
  TestOneCapability(StateTable::kDepthTest, true, "GL_DEPTH_TEST");
  TestOneCapability(StateTable::kDither, false, "GL_DITHER");
  TestOneCapability(StateTable::kMultisample, false, "GL_MULTISAMPLE");
  TestOneCapability(StateTable::kPolygonOffsetFill, true,
                    "GL_POLYGON_OFFSET_FILL");
  TestOneCapability(StateTable::kSampleAlphaToCoverage, true,
                    "GL_SAMPLE_ALPHA_TO_COVERAGE");
  TestOneCapability(StateTable::kSampleCoverage, true, "GL_SAMPLE_COVERAGE");
  TestOneCapability(StateTable::kScissorTest, true, "GL_SCISSOR_TEST");
  TestOneCapability(StateTable::kStencilTest, true, "GL_STENCIL_TEST");

  // Test multiple capability changes at once.
  st1_->Reset();
  st1_->Enable(StateTable::kStencilTest, true);
  st1_->Enable(StateTable::kDepthTest, true);
  st1_->Enable(StateTable::kScissorTest, true);
  ResetAndUpdate();
  EXPECT_EQ(3, FakeGraphicsManager::GetCallCount());
  std::vector<std::string> sorted_strings;
  sorted_strings.push_back("Enable(GL_DEPTH_TEST)");
  sorted_strings.push_back("Enable(GL_SCISSOR_TEST)");
  sorted_strings.push_back("Enable(GL_STENCIL_TEST)");
  VerifySortedCalls("Multiple capabilities", sorted_strings);
}

TEST_F(UpdateStateTableTest, UpdateMultisampleFromStateTableCapability) {
  const StateTable::Capability cap = StateTable::kMultisample;
  const std::string& gl_cap_string = "GL_MULTISAMPLE";

  EXPECT_TRUE(GetGraphicsManager()->IsValidStateTableCapability(
      StateTable::kMultisample));

  st1_->Enable(cap, false);
  ResetAndUpdate();
  EXPECT_EQ(1, FakeGraphicsManager::GetCallCount());
  EXPECT_TRUE(
      GetTraceVerifier().VerifyOneCall("Disable(" + gl_cap_string + ")"));

  // When multisample capability is not supported, we should not get any calls.
  GetGraphicsManager()->EnableFeature(
      GraphicsManager::kMultisampleCapability, false);
  EXPECT_FALSE(GetGraphicsManager()->IsValidStateTableCapability(
      StateTable::kMultisample));
  ResetAndUpdate();
  EXPECT_TRUE(GetTraceVerifier().VerifyNoCalls());

  Reset();
  UpdateSettingsInStateTable(st1_.Get(), gm_.Get());
  EXPECT_TRUE(GetTraceVerifier().VerifyNoCalls());

  // Re-enable multisample capability, should get call again.
  GetGraphicsManager()->EnableFeature(
      GraphicsManager::kMultisampleCapability, true);
  EXPECT_TRUE(GetGraphicsManager()->IsValidStateTableCapability(
      StateTable::kMultisample));
  ResetAndUpdate();
  EXPECT_EQ(1, FakeGraphicsManager::GetCallCount());
  EXPECT_TRUE(
      GetTraceVerifier().VerifyOneCall("Disable(" + gl_cap_string + ")"));

  Reset();
  UpdateSettingsInStateTable(st1_.Get(), gm_.Get());
  EXPECT_TRUE(
      GetTraceVerifier().VerifyOneCall("IsEnabled(" + gl_cap_string + ")"));
}

TEST_F(UpdateStateTableTest, InvalidStateTableCapDoesNotSuppressSubsequent) {
  const StateTable::Capability subsequent_cap = StateTable::kPolygonOffsetFill;
  const std::string& subsequent_gl_cap_string = "GL_POLYGON_OFFSET_FILL";

  // Make multisample an invalid state table capability.
  GetGraphicsManager()->EnableFeature(
      GraphicsManager::kMultisampleCapability, false);
  EXPECT_FALSE(GetGraphicsManager()->IsValidStateTableCapability(
      StateTable::kMultisample));

  // Enable the invalid multisample cap and the subsequent cap.
  st1_->Enable(StateTable::kMultisample, true);
  st1_->Enable(subsequent_cap, true);

  // Now update state. We should still get a call for GL_POLYGON_OFFSET_FILL.
  ResetAndUpdate();
  EXPECT_EQ(1, FakeGraphicsManager::GetCallCount());
  EXPECT_TRUE(GetTraceVerifier().VerifyOneCall("Enable(" +
                                               subsequent_gl_cap_string + ")"));
}

TEST_F(UpdateStateTableTest, UpdateFromStateTableCapabilityEnforced) {
  // Change one capability at a time. Only the corresponding OpenGL call should
  // be made.
  st1_->SetEnforceSettings(true);
  TestOneCapability(StateTable::kBlend, false, "GL_BLEND");
  st1_->ResetCapability(StateTable::kBlend);
  TestOneCapability(StateTable::kCullFace, false, "GL_CULL_FACE");
  st1_->ResetCapability(StateTable::kCullFace);
  TestOneCapability(StateTable::kDepthTest, false, "GL_DEPTH_TEST");
  st1_->ResetCapability(StateTable::kDepthTest);
  TestOneCapability(StateTable::kDither, true, "GL_DITHER");
  st1_->ResetCapability(StateTable::kDither);
  TestOneCapability(StateTable::kPolygonOffsetFill, false,
                    "GL_POLYGON_OFFSET_FILL");
  st1_->ResetCapability(StateTable::kPolygonOffsetFill);
  TestOneCapability(StateTable::kSampleAlphaToCoverage, false,
                    "GL_SAMPLE_ALPHA_TO_COVERAGE");
  st1_->ResetCapability(StateTable::kSampleAlphaToCoverage);
  TestOneCapability(StateTable::kSampleCoverage, false, "GL_SAMPLE_COVERAGE");
  st1_->ResetCapability(StateTable::kSampleCoverage);
  TestOneCapability(StateTable::kScissorTest, false, "GL_SCISSOR_TEST");
  st1_->ResetCapability(StateTable::kScissorTest);
  TestOneCapability(StateTable::kStencilTest, false, "GL_STENCIL_TEST");
  st1_->ResetCapability(StateTable::kStencilTest);

  // Test multiple capability changes at once.
  st1_->Enable(StateTable::kStencilTest, false);
  st1_->Enable(StateTable::kDepthTest, false);
  st1_->Enable(StateTable::kScissorTest, false);
  ResetAndUpdate();
  EXPECT_EQ(3, FakeGraphicsManager::GetCallCount());
  std::vector<std::string> sorted_strings;
  sorted_strings.push_back("Disable(GL_DEPTH_TEST)");
  sorted_strings.push_back("Disable(GL_SCISSOR_TEST)");
  sorted_strings.push_back("Disable(GL_STENCIL_TEST)");
  VerifySortedCalls("Multiple capabilities", sorted_strings);
}

TEST_F(UpdateStateTableTest, ClearFromStateTableValues) {
#define ION_TEST_VALUE(set_call, expected_string)                  \
  st1_->set_call;                                                  \
  ResetAndClear();                                                 \
  EXPECT_EQ(1, FakeGraphicsManager::GetCallCount());               \
  EXPECT_TRUE(GetTraceVerifier().VerifyOneCall(expected_string));  \
  st1_->Reset()

#define ION_TEST_CLEAR_VALUE(set_call, clear_call, expected_string1,  \
                             expected_string2, expected_string3)      \
  st1_->set_call;                                                     \
  st1_->clear_call;                                                   \
  ResetAndClear();                                                    \
  EXPECT_EQ(3, FakeGraphicsManager::GetCallCount());                  \
  {                                                                   \
    std::vector<std::string> calls;                                   \
    calls.push_back(expected_string1);                                \
    calls.push_back(expected_string2);                                \
    calls.push_back(expected_string3);                                \
    EXPECT_TRUE(GetTraceVerifier().VerifySortedCalls(calls));         \
  }                                                                   \
  st1_->Reset()

#define ION_TEST_VALUES(set_call, expected_string1, expected_string2)    \
  st1_->set_call;                                                        \
  ResetAndClear();                                                       \
  EXPECT_EQ(2, FakeGraphicsManager::GetCallCount());                     \
  EXPECT_TRUE(GetTraceVerifier().VerifyTwoCalls(                         \
  expected_string1, expected_string2));                                  \
  st1_->Reset()

  ION_TEST_VALUE(Enable(StateTable::kDither, false), "Disable(GL_DITHER)");
  EXPECT_FALSE(st0_->IsEnabled(StateTable::kDither));
  ION_TEST_VALUE(Enable(StateTable::kDither, true), "Enable(GL_DITHER)");
  ION_TEST_VALUE(Enable(StateTable::kScissorTest, true),
                 "Enable(GL_SCISSOR_TEST)");
  EXPECT_TRUE(st0_->IsEnabled(StateTable::kScissorTest));
  ION_TEST_VALUE(Enable(StateTable::kScissorTest, false),
                 "Disable(GL_SCISSOR_TEST)");
  EXPECT_FALSE(st0_->IsEnabled(StateTable::kScissorTest));
  ION_TEST_VALUES(SetClearColor(math::Vector4f(.6f, .7f, .8f, .9f)),
                  "Clear(GL_COLOR_BUFFER_BIT)",
                  "ClearColor(0.6, 0.7, 0.8, 0.9)");
  EXPECT_EQ(math::Vector4f(.6f, .7f, .8f, .9f), st0_->GetClearColor());
  ION_TEST_VALUES(SetClearDepthValue(0.8f),
                  "Clear(GL_DEPTH_BUFFER_BIT)",
                  "ClearDepthf(0.8)");
  EXPECT_EQ(0.8f, st0_->GetClearDepthValue());
  ION_TEST_VALUES(SetClearStencilValue(123456),
                  "Clear(GL_STENCIL_BUFFER_BIT)",
                  "ClearStencil(123456)");
  EXPECT_EQ(123456, st0_->GetClearStencilValue());
  ION_TEST_CLEAR_VALUE(SetClearColor(math::Vector4f(1.f, 2.f, 3.f, 4.f)),
                       SetColorWriteMasks(true, false, false, true),
                       "Clear(GL_COLOR_BUFFER_BIT)",
                       "ClearColor(1, 2, 3, 4)",
                       "ColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_TRUE)");
  EXPECT_TRUE(st0_->GetRedColorWriteMask());
  EXPECT_FALSE(st0_->GetGreenColorWriteMask());
  EXPECT_FALSE(st0_->GetBlueColorWriteMask());
  EXPECT_TRUE(st0_->GetAlphaColorWriteMask());
  ION_TEST_CLEAR_VALUE(SetClearDepthValue(0.5f), SetDepthWriteMask(false),
                       "Clear(GL_DEPTH_BUFFER_BIT)", "ClearDepthf(0.5)",
                       "DepthMask(GL_FALSE)");
  EXPECT_FALSE(st0_->GetDepthWriteMask());
  ION_TEST_VALUE(
      SetScissorBox(Range2i::BuildWithSize(Point2i(10, 20), Vector2i(30, 40))),
      "Scissor(10, 20, 30, 40)");
  EXPECT_EQ(Range2i::BuildWithSize(Point2i(10, 20), Vector2i(30, 40)),
            st0_->GetScissorBox());

  // Test multiple clears.
  st0_->ResetValue(StateTable::kClearDepthValue);
  st1_->SetClearDepthValue(0.5f);
  st1_->SetClearStencilValue(34529);
  ResetAndClear();
  EXPECT_EQ(3, FakeGraphicsManager::GetCallCount());
  std::vector<std::string> sorted_strings;
  sorted_strings.push_back(
      "Clear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)");
  sorted_strings.push_back("ClearDepthf(0.5)");
  sorted_strings.push_back("ClearStencil(34529)");
  GetTraceVerifier().VerifySortedCalls(sorted_strings);
  EXPECT_EQ(0.5f, st0_->GetClearDepthValue());
  EXPECT_EQ(34529, st0_->GetClearStencilValue());
  st1_->Reset();

  // Special case for stencil write masks, which require front and back calls.
  st1_->SetClearStencilValue(54321);
  st1_->SetStencilWriteMasks(0x13572468, 0xfeebbeef);
  ResetAndClear();
  EXPECT_EQ(4, FakeGraphicsManager::GetCallCount());
  sorted_strings.clear();
  sorted_strings.push_back("Clear(GL_STENCIL_BUFFER_BIT)");
  sorted_strings.push_back("ClearStencil(54321)");
  sorted_strings.push_back("StencilMaskSeparate(GL_BACK, 0xfeebbeef");
  sorted_strings.push_back("StencilMaskSeparate(GL_FRONT, 0x13572468");

  EXPECT_TRUE(GetTraceVerifier().VerifySortedCalls(sorted_strings));
  EXPECT_EQ(0x13572468u, st0_->GetFrontStencilWriteMask());
  EXPECT_EQ(0xfeebbeefu, st0_->GetBackStencilWriteMask());
  st1_->Reset();
#undef ION_TEST_CLEAR_VALUE
#undef ION_TEST_VALUE
#undef ION_TEST_VALUES
}

TEST_F(UpdateStateTableTest, UpdateFromStateTableValues) {
  // Change one value at a time. Only the corresponding OpenGL call should be
  // made.
#define ION_TEST_VALUE(set_call, expected_string)                 \
  st1_->set_call;                                                 \
  ResetAndUpdate();                                               \
  EXPECT_EQ(1, FakeGraphicsManager::GetCallCount());              \
  EXPECT_TRUE(GetTraceVerifier().VerifyOneCall(expected_string)); \
  st1_->Reset()

  ION_TEST_VALUE(SetBlendColor(math::Vector4f(0.f, 0.f, 0.f, .5f)),
                 "BlendColor(0, 0, 0, 0.5)");
  ION_TEST_VALUE(SetBlendColor(math::Vector4f(.2f, .3f, .4f, .5f)),
                 "BlendColor(0.2, 0.3, 0.4, 0.5)");
  ION_TEST_VALUE(
      SetBlendEquations(StateTable::kAdd, StateTable::kSubtract),
      "BlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_SUBTRACT)");
  ION_TEST_VALUE(
      SetBlendEquations(StateTable::kReverseSubtract, StateTable::kSubtract),
      "BlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_SUBTRACT)");
  ION_TEST_VALUE(
      SetBlendFunctions(StateTable::kDstColor, StateTable::kOne,
                        StateTable::kSrcAlpha, StateTable::kZero),
      "BlendFuncSeparate(GL_DST_COLOR, GL_ONE, GL_SRC_ALPHA, GL_ZERO)");
  ION_TEST_VALUE(
      SetBlendFunctions(StateTable::kOne, StateTable::kOne,
                        StateTable::kSrcAlpha, StateTable::kZero),
      "BlendFuncSeparate(GL_ONE, GL_ONE, GL_SRC_ALPHA, GL_ZERO)");
  ION_TEST_VALUE(
      SetBlendFunctions(StateTable::kOne, StateTable::kZero,
                        StateTable::kSrcAlpha, StateTable::kZero),
      "BlendFuncSeparate(GL_ONE, GL_ZERO, GL_SRC_ALPHA, GL_ZERO)");
  ION_TEST_VALUE(SetBlendFunctions(StateTable::kOne, StateTable::kZero,
                                   StateTable::kOne, StateTable::kOne),
                 "BlendFuncSeparate(GL_ONE, GL_ZERO, GL_ONE, GL_ONE)");
  ION_TEST_VALUE(SetColorWriteMasks(false, false, false, false),
                 "ColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE)");
  ION_TEST_VALUE(SetColorWriteMasks(true, false, false, false),
                 "ColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE)");
  ION_TEST_VALUE(SetColorWriteMasks(true, true, false, false),
                 "ColorMask(GL_TRUE, GL_TRUE, GL_FALSE, GL_FALSE)");
  ION_TEST_VALUE(SetColorWriteMasks(true, true, true, false),
                 "ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE)");
  ION_TEST_VALUE(SetColorWriteMasks(true, true, true, true),
                 "ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE)");
  ION_TEST_VALUE(SetCullFaceMode(StateTable::kCullFrontAndBack),
                 "CullFace(GL_FRONT_AND_BACK)");
  ION_TEST_VALUE(SetFrontFaceMode(StateTable::kClockwise), "FrontFace(GL_CW)");
  ION_TEST_VALUE(SetDepthFunction(StateTable::kDepthEqual),
                 "DepthFunc(GL_EQUAL)");
  ION_TEST_VALUE(SetDepthRange(Range1f(0.f, .4f)), "DepthRangef(0, 0.4)");
  ION_TEST_VALUE(SetDepthRange(Range1f(.2f, .5f)), "DepthRangef(0.2, 0.5)");
  ION_TEST_VALUE(SetHint(StateTable::kGenerateMipmapHint,
                         StateTable::kHintNicest),
                 "Hint(GL_GENERATE_MIPMAP_HINT, GL_NICEST)");
  ION_TEST_VALUE(SetLineWidth(.4f), "LineWidth(0.4)");
  ION_TEST_VALUE(SetPolygonOffset(.5f, .2f), "PolygonOffset(0.5, 0.2)");
  ION_TEST_VALUE(SetSampleCoverage(1.f, true), "SampleCoverage(1, GL_TRUE)");
  ION_TEST_VALUE(SetSampleCoverage(.6f, true), "SampleCoverage(0.6, GL_TRUE)");
  ION_TEST_VALUE(SetViewport(Range2i::BuildWithSize(Point2i(50, 60),
                                                    Vector2i(70, 80))),
                 "Viewport(50, 60, 70, 80)");

  //
  // Special cases for stencil functions and operations, which require front and
  // back calls.
  //

  st1_->SetStencilFunctions(StateTable::kStencilNotEqual, 42, 0xbabebabe,
                           StateTable::kStencilLess, 155, 0x87654321);
  ResetAndUpdate();
  EXPECT_EQ(2, FakeGraphicsManager::GetCallCount());
  EXPECT_TRUE(GetTraceVerifier().VerifyTwoCalls(
      "StencilFuncSeparate(GL_BACK, GL_LESS, 155, 0x87654321)",
      "StencilFuncSeparate(GL_FRONT, GL_NOTEQUAL, 42, 0xbabebabe)"));
  st1_->Reset();

  st1_->SetStencilFunctions(StateTable::kStencilAlways, 0, 0xfffffff0,
                            StateTable::kStencilAlways, 1, 0xfffffff1);
  ResetAndUpdate();
  EXPECT_EQ(2, FakeGraphicsManager::GetCallCount());
  EXPECT_TRUE(GetTraceVerifier().VerifyTwoCalls(
      "StencilFuncSeparate(GL_BACK, GL_ALWAYS, 1, 0xfffffff1)",
      "StencilFuncSeparate(GL_FRONT, GL_ALWAYS, 0, 0xfffffff0)"));
  st1_->Reset();

  st1_->SetStencilOperations(
      StateTable::kStencilInvert, StateTable::kStencilKeep,
      StateTable::kStencilDecrement, StateTable::kStencilKeep,
      StateTable::kStencilZero, StateTable::kStencilReplace);
  ResetAndUpdate();
  EXPECT_EQ(2, FakeGraphicsManager::GetCallCount());
  EXPECT_TRUE(GetTraceVerifier().VerifyTwoCalls(
      "StencilOpSeparate(GL_BACK, GL_KEEP, GL_ZERO, GL_REPLACE",
      "StencilOpSeparate(GL_FRONT, GL_INVERT, GL_KEEP, GL_DECR)"));
  st1_->Reset();

  st1_->SetStencilOperations(StateTable::kStencilKeep, StateTable::kStencilKeep,
                             StateTable::kStencilDecrement,
                             StateTable::kStencilKeep, StateTable::kStencilKeep,
                             StateTable::kStencilReplace);
  ResetAndUpdate();
  EXPECT_EQ(2, FakeGraphicsManager::GetCallCount());
  EXPECT_TRUE(GetTraceVerifier().VerifyTwoCalls(
      "StencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_REPLACE",
      "StencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_DECR)"));
  st1_->Reset();
#undef ION_TEST_VALUE
}

TEST_F(UpdateStateTableTest, UpdateFromStateTableValuesEnforced) {
// Change one value at a time. Only the corresponding OpenGL call should be
// made.
#define ION_TEST_VALUE(set_call, expected_string)                 \
  st1_->set_call;                                                 \
  ResetAndUpdate();                                               \
  EXPECT_EQ(1, FakeGraphicsManager::GetCallCount());              \
  EXPECT_TRUE(GetTraceVerifier().VerifyOneCall(expected_string)); \
  st0_->set_call;                                                 \
  ResetAndUpdate();                                               \
  EXPECT_EQ(0, FakeGraphicsManager::GetCallCount());              \
  EXPECT_TRUE(GetTraceVerifier().VerifyNoCalls());                \
  st1_->SetEnforceSettings(true);                                 \
  ResetAndUpdate();                                               \
  EXPECT_EQ(1, FakeGraphicsManager::GetCallCount());              \
  EXPECT_TRUE(GetTraceVerifier().VerifyOneCall(expected_string)); \
  st1_->Reset()

  ION_TEST_VALUE(SetBlendColor(math::Vector4f(0.f, 0.f, 0.f, .5f)),
                 "BlendColor(0, 0, 0, 0.5)");
  ION_TEST_VALUE(SetBlendColor(math::Vector4f(.2f, .3f, .4f, .5f)),
                 "BlendColor(0.2, 0.3, 0.4, 0.5)");
  ION_TEST_VALUE(SetBlendEquations(StateTable::kAdd, StateTable::kSubtract),
                 "BlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_SUBTRACT)");
  ION_TEST_VALUE(
      SetBlendEquations(StateTable::kReverseSubtract, StateTable::kSubtract),
      "BlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_SUBTRACT)");
  ION_TEST_VALUE(
      SetBlendFunctions(StateTable::kDstColor, StateTable::kOne,
                        StateTable::kSrcAlpha, StateTable::kZero),
      "BlendFuncSeparate(GL_DST_COLOR, GL_ONE, GL_SRC_ALPHA, GL_ZERO)");
  ION_TEST_VALUE(SetBlendFunctions(StateTable::kOne, StateTable::kOne,
                                   StateTable::kSrcAlpha, StateTable::kZero),
                 "BlendFuncSeparate(GL_ONE, GL_ONE, GL_SRC_ALPHA, GL_ZERO)");
  ION_TEST_VALUE(SetBlendFunctions(StateTable::kOne, StateTable::kZero,
                                   StateTable::kSrcAlpha, StateTable::kZero),
                 "BlendFuncSeparate(GL_ONE, GL_ZERO, GL_SRC_ALPHA, GL_ZERO)");
  ION_TEST_VALUE(SetBlendFunctions(StateTable::kOne, StateTable::kZero,
                                   StateTable::kOne, StateTable::kOne),
                 "BlendFuncSeparate(GL_ONE, GL_ZERO, GL_ONE, GL_ONE)");
  ION_TEST_VALUE(SetColorWriteMasks(false, false, false, false),
                 "ColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE)");
  ION_TEST_VALUE(SetColorWriteMasks(true, false, false, false),
                 "ColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE)");
  ION_TEST_VALUE(SetColorWriteMasks(true, true, false, false),
                 "ColorMask(GL_TRUE, GL_TRUE, GL_FALSE, GL_FALSE)");
  ION_TEST_VALUE(SetColorWriteMasks(true, true, true, false),
                 "ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE)");
  ION_TEST_VALUE(SetColorWriteMasks(true, true, true, true),
                 "ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE)");
  ION_TEST_VALUE(SetCullFaceMode(StateTable::kCullFrontAndBack),
                 "CullFace(GL_FRONT_AND_BACK)");
  ION_TEST_VALUE(SetFrontFaceMode(StateTable::kClockwise), "FrontFace(GL_CW)");
  ION_TEST_VALUE(SetDepthFunction(StateTable::kDepthEqual),
                 "DepthFunc(GL_EQUAL)");
  ION_TEST_VALUE(SetDepthRange(Range1f(0.f, .4f)), "DepthRangef(0, 0.4)");
  ION_TEST_VALUE(SetDepthRange(Range1f(.2f, .5f)), "DepthRangef(0.2, 0.5)");
  ION_TEST_VALUE(
      SetHint(StateTable::kGenerateMipmapHint, StateTable::kHintNicest),
      "Hint(GL_GENERATE_MIPMAP_HINT, GL_NICEST)");
  ION_TEST_VALUE(SetLineWidth(.4f), "LineWidth(0.4)");
  ION_TEST_VALUE(SetPolygonOffset(.5f, .2f), "PolygonOffset(0.5, 0.2)");
  ION_TEST_VALUE(SetSampleCoverage(1.f, true), "SampleCoverage(1, GL_TRUE)");
  ION_TEST_VALUE(SetSampleCoverage(.6f, true), "SampleCoverage(0.6, GL_TRUE)");
  ION_TEST_VALUE(
      SetViewport(Range2i::BuildWithSize(Point2i(50, 60), Vector2i(70, 80))),
      "Viewport(50, 60, 70, 80)");

  //
  // Special cases for stencil functions and operations, which require front and
  // back calls.
  //

  st1_->SetStencilFunctions(StateTable::kStencilNotEqual, 42, 0xbabebabe,
                            StateTable::kStencilLess, 155, 0x87654321);
  ResetAndUpdate();
  EXPECT_EQ(2, FakeGraphicsManager::GetCallCount());
  EXPECT_TRUE(GetTraceVerifier().VerifyTwoCalls(
      "StencilFuncSeparate(GL_BACK, GL_LESS, 155, 0x87654321)",
      "StencilFuncSeparate(GL_FRONT, GL_NOTEQUAL, 42, 0xbabebabe)"));
  st0_->SetStencilFunctions(StateTable::kStencilNotEqual, 42, 0xbabebabe,
                            StateTable::kStencilLess, 155, 0x87654321);
  ResetAndUpdate();
  EXPECT_EQ(0, FakeGraphicsManager::GetCallCount());
  EXPECT_TRUE(GetTraceVerifier().VerifyNoCalls());
  st1_->SetEnforceSettings(true);
  ResetAndUpdate();
  EXPECT_EQ(2, FakeGraphicsManager::GetCallCount());
  EXPECT_TRUE(GetTraceVerifier().VerifyTwoCalls(
      "StencilFuncSeparate(GL_BACK, GL_LESS, 155, 0x87654321)",
      "StencilFuncSeparate(GL_FRONT, GL_NOTEQUAL, 42, 0xbabebabe)"));
  st1_->Reset();

  st1_->SetStencilFunctions(StateTable::kStencilAlways, 0, 0xfffffff0,
                            StateTable::kStencilAlways, 1, 0xfffffff1);
  ResetAndUpdate();
  EXPECT_EQ(2, FakeGraphicsManager::GetCallCount());
  EXPECT_TRUE(GetTraceVerifier().VerifyTwoCalls(
      "StencilFuncSeparate(GL_BACK, GL_ALWAYS, 1, 0xfffffff1)",
      "StencilFuncSeparate(GL_FRONT, GL_ALWAYS, 0, 0xfffffff0)"));
  st0_->SetStencilFunctions(StateTable::kStencilAlways, 0, 0xfffffff0,
                            StateTable::kStencilAlways, 1, 0xfffffff1);
  ResetAndUpdate();
  EXPECT_EQ(0, FakeGraphicsManager::GetCallCount());
  GetTraceVerifier().VerifyNoCalls();
  st1_->SetEnforceSettings(true);
  ResetAndUpdate();
  EXPECT_EQ(2, FakeGraphicsManager::GetCallCount());
  EXPECT_TRUE(GetTraceVerifier().VerifyTwoCalls(
      "StencilFuncSeparate(GL_BACK, GL_ALWAYS, 1, 0xfffffff1)",
      "StencilFuncSeparate(GL_FRONT, GL_ALWAYS, 0, 0xfffffff0)"));
  st1_->Reset();

  st1_->SetStencilOperations(
      StateTable::kStencilInvert, StateTable::kStencilKeep,
      StateTable::kStencilDecrement, StateTable::kStencilKeep,
      StateTable::kStencilZero, StateTable::kStencilReplace);
  ResetAndUpdate();
  EXPECT_EQ(2, FakeGraphicsManager::GetCallCount());
  EXPECT_TRUE(GetTraceVerifier().VerifyTwoCalls(
      "StencilOpSeparate(GL_BACK, GL_KEEP, GL_ZERO, GL_REPLACE",
      "StencilOpSeparate(GL_FRONT, GL_INVERT, GL_KEEP, GL_DECR)"));
  st0_->SetStencilOperations(
      StateTable::kStencilInvert, StateTable::kStencilKeep,
      StateTable::kStencilDecrement, StateTable::kStencilKeep,
      StateTable::kStencilZero, StateTable::kStencilReplace);
  ResetAndUpdate();
  EXPECT_EQ(0, FakeGraphicsManager::GetCallCount());
  EXPECT_TRUE(GetTraceVerifier().VerifyNoCalls());
  st1_->SetEnforceSettings(true);
  ResetAndUpdate();
  EXPECT_EQ(2, FakeGraphicsManager::GetCallCount());
  GetTraceVerifier().VerifyTwoCalls(
      "StencilOpSeparate(GL_BACK, GL_KEEP, GL_ZERO, GL_REPLACE",
      "StencilOpSeparate(GL_FRONT, GL_INVERT, GL_KEEP, GL_DECR)");
  st1_->Reset();

  st1_->SetStencilOperations(StateTable::kStencilKeep, StateTable::kStencilKeep,
                             StateTable::kStencilDecrement,
                             StateTable::kStencilKeep, StateTable::kStencilKeep,
                             StateTable::kStencilReplace);
  ResetAndUpdate();
  EXPECT_EQ(2, FakeGraphicsManager::GetCallCount());
  EXPECT_TRUE(GetTraceVerifier().VerifyTwoCalls(
      "StencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_REPLACE",
      "StencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_DECR)"));
  st0_->SetStencilOperations(StateTable::kStencilKeep, StateTable::kStencilKeep,
                             StateTable::kStencilDecrement,
                             StateTable::kStencilKeep, StateTable::kStencilKeep,
                             StateTable::kStencilReplace);
  ResetAndUpdate();
  EXPECT_EQ(0, FakeGraphicsManager::GetCallCount());
  EXPECT_TRUE(GetTraceVerifier().VerifyNoCalls());
  st1_->SetEnforceSettings(true);
  ResetAndUpdate();
  EXPECT_EQ(2, FakeGraphicsManager::GetCallCount());
  EXPECT_TRUE(GetTraceVerifier().VerifyTwoCalls(
      "StencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_REPLACE",
      "StencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_DECR)"));
  st1_->Reset();
#undef ION_TEST_VALUE
}

TEST_F(UpdateStateTableTest, UpdateSettingsInStateTable) {
  // Change one value at a time. Only the corresponding OpenGL call should be
  // made.
#define ION_TEST_VALUE(set_call, expected_string)           \
  st1_->set_call;                                           \
  ResetAndUpdateSet();                                      \
  calls = base::SplitString(expected_string, ";");          \
  EXPECT_EQ(static_cast<int>(calls.size()),                 \
            FakeGraphicsManager::GetCallCount());           \
  EXPECT_TRUE(GetTraceVerifier().VerifySomeCalls(calls));   \
  st1_->Reset()

  std::vector<std::string> calls;

  TestOneSetCapability(StateTable::kBlend, true, "GL_BLEND");
  TestOneSetCapability(StateTable::kCullFace, true, "GL_CULL_FACE");
  TestOneSetCapability(StateTable::kDepthTest, true, "GL_DEPTH_TEST");
  TestOneSetCapability(StateTable::kDither, false, "GL_DITHER");
  TestOneSetCapability(StateTable::kPolygonOffsetFill, true,
                       "GL_POLYGON_OFFSET_FILL");
  TestOneSetCapability(StateTable::kSampleAlphaToCoverage, true,
                       "GL_SAMPLE_ALPHA_TO_COVERAGE");
  TestOneSetCapability(StateTable::kSampleCoverage, true, "GL_SAMPLE_COVERAGE");
  TestOneSetCapability(StateTable::kScissorTest, true, "GL_SCISSOR_TEST");
  TestOneSetCapability(StateTable::kStencilTest, true, "GL_STENCIL_TEST");

  ION_TEST_VALUE(SetBlendColor(math::Vector4f(0.f, 0.f, 0.f, .5f)),
                 "GetFloatv(GL_BLEND_COLOR");
  ION_TEST_VALUE(SetBlendColor(math::Vector4f(.2f, .3f, .4f, .5f)),
                 "GetFloatv(GL_BLEND_COLOR");
  ION_TEST_VALUE(
      SetBlendEquations(StateTable::kAdd, StateTable::kSubtract),
      "GetIntegerv(GL_BLEND_EQUATION_ALPHA;GetIntegerv(GL_BLEND_EQUATION_RGB");
  ION_TEST_VALUE(
      SetBlendEquations(StateTable::kReverseSubtract, StateTable::kSubtract),
      "GetIntegerv(GL_BLEND_EQUATION_ALPHA;GetIntegerv(GL_BLEND_EQUATION_RGB");
  ION_TEST_VALUE(SetBlendFunctions(StateTable::kDstColor, StateTable::kOne,
                                   StateTable::kSrcAlpha, StateTable::kZero),
                 "GetIntegerv(GL_BLEND_SRC_RGB;GetIntegerv(GL_BLEND_DST_RGB;"
                 "GetIntegerv(GL_BLEND_SRC_ALPHA;GetIntegerv(GL_BLEND_DST_"
                 "ALPHA;");
  ION_TEST_VALUE(SetBlendFunctions(StateTable::kOne, StateTable::kOne,
                                   StateTable::kSrcAlpha, StateTable::kZero),
                 "GetIntegerv(GL_BLEND_SRC_RGB;GetIntegerv(GL_BLEND_DST_RGB;"
                 "GetIntegerv(GL_BLEND_SRC_ALPHA;GetIntegerv(GL_BLEND_DST_"
                 "ALPHA;");
  ION_TEST_VALUE(SetBlendFunctions(StateTable::kOne, StateTable::kZero,
                                   StateTable::kSrcAlpha, StateTable::kZero),
                 "GetIntegerv(GL_BLEND_SRC_RGB;GetIntegerv(GL_BLEND_DST_RGB;"
                 "GetIntegerv(GL_BLEND_SRC_ALPHA;GetIntegerv(GL_BLEND_DST_"
                 "ALPHA;");
  ION_TEST_VALUE(SetBlendFunctions(StateTable::kOne, StateTable::kZero,
                                   StateTable::kOne, StateTable::kOne),
                 "GetIntegerv(GL_BLEND_SRC_RGB;GetIntegerv(GL_BLEND_DST_RGB;"
                 "GetIntegerv(GL_BLEND_SRC_ALPHA;GetIntegerv(GL_BLEND_DST_"
                 "ALPHA;");
  ION_TEST_VALUE(SetColorWriteMasks(false, false, false, false),
                 "GetIntegerv(GL_COLOR_WRITEMASK");
  ION_TEST_VALUE(SetColorWriteMasks(true, false, false, false),
                 "GetIntegerv(GL_COLOR_WRITEMASK");
  ION_TEST_VALUE(SetColorWriteMasks(true, true, false, false),
                 "GetIntegerv(GL_COLOR_WRITEMASK");
  ION_TEST_VALUE(SetDepthWriteMask(true), "GetIntegerv(GL_DEPTH_WRITEMASK");
  ION_TEST_VALUE(SetStencilWriteMasks(true, false),
                 "GetIntegerv(GL_STENCIL_WRITEMASK;GetIntegerv(GL_STENCIL_BACK_"
                 "WRITEMASK;");
  ION_TEST_VALUE(SetCullFaceMode(StateTable::kCullFrontAndBack),
                 "GetIntegerv(GL_CULL_FACE_MODE");
  ION_TEST_VALUE(SetFrontFaceMode(StateTable::kClockwise),
                 "GetIntegerv(GL_FRONT_FACE");
  ION_TEST_VALUE(SetDepthFunction(StateTable::kDepthEqual),
                 "GetIntegerv(GL_DEPTH_FUNC");
  ION_TEST_VALUE(SetDepthRange(Range1f(0.f, .4f)), "GetFloatv(GL_DEPTH_RANGE");
  ION_TEST_VALUE(SetDepthRange(Range1f(.2f, .5f)), "GetFloatv(GL_DEPTH_RANGE");
  ION_TEST_VALUE(SetLineWidth(.4f), "GetFloatv(GL_LINE_WIDTH");
  ION_TEST_VALUE(
      SetPolygonOffset(.5f, .2f),
      "GetFloatv(GL_POLYGON_OFFSET_FACTOR;GetFloatv(GL_POLYGON_OFFSET_UNITS");
  ION_TEST_VALUE(SetSampleCoverage(1.f, true),
                 "GetFloatv(GL_SAMPLE_COVERAGE_VALUE;GetIntegerv(GL_SAMPLE_"
                 "COVERAGE_INVERT;");
  ION_TEST_VALUE(SetSampleCoverage(.6f, true),
                 "GetFloatv(GL_SAMPLE_COVERAGE_VALUE;GetIntegerv(GL_SAMPLE_"
                 "COVERAGE_INVERT;");
  ION_TEST_VALUE(
      SetViewport(Range2i::BuildWithSize(Point2i(50, 60), Vector2i(70, 80))),
      "GetIntegerv(GL_VIEWPORT");

  ION_TEST_VALUE(SetClearColor(math::Vector4f(.6f, .7f, .8f, .9f)),
                 "GetFloatv(GL_COLOR_CLEAR_VALUE");
  ION_TEST_VALUE(SetClearDepthValue(0.8f), "GetFloatv(GL_DEPTH_CLEAR_VALUE");
  ION_TEST_VALUE(SetClearStencilValue(123456),
                 "GetIntegerv(GL_STENCIL_CLEAR_VALUE,");
  ION_TEST_VALUE(
      SetScissorBox(Range2i::BuildWithSize(Point2i(10, 20), Vector2i(30, 40))),
      "GetIntegerv(GL_SCISSOR_BOX");
  ION_TEST_VALUE(
      SetStencilFunctions(StateTable::kStencilNotEqual, 42, 0xbabebabe,
                          StateTable::kStencilLess, 155, 0x87654321),
      "GetIntegerv(GL_STENCIL_FUNC;GetIntegerv(GL_STENCIL_REF;GetIntegerv(GL_"
      "STENCIL_VALUE_MASK;GetIntegerv(GL_STENCIL_BACK_FUNC;GetIntegerv(GL_"
      "STENCIL_BACK_REF;GetIntegerv(GL_STENCIL_BACK_VALUE_MASK");

  ION_TEST_VALUE(SetStencilOperations(
                     StateTable::kStencilInvert, StateTable::kStencilKeep,
                     StateTable::kStencilDecrement, StateTable::kStencilKeep,
                     StateTable::kStencilZero, StateTable::kStencilReplace),
                 "GetIntegerv(GL_STENCIL_FAIL;GetIntegerv(GL_STENCIL_PASS_"
                 "DEPTH_FAIL;GetIntegerv(GL_STENCIL_PASS_DEPTH_PASS;"
                 "GetIntegerv(GL_STENCIL_BACK_FAIL;GetIntegerv(GL_STENCIL_BACK_"
                 "PASS_DEPTH_FAIL;GetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_PASS");
#undef ION_TEST_VALUE
}

TEST_F(UpdateStateTableTest, IgnoreDefaults) {
  st0_->Enable(StateTable::kBlend, true);
  st0_->Enable(StateTable::kCullFace, true);
  st0_->SetDepthRange(Range1f(.1f, .2f));  // Default in st1; should not appear.
  st0_->SetViewport(
      Range2i::BuildWithSize(Point2i(40, 60), Vector2i(100, 200)));

  // Changing to a default StateTable should not cause any calls to OpenGL.
  st1_.Reset(new StateTable);
  ResetAndUpdate();
  EXPECT_TRUE(GetTraceVerifier().VerifyNoCalls());
}

TEST_F(UpdateStateTableTest, MultipleChanges) {
  // Change a bunch of things in the StateTable instances and verify that the
  // new differences are sent correctly.
  std::vector<std::string> sorted_strings;

  // These will have no effect.
  st1_->SetClearDepthValue(1.f);
  st1_->SetClearColor(math::Vector4f(0.f, 1.f, 2.f, 3.f));

  // These capabilities should appear as differences (when set on st1).
  st0_->Enable(StateTable::kCullFace, true);
  // st1 has the default setting for the blend flag, so this should not result
  // in a call to OpenGL.
  st0_->Enable(StateTable::kBlend, true);
  st1_->Enable(StateTable::kScissorTest, true);
  st1_->Enable(StateTable::kCullFace, false);
  sorted_strings.push_back("Disable(GL_CULL_FACE)");
  sorted_strings.push_back("Enable(GL_SCISSOR_TEST)");

  // These capabilities should not appear as differences even though they are
  // set in both tables.
  st0_->Enable(StateTable::kPolygonOffsetFill, true);
  st1_->Enable(StateTable::kPolygonOffsetFill, true);
  st0_->Enable(StateTable::kDither, false);
  st1_->Enable(StateTable::kDither, false);

  // st1 has the default setting for the depth range, so this should not result
  // in a call to OpenGL.
  st0_->SetDepthRange(Range1f(.1f, .2f));

  // These values should not appear as differences.
  st0_->SetFrontFaceMode(StateTable::kClockwise);
  st1_->SetFrontFaceMode(StateTable::kClockwise);
  st0_->SetPolygonOffset(.5f, .4f);
  st1_->SetPolygonOffset(.5f, .4f);

  ResetAndUpdate();
  std::sort(sorted_strings.begin(), sorted_strings.end());
  VerifySortedCalls("Multiple changes", sorted_strings);
}

TEST_F(UpdateStateTableTest, Restore) {
  // This test simulates the behavior of parent and child nodes, both of which
  // have StateTables. As is the case in many scenes, the StateTable
  // representing the child does not know the proper window sizes, so it uses
  // (0,0).
  st1_.Reset(new StateTable);

  std::vector<std::string> sorted_strings;

  st0_->Enable(StateTable::kCullFace, true);
  st0_->Enable(StateTable::kDepthTest, true);
  st1_->Enable(StateTable::kPolygonOffsetFill, true);
  st1_->Enable(StateTable::kBlend, true);
  sorted_strings.push_back("Enable(GL_BLEND)");
  sorted_strings.push_back("Enable(GL_POLYGON_OFFSET_FILL)");
  // These are set only in the parent, so they shouldn't result in any calls.
  st0_->SetDepthFunction(StateTable::kDepthEqual);
  st0_->SetViewport(Range2i::BuildWithSize(Point2i(10, 20), Vector2i(30, 40)));

  // Do a standard update.
  ResetAndUpdate();
  VerifySortedCalls("Update", sorted_strings);
}

}  // namespace gfx
}  // namespace ion

#endif  // ION_PRODUCTION
