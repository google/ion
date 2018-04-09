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
#if !ION_PRODUCTION

#include "ion/gfx/renderer.h"
#include "ion/gfx/tests/renderer_common.h"
#include "ion/gfxutils/shapeutils.h"

namespace ion {
namespace gfx {

using math::Point2i;
using math::Range1i;
using math::Range1ui;
using math::Range2i;
using math::Vector2i;
using portgfx::GlContextPtr;
using testing::FakeGraphicsManager;
using testing::FakeGraphicsManagerPtr;
using testing::FakeGlContext;

static const char* kInstancedVertexShaderString = (
    "#extension GL_EXT_draw_instanced : enable\n"
    "uniform mat4 uProjectionMatrix;\n"
    "uniform mat4 uModelviewMatrix;\n"
    "attribute vec3 aVertex;\n"
    "attribute vec2 aTexCoords;\n"
    "varying vec2 vTexCoords;\n"
    "\n"
    "void main(void) {\n"
    "  vTexCoords = aTexCoords;\n"
    "  vec3 offset = vec3(15.0 * gl_InstanceID, 15.0 * gl_InstanceID, 0);\n"
    "  gl_Position = uProjectionMatrix * uModelviewMatrix *\n"
    "      vec4(aVertex + offset, 1.);\n"
    "}\n");

TEST_F(RendererTest, GetGraphicsManager) {
  RendererPtr renderer(new Renderer(gm_));
  EXPECT_EQ(static_cast<GraphicsManagerPtr>(gm_),
            renderer->GetGraphicsManager());
}

TEST_F(RendererTest, GetDefaultShaderProgram) {
  RendererPtr renderer(new Renderer(gm_));
  EXPECT_TRUE(renderer->GetDefaultShaderProgram().Get() != nullptr);
}

TEST_F(RendererTest, UpdateDefaultFramebufferFromOpenGL) {
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);

  FramebufferObjectPtr fbo(new FramebufferObject(128, 128));
  fbo->SetColorAttachment(0U, FramebufferObject::Attachment(Image::kRgba4Byte));

  GLuint system_fb, bound_fb;
  // Get the system framebuffer.
  gm_->GetIntegerv(GL_FRAMEBUFFER_BINDING,
                   reinterpret_cast<GLint*>(&system_fb));
  renderer->BindFramebuffer(fbo);
  // Binding the framebuffer should make it active.
  gm_->GetIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint*>(&bound_fb));
  EXPECT_NE(system_fb, bound_fb);
  renderer->DrawScene(root);

  // Unbinding the framebuffer should go back to the system default.
  renderer->BindFramebuffer(FramebufferObjectPtr());
  gm_->GetIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint*>(&bound_fb));
  EXPECT_EQ(system_fb, bound_fb);

  // Create a framebuffer outside of Ion.
  GLuint fb;
  gm_->GenFramebuffers(1, &fb);
  EXPECT_GT(fb, 0U);
  gm_->BindFramebuffer(GL_FRAMEBUFFER, fb);
  gm_->GetIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint*>(&bound_fb));
  EXPECT_EQ(fb, bound_fb);

  // Since we haven't updated the default binding it will be blown away.
  renderer->BindFramebuffer(fbo);
  renderer->DrawScene(root);
  gm_->GetIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint*>(&bound_fb));
  EXPECT_NE(system_fb, bound_fb);
  renderer->BindFramebuffer(FramebufferObjectPtr());
  gm_->GetIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint*>(&bound_fb));
  // The original framebuffer should be restored.
  EXPECT_EQ(system_fb, bound_fb);

  // Bind the non-Ion fbo.
  gm_->BindFramebuffer(GL_FRAMEBUFFER, fb);
  gm_->GetIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint*>(&bound_fb));
  EXPECT_EQ(fb, bound_fb);
  // Tell the renderer to update its binding.
  renderer->ClearCachedBindings();
  renderer->UpdateDefaultFramebufferFromOpenGL();
  // Binding the Ion fbo will change the binding, but it should be restored
  // later.
  renderer->BindFramebuffer(fbo);
  gm_->GetIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint*>(&bound_fb));
  EXPECT_NE(fb, bound_fb);
  EXPECT_NE(system_fb, bound_fb);
  renderer->DrawScene(root);
  gm_->GetIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint*>(&bound_fb));
  // The Ion fbo should still be bound.
  EXPECT_NE(fb, bound_fb);
  EXPECT_NE(system_fb, bound_fb);
  renderer->BindFramebuffer(FramebufferObjectPtr());
  gm_->GetIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint*>(&bound_fb));
  // The renderer should have restored the new framebuffer.
  EXPECT_EQ(fb, bound_fb);
}

TEST_F(RendererTest, UpdateStateFromOpenGL) {
  RendererPtr renderer(new Renderer(gm_));

  // Verify the default StateTable matches the default OpenGL state.
  renderer->UpdateStateFromOpenGL(kWidth, kHeight);
  {
    const StateTable& st = renderer->GetStateTable();
    EXPECT_EQ(0U, st.GetSetCapabilityCount());
    EXPECT_EQ(0U, st.GetSetValueCount());
  }

  // Modify the mock OpenGL state and try again.
  gm_->Enable(GL_SCISSOR_TEST);
  gm_->Enable(GL_STENCIL_TEST);
  gm_->DepthFunc(GL_GREATER);
  gm_->Viewport(2, 10, 120, 432);
  renderer->UpdateStateFromOpenGL(kWidth, kHeight);
  {
    const StateTable& st = renderer->GetStateTable();
    EXPECT_EQ(2U, st.GetSetCapabilityCount());
    EXPECT_TRUE(st.IsCapabilitySet(StateTable::kScissorTest));
    EXPECT_TRUE(st.IsCapabilitySet(StateTable::kStencilTest));
    EXPECT_TRUE(st.IsEnabled(StateTable::kScissorTest));
    EXPECT_TRUE(st.IsEnabled(StateTable::kStencilTest));
    EXPECT_EQ(2U, st.GetSetValueCount());
    EXPECT_EQ(StateTable::kDepthGreater, st.GetDepthFunction());
    EXPECT_EQ(math::Range2i(math::Point2i(2, 10), math::Point2i(122, 442)),
              st.GetViewport());
  }

  // Modify some more OpenGL state and try again.
  gm_->Enable(GL_BLEND);
  gm_->FrontFace(GL_CW);
  renderer->UpdateStateFromOpenGL(kWidth, kHeight);
  {
    const StateTable& st = renderer->GetStateTable();
    EXPECT_EQ(3U, st.GetSetCapabilityCount());
    EXPECT_TRUE(st.IsCapabilitySet(StateTable::kBlend));
    EXPECT_TRUE(st.IsCapabilitySet(StateTable::kScissorTest));
    EXPECT_TRUE(st.IsCapabilitySet(StateTable::kStencilTest));
    EXPECT_TRUE(st.IsEnabled(StateTable::kScissorTest));
    EXPECT_TRUE(st.IsEnabled(StateTable::kStencilTest));
    EXPECT_EQ(3U, st.GetSetValueCount());
    EXPECT_EQ(StateTable::kDepthGreater, st.GetDepthFunction());
    EXPECT_EQ(StateTable::kClockwise, st.GetFrontFaceMode());
    EXPECT_EQ(math::Range2i(math::Point2i(2, 10), math::Point2i(122, 442)),
              st.GetViewport());
  }

  // Modify all of the state for a full test.
  gm_->Enable(GL_CULL_FACE);
  gm_->Enable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  gm_->Enable(GL_DEPTH_TEST);
  gm_->Disable(GL_DITHER);
  gm_->Enable(GL_POLYGON_OFFSET_FILL);
  gm_->Enable(GL_RASTERIZER_DISCARD);
  gm_->Enable(GL_SAMPLE_ALPHA_TO_COVERAGE);
  gm_->Enable(GL_SAMPLE_COVERAGE);
  gm_->Enable(GL_SCISSOR_TEST);
  gm_->Enable(GL_STENCIL_TEST);
  gm_->BlendColor(.2f, .3f, .4f, .5f);
  gm_->BlendEquationSeparate(GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT);
  gm_->BlendFuncSeparate(GL_ONE_MINUS_CONSTANT_COLOR, GL_DST_COLOR,
                        GL_ONE_MINUS_CONSTANT_ALPHA, GL_DST_ALPHA);
  gm_->ClearColor(.5f, .6f, .7f, .8f);
  gm_->ClearDepthf(0.5f);
  gm_->ColorMask(true, false, true, false);
  gm_->CullFace(GL_FRONT_AND_BACK);
  gm_->DepthFunc(GL_GEQUAL);
  gm_->DepthRangef(0.2f, 0.7f);
  gm_->DepthMask(false);
  gm_->FrontFace(GL_CW);
  gm_->Hint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);
  gm_->LineWidth(0.4f);
  gm_->PolygonOffset(0.4f, 0.2f);
  gm_->SampleCoverage(0.5f, true);
  gm_->Scissor(4, 10, 123, 234);
  gm_->StencilFuncSeparate(GL_FRONT, GL_LEQUAL, 100, 0xbeefbeefU);
  gm_->StencilFuncSeparate(GL_BACK, GL_GREATER, 200, 0xfacefaceU);
  gm_->StencilMaskSeparate(GL_FRONT, 0xdeadfaceU);
  gm_->StencilMaskSeparate(GL_BACK, 0xcacabeadU);
  gm_->StencilOpSeparate(GL_FRONT, GL_REPLACE, GL_INCR, GL_INVERT);
  gm_->StencilOpSeparate(GL_BACK, GL_INCR_WRAP, GL_DECR_WRAP, GL_ZERO);
  gm_->ClearStencil(123);
  gm_->Viewport(16, 49, 220, 317);
  renderer->UpdateStateFromOpenGL(kWidth, kHeight);
  {
    const StateTable& st = renderer->GetStateTable();
    EXPECT_EQ(11U, st.GetSetCapabilityCount());
    EXPECT_TRUE(st.IsEnabled(StateTable::kBlend));
    EXPECT_TRUE(st.IsEnabled(StateTable::kCullFace));
    EXPECT_TRUE(st.IsEnabled(StateTable::kDebugOutputSynchronous));
    EXPECT_TRUE(st.IsEnabled(StateTable::kDepthTest));
    EXPECT_FALSE(st.IsEnabled(StateTable::kDither));
    EXPECT_TRUE(st.IsEnabled(StateTable::kPolygonOffsetFill));
    EXPECT_TRUE(st.IsEnabled(StateTable::kRasterizerDiscard));
    EXPECT_TRUE(st.IsEnabled(StateTable::kSampleAlphaToCoverage));
    EXPECT_TRUE(st.IsEnabled(StateTable::kSampleCoverage));
    EXPECT_TRUE(st.IsEnabled(StateTable::kScissorTest));
    EXPECT_TRUE(st.IsEnabled(StateTable::kStencilTest));
    EXPECT_EQ(math::Vector4f(.2f, .3f, .4f, .5f), st.GetBlendColor());
    EXPECT_EQ(StateTable::kSubtract, st.GetRgbBlendEquation());
    EXPECT_EQ(StateTable::kReverseSubtract, st.GetAlphaBlendEquation());
    EXPECT_EQ(StateTable::kOneMinusConstantColor,
              st.GetRgbBlendFunctionSourceFactor());
    EXPECT_EQ(StateTable::kDstColor, st.GetRgbBlendFunctionDestinationFactor());
    EXPECT_EQ(StateTable::kOneMinusConstantAlpha,
              st.GetAlphaBlendFunctionSourceFactor());
    EXPECT_EQ(StateTable::kDstAlpha,
              st.GetAlphaBlendFunctionDestinationFactor());
    EXPECT_EQ(math::Vector4f(.5f, .6f, .7f, .8f), st.GetClearColor());
    EXPECT_EQ(0.5f, st.GetClearDepthValue());
    EXPECT_TRUE(st.GetRedColorWriteMask());
    EXPECT_FALSE(st.GetGreenColorWriteMask());
    EXPECT_TRUE(st.GetBlueColorWriteMask());
    EXPECT_FALSE(st.GetAlphaColorWriteMask());
    EXPECT_EQ(StateTable::kCullFrontAndBack, st.GetCullFaceMode());
    EXPECT_EQ(StateTable::kDepthGreaterOrEqual, st.GetDepthFunction());
    EXPECT_EQ(math::Range1f(0.2f, 0.7f), st.GetDepthRange());
    EXPECT_FALSE(st.GetDepthWriteMask());
    EXPECT_EQ(StateTable::kClockwise, st.GetFrontFaceMode());
    EXPECT_EQ(StateTable::kHintNicest,
              st.GetHint(StateTable::kGenerateMipmapHint));
    EXPECT_EQ(0.4f, st.GetLineWidth());
    EXPECT_EQ(0.4f, st.GetPolygonOffsetFactor());
    EXPECT_EQ(0.2f, st.GetPolygonOffsetUnits());
    EXPECT_EQ(0.5f, st.GetSampleCoverageValue());
    EXPECT_TRUE(st.IsSampleCoverageInverted());
    EXPECT_EQ(math::Range2i(math::Point2i(4, 10), math::Point2i(127, 244)),
              st.GetScissorBox());
    EXPECT_EQ(StateTable::kStencilLessOrEqual, st.GetFrontStencilFunction());
    EXPECT_EQ(100, st.GetFrontStencilReferenceValue());
    EXPECT_EQ(0xbeefbeefU, st.GetFrontStencilMask());
    EXPECT_EQ(StateTable::kStencilGreater, st.GetBackStencilFunction());
    EXPECT_EQ(200, st.GetBackStencilReferenceValue());
    EXPECT_EQ(0xfacefaceU, st.GetBackStencilMask());
    EXPECT_EQ(0xdeadfaceU, st.GetFrontStencilWriteMask());
    EXPECT_EQ(0xcacabeadU, st.GetBackStencilWriteMask());

    EXPECT_EQ(StateTable::kStencilReplace, st.GetFrontStencilFailOperation());
    EXPECT_EQ(StateTable::kStencilIncrement,
              st.GetFrontStencilDepthFailOperation());
    EXPECT_EQ(StateTable::kStencilInvert, st.GetFrontStencilPassOperation());
    EXPECT_EQ(StateTable::kStencilIncrementAndWrap,
              st.GetBackStencilFailOperation());
    EXPECT_EQ(StateTable::kStencilDecrementAndWrap,
              st.GetBackStencilDepthFailOperation());
    EXPECT_EQ(StateTable::kStencilZero, st.GetBackStencilPassOperation());
    EXPECT_EQ(123, st.GetClearStencilValue());
    EXPECT_EQ(math::Range2i(math::Point2i(16, 49), math::Point2i(236, 366)),
              st.GetViewport());
  }
}

TEST_F(RendererTest, UpdateInvalidStateFromOpenGL) {
  base::LogChecker log_checker;
  RendererPtr renderer(new Renderer(gm_));

  // Set some valid and invalid GL state.
  gm_->EnableInvalidGlEnumState(true);
  gm_->Enable(GL_SCISSOR_TEST);
  gm_->Enable(GL_STENCIL_TEST);
  // This is invalid.
  gm_->DepthFunc(-1);
  gm_->Viewport(2, 10, 120, 432);
  renderer->UpdateStateFromOpenGL(kWidth, kHeight);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "GL returned an invalid value"));
  {
    const StateTable& st = renderer->GetStateTable();
    EXPECT_EQ(2U, st.GetSetCapabilityCount());
    EXPECT_TRUE(st.IsCapabilitySet(StateTable::kScissorTest));
    EXPECT_TRUE(st.IsCapabilitySet(StateTable::kStencilTest));
    EXPECT_TRUE(st.IsEnabled(StateTable::kScissorTest));
    EXPECT_TRUE(st.IsEnabled(StateTable::kStencilTest));
    EXPECT_EQ(2U, st.GetSetValueCount());
    EXPECT_EQ(StateTable::kDepthAlways, st.GetDepthFunction());
    EXPECT_EQ(math::Range2i(math::Point2i(2, 10), math::Point2i(122, 442)),
              st.GetViewport());
  }
  gm_->EnableInvalidGlEnumState(false);
  gm_->DepthFunc(GL_GEQUAL);
}

TEST_F(RendererTest, UpdateFromStateTable) {
  RendererPtr renderer(new Renderer(gm_));

  // Verify the default StateTable matches the default OpenGL state.
  const StateTable& st = renderer->GetStateTable();
  renderer->UpdateStateFromOpenGL(kWidth, kHeight);
  {
    EXPECT_EQ(0U, st.GetSetCapabilityCount());
    EXPECT_EQ(0U, st.GetSetValueCount());
  }
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  renderer->DrawScene(root);
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Viewport"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ClearColor"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ClearDepth"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_DEPTH_TEST"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_CULL_FACE"));

  // Create a StateTable with differing values from current state.
  StateTablePtr state_table(new StateTable(kWidth / 2, kHeight / 2));
  state_table->SetViewport(math::Range2i(
      math::Point2i(2, 2), math::Point2i(kWidth / 2, kHeight / 2)));
  state_table->SetClearColor(math::Vector4f(0.31f, 0.25f, 0.55f, 0.5f));
  state_table->SetClearDepthValue(0.5f);
  state_table->Enable(StateTable::kDepthTest, false);
  state_table->Enable(StateTable::kCullFace, true);
  // This is already set.
  state_table->Enable(StateTable::kScissorTest, false);

  renderer->UpdateStateFromStateTable(state_table);
  EXPECT_EQ(state_table->GetViewport(), st.GetViewport());
  EXPECT_EQ(state_table->GetClearColor(), st.GetClearColor());
  EXPECT_EQ(state_table->GetClearDepthValue(), st.GetClearDepthValue());
  EXPECT_EQ(state_table->IsEnabled(StateTable::kDepthTest),
            st.IsEnabled(StateTable::kDepthTest));
  EXPECT_EQ(state_table->IsEnabled(StateTable::kCullFace),
            st.IsEnabled(StateTable::kCullFace));

  // The next draw should trigger some additional state changes to invert the
  // changes.
  renderer->DrawScene(root);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Viewport"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearColor"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearDepth"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_DEPTH_TEST"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Disable(GL_CULL_FACE"));
  // Since the renderer thinks scissor was already disabled, nothing happens
  // here.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Disable(GL_SCISSOR"));
}

TEST_F(RendererTest, ProcessStateTable) {
  RendererPtr renderer(new Renderer(gm_));

  // Ensure the default StateTable is up to date.
  renderer->UpdateStateFromOpenGL(kWidth, kHeight);

  // Create a StateTable with a few values set.
  StateTablePtr state_table(new StateTable(kWidth / 2, kHeight / 2));
  state_table->SetViewport(math::Range2i(
      math::Point2i(2, 2), math::Point2i(kWidth / 2, kHeight / 2)));
  state_table->SetClearColor(math::Vector4f(0.31f, 0.25f, 0.55f, 0.5f));
  state_table->SetClearDepthValue(0.5f);
  state_table->Enable(StateTable::kBlend, true);
  state_table->Enable(StateTable::kStencilTest, true);
  // This is already set.
  state_table->Enable(StateTable::kScissorTest, false);

  Reset();
  renderer->ProcessStateTable(state_table);
  EXPECT_EQ(6U, trace_verifier_->GetCallCount());
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Viewport"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Clear("));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearColor"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearDepth"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_BLEND"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_STENCIL_TEST"));
  // Since the renderer thinks scissor was already disabled, nothing happens
  // here.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Disable(GL_SCISSOR"));

  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  renderer->DrawScene(root);
  Reset();
  // Check that the settings undone after the Node was processed are not made,
  // such as depth test.
  renderer->ProcessStateTable(state_table);
  EXPECT_EQ(4U, trace_verifier_->GetCallCount());
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Viewport"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Clear("));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearColor"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearDepth"));
  // These two were already set.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_BLEND"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_STENCIL_TEST"));
  // This is set in the client state table, but should not be processed.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Disable(GL_DEPTH_TEST"));

  state_table->ResetValue(StateTable::kClearColorValue);
  state_table->ResetValue(StateTable::kClearDepthValue);
  // Change the state of a few things and verify that only they change.
  state_table->Enable(StateTable::kBlend, false);
  state_table->Enable(StateTable::kScissorTest, true);
  Reset();
  renderer->ProcessStateTable(state_table);
  EXPECT_TRUE(trace_verifier_->VerifyTwoCalls("Disable(GL_BLEND",
                                              "Enable(GL_SCISSOR"));

  state_table->SetBlendColor(math::Vector4f(1.f, 2.f, 3.f, 4.f));
  state_table->SetCullFaceMode(StateTable::kCullFront);
  Reset();
  renderer->ProcessStateTable(state_table);
  EXPECT_EQ(2U, trace_verifier_->GetCallCount());
  EXPECT_TRUE(trace_verifier_->VerifyTwoCalls("BlendColor(1, 2, 3, 4)",
                                              "CullFace(GL_FRONT"));

  // Test setting enforcement.
  Reset();
  state_table->SetEnforceSettings(true);
  renderer->ProcessStateTable(state_table);
  EXPECT_EQ(7U, trace_verifier_->GetCallCount());
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Viewport"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Clear("));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ClearColor"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ClearDepth"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Disable(GL_BLEND"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_STENCIL_TEST"));
  // Since the renderer thinks scissor was already disabled, nothing happens
  // here.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Disable(GL_SCISSOR"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BlendColor(1, 2, 3, 4)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("CullFace(GL_FRONT"));
}

TEST_F(RendererTest, DestroyStateCache) {
  // Doing something that requires internal resource access will trigger some
  // gets.
  {
    Renderer::DestroyCurrentStateCache();
    RendererPtr renderer(new Renderer(gm_));
    renderer->BindFramebuffer(FramebufferObjectPtr());
    // This time a binder will get created.
    // This will trigger calls to get binding limits.
    EXPECT_EQ(3U, trace_verifier_->GetCallCount());
    EXPECT_EQ(3U, FakeGraphicsManager::GetCallCount());
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(
        "GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS"));
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 0U, 0U, 0U));
    Reset();
  }
  // Doing the same thing again results in no calls since the calls are
  // associated with the current GL context.
  {
    RendererPtr renderer(new Renderer(gm_));
    renderer->BindFramebuffer(FramebufferObjectPtr());
    EXPECT_EQ(0U, trace_verifier_->GetCallCount());
    EXPECT_EQ(0U, FakeGraphicsManager::GetCallCount());
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 0U, 0U, 0U));
    Reset();
  }
  // Destroying the cached state will trigger recreation.
  {
    // Destroying twice has no ill effects.
    Renderer::DestroyStateCache(portgfx::GlContext::GetCurrent());
    Renderer::DestroyStateCache(portgfx::GlContext::GetCurrent());
    RendererPtr renderer(new Renderer(gm_));
    renderer->BindFramebuffer(FramebufferObjectPtr());
    EXPECT_EQ(1U, trace_verifier_->GetCallCount());
    EXPECT_EQ(1U, FakeGraphicsManager::GetCallCount());
    EXPECT_EQ(0U, trace_verifier_->GetCountOf(
        "GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf(
        "GetIntegerv(GL_FRAMEBUFFER_BINDING"));
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 0U, 0U, 0U));
    Reset();
  }
  // We get the same effect if we clear the current state cache.
  {
    // Destroying twice has no ill effects.
    Renderer::DestroyCurrentStateCache();
    Renderer::DestroyStateCache(portgfx::GlContext::GetCurrent());
    RendererPtr renderer(new Renderer(gm_));
    renderer->BindFramebuffer(FramebufferObjectPtr());
    EXPECT_EQ(1U, trace_verifier_->GetCallCount());
    EXPECT_EQ(1U, FakeGraphicsManager::GetCallCount());
    EXPECT_EQ(0U, trace_verifier_->GetCountOf(
        "GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS"));
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 0U, 0U, 0U));
    Reset();
  }
}

TEST_F(RendererTest, NoScene) {
  // Nothing happens if there are no interactions with the renderer.
  {
    RendererPtr renderer(new Renderer(gm_));
    EXPECT_EQ(0U, trace_verifier_->GetCallCount());
    EXPECT_EQ(0U, FakeGraphicsManager::GetCallCount());
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 0U, 0U, 0U));
  }
  // Destroying a renderer normally requires an internal bind cache,
  // unless one has never been created, as is the case here.
  // So none of the calls made when creating a binder should be seen here.
  EXPECT_EQ(0U, trace_verifier_->GetCallCount());
  EXPECT_EQ(0U, FakeGraphicsManager::GetCallCount());
  EXPECT_EQ(0U, trace_verifier_->GetCountOf(
      "GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS"));
  Reset();

  // Doing something that requires internal resource access will trigger some
  // gets.
  {
    Renderer::DestroyCurrentStateCache();
    RendererPtr renderer(new Renderer(gm_));
    renderer->BindFramebuffer(FramebufferObjectPtr());
    // This time a binder will get created.
    EXPECT_EQ(3U, trace_verifier_->GetCallCount());
    EXPECT_EQ(3U, FakeGraphicsManager::GetCallCount());
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(
        "GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS"));
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 0U, 0U, 0U));
    Reset();
  }
  // Doing the same thing again results in no calls since the calls are
  // associated with the current GL context.
  {
    RendererPtr renderer(new Renderer(gm_));
    renderer->BindFramebuffer(FramebufferObjectPtr());
    EXPECT_EQ(0U, trace_verifier_->GetCallCount());
    EXPECT_EQ(0U, FakeGraphicsManager::GetCallCount());
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 0U, 0U, 0U));
    Reset();
  }
  // Destroying the cached state will trigger recreation.
  {
    Renderer::DestroyStateCache(portgfx::GlContext::GetCurrent());
    RendererPtr renderer(new Renderer(gm_));
    renderer->BindFramebuffer(FramebufferObjectPtr());
    EXPECT_EQ(1U, trace_verifier_->GetCallCount());
    EXPECT_EQ(1U, FakeGraphicsManager::GetCallCount());
    EXPECT_EQ(0U, trace_verifier_->GetCountOf(
        "GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS"));
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 0U, 0U, 0U));
    Reset();
  }

  // There should be no calls when the renderer is destroyed.
  EXPECT_TRUE(trace_verifier_->VerifyNoCalls());
  EXPECT_EQ(0, FakeGraphicsManager::GetCallCount());

  // Try to render using a NULL node.
  {
    // Also change to fake desktop OpenGL to test that path.
    gm_->SetVersionString("Ion fake OpenGL");
    Reset();
    Renderer::DestroyStateCache(portgfx::GlContext::GetCurrent());
    RendererPtr renderer(new Renderer(gm_));
    renderer->DrawScene(NodePtr(nullptr));
    EXPECT_EQ(3U, trace_verifier_->GetCallCount());
    EXPECT_EQ(3U, FakeGraphicsManager::GetCallCount());
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_POINT_SPRITE"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_PROGRAM_POINT_SIZE"));
    renderer->DrawScene(NodePtr(nullptr));
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 0U, 0U, 0U));
    Reset();
  }
  EXPECT_TRUE(trace_verifier_->VerifyNoCalls());
  EXPECT_EQ(0, FakeGraphicsManager::GetCallCount());
}

TEST_F(RendererTest, BasicGraph) {
  {
    std::vector<std::string> call_strings;
    RendererPtr renderer(new Renderer(gm_));
    // Draw the simplest possible scene.
    NodePtr root(new Node);
    renderer->DrawScene(root);
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 0U, 0U, 0U));

    EXPECT_EQ(2U, trace_verifier_->GetCallCount());
    EXPECT_EQ(2U, FakeGraphicsManager::GetCallCount());
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(
        "GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS"));
    Reset();
  }

  {
    base::LogChecker log_checker;
    RendererPtr renderer(new Renderer(gm_));
    // Have a state table.
    NodePtr root(new Node);
    StateTablePtr state_table(new StateTable(kWidth, kHeight));
    state_table->SetViewport(
        math::Range2i(math::Point2i(0, 0), math::Point2i(kWidth, kHeight)));
    state_table->SetClearColor(math::Vector4f(0.3f, 0.3f, 0.5f, 1.0f));
    state_table->SetClearDepthValue(0.f);
    state_table->Enable(StateTable::kDepthTest, true);
    state_table->Enable(StateTable::kCullFace, true);
    root->SetStateTable(state_table);
    Reset();
    renderer->DrawScene(root);
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 0U, 0U, 0U));
    EXPECT_FALSE(log_checker.HasAnyMessages());
    EXPECT_EQ(3U, FakeGraphicsManager::GetCallCount());
    // Only clearing should have occurred since no shapes are in the node.
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("Clear("));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearColor"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearDepth"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable"));
    Reset();

    // Add a shape to get state changes and shader creation.
    BuildRectangleShape<uint16>(data_, options_);
    root->AddShape(data_->shape);
    renderer->DrawScene(root);
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 0U, 0U, 0U));
    EXPECT_TRUE(log_checker.HasMessage("WARNING", "no value set for uniform"));
    EXPECT_GT(FakeGraphicsManager::GetCallCount(), 0);
    std::vector<std::string> call_strings;
    call_strings.push_back("Clear(");
    call_strings.push_back("CreateShader");
    call_strings.push_back("CompileShader");
    call_strings.push_back("ShaderSource");
    call_strings.push_back("GetShaderiv");
    call_strings.push_back("CreateProgram");
    call_strings.push_back("AttachShader");
    call_strings.push_back("LinkProgram");
    call_strings.push_back("GetProgramiv");
    call_strings.push_back("UseProgram");
    call_strings.push_back("Enable(GL_DEPTH_TEST)");
    call_strings.push_back("Enable(GL_CULL_FACE)");
    EXPECT_TRUE(trace_verifier_->VerifySomeCalls(call_strings));
    // The clear values have already been set.
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("ClearColor"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("ClearDepth"));
    Reset();

    // Used as the base for the enforced settings.
    renderer->DrawScene(root);
    EXPECT_EQ(3U, FakeGraphicsManager::GetCallCount());
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("Clear("));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("ClearColor"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("ClearDepth"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("Viewport"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable"));
    Reset();

    // Test setting enforcement.
    state_table->SetEnforceSettings(true);
    renderer->DrawScene(root);
    // 9 calls generated here. The 5 more calls are coming from 2 clear calls, 2
    // enable calls, and 1 viewport call.
    EXPECT_EQ(8U, FakeGraphicsManager::GetCallCount());
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("Clear("));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearColor"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearDepth"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("Viewport"));
    // Settings are enforced. As a result, the two "Enable" calls will be passed
    // to OpenGL.
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("Enable"));
  }
}

TEST_F(RendererTest, ZombieResourceBinderCache) {
  Renderer::DestroyCurrentStateCache();
  NodePtr root = BuildGraph(data_, options_, 800, 800);
  gm_->EnableFeature(GraphicsManager::kVertexArrays, false);
  base::LogChecker log_checker;
  {
    RendererPtr renderer(new Renderer(gm_));
    renderer->DrawScene(root);
    portgfx::GlContext::MakeCurrent(GlContextPtr());
  }
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "No GlContext ID"));
  // All renderer resources are now destroyed.
  {
    // Reuse the same context, which will crash when drawing if we have any old
    // resource pointers.
    portgfx::GlContext::MakeCurrent(gl_context_);
    RendererPtr renderer(new Renderer(gm_));
    renderer->DrawScene(root);
  }
  Renderer::DestroyCurrentStateCache();
  gm_->EnableFeature(GraphicsManager::kVertexArrays, true);
  {
    RendererPtr renderer(new Renderer(gm_));
    renderer->DrawScene(root);
    portgfx::GlContext::MakeCurrent(GlContextPtr());
  }
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "No GlContext ID"));
  // All renderer resources are now destroyed.
  {
    // Reuse the same context, which will crash when drawing if we have any old
    // resource pointers.
    portgfx::GlContext::MakeCurrent(gl_context_);
    RendererPtr renderer(new Renderer(gm_));
    renderer->DrawScene(root);
  }
}

TEST_F(RendererTest, VertexAttribDivisor) {
  RendererPtr renderer(new Renderer(gm_));

  static const char* kVertexShaderString =
      "attribute vec3 attribute1;\n"
      "attribute vec2 attribute2;\n";
  static const char* kFragmentShaderString = "void main() {}\n";

  BuildRectangleBufferObject(data_, options_);

  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  Attribute attribute1 = reg->Create<Attribute>(
      "attribute1", BufferObjectElement(data_->vertex_buffer,
                                        data_->vertex_buffer->AddSpec(
                                            BufferObject::kFloat, 3, 0)));
  Attribute attribute2 = reg->Create<Attribute>(
      "attribute2",
      BufferObjectElement(data_->vertex_buffer,
                          data_->vertex_buffer->AddSpec(BufferObject::kFloat, 2,
                                                        sizeof(float) * 3)));
  {
    NodePtr root(new Node);
    AttributeArrayPtr aa(new AttributeArray);
    // Set Divisor for attribute2
    attribute2.SetDivisor(1);
    aa->AddAttribute(attribute1);
    aa->AddAttribute(attribute2);
    ShapePtr shape(new Shape);
    shape->SetAttributeArray(aa);
    root->SetShaderProgram(ShaderProgram::BuildFromStrings(
        "Shader", reg, kVertexShaderString, kFragmentShaderString,
        base::AllocatorPtr()));
    root->GetShaderProgram()->SetLabel("root shader");
    root->AddShape(shape);
    renderer->DrawScene(root);
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("VertexAttribDivisor"));
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                  0U, "VertexAttribDivisor"))
                    .HasArg(2, "0x0"));
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                  1U, "VertexAttribDivisor"))
                    .HasArg(2, "0x1"));
    Reset();
  }

  gm_->EnableFeature(GraphicsManager::kInstancedArrays, false);
  {
    NodePtr root(new Node);
    AttributeArrayPtr aa(new AttributeArray);
    attribute1.SetDivisor(5);
    attribute2.SetDivisor(3);
    aa->AddAttribute(attribute1);
    aa->AddAttribute(attribute2);
    ShapePtr shape(new Shape);
    shape->SetAttributeArray(aa);
    root->SetShaderProgram(ShaderProgram::BuildFromStrings(
        "Shader", reg, kVertexShaderString, kFragmentShaderString,
        base::AllocatorPtr()));
    root->GetShaderProgram()->SetLabel("root shader");
    root->AddShape(shape);
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttribDivisor"));
  }
}

TEST_F(RendererTest, DrawElementsInstanced) {
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, 800, 800);

  {
    // DrawElements.
    renderer->DrawScene(root);
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("VertexAttribDivisor"));
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                  0U, "VertexAttribPointer"))
                    .HasArg(1, "0"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawElements("));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawArrays("));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawElementsInstanced"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawArraysInstanced"));
    Reset();

    // DrawElementsInstanced.
    root->GetChildren()[0]->GetShapes()[0]->SetInstanceCount(8);
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttribDivisor"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawElements("));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawArrays("));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawElementsInstanced("));
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                  0U, "DrawElementsInstanced"))
                    .HasArg(5, "8"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawArraysInstanced"));
    Reset();

    // vertex range testing for instanced drawing.
    // DrawElements.
    root->GetChildren()[0]->GetShapes()[0]->AddVertexRange(Range1i(1, 3));
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttribDivisor"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawElements("));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawArrays("));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawElementsInstanced"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawArraysInstanced"));
    Reset();

    // DrawElementsInstanced.
    root->GetChildren()[0]->GetShapes()[0]->SetVertexRangeInstanceCount(0, 5);
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttribDivisor"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawElements("));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawArrays("));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawElementsInstanced"));
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                  0U, "DrawElementsInstanced"))
                    .HasArg(5, "5"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawArraysInstanced"));
    Reset();
  }

  // Vertex range based drawing with kDrawInstanced disabled.
  // This will result in 1 call for the DrawElements and a warning message
  // stating that instanced drawing functions are not available.
  gm_->EnableFeature(GraphicsManager::kDrawInstanced, false);
  {
    base::LogChecker log_checker;
    // DrawElements.
    renderer->DrawScene(root);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawElements("));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawArrays("));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawElementsInstanced"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawArraysInstanced"));
    EXPECT_TRUE(log_checker.HasMessage(
        "WARNING", "ION: Instanced drawing is not available."));
  }
}

TEST_F(RendererTest, DrawArraysInstanced) {
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, 800, 800, false, false);

  {
    // DrawArrays
    renderer->DrawScene(root);
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("VertexAttribDivisor"));
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                  0U, "VertexAttribPointer"))
                    .HasArg(1, "0"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawElements("));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawArrays("));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawElementsInstanced"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawArraysInstanced"));
    Reset();

    // DrawArraysInstanced
    root->GetChildren()[0]->GetShapes()[0]->SetInstanceCount(8);
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttribDivisor"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawElements("));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawArrays("));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawElementsInstanced"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawArraysInstanced"));
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                  0U, "DrawArraysInstanced"))
                    .HasArg(4, "8"));
    Reset();

    // vertex range testing for instanced drawing.
    root->GetChildren()[0]->GetShapes()[0]->AddVertexRange(Range1i(1, 3));
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttribDivisor"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawElements("));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawArrays("));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawElementsInstanced"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawArraysInstanced"));
    Reset();

    // DrawArraysInstanced.
    root->GetChildren()[0]->GetShapes()[0]->SetVertexRangeInstanceCount(0, 5);
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttribDivisor"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawElements("));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawArrays("));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawElementsInstanced"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawArraysInstanced"));
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                  0U, "DrawArraysInstanced"))
                    .HasArg(4, "5"));
    Reset();
  }

  // Vertex range based drawing with kDrawInstanced disabled.
  // This will result in 1 call for the DrawArrays and a warning message stating
  // that instanced drawing functions are not available.
  gm_->EnableFeature(GraphicsManager::kDrawInstanced, false);
  {
    base::LogChecker log_checker;
    // DrawElements.
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawElements("));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawArrays("));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawElementsInstanced"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawArraysInstanced"));
    EXPECT_TRUE(log_checker.HasMessage(
        "WARNING", "ION: Instanced drawing is not available."));
  }
}

TEST_F(RendererTest, InstancedShaderDoesNotGenerateWarnings) {
  base::LogChecker log_checker;

  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, 800, 800, true, false,
                            kInstancedVertexShaderString,
                            kPlaneGeometryShaderString,
                            kPlaneFragmentShaderString);
  renderer->DrawScene(root);

  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST_F(RendererTest, GpuMemoryUsage) {
  {
    RendererPtr renderer(new Renderer(gm_));
    NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
    EXPECT_EQ(0U, data_->index_buffer->GetGpuMemoryUsed());
    EXPECT_EQ(0U, data_->vertex_buffer->GetGpuMemoryUsed());
    EXPECT_EQ(0U, data_->texture->GetGpuMemoryUsed());
    EXPECT_EQ(0U, data_->cubemap->GetGpuMemoryUsed());
    renderer->DrawScene(root);
    // There are 12 bytes in the index buffer, and 4 * sizeof(Vertex) in vertex
    // buffer. There are 7 32x32 RGBA texture images (one regular texture, one
    // cubemap).
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U + kVboSize, 0U, 28672U));
    EXPECT_EQ(12U, data_->index_buffer->GetGpuMemoryUsed());
    EXPECT_EQ(kVboSize, data_->vertex_buffer->GetGpuMemoryUsed());
    EXPECT_EQ(4096U, data_->texture->GetGpuMemoryUsed());
    EXPECT_EQ(24576U, data_->cubemap->GetGpuMemoryUsed());
  }
  {
    RendererPtr renderer(new Renderer(gm_));
    NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
    ShaderInputRegistryPtr reg(new ShaderInputRegistry);
    reg->IncludeGlobalRegistry();
    data_->attribute_array = new AttributeArray;
    const Attribute a = reg->Create<Attribute>("aTestAttrib", 2.f);
    EXPECT_TRUE(a.IsValid());
    data_->attribute_array->AddAttribute(a);
    data_->shader = ShaderProgram::BuildFromStrings(
        "Plane shader", reg, kPlaneVertexShaderString,
        kPlaneGeometryShaderString, kPlaneFragmentShaderString,
        base::AllocatorPtr());
    data_->shape->SetAttributeArray(data_->attribute_array);
    data_->rect->SetShaderProgram(data_->shader);
    data_->rect->ClearUniforms();
    AddPlaneShaderUniformsToNode(data_, data_->rect);
    Reset();
    renderer->DrawScene(root);
    // There are 12 bytes in the index buffer. Since there are no buffer
    // attributes, the vertex buffer never uploads its data. There are 7 32x32
    // RGBA texture images (one regular texture, one cubemap).
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U, 0U, 28672U));
    EXPECT_EQ(12U, data_->index_buffer->GetGpuMemoryUsed());
    EXPECT_EQ(0U, data_->vertex_buffer->GetGpuMemoryUsed());
    EXPECT_EQ(4096U, data_->texture->GetGpuMemoryUsed());
    EXPECT_EQ(4096U * 6U, data_->cubemap->GetGpuMemoryUsed());
  }
}

TEST_F(RendererTest, BufferAttributeTypes) {
  RendererPtr renderer(new Renderer(gm_));
  TracingHelper helper;

  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  reg->IncludeGlobalRegistry();
  reg->Add(ShaderInputRegistry::UniformSpec(
      "uTexture", kTextureUniform, "Plane texture"));
  reg->Add(ShaderInputRegistry::UniformSpec(
      "uTexture2", kTextureUniform, "Plane texture"));
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "aTestAttrib", kBufferObjectElementAttribute, "Testing attribute"));

  // Create a spec for each type.
  std::vector<SpecInfo> spec_infos;
  spec_infos.push_back(SpecInfo(data_->vertex_buffer->AddSpec(
      BufferObject::kByte, 1, 0), "GL_BYTE"));
  spec_infos.push_back(SpecInfo(data_->vertex_buffer->AddSpec(
      BufferObject::kUnsignedByte, 1, 1), "GL_UNSIGNED_BYTE"));
  spec_infos.push_back(SpecInfo(data_->vertex_buffer->AddSpec(
      BufferObject::kShort, 1, 2), "GL_SHORT"));
  // The unsigned short attribute should be kept 4-byte aligned in order to test
  // the single unsigned short attribute warning below.
  spec_infos.push_back(SpecInfo(data_->vertex_buffer->AddSpec(
      BufferObject::kUnsignedShort, 1, 4), "GL_UNSIGNED_SHORT"));
  spec_infos.push_back(SpecInfo(data_->vertex_buffer->AddSpec(
      BufferObject::kInt, 1, 6), "GL_INT"));
  spec_infos.push_back(SpecInfo(data_->vertex_buffer->AddSpec(
      BufferObject::kUnsignedInt, 1, 10), "GL_UNSIGNED_INT"));
  spec_infos.push_back(SpecInfo(data_->vertex_buffer->AddSpec(
      BufferObject::kFloat, 1, 14), "GL_FLOAT"));
  spec_infos.push_back(SpecInfo(data_->vertex_buffer->AddSpec(
      BufferObject::kInvalid, 1, 18), "GL_INVALID_ENUM"));

  data_->attribute_array = new AttributeArray;
  data_->shape->SetAttributeArray(data_->attribute_array);
  data_->shader = ShaderProgram::BuildFromStrings(
      "Plane shader", reg, kPlaneVertexShaderString,
      kPlaneFragmentShaderString, base::AllocatorPtr());
  data_->rect->SetShaderProgram(data_->shader);
  data_->rect->ClearUniforms();
  AddPlaneShaderUniformsToNode(data_, data_->rect);

  bool found_alignment_warning = false;
  bool found_single_ushort_warning = false;
  const size_t count = spec_infos.size();
  for (size_t i = 0; i < count; ++i) {
    base::LogChecker log_checker;
    SCOPED_TRACE(::testing::Message() << "Iteration " << i);
    const Attribute a = data_->shader->GetRegistry()->Create<Attribute>(
        "aTestAttrib", BufferObjectElement(data_->vertex_buffer,
                                           spec_infos[i].index));
    EXPECT_TRUE(a.IsValid());
    if (data_->attribute_array->GetAttributeCount())
      EXPECT_TRUE(data_->attribute_array->ReplaceAttribute(0, a));
    else
      data_->attribute_array->AddAttribute(a);
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("VertexAttribPointer"));
    const BufferObject::Spec& spec =
        data_->vertex_buffer->GetSpec(spec_infos[i].index);
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(0U, "VertexAttribPointer"))
            .HasArg(3, spec_infos[i].type)
            .HasArg(5, helper.ToString(
                "GLint", static_cast<int>(
                    data_->vertex_buffer->GetStructSize())))
            .HasArg(6, helper.ToString(
                "const void*", reinterpret_cast<const void*>(
                    spec.byte_offset))));
    bool has_log_message = false;
    if ((spec.byte_offset % 4 || data_->vertex_buffer->GetStructSize() % 4) &&
        !found_alignment_warning) {
      EXPECT_TRUE(log_checker.HasMessage("WARNING", "aligned"));
      found_alignment_warning = true;
      has_log_message = true;
    }
    if (spec.type == BufferObject::kUnsignedShort &&
        spec.component_count == 1 && !found_single_ushort_warning) {
      EXPECT_TRUE(log_checker.HasMessage("WARNING", "single unsigned short"));
      found_single_ushort_warning = true;
      has_log_message = true;
    }
    if (!has_log_message) {
      EXPECT_FALSE(log_checker.HasAnyMessages());
    }
  }

  // Reset data.
  data_->rect = nullptr;
  BuildRectangle(data_, options_);

  // Some of the specs are technically invalid, but are tested for coverage.
  gm_->SetErrorCode(GL_NO_ERROR);
}

TEST_F(RendererTest, PreventZombieUpdates) {
  RendererPtr renderer(new Renderer(gm_));
  TracingHelper helper;
  base::LogChecker log_checker;

  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  Reset();
  // Create resources.
  renderer->DrawScene(root);
  Reset();
  // Force resource destruction.
  data_->vertex_buffer = nullptr;
  data_->rect = nullptr;
  data_->attribute_array = nullptr;
  data_->shape = nullptr;
  root = nullptr;
  // Clearing cached bindings causes the active buffer to be put on the update
  // list.
  renderer->ClearCachedBindings();
  // Drawing should just destroy resources, and should _not_ try to update the
  // buffer. If it does then this will crash when the Renderer processes the
  // update list.
  renderer->DrawScene(root);

  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST_F(RendererTest, EnableDisableBufferAttributes) {
  RendererPtr renderer(new Renderer(gm_));
  base::LogChecker log_checker;

  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  Reset();
  data_->attribute_array->EnableAttribute(0, false);
  renderer->DrawScene(root);
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("VertexAttribPointer"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DisableVertexAttribArray"));

  Reset();
  data_->attribute_array->EnableAttribute(0, true);
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttribPointer"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("EnableVertexAttribArray"));
  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST_F(RendererTest, VertexArraysPerShaderProgram) {
  // Check that a resource is created per ShaderProgram. We can test this by
  // checking that the proper VertexAttribPointer calls are sent. This requires
  // two shader programs where the second one uses more buffer Attributes than
  // the first one.
  base::LogChecker log_checker;

  static const char* kVertexShaderString = "attribute vec3 attribute;\n";
  static const char* kVertex2ShaderString =
      "attribute vec3 attribute;\n"
      "attribute vec3 attribute2;\n";
  static const char* kFragmentShaderString = "void main() {}\n";

  BuildRectangleBufferObject(data_, options_);

  NodePtr root(new Node);
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  AttributeArrayPtr aa(new AttributeArray);
  aa->AddAttribute(reg->Create<Attribute>(
      "attribute", BufferObjectElement(
          data_->vertex_buffer, data_->vertex_buffer->AddSpec(
              BufferObject::kFloat, 3, 0))));
  aa->AddAttribute(reg->Create<Attribute>(
      "attribute2", BufferObjectElement(
          data_->vertex_buffer, data_->vertex_buffer->AddSpec(
              BufferObject::kFloat, 3, 12))));
  ShapePtr shape(new Shape);
  shape->SetAttributeArray(aa);
  root->SetShaderProgram(ShaderProgram::BuildFromStrings(
      "Shader", reg, kVertexShaderString, kFragmentShaderString,
      base::AllocatorPtr()));
  root->GetShaderProgram()->SetLabel("root shader");
  root->AddShape(shape);

  // The child uses more attributes.
  NodePtr child(new Node);
  child->SetShaderProgram(ShaderProgram::BuildFromStrings(
      "Shader", reg, kVertex2ShaderString, kFragmentShaderString,
      base::AllocatorPtr()));
  child->AddShape(shape);
  root->GetShaderProgram()->SetLabel("child shader");
  root->AddChild(child);

  {
    RendererPtr renderer(new Renderer(gm_));
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(3U, trace_verifier_->GetCountOf("BindAttribLocation"));
    EXPECT_EQ(3U, trace_verifier_->GetCountOf("VertexAttribPointer"));
    EXPECT_EQ(3U, trace_verifier_->GetCountOf("EnableVertexAttribArray"));
    EXPECT_TRUE(log_checker.HasMessage("WARNING", "contains buffer attribute"));
  }

  // If we disable the missing attribute there should be no warning.
  {
    aa->EnableBufferAttribute(1U, false);
    RendererPtr renderer(new Renderer(gm_));
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(3U, trace_verifier_->GetCountOf("BindAttribLocation"));
    EXPECT_EQ(3U, trace_verifier_->GetCountOf("VertexAttribPointer"));
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("EnableVertexAttribArray"));
    EXPECT_FALSE(log_checker.HasAnyMessages());
    aa->EnableBufferAttribute(1U, true);
  }

  base::logging_internal::SingleLogger::ClearMessages();
  // Check without vertex arrays.
  gm_->EnableFeature(GraphicsManager::kVertexArrays, false);
  {
    RendererPtr renderer(new Renderer(gm_));
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(3U, trace_verifier_->GetCountOf("BindAttribLocation"));
    EXPECT_EQ(3U, trace_verifier_->GetCountOf("VertexAttribPointer"));
    EXPECT_TRUE(log_checker.HasMessage("WARNING", "contains buffer attribute"));
  }
  gm_->EnableFeature(GraphicsManager::kVertexArrays, true);
}

TEST_F(RendererTest, VertexArraysPerThread) {
  // Check that distinct vertex arrays are created for distinct threads.
  base::LogChecker log_checker;

  static const char* kVertexShaderString = "attribute vec3 attribute;\n";
  static const char* kFragmentShaderString = "void main() {}\n";

  BuildRectangleBufferObject(data_, options_);

  NodePtr root(new Node);
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  AttributeArrayPtr aa(new AttributeArray);
  aa->AddAttribute(reg->Create<Attribute>(
      "attribute", BufferObjectElement(
          data_->vertex_buffer, data_->vertex_buffer->AddSpec(
              BufferObject::kFloat, 3, 0))));
  ShapePtr shape(new Shape);
  shape->SetAttributeArray(aa);
  root->SetShaderProgram(ShaderProgram::BuildFromStrings(
      "Shader", reg, kVertexShaderString, kFragmentShaderString,
      base::AllocatorPtr()));
  root->GetShaderProgram()->SetLabel("root shader");
  root->AddShape(shape);

  // The attribute location should be bound only once, since the program
  // object is shared between threads, while glVertexAttribPointer should be
  // called once per vertex array object.
  {
    Reset();
    GlContextPtr share_context = FakeGlContext::CreateShared(*gl_context_);
    RendererPtr renderer(new Renderer(gm_));
    std::thread render_thread(RenderingThread, renderer, share_context, &root);
    // FakeGlContext is not thread-safe, so we don't try to render concurrently.
    render_thread.join();
    renderer->DrawScene(root);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindAttribLocation"));
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("VertexAttribPointer"));
  }

  // Check without vertex arrays.
  gm_->EnableFeature(GraphicsManager::kVertexArrays, false);
  {
    Reset();
    GlContextPtr share_context = FakeGlContext::CreateShared(*gl_context_);
    RendererPtr renderer(new Renderer(gm_));
    std::thread render_thread(RenderingThread, renderer, share_context, &root);
    render_thread.join();
    renderer->DrawScene(root);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindAttribLocation"));
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("VertexAttribPointer"));
  }
  gm_->EnableFeature(GraphicsManager::kVertexArrays, true);
}

TEST_F(RendererTest, NonBufferAttributes) {
  RendererPtr renderer(new Renderer(gm_));
  TracingHelper helper;

  {
    NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
    ShaderInputRegistryPtr reg(new ShaderInputRegistry);
    reg->IncludeGlobalRegistry();
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uTexture", kTextureUniform, "Plane texture"));
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uTexture2", kTextureUniform, "Plane texture2"));
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "aTestAttrib", kFloatAttribute, "Testing attribute"));

    data_->attribute_array = new AttributeArray;
    const Attribute a = reg->Create<Attribute>("aTestAttrib", 2.f);
    EXPECT_TRUE(a.IsValid());
    data_->attribute_array->AddAttribute(a);
    data_->shader = ShaderProgram::BuildFromStrings(
        "Plane shader", reg, kPlaneVertexShaderString,
        kPlaneFragmentShaderString, base::AllocatorPtr());
    data_->shape->SetAttributeArray(data_->attribute_array);
    data_->rect->SetShaderProgram(data_->shader);
    data_->rect->ClearUniforms();
    AddPlaneShaderUniformsToNode(data_, data_->rect);
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("VertexAttrib1fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib2fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib3fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib4fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttribPointer"));
  }

  {
    NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
    ShaderInputRegistryPtr reg(new ShaderInputRegistry);
    reg->IncludeGlobalRegistry();
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uTexture", kTextureUniform, "Plane texture"));
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uTexture2", kTextureUniform, "Plane texture"));
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "aTestAttrib", kFloatVector2Attribute, "Testing attribute"));

    data_->attribute_array = new AttributeArray;
    const Attribute a = reg->Create<Attribute>(
        "aTestAttrib", math::Vector2f(1.f, 2.f));
    EXPECT_TRUE(a.IsValid());
    data_->attribute_array->AddAttribute(a);
    data_->shader = ShaderProgram::BuildFromStrings(
        "Plane shader", reg, kPlaneVertexShaderString,
        kPlaneFragmentShaderString, base::AllocatorPtr());
    data_->shape->SetAttributeArray(data_->attribute_array);
    data_->rect->SetShaderProgram(data_->shader);
    data_->rect->ClearUniforms();
    AddPlaneShaderUniformsToNode(data_, data_->rect);
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib1fv"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("VertexAttrib2fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib3fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib4fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttribPointer"));
  }

  gm_->EnableFeature(GraphicsManager::kVertexArrays, false);
  {
    RendererPtr renderer(new Renderer(gm_));
    NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
    ShaderInputRegistryPtr reg(new ShaderInputRegistry);
    reg->IncludeGlobalRegistry();
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uTexture", kTextureUniform, "Plane texture"));
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uTexture2", kTextureUniform, "Plane texture"));
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "aTestAttrib", kFloatVector3Attribute, "Testing attribute"));

    data_->attribute_array = new AttributeArray;
    const Attribute a = reg->Create<Attribute>(
        "aTestAttrib", math::Vector3f(1.f, 2.f, 3.f));
    EXPECT_TRUE(a.IsValid());
    data_->attribute_array->AddAttribute(a);
    data_->shader = ShaderProgram::BuildFromStrings(
        "Plane shader", reg, kPlaneVertexShaderString,
        kPlaneFragmentShaderString, base::AllocatorPtr());
    data_->shape->SetAttributeArray(data_->attribute_array);
    data_->rect->SetShaderProgram(data_->shader);
    data_->rect->ClearUniforms();
    AddPlaneShaderUniformsToNode(data_, data_->rect);
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib1fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib2fv"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("VertexAttrib3fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib4fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttribPointer"));
  }

  {
    RendererPtr renderer(new Renderer(gm_));
    NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
    ShaderInputRegistryPtr reg(new ShaderInputRegistry);
    reg->IncludeGlobalRegistry();
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uTexture", kTextureUniform, "Plane texture"));
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uTexture2", kTextureUniform, "Plane texture"));
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "aTestAttrib", kFloatVector4Attribute, "Testing attribute"));

    data_->attribute_array = new AttributeArray;
    const Attribute a = reg->Create<Attribute>(
        "aTestAttrib", math::Vector4f(1.f, 2.f, 3.f, 4.f));
    EXPECT_TRUE(a.IsValid());
    data_->attribute_array->AddAttribute(a);
    data_->shader = ShaderProgram::BuildFromStrings(
        "Plane shader", reg, kPlaneVertexShaderString,
        kPlaneFragmentShaderString, base::AllocatorPtr());
    data_->shape->SetAttributeArray(data_->attribute_array);
    data_->rect->SetShaderProgram(data_->shader);
    data_->rect->ClearUniforms();
    AddPlaneShaderUniformsToNode(data_, data_->rect);
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib1fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib2fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib3fv"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("VertexAttrib4fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttribPointer"));
  }

  gm_->EnableFeature(GraphicsManager::kVertexArrays, true);
  // Reset data.
  data_->rect = nullptr;
  BuildRectangle(data_, options_);
  {
    NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
    Attribute* a = data_->shape->GetAttributeArray()->GetMutableAttribute(0U);
    a->SetFixedPointNormalized(true);

    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib1fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib2fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib3fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib4fv"));
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("VertexAttribPointer"));
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(0U, "VertexAttribPointer"))
            .HasArg(4, "GL_TRUE"));
  }

  // Reset data.
  data_->rect = nullptr;
  BuildRectangle(data_, options_);
  {
    NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib1fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib2fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib3fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib4fv"));
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("VertexAttribPointer"));
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(0U, "VertexAttribPointer"))
            .HasArg(4, "GL_FALSE"));
  }

  Reset();
}

TEST_F(RendererTest, MissingInputFromRegistry) {
  // Test that if a shader defines an attribute or uniform but there is no
  // registry entry for it, a warning message is logged.
  base::LogChecker log_checker;
  RendererPtr renderer(new Renderer(gm_));

  static const char* kVertexShaderString =
      "attribute vec3 attribute1;\n"
      "attribute vec3 attribute2;\n";

  static const char* kGeometryShaderString =
      "uniform vec3 uniform1;\n";

  static const char* kFragmentShaderString =
      "uniform vec3 uniform2;\n";

  // Everything defined and added.
  {
    NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
    ShaderInputRegistryPtr reg(new ShaderInputRegistry);
    reg->IncludeGlobalRegistry();
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "attribute1", kFloatVector3Attribute, ""));
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "attribute2", kFloatVector3Attribute, ""));
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uniform1", kFloatVector3Uniform, ""));
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uniform2", kFloatVector3Uniform, ""));

    data_->attribute_array = new AttributeArray;
    data_->attribute_array->AddAttribute(
        reg->Create<Attribute>("attribute1", math::Vector3f(1.f, 2.f, 3.f)));
    data_->attribute_array->AddAttribute(
        reg->Create<Attribute>("attribute2", math::Vector3f(1.f, 2.f, 3.f)));
    reg->Create<Uniform>("uniform1", math::Vector3f(1.f, 2.f, 3.f));
    reg->Create<Uniform>("uniform2", math::Vector3f(1.f, 2.f, 3.f));
    data_->shader = ShaderProgram::BuildFromStrings(
        "Shader", reg, kVertexShaderString, kGeometryShaderString,
        kFragmentShaderString, base::AllocatorPtr());
    data_->shape->SetAttributeArray(data_->attribute_array);
    data_->rect->SetShaderProgram(data_->shader);
    data_->rect->ClearUniforms();
    data_->rect->AddUniform(
        reg->Create<Uniform>("uniform1", math::Vector3f::Zero()));
    data_->rect->AddUniform(
        reg->Create<Uniform>("uniform2", math::Vector3f::Zero()));
    Reset();
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());
  }

  // Missing a uniform that is defined in the shader.
  {
    NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
    ShaderInputRegistryPtr reg(new ShaderInputRegistry);
    reg->IncludeGlobalRegistry();
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "attribute1", kFloatVector3Attribute, ""));
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "attribute2", kFloatVector3Attribute, ""));
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uniform1", kFloatVector3Uniform, ""));
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uniform2", kFloatVector3Uniform, ""));

    data_->attribute_array = new AttributeArray;
    data_->attribute_array->AddAttribute(
        reg->Create<Attribute>("attribute1", math::Vector3f(1.f, 2.f, 3.f)));
    data_->attribute_array->AddAttribute(
        reg->Create<Attribute>("attribute2", math::Vector3f(1.f, 2.f, 3.f)));
    reg->Create<Uniform>("uniform1", math::Vector3f(1.f, 2.f, 3.f));
    reg->Create<Uniform>("uniform2", math::Vector3f(1.f, 2.f, 3.f));
    data_->shader = ShaderProgram::BuildFromStrings(
        "Shader", reg, kVertexShaderString, kGeometryShaderString,
        kFragmentShaderString, base::AllocatorPtr());
    data_->shape->SetAttributeArray(data_->attribute_array);
    data_->rect->SetShaderProgram(data_->shader);
    data_->rect->ClearUniforms();
    data_->rect->AddUniform(
        reg->Create<Uniform>("uniform2", math::Vector3f::Zero()));
    Reset();
    renderer->DrawScene(root);
    EXPECT_TRUE(log_checker.HasMessage("WARNING",
                                       "no value set for uniform 'uniform1'"));
  }

  // NULL texture value for a texture uniform.
  {
    static const char* kFragmentShaderString =
        "uniform vec3 uniform1;\n"
        "uniform sampler2D uniform2;\n";
    NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
    ShaderInputRegistryPtr reg(new ShaderInputRegistry);
    reg->IncludeGlobalRegistry();

    data_->attribute_array = new AttributeArray;
    data_->attribute_array->AddAttribute(
        reg->Create<Attribute>("attribute1", math::Vector3f(1.f, 2.f, 3.f)));
    data_->attribute_array->AddAttribute(
        reg->Create<Attribute>("attribute2", math::Vector3f(1.f, 2.f, 3.f)));
    reg->Create<Uniform>("uniform1", math::Vector3f(1.f, 2.f, 3.f));
    reg->Create<Uniform>("uniform2", TexturePtr());
    data_->shader = ShaderProgram::BuildFromStrings(
        "Shader", reg, kVertexShaderString, kGeometryShaderString,
        kFragmentShaderString, base::AllocatorPtr());
    data_->shape->SetAttributeArray(data_->attribute_array);
    data_->rect->SetShaderProgram(data_->shader);
    data_->rect->ClearUniforms();
    data_->rect->AddUniform(
        reg->Create<Uniform>("uniform1", math::Vector3f::Zero()));
    Reset();
    renderer->DrawScene(root);
    EXPECT_TRUE(log_checker.HasMessage(
        "WARNING", "no value set for uniform 'uniform2'"));
    // Sending a null texture should not crash or print a warning. No uniform
    // value should also be sent to the program.
    data_->rect->AddUniform(
        reg->Create<Uniform>("uniform2", TexturePtr()));
    Reset();
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("Uniform1i("));
  }

  // Missing attribute.
  {
    NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
    ShaderInputRegistryPtr reg(new ShaderInputRegistry);
    reg->IncludeGlobalRegistry();
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "attribute1", kFloatVector3Attribute, ""));
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uniform1", kFloatVector3Uniform, ""));
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uniform2", kFloatVector3Uniform, ""));

    data_->attribute_array = new AttributeArray;
    data_->attribute_array->AddAttribute(
        reg->Create<Attribute>("attribute1", math::Vector3f(1.f, 2.f, 3.f)));
    reg->Create<Uniform>("uniform1", math::Vector3f(1.f, 2.f, 3.f));
    reg->Create<Uniform>("uniform2", math::Vector3f(1.f, 2.f, 3.f));
    data_->shader = ShaderProgram::BuildFromStrings(
        "Shader", reg, kVertexShaderString, kGeometryShaderString,
        kFragmentShaderString, base::AllocatorPtr());
    data_->rect->AddUniform(
        reg->Create<Uniform>("uniform1", math::Vector3f::Zero()));
    data_->rect->AddUniform(
        reg->Create<Uniform>("uniform2", math::Vector3f::Zero()));
    data_->shape->SetAttributeArray(data_->attribute_array);
    data_->rect->SetShaderProgram(data_->shader);
    data_->rect->ClearUniforms();
    Reset();
    renderer->DrawScene(root);
    EXPECT_TRUE(
        log_checker.HasMessage(
            "WARNING",
            "Attribute 'attribute2' used in shader 'Shader' does not have a"));
  }

  // Missing attribute.
  {
    NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
    ShaderInputRegistryPtr reg(new ShaderInputRegistry);
    reg->IncludeGlobalRegistry();
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "attribute1", kFloatVector3Attribute, ""));
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "attribute2", kFloatVector3Attribute, ""));
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uniform2", kFloatVector3Uniform, ""));

    data_->attribute_array = new AttributeArray;
    data_->attribute_array->AddAttribute(
        reg->Create<Attribute>("attribute1", math::Vector3f(1.f, 2.f, 3.f)));
    data_->attribute_array->AddAttribute(
        reg->Create<Attribute>("attribute2", math::Vector3f(1.f, 2.f, 3.f)));
    reg->Create<Uniform>("uniform2", math::Vector3f(1.f, 2.f, 3.f));
    data_->rect->AddUniform(
        reg->Create<Uniform>("uniform2", math::Vector3f::Zero()));
    data_->shader = ShaderProgram::BuildFromStrings(
        "Shader", reg, kVertexShaderString, kGeometryShaderString,
        kFragmentShaderString, base::AllocatorPtr());
    data_->shape->SetAttributeArray(data_->attribute_array);
    data_->rect->SetShaderProgram(data_->shader);
    data_->rect->ClearUniforms();
    Reset();
    renderer->DrawScene(root);
    EXPECT_TRUE(
        log_checker.HasMessage(
            "WARNING",
            "Uniform 'uniform1' used in shader 'Shader' does not have a"));
  }

  // Reset data.
  data_->rect = nullptr;
  BuildRectangle(data_, options_);
}

TEST_F(RendererTest, ShaderRecompilationClearsUniforms) {
  base::LogChecker log_checker;

  static const char* kVertexShaderString =
      "attribute vec3 attribute1;\n"
      "attribute vec3 attribute2;\n";

  static const char* kGeometryShaderString =
      "uniform vec3 uniform1;\n";

  static const char* kFragmentShaderString =
      "uniform vec3 uniform2;\n";

  static const char* kFragmentShaderWithExtraUniformString =
      "uniform vec3 uniform2;\n"
      "uniform vec3 uniform3;\n";

  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  reg->IncludeGlobalRegistry();
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "attribute1", kFloatVector3Attribute, ""));
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "attribute2", kFloatVector3Attribute, ""));
  reg->Add(ShaderInputRegistry::UniformSpec(
      "uniform1", kFloatVector3Uniform, ""));
  reg->Add(ShaderInputRegistry::UniformSpec(
      "uniform2", kFloatVector3Uniform, ""));
  reg->Add(ShaderInputRegistry::UniformSpec(
      "uniform3", kFloatVector3Uniform, ""));

  data_->attribute_array = new AttributeArray;
  data_->attribute_array->AddAttribute(
      reg->Create<Attribute>("attribute1", math::Vector3f(1.f, 2.f, 3.f)));
  data_->attribute_array->AddAttribute(
      reg->Create<Attribute>("attribute2", math::Vector3f(1.f, 2.f, 3.f)));
  reg->Create<Uniform>("uniform1", math::Vector3f(1.f, 2.f, 3.f));
  reg->Create<Uniform>("uniform2", math::Vector3f(1.f, 2.f, 3.f));
  data_->shader = ShaderProgram::BuildFromStrings(
      "Shader", reg, kVertexShaderString, kGeometryShaderString,
      kFragmentShaderString, base::AllocatorPtr());
  data_->shape->SetAttributeArray(data_->attribute_array);
  data_->rect->SetShaderProgram(data_->shader);
  data_->rect->ClearUniforms();
  data_->rect->AddUniform(
      reg->Create<Uniform>("uniform1", math::Vector3f::Zero()));
  data_->rect->AddUniform(
      reg->Create<Uniform>("uniform2", math::Vector3f::Zero()));
  Reset();
  RendererPtr renderer(new Renderer(gm_));
  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // Now update the shader string to have another uniform, but without setting
  // a value for it.
  data_->shader->GetFragmentShader()->SetSource(
      kFragmentShaderWithExtraUniformString);
  // The warning about not setting a uniform value will be triggered only the
  // first time the shader program is bound.
  renderer->DrawScene(root);
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "There is no value set"));

  // Fixing the shader should remove the message.
  data_->shader->GetFragmentShader()->SetSource(kFragmentShaderString);
  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST_F(RendererTest, RegistryHasWrongUniformType) {
  // Test that if a shader defines an attribute of different type than the
  // registry entry for it, a warning message is logged.
  base::LogChecker log_checker;
  RendererPtr renderer(new Renderer(gm_));

  static const char* kVertexShaderString = "attribute vec3 aNimal;\n";
  static const char* kFragmentShaderString = "uniform vec4 uMbrella;\n";

  // Everything defined.
  {
    NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
    ShaderInputRegistryPtr reg(new ShaderInputRegistry);
    reg->IncludeGlobalRegistry();
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "attribute", kFloatVector3Attribute, ""));
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uniform", kFloatVector3Uniform, ""));

    data_->attribute_array = new AttributeArray;
    data_->attribute_array->AddAttribute(
        reg->Create<Attribute>("aNimal", math::Vector3f(1.f, 2.f, 3.f)));
    reg->Create<Uniform>("uMbrella", math::Vector3f(1.f, 2.f, 3.f));
    data_->shader = ShaderProgram::BuildFromStrings(
        "Shader", reg, kVertexShaderString, kFragmentShaderString,
        base::AllocatorPtr());
    data_->shape->SetAttributeArray(data_->attribute_array);
    data_->rect->SetShaderProgram(data_->shader);
    data_->rect->ClearUniforms();
    Reset();
    renderer->DrawScene(root);
    EXPECT_TRUE(
        log_checker.HasMessage("WARNING",
                               "Uniform 'uMbrella' has a different type"));
  }

  // Reset data.
  data_->rect = nullptr;
  BuildRectangle(data_, options_);
}

TEST_F(RendererTest, RegistryHasAliasedInputs) {
  // Test that if a registry has aliased inputs then a warning message is
  // logged.
  base::LogChecker log_checker;
  RendererPtr renderer(new Renderer(gm_));

  static const char* kVertexShaderString =
      "attribute vec3 attribute1;\n"
      "attribute vec3 attribute2;\n";
  static const char* kFragmentShaderString = "uniform vec4 uniform1;\n";

  // Everything defined.
  {
    NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
    ShaderInputRegistryPtr reg1(new ShaderInputRegistry);
    reg1->Add(ShaderInputRegistry::AttributeSpec(
        "attribute1", kFloatVector3Attribute, ""));
    reg1->Add(ShaderInputRegistry::UniformSpec(
        "uniform1", kFloatVector3Uniform, ""));
    ShaderInputRegistryPtr reg2(new ShaderInputRegistry);
    reg2->Add(ShaderInputRegistry::AttributeSpec(
        "attribute2", kFloatVector3Attribute, ""));
    reg1->Include(reg2);
    // Add an input to reg2 that already exists in reg1. This is only detected
    // when the shader resource is created.
    reg2->Add(ShaderInputRegistry::UniformSpec(
        "uniform1", kFloatVector3Uniform, ""));

    data_->attribute_array = new AttributeArray;
    data_->attribute_array->AddAttribute(
        reg1->Create<Attribute>("attribute1", math::Vector3f(1.f, 2.f, 3.f)));
    data_->attribute_array->AddAttribute(
        reg1->Create<Attribute>("attribute2", math::Vector3f(1.f, 2.f, 3.f)));
    reg1->Create<Uniform>("uniform", math::Vector3f(1.f, 2.f, 3.f));
    data_->shader = ShaderProgram::BuildFromStrings(
        "Shader", reg1, kVertexShaderString, kFragmentShaderString,
        base::AllocatorPtr());
    data_->shape->SetAttributeArray(data_->attribute_array);
    data_->rect->SetShaderProgram(data_->shader);
    data_->rect->ClearUniforms();
    Reset();
    renderer->DrawScene(root);
    EXPECT_TRUE(
        log_checker.HasMessage("WARNING",
                               "contains multiple definitions of some inputs"));
  }

  // Reset data.
  data_->rect = nullptr;
  BuildRectangle(data_, options_);
}

TEST_F(RendererTest, AttributeArraysShareIndexBuffer) {
  // Test that if when multiple attribute arrays (VAOs) share an index buffer
  // that the index buffer is rebound for each.
  RendererPtr renderer(new Renderer(gm_));

  static const char* kVertexShaderString =
      "attribute vec3 attribute1;\n"
      "attribute vec3 attribute2;\n";
  static const char* kFragmentShaderString = "void main() {}\n";

  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  data_->attribute_array = new AttributeArray;
  data_->attribute_array->AddAttribute(
      reg->Create<Attribute>("attribute1", math::Vector3f(1.f, 2.f, 3.f)));
  data_->attribute_array->AddAttribute(
      reg->Create<Attribute>("attribute2", math::Vector3f(1.f, 2.f, 3.f)));
  data_->shader = ShaderProgram::BuildFromStrings(
      "Shader", reg, kVertexShaderString, kFragmentShaderString,
      base::AllocatorPtr());
  data_->shape->SetAttributeArray(data_->attribute_array);
  data_->rect->SetShaderProgram(data_->shader);
  data_->rect->ClearUniforms();
  Reset();
  renderer->DrawScene(root);
  // The element array buffer should have been bound once.
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf("BindBuffer(GL_ELEMENT_ARRAY_BUFFER"));

  // Reset the renderer.
  renderer.Reset(new Renderer(gm_));
  AttributeArrayPtr array2(new AttributeArray);
  array2->AddAttribute(
      reg->Create<Attribute>("attribute1", math::Vector3f(1.f, 2.f, 3.f)));
  array2->AddAttribute(
      reg->Create<Attribute>("attribute2", math::Vector3f(1.f, 2.f, 3.f)));
  ShapePtr shape(new Shape);
  shape->SetPrimitiveType(options_->primitive_type);
  shape->SetIndexBuffer(data_->index_buffer);
  shape->SetAttributeArray(array2);
  data_->rect->AddShape(shape);

  Reset();
  renderer->DrawScene(root);
  // The element array buffer should have been bound twice.
  EXPECT_EQ(2U,
            trace_verifier_->GetCountOf("BindBuffer(GL_ELEMENT_ARRAY_BUFFER"));

  // Reset data.
  data_->rect = nullptr;
  BuildRectangle(data_, options_);
}

TEST_F(RendererTest, AttributeArrayHasAttributeShaderDoesnt) {
  // Test that if an attribute array contains an attribute that is not defined
  // in the shader then a warning message is logged.
  base::LogChecker log_checker;
  RendererPtr renderer(new Renderer(gm_));

  static const char* kVertexShaderString = "attribute vec3 attribute1;\n";
  static const char* kFragmentShaderString = "uniform vec3 uniform1;\n";

  {
    NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
    ShaderInputRegistryPtr reg(new ShaderInputRegistry);
    reg->IncludeGlobalRegistry();
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "attribute1", kBufferObjectElementAttribute, ""));
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "attribute2", kFloatVector3Attribute, ""));
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uniform1", kFloatVector3Uniform, ""));

    data_->attribute_array = new AttributeArray;
    data_->attribute_array->AddAttribute(reg->Create<Attribute>(
        "attribute1", BufferObjectElement(
            data_->vertex_buffer, data_->vertex_buffer->AddSpec(
                BufferObject::kFloat, 3, 0))));
    data_->attribute_array->AddAttribute(
        reg->Create<Attribute>("attribute2", math::Vector3f(1.f, 2.f, 3.f)));
    data_->shader = ShaderProgram::BuildFromStrings(
        "AttributeArrayHasAttributeShaderDoesnt1", reg, kVertexShaderString,
        kFragmentShaderString, base::AllocatorPtr());
    data_->shape->SetAttributeArray(data_->attribute_array);
    data_->rect->SetShaderProgram(data_->shader);
    data_->rect->ClearUniforms();
    data_->rect->AddUniform(
        reg->Create<Uniform>("uniform1", math::Vector3f(1.f, 2.f, 3.f)));
    Reset();
    renderer->DrawScene(root);
    EXPECT_TRUE(
        log_checker.HasMessage(
            "WARNING",
            "contains simple attribute 'attribute2' but the current shader"));
  }

  base::logging_internal::SingleLogger::ClearMessages();
  {
    NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
    ShaderInputRegistryPtr reg(new ShaderInputRegistry);
    reg->IncludeGlobalRegistry();
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "attribute2", kBufferObjectElementAttribute, ""));
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "attribute", kFloatVector3Attribute, ""));
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uniform", kFloatVector3Uniform, ""));

    data_->attribute_array = new AttributeArray;
    data_->attribute_array->AddAttribute(reg->Create<Attribute>(
        "attribute2", BufferObjectElement(
            data_->vertex_buffer, data_->vertex_buffer->AddSpec(
                BufferObject::kFloat, 3, 0))));
    data_->attribute_array->AddAttribute(
        reg->Create<Attribute>("attribute", math::Vector3f(1.f, 2.f, 3.f)));
    data_->shader = ShaderProgram::BuildFromStrings(
        "AttributeArrayHasAttributeShaderDoesnt2", reg, kVertexShaderString,
        kFragmentShaderString, base::AllocatorPtr());
    data_->shape->SetAttributeArray(data_->attribute_array);
    data_->rect->SetShaderProgram(data_->shader);
    data_->rect->ClearUniforms();
    data_->rect->AddUniform(
        reg->Create<Uniform>("uniform1", math::Vector3f(1.f, 2.f, 3.f)));
    Reset();
    renderer->DrawScene(root);
    EXPECT_TRUE(
        log_checker.HasMessage(
            "WARNING",
            "contains buffer attribute 'attribute2' but the current shader"));
  }

  // Reset data.
  data_->rect = nullptr;
  BuildRectangle(data_, options_);
}

TEST_F(RendererTest, ReuseSameBufferAndShader) {
  // Test that Renderer does not bind a shader or buffer when they are already
  // active.
  RendererPtr renderer(new Renderer(gm_));
  base::LogChecker log_checker;
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);

  // Create a node with the same shader as the rect, and attach it as a child
  // of the rect.
  NodePtr node(new Node);
  node->AddShape(data_->shape);
  node->SetShaderProgram(data_->shader);
  data_->rect->AddChild(node);
  Reset();
  data_->attribute_array->EnableAttribute(0, false);
  renderer->DrawScene(root);
  // The shader and data for the shape should each only have been bound once.
  // Since the default shader is never bound, its ID should be 1.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("UseProgram(0x1)"));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER, 0x1)"));

  // Reset.
  data_->rect->ClearChildren();
}

TEST_F(RendererTest, ShaderHierarchies) {
  // Test that uniforms are sent to only if the right shader is bound (if they
  // aren't an error message is logged).
  // Use a node hierarchy as follows:
  //                             Root
  //              |                      |
  // LeftA ->ShaderA         RightA -> ShaderB
  //    |                                           |
  // nodes...                            RightB -> NULL shader
  //                                                 |
  //                                            RightC -> uniform for ShaderB
  // and ensure that the Uniform in rightC is sent to the proper shader.
  RendererPtr renderer(new Renderer(gm_));
  base::LogChecker log_checker;
  // Create data.
  BuildGraph(data_, options_, kWidth, kHeight);

  // Construct graph.
  NodePtr root(new Node);
  root->AddChild(data_->rect);

  NodePtr right_a(new Node);
  root->AddChild(right_a);

  // Create a new shader.
  static const char* kVertexShaderString =
      "attribute float aFloat;\n"
      "uniform int uInt1;\n"
      "uniform int uInt2;\n";

  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "aFloat", kBufferObjectElementAttribute, "."));
  reg->Add(ShaderInputRegistry::UniformSpec("uInt1", kIntUniform, "."));
  reg->Add(ShaderInputRegistry::UniformSpec("uInt2", kIntUniform, "."));

  ShaderProgramPtr program(new ShaderProgram(reg));
  program->SetLabel("Dummy Shader");
  program->SetVertexShader(ShaderPtr(new Shader(kVertexShaderString)));
  program->SetGeometryShader(
      ShaderPtr(new Shader("Dummy Geometry Shader Source")));
  program->SetFragmentShader(
      ShaderPtr(new Shader("Dummy Fragment Shader Source")));

  // Build the right side of the graph.
  right_a->SetShaderProgram(program);

  AttributeArrayPtr attribute_array(new AttributeArray);
  attribute_array->AddAttribute(reg->Create<Attribute>(
      "aFloat", BufferObjectElement(
          data_->vertex_buffer, data_->vertex_buffer->AddSpec(
              BufferObject::kFloat, 1, 0))));
  ShapePtr shape(new Shape);
  shape->SetAttributeArray(attribute_array);

  NodePtr right_b(new Node);
  right_a->AddChild(right_b);
  NodePtr right_c(new Node);
  right_b->AddChild(right_c);
  right_b->AddUniform(reg->Create<Uniform>("uInt2", 2));

  right_c->AddShape(shape);
  right_c->AddUniform(reg->Create<Uniform>("uInt1", 3));

  Reset();
  renderer->DrawScene(root);

  // There should be no log messages.
  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST_F(RendererTest, UniformPushAndPop) {
  // Repetitive uniforms should not cause unneeded uploads.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root;
  root = BuildGraph(data_, options_, kWidth, kHeight);

  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  reg->IncludeGlobalRegistry();

  // Dummy shader with the uniforms defined.
  static const char* kVertexShaderString = "uniform int uInt;\n";
  reg->Add(ShaderInputRegistry::UniformSpec("uInt", kIntUniform, "."));

  ShaderProgramPtr program(new ShaderProgram(reg));
  program->SetLabel("Dummy Shader");
  program->SetVertexShader(ShaderPtr(new Shader(kVertexShaderString)));
  program->SetFragmentShader(
      ShaderPtr(new Shader("Dummy Fragment Shader Source")));
  data_->rect->SetShaderProgram(program);
  // Remove attribute array to prevent warnings; we are only testing uniforms
  // here.
  data_->rect->ClearUniforms();
  data_->rect->AddUniform(reg->Create<Uniform>("uInt", 1));
  data_->shape->SetAttributeArray(AttributeArrayPtr(nullptr));

  NodePtr node(new Node);
  node->AddShape(data_->shape);
  data_->rect->AddChild(node);
  node->AddUniform(reg->Create<Uniform>("uInt", 2));

  Reset();
  renderer->DrawScene(root);
  // The uniform should have been sent twice, once for each value.
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("Uniform1i"));

  // Reset.
  data_->rect = nullptr;
  data_->shape->SetAttributeArray(data_->attribute_array);
  BuildRectangle(data_, options_);
}

TEST_F(RendererTest, UniformsShareTextureUnits) {
  // Test that all textures that share the same uniform are bound to the same
  // texture unit.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  data_->rect->ClearChildren();
  data_->rect->ClearUniforms();
  data_->rect->ClearShapes();

  data_->rect->AddUniform(data_->shader->GetRegistry()->Create<Uniform>(
      "uProjectionMatrix", math::Matrix4f::Identity()));
  data_->rect->AddUniform(data_->shader->GetRegistry()->Create<Uniform>(
      "uCubeMapTexture", data_->cubemap));

  // Add many nodes with different textures bound to the same uniform; they
  // should all share the same image unit.
  static const int kNumNodes = 9;
  for (int i = 0; i < kNumNodes; ++i) {
    NodePtr node(new Node);

    TexturePtr texture(new Texture);
    texture->SetImage(0U, data_->image);
    texture->SetSampler(data_->sampler);
    node->AddUniform(data_->shader->GetRegistry()->Create<Uniform>(
        "uTexture", texture));

    texture = new Texture;
    texture->SetImage(0U, data_->image);
    texture->SetSampler(data_->sampler);
    node->AddUniform(
        data_->shader->GetRegistry()->Create<Uniform>("uTexture2", texture));

    data_->rect->AddChild(node);
  }
  Reset();
  renderer->DrawScene(root);
  // Nothing should have happened since there are no shapes.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Uniform1i"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ActiveTexture"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE0)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE1)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE2)"));

  // Add shapes.
  for (int i = 0; i < kNumNodes; ++i)
    data_->rect->GetChildren()[i]->AddShape(data_->shape);
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(24U, trace_verifier_->GetCountOf("TexImage2D"));
  // There should be 19 calls to ActiveTexture: the units will ping-pong; there
  // is also the cubemap which gets bound.
  EXPECT_EQ(19U, trace_verifier_->GetCountOf("ActiveTexture"));
  // The texture uniforms are only sent once.
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("Uniform1i"));
  EXPECT_EQ(9U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE0)"));
  EXPECT_EQ(9U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE1)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE2)"));

  // Reset data.
  data_->rect = nullptr;
  BuildRectangle(data_, options_);
}

TEST_F(RendererTest, UniformsDoNotShareTextureUnits) {
  // Test that different texture uniforms using the same texture use different
  // texture units.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  Reset();
  renderer->DrawScene(root);
  // BuildGraph uses data->texture for uTexture, uTexture2.
  // 6 for cube map + 1 for data->texture.
  EXPECT_EQ(7U, trace_verifier_->GetCountOf("TexImage2D"));
  // 1 for cube map, 2 for uTexture/uTexture2.
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("ActiveTexture"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("BindTexture"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("Uniform1i"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE0)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE1)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE2)"));

  // Reset data.
  data_->rect = nullptr;
  BuildRectangle(data_, options_);
}

TEST_F(RendererTest, UniformAreSentCorrectly) {
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  reg->IncludeGlobalRegistry();

  // Dummy shader with the uniforms defined.
  static const char* kVertexShaderString =
      "uniform int uInt;\n"
      "uniform float uFloat;\n"
      "uniform vec2 uFV2;\n"
      "uniform vec3 uFV3;\n"
      "uniform vec4 uFV4;\n"
      "uniform ivec2 uIV2;\n"
      "uniform ivec3 uIV3;\n"
      "uniform ivec4 uIV4;\n"
      "uniform mat2 uMat2;\n"
      "uniform mat3 uMat3;\n"
      "uniform mat4 uMat4;\n";

  // One of each uniform type.
  RendererPtr renderer(new Renderer(gm_));

  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  root->ClearUniforms();
  root->ClearUniformBlocks();
  UniformBlockPtr block1(new UniformBlock);
  UniformBlockPtr block2(new UniformBlock);

  PopulateUniformValues(data_->rect, block1, block2, reg, 0);
  data_->rect->AddUniformBlock(block1);
  data_->rect->AddUniformBlock(block2);

  ShaderProgramPtr program(new ShaderProgram(reg));
  program->SetLabel("Dummy Shader");
  program->SetVertexShader(ShaderPtr(new Shader(kVertexShaderString)));
  program->SetFragmentShader(
      ShaderPtr(new Shader("Dummy Fragment Shader Source")));
  data_->rect->SetShaderProgram(program);
  // Remove attribute array to prevent warnings; we are only testing uniforms
  // here.
  data_->shape->SetAttributeArray(AttributeArrayPtr(nullptr));

  {
    // Verify that the uniforms were sent only once, since there is only one
    // node.
    Reset();
    renderer->DrawScene(root);
    SCOPED_TRACE(__LINE__);
    VerifyUniformCounts(1U, trace_verifier_);
  }

  // Add another identical node with the same shape and uniforms. Since the
  // uniform values are the same no additional data should be sent to GL.
  NodePtr node(new Node);
  data_->rect->AddChild(node);
  node->AddShape(data_->shape);
  PopulateUniformValues(node, block1, block2, reg, 0);
  // Add the same uniform blocks.
  node->AddUniformBlock(block1);
  node->AddUniformBlock(block2);
  {
    Reset();
    renderer->DrawScene(root);
    SCOPED_TRACE(__LINE__);
    VerifyUniformCounts(0U, trace_verifier_);
  }

  // Use the same uniforms but with different values.
  PopulateUniformValues(node, block1, block2, reg, 1);
  {
    Reset();
    renderer->DrawScene(root);
    SCOPED_TRACE(__LINE__);
    // Verify that the uniforms were sent. Each should be sent once, when the
    // child node is processed, since the initial values were cached already.
    VerifyUniformCounts(1U, trace_verifier_);
  }

  // Set the same shader in the child node.
  node->SetShaderProgram(program);
  {
    Reset();
    renderer->DrawScene(root);
    SCOPED_TRACE(__LINE__);
    // The uniforms should have been sent twice since the child nodes blew away
    // the cached values in the last pass; both values will have to be sent this
    // time.
    VerifyUniformCounts(2U, trace_verifier_);
  }

  // Use a different shader for the child node.
  ShaderProgramPtr program2(new ShaderProgram(reg));
  program2->SetLabel("Dummy Shader");
  program2->SetVertexShader(ShaderPtr(new Shader(kVertexShaderString)));
  program2->SetFragmentShader(
      ShaderPtr(new Shader("Dummy Fragment Shader Source")));
  node->SetShaderProgram(program2);
  {
    Reset();
    renderer->DrawScene(root);
    SCOPED_TRACE(__LINE__);
    // Uniforms will have to be sent twice, once for the first program using
    // the first set of values, and then again for the second shader.
    VerifyUniformCounts(2U, trace_verifier_);
  }

  {
    Reset();
    renderer->DrawScene(root);
    SCOPED_TRACE(__LINE__);
    // Now that both caches are populated no uniforms should be sent.
    VerifyUniformCounts(0U, trace_verifier_);
  }

  // Reset.
  data_->rect = nullptr;
  data_->shape->SetAttributeArray(data_->attribute_array);
  BuildRectangle(data_, options_);
}

TEST_F(RendererTest, SetTextureImageUnitRange) {
  // Test that all textures that share the same uniform are bound to the same
  // texture unit.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  data_->rect->ClearChildren();
  data_->rect->ClearUniforms();
  data_->rect->ClearShapes();

  data_->rect->AddUniform(data_->shader->GetRegistry()->Create<Uniform>(
      "uProjectionMatrix", math::Matrix4f::Identity()));
  data_->rect->AddUniform(data_->shader->GetRegistry()->Create<Uniform>(
      "uCubeMapTexture", data_->cubemap));

  // Add many nodes with different textures bound to different uniforms which
  // try to get their own image units, if there are enough.
  static const int kNumNodes = 4;
  for (int i = 0; i < kNumNodes; ++i) {
    NodePtr node(new Node);

    TexturePtr texture(new Texture);
    texture->SetLabel("Texture_a " + base::ValueToString(i));
    texture->SetImage(0U, data_->image);
    texture->SetSampler(data_->sampler);
    node->AddUniform(data_->shader->GetRegistry()->Create<Uniform>(
        "uTexture", texture));

    texture = new Texture;
    texture->SetLabel("Texture_b " + base::ValueToString(i));
    texture->SetImage(0U, data_->image);
    texture->SetSampler(data_->sampler);
    node->AddUniform(
        data_->shader->GetRegistry()->Create<Uniform>("uTexture2", texture));

    data_->rect->AddChild(node);
  }
  Reset();

  // Add shapes to force GL calls.
  for (int i = 0; i < kNumNodes; ++i)
    data_->rect->GetChildren()[i]->AddShape(data_->shape);

  // Use two texture units.
  Reset();
  renderer->SetTextureImageUnitRange(Range1i(0, 1));
  renderer->DrawScene(root);
  EXPECT_EQ(14U, trace_verifier_->GetCountOf("TexImage2D"));
  // Image unit allocation at shader program bind time:
  // uTexture -> 0, uTexture2 -> 1, uCubeMapTexture -> 0 (LRU reuses unit 0)
  // Binding forced by first shape encountered:
  // child0: Texture_a0 -> 0, Texture_b0 -> 1, cubemap -> 0
  // Subsequent binding:
  // child1: Texture_a1 (-> 0, already active), Texture_b1 -> 1
  // child2: Texture_a2 -> 0, Texture_b2 -> 1
  // child3: Texture_a3 -> 0, Texture_b3 -> 1
  // Total actives: 8, unit 0: 4, unit 1: 4

  EXPECT_EQ(8U, trace_verifier_->GetCountOf("ActiveTexture"));
  // Texture uniform is sent when it changes image units:
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("Uniform1i"));
  EXPECT_EQ(4U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE0)"));
  EXPECT_EQ(4U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE1)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE2)"));

  // Use one texture unit.
  Reset();
  renderer->SetTextureImageUnitRange(Range1i(0, 0));
  renderer->DrawScene(root);
  // The textures are already updated.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  // One call to ActiveTexture since 1 is active from previous DrawScene.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ActiveTexture"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE0)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE1)"));
  // uCubemapTexture and uTexture2 are already mapped to unit 0, just need
  // to map uTexture to unit 0 as well.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Uniform1i"));

  // Use three texture units.
  Reset();
  renderer->SetTextureImageUnitRange(Range1i(3, 5));
  renderer->DrawScene(root);
  // The textures are already updated.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  // There should be 9 calls to ActiveTexture since we ping pong back and forth
  // between the 3 units for 3 textures. The cubemap gets a single unit and
  // reuses it, while the other textures each requre rebinding.
  EXPECT_EQ(9U, trace_verifier_->GetCountOf("ActiveTexture"));
  // The texture uniforms are only sent once since we have exactly the right
  // number of units.
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("Uniform1i"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE0)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE1)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE2)"));
  EXPECT_EQ(4U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE3)"));
  EXPECT_EQ(4U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE4)"));
  // This is used for the cubemap.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE5)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE6)"));

  // Reset data.
  data_->rect = nullptr;
  BuildRectangle(data_, options_);
}

TEST_F(RendererTest, ArrayUniforms) {
  // Add array uniform types to a node and make sure the right functions are
  // called in the renderer.
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  reg->IncludeGlobalRegistry();

  // Dummy shader with the uniforms defined.
  static const char* kVertexShaderString =
      "uniform int uInt;\n"
      "uniform float uFloat;\n"
      "uniform vec2 uFV2;\n"
      "uniform vec3 uFV3;\n"
      "uniform vec4 uFV4;\n"
      "uniform ivec2 uIV2;\n"
      "uniform ivec3 uIV3;\n"
      "uniform ivec4 uIV4;\n"
      "uniform mat2 uMat2;\n"
      "uniform mat3 uMat3;\n"
      "uniform mat4 uMat4;\n"
      "uniform sampler2D sampler;\n"
      "uniform samplerCube cubeSampler;\n"
      "uniform int uIntArray[2];\n"
      "uniform float uFloatArray[2];\n"
      "uniform vec2 uFV2Array[2];\n"
      "uniform vec3 uFV3Array[3];\n"
      "uniform vec4 uFV4Array[4];\n"
      "uniform ivec2 uIV2Array[2];\n"
      "uniform ivec3 uIV3Array[3];\n"
      "uniform ivec4 uIV4Array[4];\n"
      "uniform mat2 uMat2Array[2];\n"
      "uniform mat3 uMat3Array[3];\n"
      "uniform mat4 uMat4Array[4];\n"
      "uniform sampler2D samplerArray[2];\n"
      "uniform samplerCube cubeSamplerArray[2];\n";

  // One of each uniform type.
  RendererPtr renderer(new Renderer(gm_));

  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  root->ClearUniforms();
  root->ClearUniformBlocks();
  data_->rect->ClearUniforms();
  data_->rect->ClearUniformBlocks();

  // add all the uniforms here
  ShaderProgramPtr program(new ShaderProgram(reg));
  program->SetLabel("Dummy Shader");
  program->SetVertexShader(ShaderPtr(new Shader(kVertexShaderString)));
  program->SetFragmentShader(
      ShaderPtr(new Shader("Dummy Fragment Shader Source")));
  data_->rect->SetShaderProgram(program);
  // Remove attribute array to prevent warnings; we are only testing uniforms
  // here.
  data_->shape->SetAttributeArray(AttributeArrayPtr(nullptr));

  root->AddUniform(reg->Create<Uniform>("uInt", 13));
  root->AddUniform(reg->Create<Uniform>("uFloat", 1.5f));
  root->AddUniform(
      reg->Create<Uniform>("uFV2", math::Vector2f(2.f, 3.f)));
  root->AddUniform(
      reg->Create<Uniform>("uFV3", math::Vector3f(4.f, 5.f, 6.f)));
  root->AddUniform(reg->Create<Uniform>(
      "uFV4", math::Vector4f(7.f, 8.f, 9.f, 10.f)));
  root->AddUniform(reg->Create<Uniform>("uIV2",
                                        math::Vector2i(2, 3)));
  root->AddUniform(reg->Create<Uniform>("uIV3",
                                        math::Vector3i(4, 5, 6)));
  root->AddUniform(reg->Create<Uniform>("uIV4",
                                        math::Vector4i(7, 8, 9, 10)));
  root->AddUniform(reg->Create<Uniform>("uMat2",
                                        math::Matrix2f(1.f, 2.f,
                                                       3.f, 4.f)));
  root->AddUniform(reg->Create<Uniform>("uMat3",
                                        math::Matrix3f(1.f, 2.f, 3.f,
                                                       4.f, 5.f, 6.f,
                                                       7.f, 8.f, 9.f)));
  root->AddUniform(reg->Create<Uniform>("uMat4",
                                        math::Matrix4f(1.f, 2.f, 3.f, 4.f,
                                                       5.f, 6.f, 7.f, 8.f,
                                                       9.f, 1.f, 2.f, 3.f,
                                                       4.f, 5.f, 6.f, 7.f)));
  root->AddUniform(reg->Create<Uniform>("sampler", data_->texture));
  root->AddUniform(reg->Create<Uniform>("cubeSampler", data_->cubemap));

  TexturePtr texture1(new Texture);
  texture1->SetImage(0U, data_->image);
  texture1->SetSampler(data_->sampler);
  TexturePtr texture2(new Texture);
  texture2->SetImage(0U, data_->image);
  texture2->SetSampler(data_->sampler);
  CubeMapTexturePtr cubemap1(new CubeMapTexture);
  cubemap1->SetSampler(data_->sampler);
  CubeMapTexturePtr cubemap2(new CubeMapTexture);
  cubemap2->SetSampler(data_->sampler);  for (int i = 0; i < 6; ++i) {
    cubemap1->SetImage(static_cast<CubeMapTexture::CubeFace>(i), 0U,
                       data_->image);
    cubemap2->SetImage(static_cast<CubeMapTexture::CubeFace>(i), 0U,
                       data_->image);
  }

  std::vector<int> ints;
  ints.push_back(1);
  ints.push_back(2);
  std::vector<float> floats;
  floats.push_back(1.f);
  floats.push_back(2.f);
  std::vector<TexturePtr> textures;
  textures.push_back(texture1);
  textures.push_back(texture2);
  std::vector<CubeMapTexturePtr> cubemaps;
  cubemaps.push_back(cubemap1);
  cubemaps.push_back(cubemap2);
  std::vector<math::Vector2i> vector2is;
  vector2is.push_back(math::Vector2i(1, 2));
  vector2is.push_back(math::Vector2i(3, 4));
  std::vector<math::Vector3i> vector3is;
  vector3is.push_back(math::Vector3i(1, 2, 3));
  vector3is.push_back(math::Vector3i(4, 5, 6));
  std::vector<math::Vector4i> vector4is;
  vector4is.push_back(math::Vector4i(1, 2, 3, 4));
  vector4is.push_back(math::Vector4i(5, 6, 7, 8));
  std::vector<math::Vector2f> vector2fs;
  vector2fs.push_back(math::Vector2f(1.f, 2.f));
  vector2fs.push_back(math::Vector2f(3.f, 4.f));
  std::vector<math::Vector3f> vector3fs;
  vector3fs.push_back(math::Vector3f(1.f, 2.f, 3.f));
  vector3fs.push_back(math::Vector3f(4.f, 5.f, 6.f));
  std::vector<math::Vector4f> vector4fs;
  vector4fs.push_back(math::Vector4f(1.f, 2.f, 3.f, 4.f));
  vector4fs.push_back(math::Vector4f(5.f, 6.f, 7.f, 8.f));
  std::vector<math::Matrix2f> matrix2fs;
  matrix2fs.push_back(math::Matrix2f::Identity());
  matrix2fs.push_back(math::Matrix2f::Identity());
  std::vector<math::Matrix3f> matrix3fs;
  matrix3fs.push_back(math::Matrix3f::Identity());
  matrix3fs.push_back(math::Matrix3f::Identity());
  std::vector<math::Matrix4f> matrix4fs;
  matrix4fs.push_back(math::Matrix4f::Identity());
  matrix4fs.push_back(math::Matrix4f::Identity());

  root->AddUniform(CreateArrayUniform(reg, "uIntArray", ints));
  root->AddUniform(CreateArrayUniform(reg, "uFloatArray", floats));
  root->AddUniform(CreateArrayUniform(reg, "uIV2Array", vector2is));
  root->AddUniform(CreateArrayUniform(reg, "uIV3Array", vector3is));
  root->AddUniform(CreateArrayUniform(reg, "uIV4Array", vector4is));
  root->AddUniform(CreateArrayUniform(reg, "uFV2Array", vector2fs));
  root->AddUniform(CreateArrayUniform(reg, "uFV3Array", vector3fs));
  root->AddUniform(CreateArrayUniform(reg, "uFV4Array", vector4fs));
  root->AddUniform(CreateArrayUniform(reg, "uMat2Array", matrix2fs));
  root->AddUniform(CreateArrayUniform(reg, "uMat3Array", matrix3fs));
  root->AddUniform(CreateArrayUniform(reg, "uMat4Array", matrix4fs));
  root->AddUniform(CreateArrayUniform(reg, "samplerArray", textures));
  root->AddUniform(CreateArrayUniform(reg, "cubeSamplerArray", cubemaps));

  Reset();
  renderer->DrawScene(root);
  // Verify all the uniform types were sent.
  // 1i.
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("Uniform1i("));
  EXPECT_EQ(4U, trace_verifier_->GetCountOf("Uniform1iv("));
  EXPECT_TRUE(
      trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                        0U, "Uniform1i(")).HasArg(2, "0"));
  EXPECT_TRUE(
      trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                        1U, "Uniform1i(")).HasArg(2, "1"));

  // The int uniform.
  EXPECT_TRUE(
      trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                        0U, "Uniform1iv(")).HasArg(2, "1"));
  // The int array uniform.
  EXPECT_TRUE(
      trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                        1U, "Uniform1iv(")).HasArg(2, "2"));
  // The texture array uniform.
  EXPECT_TRUE(
      trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                        2U, "Uniform1iv(")).HasArg(2, "2"));
  // The cube map array uniform.
  EXPECT_TRUE(
      trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                        3U, "Uniform1iv(")).HasArg(2, "2"));
  for (int i = 2; i < 4; ++i) {
    const std::string f_name =
        std::string("Uniform") + base::ValueToString(i) + "f";
    const std::string i_name =
        std::string("Uniform") + base::ValueToString(i) + "i";
    const std::string mat_name =
        std::string("UniformMatrix") + base::ValueToString(i) + "fv";
    EXPECT_EQ(0U, trace_verifier_->GetCountOf(f_name + "("));
    EXPECT_EQ(2U, trace_verifier_->GetCountOf(f_name + "v("));
    EXPECT_TRUE(
        trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                          0U, f_name + "v(")).HasArg(2, "1"));
    EXPECT_TRUE(
        trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                          1U, f_name + "v(")).HasArg(2, "2"));

    EXPECT_EQ(0U, trace_verifier_->GetCountOf(i_name + "("));
    EXPECT_EQ(2U, trace_verifier_->GetCountOf(i_name + "v("));
    EXPECT_TRUE(
        trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                          0U, i_name + "v(")).HasArg(2, "1"));
    EXPECT_TRUE(
        trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                          1U, i_name + "v(")).HasArg(2, "2"));

    EXPECT_EQ(2U, trace_verifier_->GetCountOf(mat_name));
    EXPECT_TRUE(
        trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                          0U, mat_name)).HasArg(2, "1"));
    EXPECT_TRUE(
        trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                          1U, mat_name)).HasArg(2, "2"));
  }

  Reset();
  renderer->DrawScene(root);
  // Everything should be cached.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Uniform"));

  // Ensure that the array textures are evicted.
  root->SetShaderProgram(program);
  // Remove attribute array to prevent warnings; we are only testing uniforms
  // here.
  root->AddShape(data_->shape);

  // These uniforms are the same as those contained by root.
  data_->rect->AddUniform(reg->Create<Uniform>("uInt", 13));
  data_->rect->AddUniform(reg->Create<Uniform>("uFloat", 1.5f));
  data_->rect->AddUniform(
      reg->Create<Uniform>("uFV2", math::Vector2f(2.f, 3.f)));
  data_->rect->AddUniform(
      reg->Create<Uniform>("uFV3", math::Vector3f(4.f, 5.f, 6.f)));
  data_->rect->AddUniform(reg->Create<Uniform>(
      "uFV4", math::Vector4f(7.f, 8.f, 9.f, 10.f)));
  data_->rect->AddUniform(reg->Create<Uniform>("uIV2",
                                        math::Vector2i(2, 3)));
  data_->rect->AddUniform(reg->Create<Uniform>("uIV3",
                                        math::Vector3i(4, 5, 6)));
  data_->rect->AddUniform(reg->Create<Uniform>("uIV4",
                                        math::Vector4i(7, 8, 9, 10)));
  data_->rect->AddUniform(reg->Create<Uniform>("uMat2",
                                        math::Matrix2f(1.f, 2.f,
                                                       3.f, 4.f)));
  data_->rect->AddUniform(reg->Create<Uniform>("uMat3",
                                        math::Matrix3f(1.f, 2.f, 3.f,
                                                       4.f, 5.f, 6.f,
                                                       7.f, 8.f, 9.f)));
  data_->rect->AddUniform(reg->Create<Uniform>("uMat4",
                                        math::Matrix4f(1.f, 2.f, 3.f, 4.f,
                                                       5.f, 6.f, 7.f, 8.f,
                                                       9.f, 1.f, 2.f, 3.f,
                                                       4.f, 5.f, 6.f, 7.f)));
  data_->rect->AddUniform(reg->Create<Uniform>("sampler", data_->texture));
  data_->rect->AddUniform(reg->Create<Uniform>("cubeSampler", data_->cubemap));

  // Reverse following uniform arrays so they are different than those in root.
  std::reverse(ints.begin(), ints.end());
  std::reverse(floats.begin(), floats.end());
  std::reverse(textures.begin(), textures.end());
  std::reverse(cubemaps.begin(), cubemaps.end());
  std::reverse(vector2fs.begin(), vector2fs.end());
  std::reverse(vector3fs.begin(), vector3fs.end());
  std::reverse(vector4fs.begin(), vector4fs.end());
  std::reverse(vector2is.begin(), vector2is.end());
  std::reverse(vector3is.begin(), vector3is.end());
  std::reverse(vector4is.begin(), vector4is.end());
  std::reverse(matrix2fs.begin(), matrix2fs.end());
  std::reverse(matrix3fs.begin(), matrix3fs.end());
  std::reverse(matrix4fs.begin(), matrix4fs.end());
  data_->rect->AddUniform(CreateArrayUniform(reg, "uIntArray", ints));
  data_->rect->AddUniform(CreateArrayUniform(reg, "uFloatArray", floats));
  data_->rect->AddUniform(CreateArrayUniform(reg, "uIV2Array", vector2is));
  data_->rect->AddUniform(CreateArrayUniform(reg, "uIV3Array", vector3is));
  data_->rect->AddUniform(CreateArrayUniform(reg, "uIV4Array", vector4is));
  data_->rect->AddUniform(CreateArrayUniform(reg, "uFV2Array", vector2fs));
  data_->rect->AddUniform(CreateArrayUniform(reg, "uFV3Array", vector3fs));
  data_->rect->AddUniform(CreateArrayUniform(reg, "uFV4Array", vector4fs));
  data_->rect->AddUniform(CreateArrayUniform(reg, "uMat2Array", matrix2fs));
  data_->rect->AddUniform(CreateArrayUniform(reg, "uMat3Array", matrix3fs));
  data_->rect->AddUniform(CreateArrayUniform(reg, "uMat4Array", matrix4fs));
  data_->rect->AddUniform(CreateArrayUniform(reg, "samplerArray", textures));
  data_->rect->AddUniform(
      CreateArrayUniform(reg, "cubeSamplerArray", cubemaps));

  Reset();
  renderer->DrawScene(root);
  // Expect all non-texture uniforms to be sent since now data_->rect uniforms
  // replace those of root.
  EXPECT_EQ(22U, trace_verifier_->GetCountOf("Uniform"));
}

TEST_F(RendererTest, VertexArraysAndEmulator) {
  // Test that vertex arrays are enabled and used. Each test needs a fresh
  // renderer so that resources are initialized from scratch, otherwise
  // a VertexArrayEmulatorResource will not be created, since the resource
  // holder will already have a pointer to a VertexArrayResource.
  NodePtr root;

  root = BuildGraph(data_, options_, kWidth, kHeight);
  {
    RendererPtr renderer(new Renderer(gm_));
    Reset();
    renderer->DrawScene(root);
    // Vertex arrays should be bound. There is only one bind.
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindVertexArray"));
  }

  // Use the emulator.
  gm_->EnableFeature(GraphicsManager::kVertexArrays, false);
  EXPECT_FALSE(gm_->IsFeatureAvailable(GraphicsManager::kVertexArrays));
  {
    RendererPtr renderer(new Renderer(gm_));
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindVertexArray"));
    EXPECT_GT(trace_verifier_->GetCountOf("VertexAttribPointer"), 0U);
  }

  // Use vertex arrays.
  gm_->EnableFeature(GraphicsManager::kVertexArrays, true);
  {
    RendererPtr renderer(new Renderer(gm_));
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindVertexArray"));
    // We should not have to rebind the pointers.
    EXPECT_GT(trace_verifier_->GetCountOf("VertexAttribPointer"), 0U);
  }
}

TEST_F(RendererTest, VertexArrayEmulatorReuse) {
  // Test that when reusing the vertex array emulator, the bind calls are only
  // sent to OpenGL once.
  gm_->EnableFeature(GraphicsManager::kVertexArrays, false);

  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  RendererPtr renderer(new Renderer(gm_));
  Reset();
  renderer->DrawScene(root);
  // Vertex arrays are disabled.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindVertexArray"));
  // There are two buffer attributes bound, 1 index buffer, and 1 data buffer.
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("BindBuffer"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("VertexAttribPointer"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("EnableVertexAttribArray"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DisableVertexAttribArray"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawElements"));

  // Drawing again should only draw the shape again, without rebinding or
  // enabling the pointers again.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindBuffer"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttribPointer"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("EnableVertexAttribArray"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DisableVertexAttribArray"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawElements"));

  // If the same Shape is used in succession, we also shouldn't see rebinds
  // happen.
  NodePtr node(new Node);
  data_->rect->AddChild(node);
  node->AddShape(data_->shape);
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindBuffer"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttribPointer"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("EnableVertexAttribArray"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DisableVertexAttribArray"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("DrawElements"));

  // If we modify an attribute then the entire state will be resent due to the
  // notification, with the exception of attribute enable/disable states.
  Attribute* a = data_->shape->GetAttributeArray()->GetMutableAttribute(0U);
  a->SetFixedPointNormalized(true);
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindBuffer"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("VertexAttribPointer"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("EnableVertexAttribArray"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DisableVertexAttribArray"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("DrawElements"));
  data_->rect->RemoveChild(node);

  // If a different Shape is used, then the new one will be sent on the first
  // draw.
  ShaderInputRegistryPtr global_reg = ShaderInputRegistry::GetGlobalRegistry();
  AttributeArrayPtr aa(new AttributeArray);
  aa->AddAttribute(global_reg->Create<Attribute>(
      "aVertex", BufferObjectElement(data_->vertex_buffer,
                                     data_->vertex_buffer->AddSpec(
                                         BufferObject::kFloat, 3, 0))));
  aa->AddAttribute(global_reg->Create<Attribute>(
      "aTexCoords",
      BufferObjectElement(data_->vertex_buffer,
                          data_->vertex_buffer->AddSpec(BufferObject::kFloat, 2,
                                                        sizeof(float) * 3))));
  aa->AddAttribute(global_reg->Create<Attribute>(
      "aDummyCoords",
      BufferObjectElement(data_->vertex_buffer,
                          data_->vertex_buffer->AddSpec(BufferObject::kFloat, 2,
                                                        sizeof(float) * 5))));
  ShapePtr shape(new Shape);
  shape->SetAttributeArray(aa);
  data_->rect->AddShape(shape);
  std::string three_attrib_string =
      std::string("attribute vec2 aDummyCoords;\n") + kPlaneVertexShaderString;
  data_->rect->SetShaderProgram(
      ShaderProgram::BuildFromStrings("Additional dummy attribute shader",
          global_reg, three_attrib_string, kPlaneFragmentShaderString,
          base::AllocatorPtr()));

  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindBuffer"));
  EXPECT_EQ(5U, trace_verifier_->GetCountOf("VertexAttribPointer"));
  // Prepending the extra dummy attribute pushes the 2 attributes present in
  // the first shape one slot down, so the first shape disables index 0 and
  // enables index 2, then the second shape re-enables index 0.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DisableVertexAttribArray"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("EnableVertexAttribArray"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawElements"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawArrays"));

  // Drawing again should rebind both Shapes.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindBuffer"));
  EXPECT_EQ(5U, trace_verifier_->GetCountOf("VertexAttribPointer"));
  // First shape will disable index 0 and the second one will re-enable it.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DisableVertexAttribArray"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("EnableVertexAttribArray"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawElements"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawArrays"));

  data_->rect->RemoveShape(shape);
  gm_->EnableFeature(GraphicsManager::kVertexArrays, true);
}

TEST_F(RendererTest, VertexBufferUsage) {
  // Test vertex buffer usage.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root;
  TracingHelper helper;

  root = BuildGraph(data_, options_, kWidth, kHeight);
  VerifyRenderData<BufferObject::UsageMode> verify_data;
  verify_data.update_func =
      std::bind(BuildRectangleBufferObject, data_, options_);
  verify_data.call_name = "BufferData";
  verify_data.option = &options_->vertex_buffer_usage;
  verify_data.static_args.push_back(StaticArg(1, "GL_ARRAY_BUFFER"));
  verify_data.static_args.push_back(
      StaticArg(2, helper.ToString(
          "GLsizei", static_cast<int>(sizeof(Vertex) * s_num_vertices))));
  verify_data.static_args.push_back(
      StaticArg(3, helper.ToString("void*",
                                   data_->vertex_container->GetData())));
  verify_data.varying_arg_index = 4U;
  verify_data.arg_tests.push_back(
      VaryingArg<BufferObject::UsageMode>(
          0, BufferObject::kDynamicDraw, "GL_DYNAMIC_DRAW"));
  verify_data.arg_tests.push_back(
      VaryingArg<BufferObject::UsageMode>(
          0, BufferObject::kStaticDraw, "GL_STATIC_DRAW"));
  verify_data.arg_tests.push_back(
      VaryingArg<BufferObject::UsageMode>(
          0, BufferObject::kStreamDraw, "GL_STREAM_DRAW"));
  EXPECT_TRUE(VerifyRenderCalls(verify_data, trace_verifier_, renderer, root));

  Reset();
  renderer = nullptr;
  EXPECT_TRUE(VerifyReleases(1));
}

TEST_F(RendererTest, VertexBufferNoData) {
  // Test handling of nullptr, nonexistent, or empty buffer object
  // data containers.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  base::LogChecker log_checker;

  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  data_->vertex_buffer->SetData(base::DataContainerPtr(nullptr), sizeof(Vertex),
                                s_num_vertices, options_->vertex_buffer_usage);
  Reset();
  renderer->DrawScene(root);
  // Buffer is already bound.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  EXPECT_FALSE(log_checker.HasMessage("WARNING", "DataContainer is NULL"));

  Vertex* vertices = new Vertex[2];
  vertices[0].point_coords.Set(-1, 0,  1);
  vertices[0].tex_coords.Set(0.f, 1.f);
  vertices[1].point_coords.Set(1, 0, 1);
  vertices[1].tex_coords.Set(1.f, 1.f);
  base::DataContainerPtr data =
      base::DataContainer::Create<Vertex>(
          vertices, base::DataContainer::ArrayDeleter<Vertex>, true,
          data_->vertex_buffer->GetAllocator());
  data_->vertex_buffer->SetData(data, 0U, s_num_vertices,
                                options_->vertex_buffer_usage);
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "struct size is 0"));

  data_->vertex_buffer->SetData(data, sizeof(vertices[0]), 0U,
                                options_->vertex_buffer_usage);
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "struct count is 0"));

  // Reset data.
  data_->rect = nullptr;
  BuildRectangle(data_, options_);
}

TEST_F(RendererTest, VertexBufferSubData) {
  // Test handling of BufferObject sub-data.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);

  Reset();
  renderer->DrawScene(root);
  DCHECK_EQ(static_cast<GLenum>(GL_NO_ERROR), gm_->GetError());
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));

  Vertex* vertices = new Vertex[2];
  vertices[0].point_coords.Set(-1, 0,  1);
  vertices[0].tex_coords.Set(0.f, 1.f);
  vertices[1].point_coords.Set(1, 0, 1);
  vertices[1].tex_coords.Set(1.f, 1.f);
  base::DataContainerPtr sub_data =
      base::DataContainer::Create<Vertex>(
          vertices, base::DataContainer::ArrayDeleter<Vertex>, true,
          data_->vertex_buffer->GetAllocator());

  int vert_size = static_cast<uint32>(sizeof(Vertex));
  data_->vertex_buffer->SetSubData(math::Range1ui(0, vert_size * 2), sub_data);
  Reset();
  renderer->DrawScene(root);
  // Buffer sub-data does not affect memory usage.
  EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U + kVboSize, 0U, 28672U));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferSubData(GL_ARRAY_BUFFER"));

  // Reset data.
  data_->rect = nullptr;
  BuildRectangle(data_, options_);
}

TEST_F(RendererTest, VertexBufferCopySubData) {
  // Test handling of BufferObject sub-data.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);

  Reset();
  renderer->DrawScene(root);
  DCHECK_EQ(static_cast<GLenum>(GL_NO_ERROR), gm_->GetError());
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));

  // Copy second vertex into first vertex.
  uint32 vert_size = static_cast<uint32>(sizeof(Vertex));
  data_->vertex_buffer->CopySubData(
      data_->vertex_buffer, math::Range1ui(0, vert_size), vert_size);
  Reset();
  renderer->DrawScene(root);
  // Buffer sub-data does not affect memory usage.
  EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U + kVboSize, 0U, 28672U));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf(
      "CopyBufferSubData(GL_ARRAY_BUFFER, GL_ARRAY_BUFFER"));

  // Copy between BufferObjects.
  math::Range1ui range(0, static_cast<uint32>(sizeof(float)));
  data_->vertex_buffer->CopySubData(data_->index_buffer, range, 0);
  Reset();
  renderer->DrawScene(root);
  // Buffer sub-data does not affect memory usage.
  EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U + kVboSize, 0U, 28672U));
  const std::string str = trace_verifier_->GetTraceString();
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_COPY_READ_BUFFER"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_COPY_WRITE_BUFFER"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf(
      "CopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER"));

  // Reset data.
  data_->rect = nullptr;
  BuildRectangle(data_, options_);
}

TEST_F(RendererTest, VertexBufferCopySubDataEmulation) {
  // Test emulation of glCopyBufferSubData.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);

  // Disable all, expect to make copies through unwiped DataContainers.
  gm_->EnableFeature(GraphicsManager::kCopyBufferSubData, false);
  gm_->EnableFeature(GraphicsManager::kMapBuffer, false);
  gm_->EnableFeature(GraphicsManager::kMapBufferRange, false);

  Reset();
  renderer->DrawScene(root);
  DCHECK_EQ(static_cast<GLenum>(GL_NO_ERROR), gm_->GetError());
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));

  // Copy with a single BufferObject, copy third vertex into first and then
  // second into third. Expect to use unwiped DataContainers for copy.
  const Vertex before1 = data_->vertex_buffer->GetData()->GetData<Vertex>()[1];
  const Vertex before2 = data_->vertex_buffer->GetData()->GetData<Vertex>()[2];
  int vert_size = static_cast<uint32>(sizeof(Vertex));
  data_->vertex_buffer->CopySubData(
      data_->vertex_buffer, math::Range1ui(0, vert_size), vert_size * 2);
  data_->vertex_buffer->CopySubData(
      data_->vertex_buffer,
      math::Range1ui(2 * vert_size, 3 * vert_size), vert_size);
  Reset();
  renderer->DrawScene(root);
  const Vertex after0 = data_->vertex_buffer->GetData()->GetData<Vertex>()[0];
  const Vertex after1 = data_->vertex_buffer->GetData()->GetData<Vertex>()[1];
  const Vertex after2 = data_->vertex_buffer->GetData()->GetData<Vertex>()[2];
  EXPECT_TRUE(after0 == before2);
  EXPECT_TRUE(after1 == before1);
  EXPECT_TRUE(after2 == before1);

  // Copy between BufferObjects.
  float* v = data_->index_buffer->GetData()->GetMutableData<float>();
  v[0] = 3.14159f;
  v[1] = 2.7182f;
  data_->index_buffer->SetData(data_->index_buffer->GetData(),
                               data_->index_buffer->GetStructSize(),
                               data_->index_buffer->GetCount(),
                               BufferObject::kDynamicDraw);
  uint32 float_size = static_cast<uint32>(sizeof(float));
  data_->vertex_buffer->CopySubData(
      data_->index_buffer, math::Range1ui(0, float_size), 0U);
  data_->vertex_buffer->CopySubData(
      data_->index_buffer, math::Range1ui(2 * float_size, 3 * float_size),
      float_size);
  Reset();
  renderer->DrawScene(root);
  const Vertex after = data_->vertex_buffer->GetData()->GetData<Vertex>()[0];
  EXPECT_EQ(v[0], after.point_coords[0]);
  EXPECT_EQ(v[1], after.point_coords[2]);

  // Enable MapBuffer.
  gm_->EnableFeature(GraphicsManager::kCopyBufferSubData, false);
  gm_->EnableFeature(GraphicsManager::kMapBuffer, true);
  gm_->EnableFeature(GraphicsManager::kMapBufferRange, true);

  // Copy first into second, then second into third.
  const Vertex before0 = data_->vertex_buffer->GetData()->GetData<Vertex>()[0];
  data_->vertex_buffer->CopySubData(
      data_->vertex_buffer, math::Range1ui(vert_size, 2 * vert_size), 0);
  data_->vertex_buffer->CopySubData(
      data_->vertex_buffer,
      math::Range1ui(2 * vert_size, 3 * vert_size), vert_size);
  Reset();
  renderer->DrawScene(root);
  // Expect to use Map/UnmapBuffer to extract bytes for the copy.
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("MapBuffer"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("UnmapBuffer"));
  renderer->MapBufferObjectData(data_->vertex_buffer, Renderer::kReadOnly);
  const Vertex* after_verts = static_cast<const Vertex*>(
      data_->vertex_buffer->GetMappedPointer());
  EXPECT_EQ(before0, after_verts[0]);
  EXPECT_EQ(before0, after_verts[1]);
  EXPECT_EQ(before0, after_verts[2]);
  renderer->UnmapBufferObjectData(data_->vertex_buffer);

  // Disable all, expect to make copies through allocated buffers.
  gm_->EnableFeature(GraphicsManager::kCopyBufferSubData, false);
  gm_->EnableFeature(GraphicsManager::kMapBuffer, false);
  gm_->EnableFeature(GraphicsManager::kMapBufferRange, false);

  // Copy between BufferObjects.
  // NULL vertex_buffer's data so it uses allocated memory to effect the copy.
  data_->vertex_buffer->SetData(base::DataContainerPtr(),
                                data_->vertex_buffer->GetStructSize(),
                                data_->vertex_buffer->GetCount(),
                                BufferObject::kDynamicDraw);
  data_->vertex_buffer->CopySubData(
      data_->index_buffer, math::Range1ui(0, float_size), 0U);
  data_->vertex_buffer->CopySubData(
      data_->index_buffer, math::Range1ui(2 * float_size, 3 * float_size),
      float_size);
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("BufferSubData(GL_ARRAY_BUFFER"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("MapBuffer"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("UnmapBuffer"));

  // Reset data.
  data_->rect = nullptr;
  BuildRectangle(data_, options_);
}

TEST_F(RendererTest, IndexBufferUsage) {
  // Test index buffer usage.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root;
  TracingHelper helper;

  root = BuildGraph(data_, options_, kWidth, kHeight);
  VerifyRenderData<BufferObject::UsageMode> verify_data;
  verify_data.update_func =
      std::bind(BuildRectangleShape<uint16>, data_, options_);
  verify_data.call_name = "BufferData";
  verify_data.option = &options_->index_buffer_usage;
  verify_data.static_args = {
      {1, "GL_ELEMENT_ARRAY_BUFFER"},
      {2, helper.ToString("GLsizei",
                          static_cast<int>(sizeof(uint16) * s_num_indices))},
      {3, helper.ToString("void*", data_->index_container->GetData())}
  };
  verify_data.varying_arg_index = 4U;
  // It's the second call in this case because the vertex buffer is bound first
  // since this is the initial draw.
  verify_data.arg_tests = {
      {0, BufferObject::kDynamicDraw, "GL_DYNAMIC_DRAW"},
      {0, BufferObject::kStaticDraw, "GL_STATIC_DRAW"},
      {0, BufferObject::kStreamDraw, "GL_STREAM_DRAW"}
  };
  EXPECT_TRUE(VerifyRenderCalls(verify_data, trace_verifier_, renderer, root));

  Reset();
  renderer = nullptr;
  EXPECT_TRUE(VerifyReleases(1));
}

TEST_F(RendererTest, ProgramAndShaderInfoLogs) {
  {
    RendererPtr renderer(new Renderer(gm_));
    NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);;
    Reset();
    renderer->DrawScene(root);
    // Info logs are empty when there are no errors.
    EXPECT_EQ("", data_->shader->GetInfoLog());
    EXPECT_EQ("", data_->shader->GetFragmentShader()->GetInfoLog());
    EXPECT_EQ("", data_->shader->GetGeometryShader()->GetInfoLog());
    EXPECT_EQ("", data_->shader->GetVertexShader()->GetInfoLog());
  }

  VerifyFunctionFailure(data_, options_, gm_, "CompileShader",
                        "Unable to compile");
  // Check that the info log was set.
  EXPECT_EQ("Shader compilation is set to always fail.",
            data_->shader->GetVertexShader()->GetInfoLog());
  EXPECT_EQ("Shader compilation is set to always fail.",
            data_->shader->GetGeometryShader()->GetInfoLog());
  EXPECT_EQ("Shader compilation is set to always fail.",
            data_->shader->GetFragmentShader()->GetInfoLog());
  EXPECT_EQ("", data_->shader->GetInfoLog());
  // Reset data.
  data_->rect = nullptr;
  data_->shader = nullptr;
  BuildRectangle(data_, options_);

  Reset();
  VerifyFunctionFailure(data_, options_, gm_, "LinkProgram",
                        "Unable to link");
  // Check that the info log was set.
  EXPECT_EQ("", data_->shader->GetVertexShader()->GetInfoLog());
  EXPECT_EQ("", data_->shader->GetGeometryShader()->GetInfoLog());
  EXPECT_EQ("", data_->shader->GetFragmentShader()->GetInfoLog());
  EXPECT_EQ("Program linking is set to always fail.",
            data_->shader->GetInfoLog());
}

TEST_F(RendererTest, FunctionFailures) {
  // Misc tests for error handling when some functions fail.
  base::LogChecker log_checker;
  {
    RendererPtr renderer(new Renderer(gm_));
    NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);;
    Reset();
    renderer->DrawScene(root);
  }
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // Check that Renderer catches failed compilation.
  gm_->EnableFeature(GraphicsManager::kVertexArrays, false);
  Reset();
  VerifyFunctionFailure(data_, options_, gm_, "CompileShader",
                        "Unable to compile");
  // Check that Renderer catches failed program creation.
  Reset();
  VerifyFunctionFailure(data_, options_, gm_, "CreateProgram",
                        "Unable to create shader program object");
  // Check that Renderer catches failed shader creation.
  Reset();
  VerifyFunctionFailure(data_, options_, gm_, "CreateShader",
                        "Unable to create shader object");
  // Check that Renderer catches failed linking.
  Reset();
  VerifyFunctionFailure(data_, options_, gm_, "LinkProgram",
                        "Unable to link");

  gm_->EnableFeature(GraphicsManager::kVertexArrays, true);
  Reset();
  VerifyFunctionFailure(data_, options_, gm_, "CompileShader",
                        "Unable to compile");
  // Check that Renderer catches failed program creation.

  Reset();
  VerifyFunctionFailure(data_, options_, gm_, "CreateProgram",
                        "Unable to create shader program object");
  // Check that Renderer catches failed shader creation.
  Reset();
  VerifyFunctionFailure(data_, options_, gm_, "CreateShader",
                        "Unable to create shader object");
  // Check that Renderer catches failed linking.
  Reset();
  VerifyFunctionFailure(data_, options_, gm_, "LinkProgram",
                        "Unable to link");

  // Check that Renderer catches failed buffer id generation.
  Reset();
  VerifyFunctionFailure(data_, options_, gm_, "GenBuffers",
                        "Unable to create buffer");
  // Check that Renderer catches failed sampler id generation.
  Reset();
  VerifyFunctionFailure(data_, options_, gm_, "GenSamplers",
                        "Unable to create sampler");
  // Check that Renderer catches failed framebuffer id generation.
  Reset();
  VerifyFunctionFailure(data_, options_, gm_, "GenFramebuffers",
                        "Unable to create framebuffer");
  // Check that Renderer catches failed renderbuffer id generation.
  Reset();
  VerifyFunctionFailure(data_, options_, gm_, "GenRenderbuffers",
                        "Unable to create renderbuffer");
  // Check that Renderer catches failed texture id generation.
  Reset();
  VerifyFunctionFailure(data_, options_, gm_, "GenTextures",
                        "Unable to create texture");
  // Check that Renderer catches failed vertex array id generation.
  Reset();
  VerifyFunctionFailure(data_, options_, gm_, "GenVertexArrays",
                        "Unable to create vertex array");
}

TEST_F(RendererTest, PrimitiveType) {
  // Test primitive type.
  NodePtr root;
  TracingHelper helper;
  base::LogChecker log_checker;

  root = BuildGraph(data_, options_, kWidth, kHeight);
  VerifyRenderData<Shape::PrimitiveType> verify_data;
  verify_data.update_func =
      std::bind(BuildRectangleShape<uint16>, data_, options_);
          verify_data.call_name = "DrawElements";
  verify_data.option = &options_->primitive_type;
  verify_data.static_args.push_back(
      StaticArg(2, helper.ToString("GLsizei", s_num_indices)));
  verify_data.static_args.push_back(StaticArg(3, "GL_UNSIGNED_SHORT"));
  verify_data.static_args.push_back(StaticArg(4, "NULL"));
  verify_data.varying_arg_index = 1U;
  verify_data.arg_tests.push_back(
      VaryingArg<Shape::PrimitiveType>(0, Shape::kLines, "GL_LINES"));
  verify_data.arg_tests.push_back(
      VaryingArg<Shape::PrimitiveType>(
          0, Shape::kLineLoop, "GL_LINE_LOOP"));
  verify_data.arg_tests.push_back(
      VaryingArg<Shape::PrimitiveType>(
          0, Shape::kLineStrip, "GL_LINE_STRIP"));
  verify_data.arg_tests.push_back(
      VaryingArg<Shape::PrimitiveType>(0, Shape::kPoints, "GL_POINTS"));
  verify_data.arg_tests.push_back(
      VaryingArg<Shape::PrimitiveType>(0, Shape::kTriangles, "GL_TRIANGLES"));
  verify_data.arg_tests.push_back(
      VaryingArg<Shape::PrimitiveType>(
          0, Shape::kTriangleFan, "GL_TRIANGLE_FAN"));
  verify_data.arg_tests.push_back(
      VaryingArg<Shape::PrimitiveType>(
          0, Shape::kTriangleStrip, "GL_TRIANGLE_STRIP"));
  {
    RendererPtr renderer(new Renderer(gm_));
    EXPECT_TRUE(
        VerifyRenderCalls(verify_data, trace_verifier_, renderer, root));
  }

  // Check some corner cases.
  gm_->EnableFeature(GraphicsManager::kVertexArrays, false);
  {
    RendererPtr renderer(new Renderer(gm_));
    // Destroy the data in the datacontainer - should not get an error message.
    data_->vertex_container = nullptr;
    // The attribute_array must be destroyed as well to trigger a rebind.
    data_->attribute_array = nullptr;
    BuildRectangleAttributeArray(data_, options_);
    data_->vertex_buffer->SetData(
        data_->vertex_container, sizeof(Vertex), s_num_vertices,
        options_->vertex_buffer_usage);
    Reset();
    renderer->DrawScene(root);
    // The buffer object should be updated even with null datacontainer.
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("Draw"));
    EXPECT_FALSE(log_checker.HasMessage("WARNING", "Unable to draw shape"));
    // Restore the data.
    BuildRectangleBufferObject(data_, options_);
    BuildRectangleAttributeArray(data_, options_);
  }

  gm_->EnableFeature(GraphicsManager::kVertexArrays, true);
  RendererPtr renderer(new Renderer(gm_));
  // Destroy the data in the datacontainer, should not get an error message.
  data_->vertex_container = nullptr;
  // The attribute_array must be destroyed as well to trigger a rebind.
  data_->attribute_array = nullptr;
  BuildRectangleAttributeArray(data_, options_);
  data_->vertex_buffer->SetData(
      data_->vertex_container, sizeof(Vertex), s_num_vertices,
      options_->vertex_buffer_usage);
  Reset();
  renderer->DrawScene(root);
  // The buffer object should be updated even with null datacontainer.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Draw"));
  EXPECT_FALSE(log_checker.HasMessage("WARNING", "Unable to draw shape"));
  // Restore the data.
  BuildRectangleBufferObject(data_, options_);
  BuildRectangleAttributeArray(data_, options_);

  // Do the same with the index buffer.
  data_->index_container = nullptr;
  data_->index_buffer->SetData(data_->index_container, sizeof(uint16),
                               s_num_indices, options_->index_buffer_usage);
  Reset();
  renderer->DrawScene(root);
  // The index buffer object should be updated.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf(
      "BindBuffer(GL_ELEMENT_ARRAY_BUFFER"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf(
      "BufferData(GL_ELEMENT_ARRAY_BUFFER"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Draw"));
  EXPECT_FALSE(log_checker.HasMessage("WARNING", "Unable to draw shape"));
  // Restore the data.
  BuildRectangleShape<uint16>(data_, options_);

  // Check that the shape is not drawn if the IndexBuffer has no indices.
  data_->shape->SetIndexBuffer(IndexBufferPtr(new IndexBuffer));
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Draw"));
  data_->shape->SetIndexBuffer(data_->index_buffer);

  // Check that if there are no index buffers then DrawArrays is used. By
  // default, all vertices should be used.
  data_->shape->SetPrimitiveType(Shape::kPoints);
  data_->shape->SetIndexBuffer(IndexBufferPtr(nullptr));
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawElements"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawArrays(GL_POINTS, 0, 4)"));
  // Try different vertex range settings.
  data_->shape->AddVertexRange(Range1i(1, 3));
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawArrays(GL_POINTS, 1, 2)"));
  data_->shape->AddVertexRange(Range1i(3, 4));
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawArrays(GL_POINTS, 1, 2)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawArrays(GL_POINTS, 3, 1)"));
  data_->shape->EnableVertexRange(0, false);
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawArrays(GL_POINTS, 3, 1)"));
  data_->shape->EnableVertexRange(0, true);
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawArrays(GL_POINTS, 1, 2)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawArrays(GL_POINTS, 3, 1)"));
  data_->shape->ClearVertexRanges();
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawArrays(GL_POINTS, 0, 4)"));
  data_->shape->SetIndexBuffer(data_->index_buffer);

  // Check that if the shape has no attribute array that it is not drawn.
  data_->shape->SetAttributeArray(AttributeArrayPtr(nullptr));
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Draw"));
  data_->shape->SetAttributeArray(data_->attribute_array);

  Reset();
  renderer = nullptr;
}

TEST_F(RendererTest, MultipleRenderers) {
  // Multiple Renderers create multiple instances of the same resources.
  // There isn't (yet) a way to get at the internal state of the
  // ResourceManager.
  gm_->EnableFeature(GraphicsManager::kSamplerObjects, false);
  {
    RendererPtr renderer1(new Renderer(gm_));
    RendererPtr renderer2(new Renderer(gm_));

    // Draw the simplest possible scene.
    NodePtr root = BuildGraph(data_, options_, 800, 800);
    // Drawing will create resources.
    renderer1->DrawScene(root);
    renderer2->DrawScene(root);
    // Improve coverage by changing a group bit.
    data_->sampler->SetWrapS(Sampler::kMirroredRepeat);
    renderer1->DrawScene(root);
    renderer2->DrawScene(root);

    // Each renderer has its own resources and memory counts.
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer1, 12U + kVboSize, 0U, 28672U));
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer2, 12U + kVboSize, 0U, 28672U));
    // Memory usage per holder should be doubled; one resource per renderer.
    EXPECT_EQ(24U, data_->index_buffer->GetGpuMemoryUsed());
    EXPECT_EQ(8U * sizeof(Vertex), data_->vertex_buffer->GetGpuMemoryUsed());
    EXPECT_EQ(8192U, data_->texture->GetGpuMemoryUsed());
    EXPECT_EQ(49152U, data_->cubemap->GetGpuMemoryUsed());

    Reset();
    // Force calls to OnDestroyed().
    data_->attribute_array = nullptr;
    data_->vertex_buffer = nullptr;
    data_->index_buffer = nullptr;
    data_->shader = nullptr;
    data_->shape = nullptr;
    data_->texture = nullptr;
    data_->cubemap = nullptr;
    data_->rect = nullptr;
    root->ClearChildren();
    root->ClearUniforms();
    root->SetShaderProgram(ShaderProgramPtr(nullptr));
    // Force calls to ReleaseAll().
    renderer1->DrawScene(root);
    renderer2->DrawScene(root);
    // It can take two calls to free up all resources because some may be added
    // to the release queue during traversal.
    renderer1->DrawScene(root);
    renderer2->DrawScene(root);
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer1, 0U, 0U, 0U));
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer2, 0U, 0U, 0U));
    // Everything will be destroyed since the resources go away.
    std::vector<std::string> call_strings;
    call_strings.push_back("Clear");
    call_strings.push_back("Clear");
    call_strings.push_back("Clear");
    call_strings.push_back("Clear");
    call_strings.push_back("DeleteBuffers");
    call_strings.push_back("DeleteBuffers");
    call_strings.push_back("DeleteBuffers");
    call_strings.push_back("DeleteBuffers");
    call_strings.push_back("DeleteProgram");
    call_strings.push_back("DeleteProgram");
    call_strings.push_back("DeleteShader");
    call_strings.push_back("DeleteShader");
    call_strings.push_back("DeleteShader");
    call_strings.push_back("DeleteShader");
    call_strings.push_back("DeleteShader");
    call_strings.push_back("DeleteShader");
    call_strings.push_back("DeleteTextures");
    call_strings.push_back("DeleteTextures");
    call_strings.push_back("DeleteTextures");
    call_strings.push_back("DeleteTextures");
    call_strings.push_back("DeleteVertexArrays");
    call_strings.push_back("DeleteVertexArrays");
    EXPECT_TRUE(trace_verifier_->VerifySortedCalls(call_strings));
    Reset();
    root = nullptr;
    renderer1 = nullptr;
    renderer2 = nullptr;
    EXPECT_EQ(0U, trace_verifier_->GetCallCount());
  }
  // Reset data.
  BuildRectangle(data_, options_);

  {
    RendererPtr renderer1(new Renderer(gm_));
    RendererPtr renderer2(new Renderer(gm_));
    RendererPtr renderer3(new Renderer(gm_));

    // Draw the simplest possible scene.
    NodePtr root = BuildGraph(data_, options_, 800, 800);
    renderer1->DrawScene(root);
    renderer2->DrawScene(root);
    renderer3->DrawScene(root);
    Reset();
    // Force resource deletion from a renderer.
    renderer1 = nullptr;
    // Force calls to OnDestroyed().
    root = nullptr;
    data_->shape = nullptr;
    data_->rect = nullptr;
    renderer2 = nullptr;
    renderer3 = nullptr;
    EXPECT_TRUE(VerifyReleases(3));
  }

  {
    RendererPtr renderer1(new Renderer(gm_));
    RendererPtr renderer2(new Renderer(gm_));
    RendererPtr renderer3(new Renderer(gm_));

    // Draw the simplest possible scene.
    NodePtr root = BuildGraph(data_, options_, 800, 800);
    renderer1->DrawScene(root);
    renderer2->DrawScene(root);
    renderer3->DrawScene(root);
    Reset();

    // Clear resources to improve coverage.
    renderer3->ClearAllResources();
    EXPECT_TRUE(VerifyReleases(1));
    Reset();
    renderer1->ClearResources(data_->attribute_array.Get());
    renderer2->ClearTypedResources(Renderer::kTexture);
    renderer1 = nullptr;
    // Force calls to OnDestroyed().
    root = nullptr;
    data_->shape = nullptr;
    data_->rect = nullptr;
    renderer2 = nullptr;
    EXPECT_TRUE(VerifyReleases(2));
  }

  // Reset data.
  BuildRectangle(data_, options_);

  gm_->EnableFeature(GraphicsManager::kSamplerObjects, true);
}

TEST_F(RendererTest, Clearing) {
  NodePtr node(new Node);
  StateTablePtr state_table;
  RendererPtr renderer(new Renderer(gm_));

  state_table = new StateTable();
  state_table->SetClearDepthValue(0.5f);
  node->SetStateTable(state_table);

  Reset();
  renderer->DrawScene(node);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearDepthf(0.5)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Clear(GL_DEPTH_BUFFER_BIT)"));

  state_table = new StateTable();
  state_table->SetClearColor(math::Vector4f(0.3f, 0.3f, 0.5f, 1.0f));
  state_table->SetClearDepthValue(0.25f);
  node->SetStateTable(state_table);

  Reset();
  renderer->DrawScene(node);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearColor(0.3, 0.3, 0.5, 1)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearDepthf(0.25)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf(
      "Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)"));

  state_table = new StateTable();
  state_table->SetClearStencilValue(27);
  node->SetStateTable(state_table);

  Reset();
  renderer->DrawScene(node);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearStencil(27)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Clear(GL_STENCIL_BUFFER_BIT)"));

  state_table = new StateTable();
  state_table->SetClearDepthValue(0.15f);
  state_table->SetClearColor(math::Vector4f(0.2f, 0.1f, 0.5f, 0.3f));
  state_table->SetClearStencilValue(123);
  node->SetStateTable(state_table);

  Reset();
  renderer->DrawScene(node);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearColor(0.2, 0.1, 0.5, 0.3)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearDepthf(0.15)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearStencil(123)"));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf(
                "Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | "
                "GL_STENCIL_BUFFER_BIT)"));

  // In a simple hierarchy with the clears at the root, no other nodes should
  // trigger a clear.
  NodePtr child1(new Node);
  NodePtr child2(new Node);
  node->AddChild(child1);
  node->AddChild(child2);
  state_table = new StateTable();
  state_table->Enable(StateTable::kBlend, true);
  child1->SetStateTable(state_table);

  state_table = new StateTable();
  state_table->Enable(StateTable::kCullFace, true);
  child2->SetStateTable(state_table);

  Reset();
  renderer->DrawScene(node);
  // The particular values are already set.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ClearColor"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ClearDepthf"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ClearStencil"));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf(
                "Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | "
                "GL_STENCIL_BUFFER_BIT)"));

  // If an internal node clears, only it should be cleared.
  NodePtr parent(new Node);
  parent->AddChild(node);
  state_table = new StateTable();
  state_table->Enable(StateTable::kStencilTest, true);
  parent->SetStateTable(state_table);

  Reset();
  renderer->DrawScene(node);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ClearColor"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ClearDepthf"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ClearStencil"));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf(
                "Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | "
                "GL_STENCIL_BUFFER_BIT)"));

  // Test that clear colors are propagated correctly.
  NodePtr clear_node_blue(new Node);
  state_table = new StateTable();
  state_table->SetClearColor(math::Vector4f(0.0f, 0.0f, 1.0f, 1.0f));
  clear_node_blue->SetStateTable(state_table);

  NodePtr clear_node_black(new Node);
  state_table = new StateTable();
  state_table->SetClearColor(math::Vector4f(0.0f, 0.0f, 0.0f, 0.0f));
  clear_node_black->SetStateTable(state_table);

  BuildGraph(data_, options_, kWidth, kHeight);
  NodePtr shape_node(new Node);
  shape_node->SetShaderProgram(data_->shader);
  AddPlaneShaderUniformsToNode(data_, shape_node);
  shape_node->AddShape(data_->shape);
  shape_node->AddChild(clear_node_black);

  Reset();
  renderer->DrawScene(clear_node_blue);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearColor"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearColor(0, 0, 1, 1)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Clear(GL_COLOR_BUFFER_BIT)"));

  Reset();
  renderer->DrawScene(shape_node);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearColor"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearColor(0, 0, 0, 0)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Clear(GL_COLOR_BUFFER_BIT)"));
}

TEST_F(RendererTest, ClearingResources) {
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, 800, 800);
  FramebufferObjectPtr fbo(new FramebufferObject(128, 128));
  fbo->SetColorAttachment(0U, FramebufferObject::Attachment(Image::kRgba4Byte));
  fbo->SetColorAttachment(1U, FramebufferObject::Attachment(Image::kRgba4Byte));
  fbo->SetColorAttachment(3U, FramebufferObject::Attachment(Image::kRgba4Byte));
  renderer->BindFramebuffer(fbo);
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteBuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteFramebuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteProgram"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteSamplers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteShader"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteTextures"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteVertexArrays"));
  Reset();
  EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U + kVboSize, 98304U, 28672U));
  EXPECT_EQ(12U, data_->index_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(kVboSize, data_->vertex_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(98304U, fbo->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, data_->texture->GetGpuMemoryUsed());
  EXPECT_EQ(24576U, data_->cubemap->GetGpuMemoryUsed());

  // Clear an entire scene at once.
  Reset();
  renderer->ClearAllResources();
  // Check that all memory was released.
  EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 0U, 0U, 0U));
  EXPECT_EQ(0U, data_->index_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(0U, data_->vertex_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(0U, fbo->GetGpuMemoryUsed());
  EXPECT_EQ(0U, data_->texture->GetGpuMemoryUsed());
  EXPECT_EQ(0U, data_->cubemap->GetGpuMemoryUsed());
  renderer->BindFramebuffer(fbo);
  renderer->DrawScene(root);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DeleteFramebuffers"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("DeleteBuffers"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DeleteProgram"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("DeleteShader"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("DeleteTexture"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DeleteVertexArrays"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenVertexArrays(1"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("GenBuffers(1"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenFramebuffers"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("GenRenderbuffers"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("GenTextures(1, "));
  // The texture is bound twice, once for the framebuffer, and again when it is
  // used.
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  EXPECT_EQ(6U, trace_verifier_->GetCountOf("TexParameteri(GL_TEXTURE_2D"));
  EXPECT_EQ(
      7U, trace_verifier_->GetCountOf("PixelStorei(GL_UNPACK_ALIGNMENT, 1)"));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf(
                "TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, "
                "GL_UNSIGNED_BYTE"));
  EXPECT_EQ(7U, trace_verifier_->GetCountOf("SamplerParameteri"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("SamplerParameterf"));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_CUBE_MAP"));
  EXPECT_EQ(6U,
            trace_verifier_->GetCountOf("TexParameteri(GL_TEXTURE_CUBE_MAP"));
  EXPECT_EQ(6U,
            trace_verifier_->GetCountOf("TexImage2D(GL_TEXTURE_CUBE_MAP"));
  EXPECT_EQ(
      1U,
      trace_verifier_->GetCountOf("BindBuffer(GL_ELEMENT_ARRAY_BUFFER"));
  EXPECT_EQ(
      1U,
      trace_verifier_->GetCountOf("BufferData(GL_ELEMENT_ARRAY_BUFFER, 12"));
  // Everything should be recreated.
  EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U + kVboSize, 98304U, 28672U));
  EXPECT_EQ(12U, data_->index_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(kVboSize, data_->vertex_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(98304U, fbo->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, data_->texture->GetGpuMemoryUsed());
  EXPECT_EQ(24576U, data_->cubemap->GetGpuMemoryUsed());

  // AttributeArray.
  Reset();
  renderer->ClearResources(data_->attribute_array.Get());
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteBuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteFramebuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteProgram"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteSamplers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteShader"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteTextures"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DeleteVertexArrays"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenVertexArrays(1"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindVertexArray"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("VertexAttribPointer"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("EnableVertexAttribArray"));
  // Check memory usage.
  EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U + kVboSize, 98304U, 28672U));
  EXPECT_EQ(12U, data_->index_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(kVboSize, data_->vertex_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(98304U, fbo->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, data_->texture->GetGpuMemoryUsed());
  EXPECT_EQ(24576U, data_->cubemap->GetGpuMemoryUsed());

  // BufferObject.
  Reset();
  renderer->ClearResources(data_->vertex_buffer.Get());
  // Check memory usage.
  EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U, 98304U, 28672U));
  EXPECT_EQ(12U, data_->index_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(0U, data_->vertex_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(98304U, fbo->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, data_->texture->GetGpuMemoryUsed());
  EXPECT_EQ(24576U, data_->cubemap->GetGpuMemoryUsed());
  renderer->DrawScene(root);
  // Check memory usage.
  EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U + kVboSize, 98304U, 28672U));
  EXPECT_EQ(12U, data_->index_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(kVboSize, data_->vertex_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(98304U, fbo->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, data_->texture->GetGpuMemoryUsed());
  EXPECT_EQ(24576U, data_->cubemap->GetGpuMemoryUsed());
  // Check calls.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DeleteBuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteFramebuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteProgram"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteShader"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteTextures"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteVertexArrays"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenBuffers(1"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));

  // CubeMapTexture.
  Reset();
  renderer->ClearResources(data_->cubemap.Get());
  // Check memory usage.
  EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U + kVboSize, 98304U, 4096U));
  EXPECT_EQ(12U, data_->index_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(kVboSize, data_->vertex_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(98304U, fbo->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, data_->texture->GetGpuMemoryUsed());
  EXPECT_EQ(0U, data_->cubemap->GetGpuMemoryUsed());
  renderer->DrawScene(root);
  // Check memory usage.
  EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U + kVboSize, 98304U, 28672U));
  EXPECT_EQ(12U, data_->index_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(kVboSize, data_->vertex_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(98304U, fbo->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, data_->texture->GetGpuMemoryUsed());
  EXPECT_EQ(24576U, data_->cubemap->GetGpuMemoryUsed());
  // Check calls.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteBuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteFramebuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteProgram"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteSamplers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteShader"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DeleteTextures"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteVertexArrays"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenTextures(1, "));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_CUBE_MAP"));
  EXPECT_EQ(6U,
            trace_verifier_->GetCountOf("TexParameteri(GL_TEXTURE_CUBE_MAP"));
  EXPECT_EQ(
      6U, trace_verifier_->GetCountOf("PixelStorei(GL_UNPACK_ALIGNMENT, 1)"));
  EXPECT_EQ(6U,
            trace_verifier_->GetCountOf("TexImage2D(GL_TEXTURE_CUBE_MAP"));

  // Framebuffer.
  Reset();
  renderer->ClearResources(fbo.Get());
  // Check memory usage.
  EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U + kVboSize, 0U, 28672U));
  EXPECT_EQ(12U, data_->index_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(kVboSize, data_->vertex_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(0U, fbo->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, data_->texture->GetGpuMemoryUsed());
  EXPECT_EQ(24576U, data_->cubemap->GetGpuMemoryUsed());
  renderer->BindFramebuffer(fbo);
  renderer->DrawScene(root);
  // Check memory usage.
  EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U + kVboSize, 98304U, 28672U));
  EXPECT_EQ(12U, data_->index_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(kVboSize, data_->vertex_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(98304U, fbo->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, data_->texture->GetGpuMemoryUsed());
  EXPECT_EQ(24576U, data_->cubemap->GetGpuMemoryUsed());
  // Check calls.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteBuffers"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DeleteFramebuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteProgram"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteSamplers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteShader"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteTextures"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteVertexArrays"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenSamplers"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenFramebuffers"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("GenRenderbuffers"));

  // Sampler.
  Reset();
  renderer->ClearResources(data_->sampler.Get());
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteBuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteFramebuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteProgram"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DeleteSamplers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteShader"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteTextures"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteVertexArrays"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenSamplers"));
  // The sampler should be bound for both the texture and cubemap. The texture
  // is bound twice, once when it is created, and again after it is bound to a
  // uniform.
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("BindSampler"));
  EXPECT_EQ(7U, trace_verifier_->GetCountOf("SamplerParameteri"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("SamplerParameterf"));

  // Shader.
  Reset();
  renderer->ClearResources(data_->shader->GetFragmentShader().Get());
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteBuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteFramebuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteProgram"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteSamplers"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DeleteShader"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteTextures"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteVertexArrays"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ShaderSource"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("AttachShader"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("CreateProgram"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("BindAttribLocation"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("GetActiveAttrib"));
  EXPECT_EQ(5U, trace_verifier_->GetCountOf("GetActiveUniform"));
  EXPECT_EQ(5U, trace_verifier_->GetCountOf("GetUniformLocation"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("LinkProgram"));

  // ShaderProgram.
  Reset();
  renderer->ClearResources(data_->shader.Get());
  if (gm_->IsFeatureAvailable(GraphicsManager::kTransformFeedback)) {
    data_->shader->SetCapturedVaryings({"vTexCoords"});
  }
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteBuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteFramebuffers"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DeleteProgram"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteSamplers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteShader"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteTextures"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteVertexArrays"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("AttachShader"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("CreateProgram"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("BindAttribLocation"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("GetActiveAttrib"));
  EXPECT_EQ(5U, trace_verifier_->GetCountOf("GetActiveUniform"));
  EXPECT_EQ(5U, trace_verifier_->GetCountOf("GetUniformLocation"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("LinkProgram"));
  if (gm_->IsFeatureAvailable(GraphicsManager::kTransformFeedback)) {
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("TransformFeedbackVaryings"));
    data_->shader->SetCapturedVaryings({});
  }

  // Texture.
  Reset();
  renderer->ClearResources(data_->texture.Get());
  // Check memory usage.
  EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U + kVboSize, 98304U, 24576U));
  EXPECT_EQ(12U, data_->index_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(kVboSize, data_->vertex_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(98304U, fbo->GetGpuMemoryUsed());
  EXPECT_EQ(0U, data_->texture->GetGpuMemoryUsed());
  EXPECT_EQ(24576U, data_->cubemap->GetGpuMemoryUsed());
  renderer->DrawScene(root);
  // Check memory usage.
  EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U + kVboSize, 98304U,
                                   28672U));
  EXPECT_EQ(12U, data_->index_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(kVboSize, data_->vertex_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(98304U, fbo->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, data_->texture->GetGpuMemoryUsed());
  EXPECT_EQ(24576U, data_->cubemap->GetGpuMemoryUsed());
  // Check calls.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteBuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteFramebuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteProgram"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteSamplers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteShader"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DeleteTextures"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteVertexArrays"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenTextures(1, "));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  EXPECT_EQ(6U, trace_verifier_->GetCountOf("TexParameteri(GL_TEXTURE_2D"));
  EXPECT_EQ(
      1U, trace_verifier_->GetCountOf("PixelStorei(GL_UNPACK_ALIGNMENT, 1)"));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf(
                "TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, "
                "GL_UNSIGNED_BYTE"));

  // Clear all Shaders.
  Reset();
  renderer->ClearTypedResources(Renderer::kShader);
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteBuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteFramebuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteProgram"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteSamplers"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("DeleteShader"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteTextures"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteVertexArrays"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("ShaderSource"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("AttachShader"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("CreateProgram"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("BindAttribLocation"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("GetActiveAttrib"));
  EXPECT_EQ(5U, trace_verifier_->GetCountOf("GetActiveUniform"));
  EXPECT_EQ(5U, trace_verifier_->GetCountOf("GetUniformLocation"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("LinkProgram"));

  // Remove some of the attachments from the framebuffer object.
  fbo->SetColorAttachment(0U, FramebufferObject::Attachment());
  fbo->SetColorAttachment(3U, FramebufferObject::Attachment());
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteFramebuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteTextures"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("DeleteRenderbuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenRenderbuffers"));
  EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U + kVboSize, 32768U,
                                   28672U));
  EXPECT_EQ(12U, data_->index_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(kVboSize, data_->vertex_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(32768U, fbo->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, data_->texture->GetGpuMemoryUsed());
  EXPECT_EQ(24576U, data_->cubemap->GetGpuMemoryUsed());
}

TEST_F(RendererTest, DisabledNodes) {
  // Build a graph with multiple nodes, each with a StateTable that enables
  // a different capability.
  // The graph looks like this:
  //        a
  //     b     c
  //          d e
  NodePtr a(new Node);
  NodePtr b(new Node);
  NodePtr c(new Node);
  NodePtr d(new Node);
  NodePtr e(new Node);

  StateTablePtr state_table;

  state_table = new StateTable();
  state_table->Enable(StateTable::kBlend, true);
  a->SetStateTable(state_table);

  state_table = new StateTable();
  state_table->Enable(StateTable::kCullFace, true);
  b->SetStateTable(state_table);

  state_table = new StateTable();
  state_table->Enable(StateTable::kDepthTest, true);
  c->SetStateTable(state_table);

  state_table = new StateTable();
  state_table->Enable(StateTable::kScissorTest, true);
  d->SetStateTable(state_table);

  state_table = new StateTable();
  state_table->Enable(StateTable::kStencilTest, true);
  e->SetStateTable(state_table);

  BuildRectangleShape<uint16>(data_, options_);
  a->AddShape(data_->shape);
  b->AddShape(data_->shape);
  c->AddShape(data_->shape);
  d->AddShape(data_->shape);
  e->AddShape(data_->shape);

  a->AddChild(b);
  a->AddChild(c);
  c->AddChild(d);
  c->AddChild(e);

  AddDefaultUniformsToNode(a);

  // Draw the scene.
  RendererPtr renderer(new Renderer(gm_));
  renderer->DrawScene(a);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_BLEND)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_CULL_FACE)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_DEPTH_TEST)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_SCISSOR_TEST)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_STENCIL_TEST)"));

  // Disable node b and render again.
  Reset();
  b->Enable(false);
  a->GetStateTable()->Enable(StateTable::kBlend, false);
  renderer->DrawScene(a);
  // The blend state won't be sent again because it is already enabled from the
  // first draw call.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_BLEND)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_CULL_FACE)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_DEPTH_TEST)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_SCISSOR_TEST)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_STENCIL_TEST)"));

  // Disable node c and render again.
  Reset();
  c->Enable(false);
  renderer->DrawScene(a);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_BLEND)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_CULL_FACE)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_DEPTH_TEST)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_SCISSOR_TEST)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_STENCIL_TEST)"));
}

TEST_F(RendererTest, StateCompression) {
  NodePtr a(new Node);
  NodePtr b(new Node);
  NodePtr c(new Node);
  NodePtr d(new Node);
  NodePtr e(new Node);

  StateTablePtr state_table;

  state_table = new StateTable();
  state_table->Enable(StateTable::kBlend, true);
  a->SetStateTable(state_table);

  state_table = new StateTable();
  state_table->Enable(StateTable::kCullFace, true);
  b->SetStateTable(state_table);

  state_table = new StateTable();
  state_table->Enable(StateTable::kBlend, true);
  state_table->Enable(StateTable::kDepthTest, true);
  c->SetStateTable(state_table);

  state_table = new StateTable();
  state_table->Enable(StateTable::kScissorTest, true);
  d->SetStateTable(state_table);

  state_table = new StateTable();
  state_table->Enable(StateTable::kStencilTest, true);
  e->SetStateTable(state_table);

  BuildRectangleShape<uint16>(data_, options_);
  a->AddShape(data_->shape);
  b->AddShape(data_->shape);
  c->AddShape(data_->shape);
  d->AddShape(data_->shape);
  e->AddShape(data_->shape);

  AddDefaultUniformsToNode(a);
  AddDefaultUniformsToNode(b);
  AddDefaultUniformsToNode(c);

  // Draw a, which should set blend and nothing else.
  RendererPtr renderer(new Renderer(gm_));
  renderer->DrawScene(a);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Disable(GL_BLEND)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_BLEND)"));

  Reset();
  renderer->DrawScene(c);
  // Drawing c should just enable depth test, since blending is already enabled.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Disable(GL_BLEND)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_BLEND)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_DEPTH_TEST)"));

  Reset();
  renderer->DrawScene(b);
  // Drawing b should disable blending and depth test but enable cull face.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Disable(GL_BLEND)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_BLEND)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_DEPTH_TEST)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Disable(GL_DEPTH_TEST)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Disable(GL_CULL_FACE)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_CULL_FACE)"));

  // Try hierarchies of nodes; the graphs look like this:
  //     a     c
  //     b    d e
  a->AddChild(b);
  Reset();
  renderer->DrawScene(a);
  // When a is drawn cull face is disabled but blending enabled, and then
  // the cull face re-enabled when b is drawn. Depth testing should not be
  // modified since it is currently disabled.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Disable(GL_CULL_FACE)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_BLEND)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_CULL_FACE)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Disable(GL_BLEND)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_DEPTH_TEST)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Disable(GL_DEPTH_TEST)"));

  c->AddChild(d);
  c->AddChild(e);
  Reset();
  renderer->DrawScene(c);
  // First cull face is disabled since none of the nodes use it, then depth test
  // is enabled (blending is already enabled!), and will stay so through
  // inheritance.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Disable(GL_CULL_FACE)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_BLEND)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Disable(GL_BLEND)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_DEPTH_TEST)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Disable(GL_DEPTH_TEST)"));

  // Drawing d will enable scissor test, while drawing e will disable it and
  // enable stencil test.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_SCISSOR_TEST)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_STENCIL_TEST)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Disable(GL_SCISSOR_TEST)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Disable(GL_STENCIL_TEST)"));

  // Create a scene that is very deep (> 16) and ensure state changes happen.
  renderer.Reset(nullptr);
  Renderer::DestroyCurrentStateCache();
  renderer.Reset(new Renderer(gm_));
  Reset();
  NodePtr root(new Node);
  AddDefaultUniformsToNode(root);
  NodePtr last_node = root;
  // Flip each cap in a new child node.
  for (int i = 0; i < StateTable::GetCapabilityCount(); ++i) {
    NodePtr node(new Node);
    StateTablePtr state_table(new StateTable);
    const StateTable::Capability cap = static_cast<StateTable::Capability>(i);
    if (cap == StateTable::kDither || cap == StateTable::kMultisample)
      state_table->Enable(cap, false);
    else
      state_table->Enable(cap, true);
    node->SetStateTable(state_table);
    node->AddShape(data_->shape);
    last_node->AddChild(node);
    last_node = node;
  }
  // Flip them back...
  for (int i = 0; i < StateTable::GetCapabilityCount(); ++i) {
    NodePtr node(new Node);
    StateTablePtr state_table(new StateTable);
    const StateTable::Capability cap = static_cast<StateTable::Capability>(i);
    if (cap == StateTable::kDither || cap == StateTable::kMultisample)
      state_table->Enable(cap, true);
    else
      state_table->Enable(cap, false);
    node->SetStateTable(state_table);
    node->AddShape(data_->shape);
    last_node->AddChild(node);
    last_node = node;
  }
  // ... and back again.
  for (int i = 0; i < StateTable::GetCapabilityCount(); ++i) {
    NodePtr node(new Node);
    StateTablePtr state_table(new StateTable);
    const StateTable::Capability cap = static_cast<StateTable::Capability>(i);
    if (cap == StateTable::kDither || cap == StateTable::kMultisample)
      state_table->Enable(cap, false);
    else
      state_table->Enable(cap, true);
    node->SetStateTable(state_table);
    node->AddShape(data_->shape);
    last_node->AddChild(node);
    last_node = node;
  }
  renderer->DrawScene(root);
  EXPECT_EQ(StateTable::GetCapabilityCount() * 2 - 2,
            static_cast<int>(trace_verifier_->GetCountOf("Enable")));
  EXPECT_EQ(StateTable::GetCapabilityCount() + 2,
            static_cast<int>(trace_verifier_->GetCountOf("Disable")));
}

TEST_F(RendererTest, ReadImage) {
  ImagePtr image;
  RendererPtr renderer(new Renderer(gm_));
  base::AllocatorPtr al;

  image = renderer->ReadImage(
      Range2i::BuildWithSize(Point2i(0, 0), Vector2i(50U, 80U)),
      Image::kRgb565, al);
  EXPECT_TRUE(image->GetData()->GetData() != nullptr);
  EXPECT_EQ(Image::kRgb565, image->GetFormat());
  EXPECT_EQ(50U, image->GetWidth());
  EXPECT_EQ(80U, image->GetHeight());

  image = renderer->ReadImage(
      Range2i::BuildWithSize(Point2i(20, 10), Vector2i(50U, 80U)),
      Image::kRgba8888, al);
  EXPECT_TRUE(image->GetData()->GetData() != nullptr);
  EXPECT_EQ(Image::kRgba8888, image->GetFormat());
  EXPECT_EQ(50U, image->GetWidth());
  EXPECT_EQ(80U, image->GetHeight());

  FramebufferObjectPtr fbo(new FramebufferObject(128, 128));
  fbo->SetColorAttachment(0U, FramebufferObject::Attachment(Image::kRgba4Byte));
  renderer->BindFramebuffer(fbo);
  image = renderer->ReadImage(
      Range2i::BuildWithSize(Point2i(0, 0), Vector2i(128U, 128U)),
      Image::kRgb888, al);
  EXPECT_TRUE(image->GetData()->GetData() != nullptr);
  EXPECT_EQ(Image::kRgb888, image->GetFormat());
  EXPECT_EQ(128U, image->GetWidth());
  EXPECT_EQ(128U, image->GetHeight());
  renderer->BindFramebuffer(FramebufferObjectPtr());

  image = renderer->ReadImage(
      Range2i::BuildWithSize(Point2i(20, 10), Vector2i(50U, 80U)),
      Image::kRgba8888, al);
  EXPECT_TRUE(image->GetData()->GetData() != nullptr);
  EXPECT_EQ(Image::kRgba8888, image->GetFormat());
  EXPECT_EQ(50U, image->GetWidth());
  EXPECT_EQ(80U, image->GetHeight());
}

TEST_F(RendererTest, MappedBuffer) {
  base::LogChecker log_checker;
  RendererPtr renderer(new Renderer(gm_));
  // Ensure static data is available.
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  Reset();

  const BufferObject::MappedBufferData::DataSource kInvalidSource =
      base::InvalidEnumValue<BufferObject::MappedBufferData::DataSource>();

  const BufferObject::MappedBufferData& mbd =
      data_->vertex_buffer->GetMappedData();
  const Range1ui full_range(
      0U, static_cast<uint32>(data_->vertex_buffer->GetStructSize() *
                              data_->vertex_buffer->GetCount()));

  // The buffer should not have any mapped data by default.
  EXPECT_TRUE(mbd.range.IsEmpty());
  EXPECT_TRUE(mbd.pointer == nullptr);

  // nullptr BufferObjectPtrs should trigger warning.
  renderer->MapBufferObjectData(BufferObjectPtr(), Renderer::kWriteOnly);
  EXPECT_TRUE(
      log_checker.HasMessage("WARNING", "A NULL BufferObject was passed"));
  renderer->UnmapBufferObjectData(BufferObjectPtr());
  EXPECT_TRUE(
      log_checker.HasMessage("WARNING", "A NULL BufferObject was passed"));

  gm_->EnableFeature(GraphicsManager::kMapBuffer, false);
  gm_->EnableFeature(GraphicsManager::kMapBufferBase, false);
  gm_->EnableFeature(GraphicsManager::kMapBufferRange, false);
  renderer->MapBufferObjectData(data_->vertex_buffer, Renderer::kWriteOnly);
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // The data should have been mapped with a client-side pointer.
  EXPECT_EQ(full_range, mbd.range);
  EXPECT_FALSE(mbd.pointer == nullptr);

  // Trying to map again should log a warning.
  EXPECT_FALSE(log_checker.HasAnyMessages());
  renderer->MapBufferObjectData(data_->vertex_buffer, Renderer::kWriteOnly);
  EXPECT_TRUE(log_checker.HasMessage(
      "WARNING", "buffer that is already mapped was passed"));

  // Unmapping the buffer should free the pointer.
  renderer->UnmapBufferObjectData(data_->vertex_buffer);
  EXPECT_TRUE(mbd.range.IsEmpty());
  EXPECT_TRUE(mbd.pointer == nullptr);
  EXPECT_EQ(kInvalidSource, mbd.data_source);
  // Unmapping again should log a warning.
  renderer->UnmapBufferObjectData(data_->vertex_buffer);
  EXPECT_TRUE(
      log_checker.HasMessage("WARNING", "unmapped BufferObject was passed"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("MapBuffer"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("UnmapBuffer"));

  // Now use the GL functions.
  gm_->EnableFeature(GraphicsManager::kMapBuffer, true);
  gm_->EnableFeature(GraphicsManager::kMapBufferBase, true);

  // Simulate a failed GL call.
  gm_->SetForceFunctionFailure("MapBuffer", true);
  renderer->MapBufferObjectData(data_->vertex_buffer, Renderer::kWriteOnly);
  gm_->SetErrorCode(GL_NO_ERROR);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("MapBuffer"));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Failed to allocate data for"));
  gm_->SetForceFunctionFailure("MapBuffer", false);

  Reset();
  renderer->MapBufferObjectData(data_->vertex_buffer, Renderer::kWriteOnly);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("MapBuffer"));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "MapBuffer"))
          .HasArg(2, "GL_WRITE_ONLY"));

  EXPECT_FALSE(log_checker.HasAnyMessages());
  renderer->MapBufferObjectData(data_->vertex_buffer, Renderer::kWriteOnly);
  EXPECT_TRUE(log_checker.HasMessage(
      "WARNING", "buffer that is already mapped was passed"));

  // Check that the mapped data changed.
  EXPECT_EQ(full_range, mbd.range);
  EXPECT_FALSE(mbd.pointer == nullptr);
  EXPECT_EQ(BufferObject::MappedBufferData::kGpuMapped, mbd.data_source);

  Reset();
  renderer->UnmapBufferObjectData(data_->vertex_buffer);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("UnmapBuffer"));
  EXPECT_TRUE(mbd.pointer == nullptr);
  EXPECT_EQ(kInvalidSource, mbd.data_source);
  renderer->UnmapBufferObjectData(data_->vertex_buffer);
  EXPECT_TRUE(
      log_checker.HasMessage("WARNING", "unmapped BufferObject was passed"));
  // An additional call should not have been made.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("UnmapBuffer"));

  // Map using different access modes.
  Reset();
  renderer->MapBufferObjectData(data_->vertex_buffer, Renderer::kReadOnly);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("MapBuffer"));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "MapBuffer"))
          .HasArg(2, "GL_READ_ONLY"));
  EXPECT_EQ(BufferObject::MappedBufferData::kGpuMapped, mbd.data_source);
  renderer->UnmapBufferObjectData(data_->vertex_buffer);
  EXPECT_EQ(kInvalidSource, mbd.data_source);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("UnmapBuffer"));
  Reset();
  renderer->MapBufferObjectData(data_->vertex_buffer, Renderer::kReadWrite);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("MapBuffer"));
  EXPECT_EQ(BufferObject::MappedBufferData::kGpuMapped, mbd.data_source);
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "MapBuffer"))
          .HasArg(2, "GL_READ_WRITE"));
  renderer->UnmapBufferObjectData(data_->vertex_buffer);
  EXPECT_EQ(kInvalidSource, mbd.data_source);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("UnmapBuffer"));

  // Check that when the range is the entire buffer and MapBufferRange() is not
  // supported that we fall back to MapBuffer().
  Reset();
  renderer->MapBufferObjectDataRange(
      data_->vertex_buffer, Renderer::kWriteOnly, full_range);
  // Despite the call to MapBufferObjectDataRange(), MapBuffer() should have
  // been called.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("MapBuffer"));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "MapBuffer"))
          .HasArg(2, "GL_WRITE_ONLY"));
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(BufferObject::MappedBufferData::kGpuMapped, mbd.data_source);

  // The entire buffer should be mapped.
  EXPECT_EQ(full_range, mbd.range);
  EXPECT_FALSE(mbd.pointer == nullptr);
  renderer->UnmapBufferObjectData(data_->vertex_buffer);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("UnmapBuffer"));
  EXPECT_EQ(kInvalidSource, mbd.data_source);

  // Check that platforms that do not support MapBufferRange() fall back to
  // the BufferObject's unwiped DataContainer.
  Range1ui range(4U, 8U);
  Reset();
  renderer->MapBufferObjectDataRange(
      data_->vertex_buffer, Renderer::kWriteOnly, range);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("MapBuffer"));
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(BufferObject::MappedBufferData::kDataContainer, mbd.data_source);

  // The range should be mapped.
  EXPECT_EQ(range, mbd.range);
  EXPECT_FALSE(mbd.pointer == nullptr);
  EXPECT_EQ(BufferObject::MappedBufferData::kDataContainer, mbd.data_source);
  renderer->UnmapBufferObjectData(data_->vertex_buffer);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("UnmapBuffer"));
  EXPECT_TRUE(mbd.pointer == nullptr);
  EXPECT_EQ(kInvalidSource, mbd.data_source);

  // Without DataContainer, expect to use kAllocated.
  data_->vertex_buffer->SetData(base::DataContainerPtr(),
                                data_->vertex_buffer->GetStructSize(),
                                data_->vertex_buffer->GetCount(),
                                options_->vertex_buffer_usage);

  // Map a range of data using a client side pointer.
  Reset();
  gm_->EnableFeature(GraphicsManager::kMapBuffer, false);
  gm_->EnableFeature(GraphicsManager::kMapBufferBase, false);
  gm_->EnableFeature(GraphicsManager::kMapBufferRange, false);
  renderer->MapBufferObjectDataRange(
      data_->vertex_buffer, Renderer::kWriteOnly, Range1ui());
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "Ignoring empty range"));
  // Reading an allocated buffer should complain about reading uninitialized
  // memory.
  renderer->MapBufferObjectDataRange(
      data_->vertex_buffer, Renderer::kReadOnly, range);
  EXPECT_TRUE(log_checker.HasMessage(
      "WARNING", "mapped bytes are uninitialized"));
  // Check that the mapped data changed.
  EXPECT_EQ(range, mbd.range);
  EXPECT_FALSE(mbd.pointer == nullptr);
  EXPECT_EQ(BufferObject::MappedBufferData::kAllocated, mbd.data_source);

  // Trying to map again should log a warning.
  EXPECT_FALSE(log_checker.HasAnyMessages());
  renderer->MapBufferObjectDataRange(
      data_->vertex_buffer, Renderer::kWriteOnly, Range1ui());
  EXPECT_TRUE(log_checker.HasMessage(
      "WARNING", "buffer that is already mapped was passed"));
  EXPECT_EQ(BufferObject::MappedBufferData::kAllocated, mbd.data_source);

  // Unmapping the buffer should free the pointer.
  renderer->UnmapBufferObjectData(data_->vertex_buffer);
  EXPECT_TRUE(mbd.range.IsEmpty());
  EXPECT_TRUE(mbd.pointer == nullptr);
  EXPECT_EQ(kInvalidSource, mbd.data_source);
  renderer->UnmapBufferObjectData(data_->vertex_buffer);
  EXPECT_TRUE(
      log_checker.HasMessage("WARNING", "unmapped BufferObject was passed"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("MapBufferRange"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("UnmapBuffer"));

  // Now use the GL function.
  gm_->EnableFeature(GraphicsManager::kMapBufferRange, true);
  gm_->EnableFeature(GraphicsManager::kMapBufferBase, true);

  // Simulate a failed GL call.
  gm_->SetForceFunctionFailure("MapBufferRange", true);
  renderer->MapBufferObjectDataRange(
      data_->vertex_buffer, Renderer::kWriteOnly, range);
  gm_->SetErrorCode(GL_NO_ERROR);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("MapBufferRange"));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Failed to allocate data for"));
  gm_->SetForceFunctionFailure("MapBufferRange", false);

  Reset();
  // An empty range should only log a warning message.
  renderer->MapBufferObjectDataRange(
      data_->vertex_buffer, Renderer::kWriteOnly, Range1ui());
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("MapBufferRange"));
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "Ignoring empty range"));
  EXPECT_TRUE(mbd.range.IsEmpty());
  EXPECT_TRUE(mbd.pointer == nullptr);
  EXPECT_EQ(kInvalidSource, mbd.data_source);

  // Try a range that is too large.
  renderer->MapBufferObjectDataRange(
      data_->vertex_buffer, Renderer::kWriteOnly, Range1ui(0, 16384U));
  gm_->SetErrorCode(GL_NO_ERROR);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("MapBufferRange"));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Failed to allocate data for"));
  EXPECT_TRUE(mbd.range.IsEmpty());
  EXPECT_TRUE(mbd.pointer == nullptr);
  EXPECT_EQ(kInvalidSource, mbd.data_source);

  Reset();
  renderer->MapBufferObjectDataRange(
      data_->vertex_buffer, Renderer::kWriteOnly, range);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("MapBufferRange"));
  EXPECT_EQ(BufferObject::MappedBufferData::kGpuMapped, mbd.data_source);
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "MapBuffer"))
          .HasArg(4, "GL_MAP_WRITE_BIT"));
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // Check that the mapped data changed.
  Reset();
  EXPECT_EQ(range, mbd.range);
  EXPECT_FALSE(mbd.pointer == nullptr);

  // Try again to get a warning.
  renderer->MapBufferObjectDataRange(
      data_->vertex_buffer, Renderer::kWriteOnly, Range1ui());
  EXPECT_TRUE(log_checker.HasMessage(
      "WARNING", "buffer that is already mapped was passed"));
  EXPECT_EQ(BufferObject::MappedBufferData::kGpuMapped, mbd.data_source);

  renderer->UnmapBufferObjectData(data_->vertex_buffer);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("UnmapBuffer"));
  EXPECT_TRUE(mbd.range.IsEmpty());
  EXPECT_TRUE(mbd.pointer == nullptr);
  EXPECT_EQ(kInvalidSource, mbd.data_source);
  renderer->UnmapBufferObjectData(data_->vertex_buffer);
  // An additional call should not have been made.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("UnmapBuffer"));
  EXPECT_TRUE(
      log_checker.HasMessage("WARNING", "unmapped BufferObject was passed"));
  EXPECT_EQ(kInvalidSource, mbd.data_source);

  // Map using different access modes.
  Reset();
  renderer->MapBufferObjectDataRange(
      data_->vertex_buffer, Renderer::kReadOnly, range);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("MapBufferRange"));
  EXPECT_EQ(BufferObject::MappedBufferData::kGpuMapped, mbd.data_source);
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "MapBuffer"))
          .HasArg(4, "GL_MAP_READ_BIT"));
  renderer->UnmapBufferObjectData(data_->vertex_buffer);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("UnmapBuffer"));
  EXPECT_EQ(kInvalidSource, mbd.data_source);
  Reset();
  renderer->MapBufferObjectDataRange(
      data_->vertex_buffer, Renderer::kReadWrite, range);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("MapBufferRange"));
  EXPECT_EQ(BufferObject::MappedBufferData::kGpuMapped, mbd.data_source);
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "MapBuffer"))
          .HasArg(4, "GL_MAP_READ_BIT | GL_MAP_WRITE_BIT"));
  renderer->UnmapBufferObjectData(data_->vertex_buffer);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("UnmapBuffer"));
  EXPECT_EQ(kInvalidSource, mbd.data_source);

  // Reset data.
  data_->rect = nullptr;
  data_->vertex_container = nullptr;
  BuildRectangle(data_, options_);
}

TEST_F(RendererTest, Flags) {
  // Test that flags can be set properly.
  RendererPtr renderer(new Renderer(gm_));
  // By default all process flags are set.
  Renderer::Flags flags = Renderer::AllProcessFlags();
  EXPECT_EQ(flags, renderer->GetFlags());

  flags.reset(Renderer::kProcessReleases);
  renderer->ClearFlag(Renderer::kProcessReleases);
  EXPECT_EQ(flags, renderer->GetFlags());

  flags.set(Renderer::kRestoreShaderProgram);
  renderer->SetFlag(Renderer::kRestoreShaderProgram);
  EXPECT_EQ(flags, renderer->GetFlags());

  flags.set(Renderer::kProcessInfoRequests);
  renderer->SetFlag(Renderer::kProcessInfoRequests);
  EXPECT_EQ(flags, renderer->GetFlags());

  flags.reset(Renderer::kProcessInfoRequests);
  renderer->ClearFlag(Renderer::kProcessInfoRequests);
  EXPECT_EQ(flags, renderer->GetFlags());

  // Setting no flags should do nothing.
  renderer->ClearFlags(Renderer::AllFlags());
  renderer->SetFlags(Renderer::Flags());
  EXPECT_EQ(0U, renderer->GetFlags().count());

  // Multiple flags.
  flags.reset();
  flags.set(Renderer::kProcessInfoRequests);
  flags.set(Renderer::kProcessReleases);
  flags.set(Renderer::kRestoreShaderProgram);
  flags.set(Renderer::kRestoreVertexArray);
  renderer->SetFlags(flags);
  EXPECT_EQ(4U, flags.count());
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kProcessInfoRequests));
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kProcessReleases));
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kRestoreShaderProgram));
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kRestoreVertexArray));

  // Clearing no flags should do nothing.
  flags.reset();
  renderer->ClearFlags(flags);
  EXPECT_EQ(4U, renderer->GetFlags().count());
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kProcessInfoRequests));
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kProcessReleases));
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kRestoreShaderProgram));
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kRestoreVertexArray));

  // Setting no flags should do nothing.
  flags.reset();
  renderer->SetFlags(flags);
  EXPECT_EQ(4U, renderer->GetFlags().count());
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kProcessInfoRequests));
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kProcessReleases));
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kRestoreShaderProgram));
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kRestoreVertexArray));

  // Try to reset some unset flags.
  flags.reset();
  renderer->ClearFlags(flags);
  EXPECT_EQ(4U, renderer->GetFlags().count());
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kProcessInfoRequests));
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kProcessReleases));
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kRestoreShaderProgram));
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kRestoreVertexArray));

  // Nothing should have changed.
  renderer->ClearFlags(flags);
  EXPECT_EQ(4U, renderer->GetFlags().count());
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kProcessInfoRequests));
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kProcessReleases));
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kRestoreShaderProgram));
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kRestoreVertexArray));

  // Reset some set flags.
  flags.reset();
  flags.set(Renderer::kProcessReleases);
  flags.set(Renderer::kRestoreShaderProgram);
  renderer->ClearFlags(flags);
  EXPECT_EQ(2U, renderer->GetFlags().count());
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kProcessInfoRequests));
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kRestoreVertexArray));
}

TEST_F(RendererTest, FlagsBehavior) {
  // Test the behavior of the Renderer when different flags are set.

  // Test kProcessInfoRequests.
  {
    RendererPtr renderer(new Renderer(gm_));
    ResourceManager* manager = renderer->GetResourceManager();
    CallbackHelper<ResourceManager::PlatformInfo> callback;

    manager->RequestPlatformInfo(
        std::bind(&CallbackHelper<ResourceManager::PlatformInfo>::Callback,
                  &callback, std::placeholders::_1));

    renderer->ClearFlag(Renderer::kProcessInfoRequests);
    renderer->DrawScene(NodePtr());
    EXPECT_FALSE(callback.was_called);

    renderer->SetFlag(Renderer::kProcessInfoRequests);
    renderer->DrawScene(NodePtr());
    EXPECT_TRUE(callback.was_called);

    // It is possible that in our test platform, we cannot grab some of the
    // capabilities and it will generate an error.
    gm_->SetErrorCode(GL_NO_ERROR);
  }

  // Test kProcessReleases.
  {
    RendererPtr renderer(new Renderer(gm_));
    NodePtr root = BuildGraph(data_, options_, 800, 800);
    // Drawing will create resources.
    renderer->DrawScene(root);
    Reset();
    // These will trigger resources to be released.
    data_->attribute_array = nullptr;
    data_->vertex_buffer = nullptr;
    data_->index_buffer = nullptr;
    data_->shader = nullptr;
    data_->shape = nullptr;
    data_->rect = nullptr;
    root->ClearChildren();
    root->SetShaderProgram(ShaderProgramPtr(nullptr));
    // Tell the renderer not to process releases.
    renderer->ClearFlag(Renderer::kProcessReleases);
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteBuffers"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteProgram"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteShader"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteVertexArrays"));

    // Tell the renderer to process releases.
    renderer->SetFlag(Renderer::kProcessReleases);
    Reset();
    renderer->DrawScene(root);
    // Most objects will be destroyed since the resources go away.
    std::vector<std::string> call_strings;
    call_strings.push_back("Clear");
    call_strings.push_back("DeleteBuffers");
    call_strings.push_back("DeleteBuffers");
    call_strings.push_back("DeleteProgram");
    call_strings.push_back("DeleteShader");
    call_strings.push_back("DeleteShader");
    call_strings.push_back("DeleteShader");
    call_strings.push_back("DeleteVertexArrays");
    EXPECT_TRUE(trace_verifier_->VerifySortedCalls(call_strings));

    // Reset data.
    data_->rect = nullptr;
    BuildRectangle(data_, options_);
  }

  // Test k(Restore|Save)*.
  {
    RendererPtr renderer(new Renderer(gm_));
    EXPECT_TRUE(VerifySaveAndRestoreFlag(data_, options_,
        gm_, renderer, trace_verifier_, Renderer::kSaveActiveTexture,
        Renderer::kRestoreActiveTexture, "GetIntegerv(GL_ACTIVE_TEXTURE",
        "ActiveTexture"));
  }
  {
    RendererPtr renderer(new Renderer(gm_));
    EXPECT_TRUE(VerifySaveAndRestoreFlag(data_, options_,
        gm_, renderer, trace_verifier_, Renderer::kSaveArrayBuffer,
        Renderer::kRestoreArrayBuffer, "GetIntegerv(GL_ARRAY_BUFFER_BINDING",
        "BindBuffer(GL_ARRAY_BUFFER"));
  }
  {
    RendererPtr renderer(new Renderer(gm_));
    EXPECT_TRUE(VerifySaveAndRestoreFlag(data_, options_,
        gm_, renderer, trace_verifier_, Renderer::kSaveElementArrayBuffer,
        Renderer::kRestoreElementArrayBuffer,
        "GetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING",
        "BindBuffer(GL_ELEMENT_ARRAY_BUFFER"));
  }
  {
    RendererPtr renderer(new Renderer(gm_));
    FramebufferObjectPtr fbo(new FramebufferObject(
        128, 128));
    fbo->SetColorAttachment(0U,
                            FramebufferObject::Attachment(Image::kRgba4Byte));
    renderer->BindFramebuffer(fbo);
    EXPECT_TRUE(VerifySaveAndRestoreFlag(data_, options_,
        gm_, renderer, trace_verifier_, Renderer::kSaveFramebuffer,
        Renderer::kRestoreFramebuffer, "GetIntegerv(GL_FRAMEBUFFER_BINDING",
        "BindFramebuffer"));
    EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == nullptr);
  }
  {
    // We might save a program marked for deletion which will be destroyed
    // the second we bind any other program, and it will be impossible to
    // rebind it. Therefore, check for a call to IsProgram instead of
    // UseProgram.
    RendererPtr renderer(new Renderer(gm_));
    EXPECT_TRUE(VerifySaveAndRestoreFlag(data_, options_,
        gm_, renderer, trace_verifier_, Renderer::kSaveShaderProgram,
        Renderer::kRestoreShaderProgram, "GetIntegerv(GL_CURRENT_PROGRAM",
        "IsProgram"));
  }
  {
    RendererPtr renderer(new Renderer(gm_));
    EXPECT_TRUE(VerifySaveAndRestoreFlag(data_, options_,
        gm_, renderer, trace_verifier_, Renderer::kSaveVertexArray,
        Renderer::kRestoreVertexArray, "GetIntegerv(GL_VERTEX_ARRAY_BINDING",
        "BindVertexArray"));
  }
  {
    // Saving and restoring StateTables is a little more complicated.
    RendererPtr renderer(new Renderer(gm_));

    renderer->ClearFlag(Renderer::kRestoreStateTable);
    renderer->SetFlag(Renderer::kSaveStateTable);
    Reset();
    renderer->DrawScene(NodePtr());
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("IsEnabled(GL_DEPTH_TEST"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("IsEnabled(GL_BLEND"));
    renderer->ClearFlag(Renderer::kSaveStateTable);

    // Now change a bunch of state.
    Reset();
    NodePtr root = BuildGraph(data_, options_, 800, 800);
    root->GetStateTable()->Enable(StateTable::kDepthTest, false);
    root->GetStateTable()->Enable(StateTable::kBlend, true);
    renderer->DrawScene(root);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("Disable(GL_DEPTH_TEST"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_BLEND"));

    // Drawing again with no flags set should do nothing.
    Reset();
    renderer->DrawScene(NodePtr());
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("Disable"));

    Reset();
    renderer->SetFlag(Renderer::kRestoreStateTable);
    renderer->DrawScene(NodePtr());
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("Disable(GL_DEPTH_TEST"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_BLEND"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_DEPTH_TEST"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("Disable(GL_BLEND"));
  }

  // Test all save/restore flags simultaneously.
  {
    RendererPtr renderer(new Renderer(gm_));
    EXPECT_TRUE(
        VerifyAllSaveAndRestoreFlags(data_, options_, gm_, renderer));
  }

  // Test kClear*.
  {
    EXPECT_TRUE(VerifyClearFlag(data_, options_, gm_,
                                Renderer::kClearActiveTexture,
                                GL_ACTIVE_TEXTURE, GL_TEXTURE0));
    EXPECT_TRUE(VerifyClearFlag(data_, options_, gm_,
                                Renderer::kClearArrayBuffer,
                                GL_ARRAY_BUFFER_BINDING, 0));
    EXPECT_TRUE(VerifyClearFlag(data_, options_, gm_,
                                Renderer::kClearElementArrayBuffer,
                                GL_ELEMENT_ARRAY_BUFFER_BINDING, 0));
    EXPECT_TRUE(VerifyClearFlag(data_, options_, gm_,
                                Renderer::kClearFramebuffer,
                                GL_FRAMEBUFFER_BINDING, 0));
    EXPECT_TRUE(VerifyClearImageUnitFlag(
        data_, options_, gm_, Renderer::kClearSamplers, GL_SAMPLER_BINDING, 0));
    EXPECT_TRUE(VerifyClearFlag(data_, options_, gm_,
                                Renderer::kClearShaderProgram,
                                GL_CURRENT_PROGRAM, 0));
    EXPECT_TRUE(VerifyClearImageUnitFlag(data_, options_, gm_,
                                         Renderer::kClearCubemaps,
                                         GL_TEXTURE_BINDING_CUBE_MAP, 0));
    EXPECT_TRUE(VerifyClearImageUnitFlag(data_, options_, gm_,
                                         Renderer::kClearTextures,
                                         GL_TEXTURE_BINDING_2D, 0));
    EXPECT_TRUE(VerifyClearFlag(data_, options_, gm_,
                                Renderer::kClearVertexArray,
                                GL_VERTEX_ARRAY_BINDING, 0));

    // Check some corner cases. First, clearing the framebuffer should also
    // clear the cached FramebufferPtr.
    {
      NodePtr root = BuildGraph(data_, options_, 800, 800);
      RendererPtr renderer(new Renderer(gm_));
      FramebufferObjectPtr fbo(new FramebufferObject(128, 128));
      fbo->SetColorAttachment(0U,
                              FramebufferObject::Attachment(Image::kRgba4Byte));
      renderer->BindFramebuffer(fbo);

      renderer->SetFlag(Renderer::kClearFramebuffer);
      renderer->DrawScene(root);
      // The framebuffer should have been cleared.
      EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == nullptr);
    }

    // Restoring a program binding should override clearing it.
    {
      NodePtr root = BuildGraph(data_, options_, 800, 800);
      RendererPtr renderer(new Renderer(gm_));
      renderer->DrawScene(root);
      Reset();
      renderer->SetFlag(Renderer::kClearShaderProgram);
      renderer->SetFlag(Renderer::kRestoreShaderProgram);
      renderer->SetFlag(Renderer::kSaveShaderProgram);
      renderer->DrawScene(root);
      EXPECT_EQ(0U, trace_verifier_->GetCountOf("UseProgram(0x0)"));
    }

    // Restoring a VAO binding should override clearing it.
    {
      NodePtr root = BuildGraph(data_, options_, 800, 800);
      RendererPtr renderer(new Renderer(gm_));
      renderer->DrawScene(root);
      Reset();
      renderer->SetFlag(Renderer::kClearVertexArray);
      renderer->SetFlag(Renderer::kRestoreVertexArray);
      renderer->SetFlag(Renderer::kSaveVertexArray);
      renderer->DrawScene(NodePtr());
      EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindVertexArray(0x0)"));
    }
  }

  // Test framebuffer invalidation.
  {
    RendererPtr renderer(new Renderer(gm_));
    NodePtr root = BuildGraph(data_, options_, 800, 800);
    // Drawing will create resources.
    renderer->ClearFlags(Renderer::AllInvalidateFlags());
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("InvalidateFramebuffer"));
    Reset();
    renderer->SetFlag(Renderer::kInvalidateDepthAttachment);
    renderer->SetFlag(Renderer::kInvalidateStencilAttachment);
    renderer->DrawScene(root);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(
        "InvalidateFramebuffer(GL_DRAW_FRAMEBUFFER, 2"));
    Reset();
    renderer->ClearFlags(Renderer::AllInvalidateFlags());
    renderer->SetFlag(Renderer::kInvalidateColorAttachment);
    renderer->SetFlag(
        static_cast<Renderer::Flag>(Renderer::kInvalidateColorAttachment + 1));
    renderer->SetFlag(
        static_cast<Renderer::Flag>(Renderer::kInvalidateColorAttachment + 2));
    renderer->DrawScene(root);
    // Additional color attachments for the default framebuffer will be
    // ignored.
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(
         "InvalidateFramebuffer(GL_DRAW_FRAMEBUFFER, 1"));
    Reset();
    FramebufferObjectPtr fbo(new FramebufferObject(128, 128));
    fbo->SetColorAttachment(0U,
                            FramebufferObject::Attachment(Image::kRgba4Byte));
    renderer->BindFramebuffer(fbo);
    renderer->DrawScene(root);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(
        "InvalidateFramebuffer(GL_DRAW_FRAMEBUFFER, 3"));
    Reset();
    renderer->ClearFlags(Renderer::AllInvalidateFlags());
    renderer->SetFlag(Renderer::kInvalidateDepthAttachment);
    renderer->SetFlag(Renderer::kInvalidateStencilAttachment);
    renderer->DrawScene(root);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(
        "InvalidateFramebuffer(GL_DRAW_FRAMEBUFFER, 2"));
    Reset();
    gm_->EnableFeature(GraphicsManager::kInvalidateFramebuffer, false);
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("InvalidateFramebuffer"));
    gm_->EnableFeature(GraphicsManager::kInvalidateFramebuffer, true);
  }
}

TEST_F(RendererTest, InitialUniformValue) {
  // Check that it is possible to set initial Uniform values.
  NodePtr node(new Node);
  BuildRectangleShape<uint16>(data_, options_);
  node->AddShape(data_->shape);

  const math::Matrix4f mat1(1.f, 2.f, 3.f, 4.f,
                           5.f, 6.f, 7.f, 8.f,
                           9.f, 1.f, 2.f, 3.f,
                           4.f, 5.f, 6.f, 7.f);
  const math::Matrix4f mat2(1.f, 2.f, 3.f, 4.f,
                           9.f, 1.f, 2.f, 3.f,
                           5.f, 6.f, 7.f, 8.f,
                           4.f, 5.f, 6.f, 7.f);
  const math::Vector4f vec(1.f, 2.f, 3.f, 4.f);


  RendererPtr renderer(new Renderer(gm_));

  // Create some uniform values.
  const ShaderInputRegistryPtr& reg = ShaderInputRegistry::GetGlobalRegistry();
  const Uniform modelview_matrix =
      reg->Create<Uniform>("uModelviewMatrix", mat1);
  const Uniform projection_matrix =
      reg->Create<Uniform>("uProjectionMatrix", mat2);
  const Uniform color = reg->Create<Uniform>("uBaseColor", vec);

  renderer->SetInitialUniformValue(modelview_matrix);
  renderer->SetInitialUniformValue(projection_matrix);
  renderer->SetInitialUniformValue(color);

  // Check that the values were set correctly.
  ResourceManager* manager = renderer->GetResourceManager();
  CallbackHelper<ResourceManager::ProgramInfo> callback;
  manager->RequestAllResourceInfos<ShaderProgram, ResourceManager::ProgramInfo>(
      std::bind(&CallbackHelper<ResourceManager::ProgramInfo>::Callback,
                &callback, std::placeholders::_1));
  renderer->DrawScene(node);
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(1U, callback.infos.size());
  EXPECT_EQ(3U, callback.infos[0].uniforms.size());
  EXPECT_EQ("uProjectionMatrix", callback.infos[0].uniforms[0].name);
  EXPECT_EQ(static_cast<GLuint>(GL_FLOAT_MAT4),
            callback.infos[0].uniforms[0].type);
  EXPECT_EQ(mat2, callback.infos[0].uniforms[0].value.Get<math::Matrix4f>());


  EXPECT_EQ("uModelviewMatrix", callback.infos[0].uniforms[1].name);
  EXPECT_EQ(static_cast<GLuint>(GL_FLOAT_MAT4),
            callback.infos[0].uniforms[1].type);
  EXPECT_EQ(mat1, callback.infos[0].uniforms[1].value.Get<math::Matrix4f>());

  EXPECT_EQ("uBaseColor", callback.infos[0].uniforms[2].name);
  EXPECT_EQ(static_cast<GLuint>(GL_FLOAT_VEC4),
            callback.infos[0].uniforms[2].type);
  EXPECT_TRUE(math::VectorBase4f::AreValuesEqual(
      vec, callback.infos[0].uniforms[2].value.Get<math::VectorBase4f>()));
}

TEST_F(RendererTest, CombinedUniformsSent) {
  // Check that combined uniforms that change are sent.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, 800, 800);
  const ShaderInputRegistryPtr& global_reg =
        ShaderInputRegistry::GetGlobalRegistry();
  root->AddUniform(global_reg->Create<Uniform>("uModelviewMatrix",
      math::TranslationMatrix(math::Vector3f(0.5f, 0.5f, 0.5f))));
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("UniformMatrix4fv"));
  Reset();
  renderer->DrawScene(root);
  // Combined uModelviewMatrix generates a new stamp so it is sent.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("UniformMatrix4fv"));
  data_->rect->SetUniformValue(
      data_->rect->GetUniformIndex("uModelviewMatrix"),
      math::TranslationMatrix(math::Vector3f(-0.5f, 0.5f, 0.0f)));
  Reset();
  renderer->DrawScene(root);
  // The combined uniform is different, so it should have been sent.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("UniformMatrix4fv"));
}

TEST_F(RendererTest, GeneratedUniformsSent) {
  // Check that generated uniforms are properly created and sent.
  TracingHelper helper;

  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  reg->IncludeGlobalRegistry();
  reg->Add(ShaderInputRegistry::UniformSpec(
      "uTranslationMatrix", kMatrix4x4Uniform, "", CombineMatrices,
      ExtractTranslation));
  // Add the spec for the generated uniform.
  reg->Add(ShaderInputRegistry::UniformSpec("uTranslationX", kFloatUniform));
  reg->Add(ShaderInputRegistry::UniformSpec("uTranslationY", kFloatUniform));
  reg->Add(ShaderInputRegistry::UniformSpec("uTranslationZ", kFloatUniform));
  BuildGraph(data_, options_, 800, 800);

  static const char* kVertexShaderString =
      "attribute vec3 aVertex;\n"
      "attribute vec2 aTexCoords;\n"
      "uniform mat4 uTranslationMatrix;\n";

  static const char* kFragmentShaderString =
      "uniform float uTranslationX;\n"
      "uniform float uTranslationY;\n"
      "uniform float uTranslationZ;\n";

  ShaderProgramPtr shader = ShaderProgram::BuildFromStrings(
      "Shader", reg, kVertexShaderString, kFragmentShaderString,
      base::AllocatorPtr());
  data_->shape->SetAttributeArray(data_->attribute_array);
  data_->rect->SetShaderProgram(data_->shader);
  data_->rect->ClearUniforms();
  data_->rect->AddUniform(
      reg->Create<Uniform>("uniform1", math::Vector3f::Zero()));
  data_->rect->AddUniform(
          reg->Create<Uniform>("uniform2", math::Vector3f::Zero()));

  RendererPtr renderer(new Renderer(gm_));
  NodePtr root(new Node);
  root->AddUniform(reg->Create<Uniform>("uTranslationMatrix",
      math::TranslationMatrix(math::Vector3f(0.5f, 0.5f, 0.5f))));
  root->SetShaderProgram(shader);

  NodePtr child1(new Node);
  child1->AddUniform(reg->Create<Uniform>("uTranslationMatrix",
      math::TranslationMatrix(math::Vector3f(2.0f, 4.0f, 6.0f))));

  NodePtr child2(new Node);
  child2->AddUniform(reg->Create<Uniform>("uTranslationMatrix",
      math::TranslationMatrix(math::Vector3f(10.0f, 8.0f, 6.0f))));

  root->AddChild(child1);
  child1->AddChild(child2);
  root->AddShape(data_->shape);
  child1->AddShape(data_->shape);
  child2->AddShape(data_->shape);

  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("UniformMatrix4fv"));
  EXPECT_EQ(9U, trace_verifier_->GetCountOf("Uniform1fv"));
  math::Vector3f vec;
  vec.Set(0.5f, 0.5f, 0.5f);
  math::Matrix4f mat = math::Transpose(
      math::TranslationMatrix(vec));
  const float* mat_floats = &mat[0][0];
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                0U, "UniformMatrix4fv"))
                  .HasArg(4, helper.ToString("GLmatrix4*", mat_floats)));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                0U, "Uniform1fv"))
                  .HasArg(3, base::ValueToString(vec[0])));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                1U, "Uniform1fv"))
                  .HasArg(3, base::ValueToString(vec[1])));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                2U, "Uniform1fv"))
                  .HasArg(3, base::ValueToString(vec[2])));
  vec.Set(2.5f, 4.5f, 6.5f);
  mat = math::Transpose(math::TranslationMatrix(vec));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                1U, "UniformMatrix4fv"))
                  .HasArg(4, helper.ToString("GLmatrix4*", mat_floats)));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                3U, "Uniform1fv"))
                  .HasArg(3, base::ValueToString(vec[0])));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                4U, "Uniform1fv"))
                  .HasArg(3, base::ValueToString(vec[1])));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                5U, "Uniform1fv"))
                  .HasArg(3, base::ValueToString(vec[2])));
  vec.Set(12.5f, 12.5f, 12.5f);
  mat = math::Transpose(math::TranslationMatrix(vec));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                2U, "UniformMatrix4fv"))
                  .HasArg(4, helper.ToString("GLmatrix4*", mat_floats)));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                6U, "Uniform1fv"))
                  .HasArg(3, base::ValueToString(vec[0])));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                7U, "Uniform1fv"))
                  .HasArg(3, base::ValueToString(vec[1])));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                8U, "Uniform1fv"))
                  .HasArg(3, base::ValueToString(vec[2])));
}

TEST_F(RendererTest, ConcurrentShader) {
  // Check that different threads can have different uniform values
  // set on the same shader when per-thread uniforms are enabled.
  static const char* kVertexShaderString =
      "uniform float uFloat;\n"
      "void main(){}\n";
  static const char* kFragmentShaderString =
      "void main(){}\n";

  RendererPtr renderer(new Renderer(gm_));
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);

  BuildRectangleShape<uint16>(data_, options_);
  NodePtr root(new Node);
  root->AddShape(data_->shape);
  size_t uindex = root->AddUniform(reg->Create<Uniform>("uFloat", 0.f));

  ResourceManager* manager = renderer->GetResourceManager();
  std::vector<ResourceManager::ProgramInfo> other_infos;

  {
    // Default: shared uniforms
    Reset();
    GlContextPtr share_context = FakeGlContext::CreateShared(*gl_context_);
    ShaderProgramPtr shader = ShaderProgram::BuildFromStrings(
        "Shader", reg, kVertexShaderString, kFragmentShaderString,
        base::AllocatorPtr());
    root->SetShaderProgram(shader);
    root->SetUniformValue(uindex, -7.f);
    CallbackHelper<ResourceManager::ProgramInfo> before, after;
    manager->RequestAllResourceInfos<ShaderProgram,
                                     ResourceManager::ProgramInfo>(
        std::bind(&CallbackHelper<ResourceManager::ProgramInfo>::Callback,
                  &before, std::placeholders::_1));
    renderer->DrawScene(root);

    EXPECT_TRUE(before.was_called);
    EXPECT_EQ(1U, before.infos.size());
    EXPECT_EQ(1U, before.infos[0].uniforms.size());
    EXPECT_EQ(-7.f, before.infos[0].uniforms[0].value.Get<float>());

    std::thread uniform_thread(UniformThread, renderer, share_context, root,
                               uindex, 2.f, &other_infos);
    uniform_thread.join();

    EXPECT_EQ(1U, other_infos.size());
    EXPECT_EQ(1U, other_infos[0].uniforms.size());
    EXPECT_EQ(2.f, other_infos[0].uniforms[0].value.Get<float>());

    manager->RequestAllResourceInfos<ShaderProgram,
                                     ResourceManager::ProgramInfo>(
        std::bind(&CallbackHelper<ResourceManager::ProgramInfo>::Callback,
                  &after, std::placeholders::_1));
    renderer->ProcessResourceInfoRequests();
    EXPECT_TRUE(after.was_called);
    EXPECT_EQ(1U, after.infos.size());
    EXPECT_EQ(1U, after.infos[0].uniforms.size());
    EXPECT_EQ(2.f, after.infos[0].uniforms[0].value.Get<float>());
  }

  {
    // Per-thread uniforms
    Reset();
    GlContextPtr share_context = FakeGlContext::CreateShared(*gl_context_);
    ShaderProgramPtr shader = ShaderProgram::BuildFromStrings(
        "Shader", reg, kVertexShaderString, kFragmentShaderString,
        base::AllocatorPtr());
    shader->SetConcurrent(true);
    root->SetShaderProgram(shader);
    root->SetUniformValue(uindex, -7.f);
    CallbackHelper<ResourceManager::ProgramInfo> before, after;
    manager->RequestAllResourceInfos<ShaderProgram,
                                     ResourceManager::ProgramInfo>(
        std::bind(&CallbackHelper<ResourceManager::ProgramInfo>::Callback,
                  &before, std::placeholders::_1));
    renderer->DrawScene(root);

    EXPECT_TRUE(before.was_called);
    EXPECT_EQ(1U, before.infos.size());
    EXPECT_EQ(1U, before.infos[0].uniforms.size());
    EXPECT_EQ(-7.f, before.infos[0].uniforms[0].value.Get<float>());

    std::thread uniform_thread(UniformThread, renderer, share_context, root,
                               uindex, 2.f, &other_infos);
    uniform_thread.join();

    EXPECT_EQ(1U, other_infos.size());
    EXPECT_EQ(1U, other_infos[0].uniforms.size());
    EXPECT_EQ(2.f, other_infos[0].uniforms[0].value.Get<float>());

    manager->RequestAllResourceInfos<ShaderProgram,
                                     ResourceManager::ProgramInfo>(
        std::bind(&CallbackHelper<ResourceManager::ProgramInfo>::Callback,
                  &after, std::placeholders::_1));
    renderer->ProcessResourceInfoRequests();
    EXPECT_TRUE(after.was_called);
    EXPECT_EQ(1U, after.infos.size());
    EXPECT_EQ(1U, after.infos[0].uniforms.size());
    EXPECT_EQ(-7.f, after.infos[0].uniforms[0].value.Get<float>());
  }
}


TEST_F(RendererTest, CreateResourceWithExternallyManagedId) {
  NodePtr root = BuildGraph(data_, options_, 800, 800);

  // Test out the individual resource creation functions.
  RendererPtr renderer(new Renderer(gm_));
  // Ensure a resource binder exists.
  renderer->DrawScene(NodePtr());

  // BufferObject.
  GLuint id;
  gm_->GenBuffers(1, &id);
  gm_->BindBuffer(GL_ARRAY_BUFFER, id);
  Reset();
  // An invalid ID does nothing.
  renderer->CreateResourceWithExternallyManagedId(data_->vertex_buffer.Get(),
                                                  2345U);
  EXPECT_EQ(1U, trace_verifier_->GetCallCount());
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("IsBuffer"));

  renderer->CreateResourceWithExternallyManagedId(data_->vertex_buffer.Get(),
                                                  id);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenBuffers(1"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER, 0x" +
                                            base::ValueToString(id)));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));

  // IndexBuffer.
  gm_->GenBuffers(1, &id);
  gm_->BindBuffer(GL_ARRAY_BUFFER, id);
  Reset();
  // An invalid ID does nothing.
  renderer->CreateResourceWithExternallyManagedId(
      data_->shape->GetIndexBuffer().Get(), 2345U);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("IsBuffer"));

  renderer->CreateResourceWithExternallyManagedId(
      data_->shape->GetIndexBuffer().Get(), id);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenBuffers(1"));
  EXPECT_EQ(
      1U, trace_verifier_->GetCountOf("BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0x" +
                                      base::ValueToString(id)));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf("BufferData(GL_ELEMENT_ARRAY_BUFFER"));

  gm_->EnableFeature(GraphicsManager::kSamplerObjects, false);
  // Texture.
  gm_->GenTextures(1, &id);
  gm_->BindTexture(GL_TEXTURE_2D, id);
  Reset();
  // An invalid ID does nothing.
  renderer->CreateResourceWithExternallyManagedId(data_->texture.Get(), 2345U);
  EXPECT_EQ(1U, trace_verifier_->GetCallCount());
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("IsTexture"));

  renderer->CreateResourceWithExternallyManagedId(data_->texture.Get(), id);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenTextures(1, "));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D, 0x" +
                                            base::ValueToString(id)));
  EXPECT_EQ(12U, trace_verifier_->GetCountOf("TexParameteri(GL_TEXTURE_2D"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("TexParameterf(GL_TEXTURE_2D"));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf("PixelStorei(GL_UNPACK_ALIGNMENT, 1)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf(
                    "TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, "
                    "GL_UNSIGNED_BYTE"));
  gm_->EnableFeature(GraphicsManager::kSamplerObjects, true);

  Reset();
  // Destroy all resources.
  renderer.Reset(nullptr);
  data_->attribute_array = nullptr;
  data_->vertex_buffer = nullptr;
  data_->index_buffer = nullptr;
  data_->shader = nullptr;
  data_->shape = nullptr;
  data_->texture = nullptr;
  data_->rect = nullptr;
  // Check that the managed resources were not deleted.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Delete"));
}

TEST_F(RendererTest, CreateExternalFramebufferProxy) {
  NodePtr root = BuildGraph(data_, options_, 800, 800);
  RendererPtr renderer(new Renderer(gm_));
  // Ensure a resource binder exists.
  renderer->DrawScene(NodePtr());
  // Create a framebuffer outside of Ion.
  GLuint fbid, bound_fb, texid;
  gm_->GenFramebuffers(1, &fbid);
  EXPECT_GT(fbid, 0U);
  gm_->BindFramebuffer(GL_FRAMEBUFFER, fbid);
  gm_->GetIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint*>(&bound_fb));
  EXPECT_EQ(fbid, bound_fb);
  gm_->GenTextures(1, &texid);
  gm_->BindTexture(GL_TEXTURE_2D, texid);
  GLenum internal_format = GL_RGBA;
  GLenum format = GL_RGBA;
  gm_->TexImage2D(GL_TEXTURE_2D, 0, internal_format, 128, 128, 0, format,
                  GL_UNSIGNED_BYTE, nullptr);
  gm_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            texid, 0);
  Reset();
  // Create a proxy framebuffer.
  Range2i::Size size(128, 128);
  Image::Format color_format = Image::kRgba8888;
  Image::Format depth_format = Image::kRenderbufferDepth16;
  FramebufferObjectPtr fbo = renderer->CreateExternalFramebufferProxy(
      size, color_format, depth_format, 1);
  EXPECT_EQ(fbid, renderer->GetResourceGlId(fbo.Get()));
  renderer->DrawScene(NodePtr());
  // Ensure that Ion did not try to generate an FBO or make attachments.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenFramebuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenRenderbuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenTextures"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("FramebufferRenderbuffer"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("FramebufferTexture"));
  // Destroy all resources.
  renderer.Reset(nullptr);
  data_->attribute_array = nullptr;
  data_->vertex_buffer = nullptr;
  data_->index_buffer = nullptr;
  data_->shader = nullptr;
  data_->shape = nullptr;
  data_->texture = nullptr;
  data_->rect = nullptr;
  // Check that the external resources were not deleted.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Delete"));
}

TEST_F(RendererTest, CreateOrUpdateResources) {
  NodePtr root = BuildGraph(data_, options_, 800, 800);

  // Test out the individual resource creation functions.
  {
    RendererPtr renderer(new Renderer(gm_));

    // AttributeArray. Only buffer data will be bound and sent.
    Reset();
    renderer->CreateOrUpdateResource(data_->attribute_array.Get());
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenBuffers(1"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));
  }

  gm_->EnableFeature(GraphicsManager::kSamplerObjects, false);
  {
    RendererPtr renderer(new Renderer(gm_));

    // BufferObject.
    Reset();
    renderer->CreateOrUpdateResource(data_->vertex_buffer.Get());
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenBuffers(1"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));

    // ShaderProgram.
    Reset();
    renderer->CreateOrUpdateResource(data_->shader.Get());
    EXPECT_EQ(1U,
              trace_verifier_->GetCountOf("CreateShader(GL_VERTEX_SHADER"));
    EXPECT_EQ(1U,
              trace_verifier_->GetCountOf("CreateShader(GL_GEOMETRY_SHADER"));
    EXPECT_EQ(1U,
              trace_verifier_->GetCountOf("CreateShader(GL_FRAGMENT_SHADER"));
    EXPECT_EQ(3U, trace_verifier_->GetCountOf("ShaderSource"));
    EXPECT_EQ(3U, trace_verifier_->GetCountOf("AttachShader"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("CreateProgram"));
    EXPECT_EQ(3U, trace_verifier_->GetCountOf("BindAttribLocation"));
    EXPECT_EQ(3U, trace_verifier_->GetCountOf("GetActiveAttrib"));
    EXPECT_EQ(5U, trace_verifier_->GetCountOf("GetActiveUniform"));
    EXPECT_EQ(5U, trace_verifier_->GetCountOf("GetUniformLocation"));
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("LinkProgram"));

    // Texture.
    Reset();
    renderer->CreateOrUpdateResource(data_->texture.Get());
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenTextures(1, "));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
    EXPECT_EQ(12U, trace_verifier_->GetCountOf("TexParameteri(GL_TEXTURE_2D"));
    EXPECT_EQ(3U, trace_verifier_->GetCountOf("TexParameterf(GL_TEXTURE_2D"));
    EXPECT_EQ(
        1U, trace_verifier_->GetCountOf("PixelStorei(GL_UNPACK_ALIGNMENT, 1)"));
    EXPECT_EQ(1U,
              trace_verifier_->GetCountOf(
                  "TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, "
                  "GL_UNSIGNED_BYTE"));
    // Cubemap.
    Reset();
    renderer->CreateOrUpdateResource(data_->cubemap.Get());
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenTextures(1, "));
    EXPECT_EQ(1U,
              trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_CUBE_MAP"));
    EXPECT_EQ(12U,
              trace_verifier_->GetCountOf("TexParameteri(GL_TEXTURE_CUBE_MAP"));
    EXPECT_EQ(3U,
              trace_verifier_->GetCountOf("TexParameterf(GL_TEXTURE_CUBE_MAP"));
    EXPECT_EQ(
        6U, trace_verifier_->GetCountOf("PixelStorei(GL_UNPACK_ALIGNMENT, 1)"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(
                      "TexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, "
                      "32, 32, 0, GL_RGBA, "
                      "GL_UNSIGNED_BYTE"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(
                      "TexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, "
                      "32, 32, 0, GL_RGBA, "
                      "GL_UNSIGNED_BYTE"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(
                      "TexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, "
                      "32, 32, 0, GL_RGBA, "
                      "GL_UNSIGNED_BYTE"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(
                      "TexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA, "
                      "32, 32, 0, GL_RGBA, "
                      "GL_UNSIGNED_BYTE"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(
                      "TexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGBA, "
                      "32, 32, 0, GL_RGBA, "
                      "GL_UNSIGNED_BYTE"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(
                      "TexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGBA, "
                      "32, 32, 0, GL_RGBA, "
                      "GL_UNSIGNED_BYTE"));
  }
  gm_->EnableFeature(GraphicsManager::kSamplerObjects, true);

  {
    // Shape (the index buffer and the Shape's attribute array).
    RendererPtr renderer(new Renderer(gm_));
    Reset();
    renderer->CreateOrUpdateShapeResources(data_->shape);
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("GenBuffers(1"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));
    EXPECT_EQ(
        1U,
        trace_verifier_->GetCountOf("BindBuffer(GL_ELEMENT_ARRAY_BUFFER"));
    EXPECT_EQ(
        1U,
        trace_verifier_->GetCountOf("BufferData(GL_ELEMENT_ARRAY_BUFFER, 12"));
  }

  gm_->EnableFeature(GraphicsManager::kSamplerObjects, false);
  {
    // Create an entire scene at once, which has all of the above except a
    // FramebufferObject.
    RendererPtr renderer(new Renderer(gm_));
    Reset();
    renderer->CreateOrUpdateResources(root);
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("GenBuffers(1"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("GenTextures(1, "));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
    EXPECT_EQ(
        1U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_CUBE_MAP"));
    EXPECT_EQ(12U, trace_verifier_->GetCountOf("TexParameteri(GL_TEXTURE_2D"));
    EXPECT_EQ(3U, trace_verifier_->GetCountOf("TexParameterf(GL_TEXTURE_2D"));
    EXPECT_EQ(
        7U, trace_verifier_->GetCountOf("PixelStorei(GL_UNPACK_ALIGNMENT, 1)"));
    EXPECT_EQ(1U,
              trace_verifier_->GetCountOf(
                  "TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, "
                  "GL_UNSIGNED_BYTE"));
    EXPECT_EQ(
        1U,
        trace_verifier_->GetCountOf("BindBuffer(GL_ELEMENT_ARRAY_BUFFER"));
    EXPECT_EQ(
        1U,
        trace_verifier_->GetCountOf("BufferData(GL_ELEMENT_ARRAY_BUFFER, 12"));
  }
  {
    // Try the same thing, but this time with the Textures in a UniformBlock.
    RendererPtr renderer(new Renderer(gm_));
    data_->rect->ClearUniforms();
    UniformBlockPtr block(new UniformBlock);
    data_->rect->AddUniformBlock(block);
    const ShaderInputRegistryPtr& reg =
        data_->rect->GetShaderProgram()->GetRegistry();
    block->AddUniform(reg->Create<Uniform>("uTexture", data_->texture));
    block->AddUniform(reg->Create<Uniform>("uTexture2", data_->texture));
    data_->rect->AddUniform(
        reg->Create<Uniform>("uCubeMapTexture", data_->cubemap));
    data_->rect->AddUniform(reg->Create<Uniform>(
        "uModelviewMatrix",
        math::TranslationMatrix(math::Vector3f(-1.5f, 1.5f, 0.0f))));
    data_->rect->AddUniform(
        reg->Create<Uniform>("uProjectionMatrix", math::Matrix4f::Identity()));

    Reset();
    renderer->CreateOrUpdateResources(root);

    EXPECT_EQ(2U, trace_verifier_->GetCountOf("GenBuffers(1"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("GenTextures(1, "));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
    EXPECT_EQ(
        1U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_CUBE_MAP"));
    EXPECT_EQ(12U, trace_verifier_->GetCountOf("TexParameteri(GL_TEXTURE_2D"));
    EXPECT_EQ(3U, trace_verifier_->GetCountOf("TexParameterf(GL_TEXTURE_2D"));
    EXPECT_EQ(
        7U, trace_verifier_->GetCountOf("PixelStorei(GL_UNPACK_ALIGNMENT, 1)"));
    EXPECT_EQ(1U,
              trace_verifier_->GetCountOf(
                  "TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, "
                  "GL_UNSIGNED_BYTE"));
    EXPECT_EQ(
        1U,
        trace_verifier_->GetCountOf("BindBuffer(GL_ELEMENT_ARRAY_BUFFER"));
    EXPECT_EQ(
        1U,
        trace_verifier_->GetCountOf("BufferData(GL_ELEMENT_ARRAY_BUFFER, 12"));
  }
  {
    // One more time but with the UniformBlocks disabled; the textures shouldn't
    // be sent (though the cubemaps will be).
    RendererPtr renderer(new Renderer(gm_));
    data_->rect->GetUniformBlocks()[0]->Enable(false);
    Reset();
    renderer->CreateOrUpdateResources(root);

    EXPECT_EQ(2U, trace_verifier_->GetCountOf("GenBuffers(1"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenTextures(1, "));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexParameteri(GL_TEXTURE_2D"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexParameterf(GL_TEXTURE_2D"));
    EXPECT_EQ(
        6U, trace_verifier_->GetCountOf("PixelStorei(GL_UNPACK_ALIGNMENT, 1)"));
    EXPECT_EQ(0U,
              trace_verifier_->GetCountOf(
                  "TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, "
                  "GL_UNSIGNED_BYTE"));
    EXPECT_EQ(
        1U,
        trace_verifier_->GetCountOf("BindBuffer(GL_ELEMENT_ARRAY_BUFFER"));
    EXPECT_EQ(
        1U,
        trace_verifier_->GetCountOf("BufferData(GL_ELEMENT_ARRAY_BUFFER, 12"));
  }
  gm_->EnableFeature(GraphicsManager::kSamplerObjects, true);

  {
    // Check that we never create resources for disabled Nodes.
    RendererPtr renderer(new Renderer(gm_));
    Reset();
    root->Enable(false);
    renderer->CreateOrUpdateResources(root);
    EXPECT_EQ(0U, trace_verifier_->GetCallCount());
  }
}

TEST_F(RendererTest, BindResource) {
  NodePtr root = BuildGraph(data_, options_, 800, 800);

  // Test out the individual resource creation functions.
  gm_->EnableFeature(GraphicsManager::kSamplerObjects, false);
  RendererPtr renderer(new Renderer(gm_));

  // BufferObject.
  Reset();
  renderer->BindResource(data_->vertex_buffer.Get());
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenBuffers(1"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));

  // FramebufferObject.
  Reset();
  FramebufferObjectPtr fbo(new FramebufferObject(data_->image->GetWidth(),
                                                 data_->image->GetHeight()));
  TexturePtr texture(new Texture);
  texture->SetImage(0U, data_->image);
  texture->SetSampler(data_->sampler);
  fbo->SetColorAttachment(0U, FramebufferObject::Attachment(texture));
  fbo->SetColorAttachment(1U, FramebufferObject::Attachment(Image::kRgba8ui));
  fbo->SetColorAttachment(2U, FramebufferObject::Attachment(Image::kRgba8ui));
  fbo->SetDepthAttachment(
      FramebufferObject::Attachment(Image::kRenderbufferDepth16));
  renderer->BindResource(fbo.Get());
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenFramebuffers(1"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindFramebuffer"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("GenRenderbuffer"));
  // The unbound stencil and color attachments will be set to 0 explicitly.
  EXPECT_EQ(5U, trace_verifier_->GetCountOf("FramebufferRenderbuffer"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("FramebufferTexture2D"));
  // The texture has to be created to bind it as an attachment.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenTextures"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawBuffers"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ReadBuffer"));

  // ShaderProgram.
  Reset();
  renderer->BindResource(data_->shader.Get());
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf("CreateShader(GL_VERTEX_SHADER"));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf("CreateShader(GL_GEOMETRY_SHADER"));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf("CreateShader(GL_FRAGMENT_SHADER"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("ShaderSource"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("AttachShader"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("CreateProgram"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("BindAttribLocation"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("GetActiveAttrib"));
  EXPECT_EQ(5U, trace_verifier_->GetCountOf("GetActiveUniform"));
  EXPECT_EQ(5U, trace_verifier_->GetCountOf("GetUniformLocation"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("LinkProgram"));

  // Texture.
  Reset();
  renderer->BindResource(data_->texture.Get());
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenTextures(1, "));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  EXPECT_EQ(12U, trace_verifier_->GetCountOf("TexParameteri(GL_TEXTURE_2D"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("TexParameterf(GL_TEXTURE_2D"));
  EXPECT_EQ(
      1U, trace_verifier_->GetCountOf("PixelStorei(GL_UNPACK_ALIGNMENT, 1)"));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf(
                "TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, "
                "GL_UNSIGNED_BYTE"));
  // Cubemap.
  Reset();
  renderer->BindResource(data_->cubemap.Get());
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenTextures(1, "));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_CUBE_MAP"));
  EXPECT_EQ(12U,
            trace_verifier_->GetCountOf("TexParameteri(GL_TEXTURE_CUBE_MAP"));
  EXPECT_EQ(3U,
            trace_verifier_->GetCountOf("TexParameterf(GL_TEXTURE_CUBE_MAP"));
  EXPECT_EQ(
      6U, trace_verifier_->GetCountOf("PixelStorei(GL_UNPACK_ALIGNMENT, 1)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf(
                    "TexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, "
                    "32, 32, 0, GL_RGBA, "
                    "GL_UNSIGNED_BYTE"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf(
                    "TexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, "
                    "32, 32, 0, GL_RGBA, "
                    "GL_UNSIGNED_BYTE"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf(
                    "TexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, "
                    "32, 32, 0, GL_RGBA, "
                    "GL_UNSIGNED_BYTE"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf(
                    "TexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA, "
                    "32, 32, 0, GL_RGBA, "
                    "GL_UNSIGNED_BYTE"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf(
                    "TexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGBA, "
                    "32, 32, 0, GL_RGBA, "
                    "GL_UNSIGNED_BYTE"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf(
                    "TexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGBA, "
                    "32, 32, 0, GL_RGBA, "
                    "GL_UNSIGNED_BYTE"));
}

TEST_F(RendererTest, GetResourceGlId) {
  BuildGraph(data_, options_, 800, 800);
  RendererPtr renderer(new Renderer(gm_));
  gm_->EnableFeature(GraphicsManager::kSamplerObjects, false);
  EXPECT_EQ(1U, renderer->GetResourceGlId(data_->vertex_buffer.Get()));
  EXPECT_EQ(1U, renderer->GetResourceGlId(data_->shader.Get()));
  EXPECT_EQ(1U, renderer->GetResourceGlId(data_->texture.Get()));
  EXPECT_EQ(2U, renderer->GetResourceGlId(data_->cubemap.Get()));
  gm_->EnableFeature(GraphicsManager::kSamplerObjects, true);
  EXPECT_EQ(1U, renderer->GetResourceGlId(data_->sampler.Get()));
}

TEST_F(RendererTest, ReleaseResources) {
  NodePtr root = BuildGraph(data_, options_, 800, 800);

  RendererPtr renderer(new Renderer(gm_));

  const size_t initial_usage =
      renderer->GetGpuMemoryUsage(ion::gfx::Renderer::kTexture);

  // Verify at least Texture memory reduces after a ReleaseResources() call.
  Reset();
  renderer->DrawScene(data_->rect);
  GLuint tex_id = renderer->GetResourceGlId(data_->texture.Get());
  EXPECT_TRUE(gm_->IsTexture(tex_id));

  const size_t uploaded_usage =
      renderer->GetGpuMemoryUsage(ion::gfx::Renderer::kTexture);

  Reset();
  // Force calls to OnDestroyed().
  DestroyGraph(data_, &root);

  const size_t post_mark_usage =
      renderer->GetGpuMemoryUsage(ion::gfx::Renderer::kTexture);

  // In fact the texture's final ref doesn't go away until in the
  // ReleaseResources - the ShaderProgram has a final ref on it.
  renderer->ReleaseResources();

  const size_t post_release_usage =
      renderer->GetGpuMemoryUsage(ion::gfx::Renderer::kTexture);

  EXPECT_EQ(uploaded_usage, post_mark_usage);
  EXPECT_LT(post_release_usage, uploaded_usage);
  EXPECT_EQ(initial_usage, post_release_usage);
  EXPECT_FALSE(gm_->IsTexture(tex_id));

  // Resources should unconditionally be released when the renderer is
  // destroyed.
  root = BuildGraph(data_, options_, 800, 800);
  renderer->DrawScene(data_->rect);
  tex_id = renderer->GetResourceGlId(data_->texture.Get());
  EXPECT_TRUE(gm_->IsTexture(tex_id));
  renderer.Reset();
  EXPECT_FALSE(gm_->IsTexture(tex_id));

  root = BuildGraph(data_, options_, 800, 800);
  renderer.Reset(new Renderer(gm_));
  renderer->DrawScene(data_->rect);
  tex_id = renderer->GetResourceGlId(data_->texture.Get());
  EXPECT_TRUE(gm_->IsTexture(tex_id));
  DestroyGraph(data_, &root);
  renderer.Reset();
  EXPECT_FALSE(gm_->IsTexture(tex_id));

  // Resources should be released when BindFramebuffer() is called, but only
  // when the kProcessReleases flag is set.
  renderer.Reset(new Renderer(gm_));
  root = BuildGraph(data_, options_, 800, 800);
  renderer->DrawScene(data_->rect);
  tex_id = renderer->GetResourceGlId(data_->texture.Get());
  EXPECT_TRUE(gm_->IsTexture(tex_id));
  DestroyGraph(data_, &root);
  renderer->ClearFlag(Renderer::kProcessReleases);
  renderer->BindFramebuffer(FramebufferObjectPtr());
  EXPECT_TRUE(gm_->IsTexture(tex_id));
  renderer->SetFlag(Renderer::kProcessReleases);
  renderer->BindFramebuffer(FramebufferObjectPtr());
  EXPECT_FALSE(gm_->IsTexture(tex_id));

  // Resources should be released when CreateOrUpdateResources() is called, but
  // only when the kProcessReleases flag is set.
  NodePtr dummy(new Node);
  root = BuildGraph(data_, options_, 800, 800);
  renderer->DrawScene(data_->rect);
  tex_id = renderer->GetResourceGlId(data_->texture.Get());
  EXPECT_TRUE(gm_->IsTexture(tex_id));
  DestroyGraph(data_, &root);
  renderer->ClearFlag(Renderer::kProcessReleases);
  renderer->CreateOrUpdateResources(dummy);
  EXPECT_TRUE(gm_->IsTexture(tex_id));
  renderer->SetFlag(Renderer::kProcessReleases);
  renderer->CreateOrUpdateResources(dummy);
  EXPECT_FALSE(gm_->IsTexture(tex_id));

  // Resources should be released when DrawScene() is called, but only when the
  // kProcessReleases flag is set.
  root = BuildGraph(data_, options_, 800, 800);
  renderer->DrawScene(data_->rect);
  tex_id = renderer->GetResourceGlId(data_->texture.Get());
  EXPECT_TRUE(gm_->IsTexture(tex_id));
  DestroyGraph(data_, &root);
  renderer->ClearFlag(Renderer::kProcessReleases);
  renderer->DrawScene(dummy);
  EXPECT_TRUE(gm_->IsTexture(tex_id));
  renderer->SetFlag(Renderer::kProcessReleases);
  renderer->DrawScene(dummy);
  EXPECT_FALSE(gm_->IsTexture(tex_id));

  // Resources should be released when BindResource() is called, but only when
  // the kProcessReleases flag is set.
  FramebufferObjectPtr fbo(new FramebufferObject(64, 64));
  fbo->SetColorAttachment(0U, FramebufferObject::Attachment(Image::kRgba8888));
  root = BuildGraph(data_, options_, 800, 800);
  renderer->DrawScene(data_->rect);
  tex_id = renderer->GetResourceGlId(data_->texture.Get());
  EXPECT_TRUE(gm_->IsTexture(tex_id));
  DestroyGraph(data_, &root);
  renderer->ClearFlag(Renderer::kProcessReleases);
  renderer->BindResource(fbo.Get());
  EXPECT_TRUE(gm_->IsTexture(tex_id));
  renderer->SetFlag(Renderer::kProcessReleases);
  renderer->BindResource(fbo.Get());
  EXPECT_FALSE(gm_->IsTexture(tex_id));
}

TEST_F(RendererTest, AbandonResources) {
  // Deleting the Renderer should result in glDelete* unless ClearAllResources
  // is called with force_abandon = true.
  base::LogChecker log_checker;
  {
    NodePtr root = BuildGraph(data_, options_, 800, 800);
    RendererPtr renderer(new Renderer(gm_));
    renderer->DrawScene(data_->rect);
    renderer.Reset();
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("DeleteTextures("));
  }
  Reset();
  {
    NodePtr root = BuildGraph(data_, options_, 800, 800);
    RendererPtr renderer(new Renderer(gm_));
    renderer->DrawScene(data_->rect);
    renderer->ClearAllResources(true);
    renderer.Reset();
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteTextures("));
  }
}

TEST_F(RendererTest, ClearCachedBindings) {
  NodePtr root = BuildGraph(data_, options_, 800, 800);

  {
    // AttributeArray (just binds attribute buffers).
    RendererPtr renderer(new Renderer(gm_));
    // This will create all resources.
    renderer->DrawScene(data_->rect);
    // Updating the array will trigger any buffers it references.
    renderer->RequestForcedUpdate(data_->attribute_array.Get());
    Reset();
    renderer->DrawScene(data_->rect);
    // The vertex array state will be refreshed, since CreateOrUpdateResources
    // sets the modified bit.
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("VertexAttribPointer"));

    renderer->ClearCachedBindings();
    Reset();
    renderer->DrawScene(data_->rect);
    // This time the VAO state will not be refreshed, since no resources
    // on which it depends were modified.
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindVertexArray"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttribPointer"));
  }

  {
    // BufferObject.
    RendererPtr renderer(new Renderer(gm_));
    // This will create all resources.
    renderer->DrawScene(data_->rect);
    renderer->RequestForcedUpdate(data_->vertex_buffer.Get());
    Reset();
    renderer->DrawScene(data_->rect);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));

    renderer->ClearCachedBindings();
    Reset();
    renderer->DrawScene(data_->rect);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  }

  {
    // ShaderProgram.
    RendererPtr renderer(new Renderer(gm_));
    // This will create all resources.
    renderer->DrawScene(data_->rect);
    renderer->RequestForcedUpdate(data_->shader.Get());
    Reset();
    renderer->DrawScene(data_->rect);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("CreateProgram"));

    renderer->ClearCachedBindings();
    Reset();
    renderer->DrawScene(data_->rect);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("UseProgram"));
  }

  {
    // Texture.
    RendererPtr renderer(new Renderer(gm_));
    // This will create all resources.
    renderer->DrawScene(data_->rect);
    renderer->RequestForcedUpdate(data_->texture.Get());
    Reset();
    renderer->DrawScene(data_->rect);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));

    renderer->ClearCachedBindings();
    Reset();
    renderer->DrawScene(data_->rect);
    // The texture is bound twice, once when created, and again when bound to a
    // uniform.
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  }

  {
    // Shape (the index buffer and the Shape's attribute array's buffers).
    RendererPtr renderer(new Renderer(gm_));
    // This will create all resources.
    renderer->DrawScene(data_->rect);
    renderer->RequestForcedShapeUpdates(data_->shape);
    Reset();
    renderer->DrawScene(data_->rect);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
    EXPECT_EQ(
        1U,
        trace_verifier_->GetCountOf("BindBuffer(GL_ELEMENT_ARRAY_BUFFER"));

    renderer->ClearCachedBindings();
    Reset();
    renderer->DrawScene(data_->rect);
    // Only the element buffer should be rebound, as part of the workaround
    // for broken drivers that don't save element buffer binding in the VAO.
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindVertexArray"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
    EXPECT_EQ(
        1U,
        trace_verifier_->GetCountOf("BindBuffer(GL_ELEMENT_ARRAY_BUFFER"));
  }
}

TEST_F(RendererTest, ForcedUpdateCausesCacheClear) {
  NodePtr root = BuildGraph(data_, options_, 800, 800);

  {
    // AttributeArray (just binds attribute buffers).
    RendererPtr renderer(new Renderer(gm_));
    // This will create all resources.
    renderer->DrawScene(data_->rect);
    Reset();
    renderer->RequestForcedUpdate(data_->attribute_array.Get());
    EXPECT_EQ(0U, trace_verifier_->GetCallCount());
    Reset();
    renderer->DrawScene(data_->rect);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  }

  {
    // BufferObject.
    RendererPtr renderer(new Renderer(gm_));
    // This will create all resources.
    renderer->DrawScene(data_->rect);
    Reset();
    renderer->RequestForcedUpdate(data_->vertex_buffer.Get());
    EXPECT_EQ(0U, trace_verifier_->GetCallCount());
    renderer->DrawScene(data_->rect);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  }

  {
    // ShaderProgram.
    RendererPtr renderer(new Renderer(gm_));
    // This will create all resources.
    renderer->DrawScene(data_->rect);
    Reset();
    renderer->RequestForcedUpdate(data_->shader.Get());
    EXPECT_EQ(0U, trace_verifier_->GetCallCount());
    renderer->DrawScene(data_->rect);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("UseProgram"));
  }

  {
    // Texture.
    RendererPtr renderer(new Renderer(gm_));
    // This will create all resources.
    renderer->DrawScene(data_->rect);
    Reset();
    renderer->RequestForcedUpdate(data_->texture.Get());
    EXPECT_EQ(0U, trace_verifier_->GetCallCount());
    renderer->DrawScene(data_->rect);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  }

  {
    // Shape (the index buffer and the Shape's attribute array's buffers).
    RendererPtr renderer(new Renderer(gm_));
    // This will create all resources.
    renderer->DrawScene(data_->rect);
    Reset();
    renderer->RequestForcedShapeUpdates(data_->shape);
    EXPECT_EQ(0U, trace_verifier_->GetCallCount());
    Reset();
    renderer->DrawScene(data_->rect);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
    EXPECT_EQ(
        1U,
        trace_verifier_->GetCountOf("BindBuffer(GL_ELEMENT_ARRAY_BUFFER"));
  }

  {
    // Entire scene.
    RendererPtr renderer(new Renderer(gm_));
    renderer->DrawScene(data_->rect);
    Reset();
    renderer->RequestForcedUpdates(data_->rect);
    EXPECT_EQ(0U, trace_verifier_->GetCallCount());
    renderer->DrawScene(data_->rect);
    EXPECT_EQ(3U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("UseProgram"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
    EXPECT_EQ(
        1U,
        trace_verifier_->GetCountOf("BindBuffer(GL_ELEMENT_ARRAY_BUFFER"));
  }
}

TEST_F(RendererTest, DebugLabels) {
  NodePtr root = BuildGraph(data_, options_, 800, 800);

  RendererPtr renderer(new Renderer(gm_));
  renderer->DrawScene(data_->rect);

  Reset();
  data_->attribute_array->SetLabel("label");
  renderer->DrawScene(data_->rect);
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "LabelObject"))
          .HasArg(1, "GL_VERTEX_ARRAY_OBJECT")
          .HasArg(3, "5")
          .HasArg(4, "\"label\""));

  Reset();
  data_->vertex_buffer->SetLabel("label");
  renderer->DrawScene(data_->rect);
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "LabelObject"))
          .HasArg(1, "GL_BUFFER_OBJECT")
          .HasArg(3, "5")
          .HasArg(4, "\"label\""));

  Reset();
  data_->shader->SetLabel("label");
  renderer->DrawScene(data_->rect);
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "LabelObject"))
          .HasArg(1, "GL_PROGRAM_OBJECT")
          .HasArg(3, "5")
          .HasArg(4, "\"label\""));

  Reset();
  data_->shader->GetVertexShader()->SetLabel("label");
  renderer->DrawScene(data_->rect);
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "LabelObject"))
          .HasArg(1, "GL_SHADER_OBJECT")
          .HasArg(3, "5")
          .HasArg(4, "\"label\""));

  Reset();
  data_->shader->GetFragmentShader()->SetLabel("label");
  renderer->DrawScene(data_->rect);
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "LabelObject"))
          .HasArg(1, "GL_SHADER_OBJECT")
          .HasArg(3, "5")
          .HasArg(4, "\"label\""));

  Reset();
  data_->texture->SetLabel("label");
  renderer->DrawScene(data_->rect);
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "LabelObject"))
          .HasArg(1, "GL_TEXTURE")
          .HasArg(3, "5")
          .HasArg(4, "\"label\""));

  FramebufferObjectPtr fbo(new FramebufferObject(128, 128));
  fbo->SetColorAttachment(0U, FramebufferObject::Attachment(Image::kRgba4Byte));
  fbo->SetLabel("label");

  Reset();
  renderer->BindFramebuffer(fbo);
  renderer->DrawScene(data_->rect);
  renderer->BindFramebuffer(FramebufferObjectPtr());
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "LabelObject"))
          .HasArg(1, "GL_FRAMEBUFFER")
          .HasArg(3, "5")
          .HasArg(4, "\"label\""));
}

TEST_F(RendererTest, DebugMarkers) {
  NodePtr root = BuildGraph(data_, options_, 800, 800);

  RendererPtr renderer(new Renderer(gm_));
  Reset();
  renderer->DrawScene(root);

  const std::vector<std::string> calls =
      base::SplitString(trace_verifier_->GetTraceString(), "\n");
  // Check that certain functions are grouped.
  const std::string plane_shader =
      "Plane shader [" + base::ValueToString(data_->shader.Get()) + "]";
  const std::string plane_vertex_shader =
      "Plane shader vertex shader [" +
      base::ValueToString(data_->shader->GetVertexShader().Get()) + "]";
  const std::string texture_address = base::ValueToString(data_->texture.Get());
  const std::string cubemap_address = base::ValueToString(data_->cubemap.Get());
  const std::string texture_length =
      base::ValueToString(texture_address.length() + 10U);
  const std::string cubemap_length =
      base::ValueToString(cubemap_address.length() + 18U);
  std::string texture_markers =
      "-->Texture [" + texture_address + "]:\n"
      "-->Texture [" + texture_address + "]:\n"
      "-->Cubemap Texture [" + cubemap_address + "]:\n";

  // 
  EXPECT_EQ(">" + plane_shader + ":", calls[7]);
  EXPECT_EQ("-->" + plane_vertex_shader + ":", calls[8]);
  EXPECT_EQ("    CreateShader(type = GL_VERTEX_SHADER)", calls[9]);

  std::string modelview_markers;
  {
    Reset();
    // There should be no ill effects from popping early.
    renderer->PopDebugMarker();
    renderer->PopDebugMarker();
    renderer->PopDebugMarker();
    renderer->DrawScene(root);
    // uModelviewMatrix uses a temporary Uniform when combining so we need to
    // extract the string from the trace to get proper addresses.
    const std::string actual = trace_verifier_->GetTraceString();
    size_t start = actual.find("-->uModelviewMatrix");
    EXPECT_NE(std::string::npos, start);
    size_t end =
        actual.find_first_of('\n', actual.find_first_of('\n', start) + 1);
    modelview_markers = actual.substr(start, end - start + 1);
    // Check for a pop.
    const std::string expected(
        "Clear(mask = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)\n"
        ">" + plane_shader + ":\n" +
        texture_markers +
        modelview_markers +
        "DrawElements(mode = GL_TRIANGLES, count = 6, type = "
        "GL_UNSIGNED_SHORT, indices = NULL)\n");
    EXPECT_TRUE(base::testing::MultiLineStringsEqual(expected, actual));
  }

  // Test a marker wrapping a draw.
  {
    Reset();
    renderer->PushDebugMarker("Marker");
    renderer->PopDebugMarker();
    renderer->DrawScene(root);
    const std::string expected(
        ">Marker:\n"
        "Clear(mask = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)\n"
        ">" + plane_shader + ":\n" +
        texture_markers +
        modelview_markers +
        "DrawElements(mode = GL_TRIANGLES, count = 6, type = "
        "GL_UNSIGNED_SHORT, indices = NULL)\n");
    EXPECT_TRUE(base::testing::MultiLineStringsEqual(
        expected, trace_verifier_->GetTraceString()));
  }

  texture_markers = base::ReplaceString(texture_markers, "    ", "      ");
  texture_markers = base::ReplaceString(texture_markers, "-->", "---->");
  modelview_markers = base::ReplaceString(modelview_markers, "    ", "      ");
  modelview_markers = base::ReplaceString(modelview_markers, "-->", "---->");
  {
    Reset();
    renderer->PushDebugMarker("My scene");
    renderer->DrawScene(root);
    renderer->PopDebugMarker();
    // Extra pops should have no ill effects.
    renderer->PopDebugMarker();
    renderer->PopDebugMarker();
    const std::string expected(
        ">My scene:\n"
        "  Clear(mask = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)\n"
        "-->" + plane_shader + ":\n" +
        texture_markers +
        modelview_markers +
        "  DrawElements(mode = GL_TRIANGLES, count = 6, type = "
        "GL_UNSIGNED_SHORT, indices = NULL)\n");
    EXPECT_TRUE(base::testing::MultiLineStringsEqual(
        expected, trace_verifier_->GetTraceString()));
  }

  {
    Reset();
    renderer->PushDebugMarker("My scene");
    renderer->DrawScene(root);
    const std::string expected(
        ">My scene:\n"
        "  Clear(mask = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)\n"
        "-->" + plane_shader + ":\n" +
        texture_markers +
        modelview_markers +
        "  DrawElements(mode = GL_TRIANGLES, count = 6, type = "
        "GL_UNSIGNED_SHORT, indices = NULL)\n");
    EXPECT_TRUE(base::testing::MultiLineStringsEqual(
        expected, trace_verifier_->GetTraceString()));

    Reset();
    renderer->DrawScene(root);
    // There should still be indentation since we never popped the old marker.
    const std::string expected2(
        "  Clear(mask = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)\n"
        "-->" + plane_shader + ":\n" +
        texture_markers +
        modelview_markers +
        "  DrawElements(mode = GL_TRIANGLES, count = 6, type = "
        "GL_UNSIGNED_SHORT, indices = NULL)\n");
    EXPECT_TRUE(base::testing::MultiLineStringsEqual(
        expected2, trace_verifier_->GetTraceString()));

    texture_markers = base::ReplaceString(texture_markers, "    ", "      ");
    texture_markers = base::ReplaceString(texture_markers, "-->", "---->");
    modelview_markers =
        base::ReplaceString(modelview_markers, "    ", "      ");
    modelview_markers =
        base::ReplaceString(modelview_markers, "-->", "---->");
    Reset();
    renderer->PushDebugMarker("Marker 2");
    renderer->DrawScene(root);
    renderer->PopDebugMarker();
    renderer->PopDebugMarker();
    const std::string expected3(
        "-->Marker 2:\n"
        "    Clear(mask = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)\n"
        "---->" + plane_shader + ":\n" +
        texture_markers +
        modelview_markers +
        "    DrawElements(mode = GL_TRIANGLES, count = 6, type = "
        "GL_UNSIGNED_SHORT, indices = NULL)\n");
    EXPECT_TRUE(base::testing::MultiLineStringsEqual(
        expected3, trace_verifier_->GetTraceString()));
  }
}

TEST_F(RendererTest, MatrixAttributes) {
  base::LogChecker log_checker;
  RendererPtr renderer(new Renderer(gm_));

  static const char* kVertexShaderString =
      "attribute mat2 aMat2;\n"
      "attribute mat3 aMat3;\n"
      "attribute mat4 aMat4;\n";

  static const char* kFragmentShaderString =
      "uniform vec3 uniform1;\n"
      "uniform vec3 uniform2;\n";

  {
    NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
    ShaderInputRegistryPtr reg(new ShaderInputRegistry);
    reg->IncludeGlobalRegistry();

    data_->attribute_array = new AttributeArray;
    data_->attribute_array->AddAttribute(
        reg->Create<Attribute>("aMat2", math::Matrix2f(1.f, 2.f,
                                                       3.f, 4.f)));
    data_->attribute_array->AddAttribute(
        reg->Create<Attribute>("aMat3",
                               math::Matrix3f(1.f, 2.f, 3.f,
                                              4.f, 5.f, 6.f,
                                              7.f, 8.f, 9.f)));
    data_->attribute_array->AddAttribute(
        reg->Create<Attribute>("aMat4",
                               math::Matrix4f(1.f, 2.f, 3.f, 4.f,
                                              5.f, 6.f, 7.f, 8.f,
                                              9.f, 10.f, 11.f, 12.f,
                                              13.f, 14.f, 15.f, 16.f)));
    reg->Create<Uniform>("uniform1", math::Vector3f(1.f, 2.f, 3.f));
    reg->Create<Uniform>("uniform2", math::Vector3f(1.f, 2.f, 3.f));
    data_->shader = ShaderProgram::BuildFromStrings(
        "Shader", reg, kVertexShaderString, kFragmentShaderString,
        base::AllocatorPtr());
    data_->shape->SetAttributeArray(data_->attribute_array);
    data_->rect->SetShaderProgram(data_->shader);
    data_->rect->ClearUniforms();
    data_->rect->AddUniform(
        reg->Create<Uniform>("uniform1", math::Vector3f::Zero()));
    data_->rect->AddUniform(
            reg->Create<Uniform>("uniform2", math::Vector3f::Zero()));
    Reset();
    renderer->DrawScene(root);
    // Check that the columns of matrix attributes are sent individually.
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("VertexAttrib2fv"));
    EXPECT_EQ(3U, trace_verifier_->GetCountOf("VertexAttrib3fv"));
    EXPECT_EQ(4U, trace_verifier_->GetCountOf("VertexAttrib4fv"));
    EXPECT_FALSE(log_checker.HasAnyMessages());
  }

  // Try the matrices as buffer objects.
  {
    NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
    ShaderInputRegistryPtr reg(new ShaderInputRegistry);
    reg->IncludeGlobalRegistry();

    data_->attribute_array = new AttributeArray;
    data_->attribute_array->AddAttribute(
        reg->Create<Attribute>(
            "aMat2", BufferObjectElement(
                data_->vertex_buffer, data_->vertex_buffer->AddSpec(
                    BufferObject::kFloatMatrixColumn2, 2, 0))));
    data_->attribute_array->AddAttribute(
        reg->Create<Attribute>(
            "aMat3", BufferObjectElement(
                data_->vertex_buffer, data_->vertex_buffer->AddSpec(
                    BufferObject::kFloatMatrixColumn3, 3, 16))));
    data_->attribute_array->AddAttribute(
        reg->Create<Attribute>(
            "aMat4", BufferObjectElement(
                data_->vertex_buffer, data_->vertex_buffer->AddSpec(
                    BufferObject::kFloatMatrixColumn4, 4, 48))));
    reg->Create<Uniform>("uniform1", math::Vector3f(1.f, 2.f, 3.f));
    reg->Create<Uniform>("uniform2", math::Vector3f(1.f, 2.f, 3.f));
    data_->shader = ShaderProgram::BuildFromStrings(
        "Shader", reg, kVertexShaderString, kFragmentShaderString,
        base::AllocatorPtr());
    data_->shape->SetAttributeArray(data_->attribute_array);
    data_->rect->SetShaderProgram(data_->shader);
    data_->rect->ClearUniforms();
    data_->rect->AddUniform(
        reg->Create<Uniform>("uniform1", math::Vector3f::Zero()));
    data_->rect->AddUniform(
        reg->Create<Uniform>("uniform2", math::Vector3f::Zero()));
    Reset();
    renderer->DrawScene(root);
    // Each column must be enabled separately.
    for (int i = 0; i < 9; ++i)
      EXPECT_EQ(1U,
                trace_verifier_->GetCountOf("EnableVertexAttribArray(0x" +
                                            base::ValueToString(i) + ")"));
    // Check that the each column of the matrix attributes were sent.
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("VertexAttribPointer(0x0, 2"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("VertexAttribPointer(0x1, 2"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("VertexAttribPointer(0x2, 3"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("VertexAttribPointer(0x3, 3"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("VertexAttribPointer(0x4, 3"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("VertexAttribPointer(0x5, 4"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("VertexAttribPointer(0x6, 4"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("VertexAttribPointer(0x7, 4"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("VertexAttribPointer(0x8, 4"));
    EXPECT_FALSE(log_checker.HasAnyMessages());
  }
}

TEST_F(RendererTest, BackgroundUpload) {
  GlContextPtr gl_context_background =
      FakeGlContext::CreateShared(*gl_context_);
  portgfx::GlContext::MakeCurrent(gl_context_background);
  FakeGraphicsManagerPtr gm(new FakeGraphicsManager());
  // Ideally, we could have a single texture unit, but the implementation
  // doesn't allow it, so we work around it below.
  gm->SetMaxTextureImageUnits(2);

  RendererPtr renderer(new Renderer(gm));

  // Create one Image to use for all Textures that we create.
  static const uint32 kImageWidth = 16;
  static const uint32 kImageHeight = 16;
  ImagePtr image(new Image);
  base::AllocatorPtr alloc =
      base::AllocationManager::GetDefaultAllocatorForLifetime(
          base::kShortTerm);
  base::DataContainerPtr data = base::DataContainer::CreateOverAllocated<uint8>(
      kImageWidth * kImageHeight, nullptr, alloc);
  image->Set(Image::kLuminance, kImageWidth, kImageHeight, data);

  // Create textures.
  TexturePtr texture1(new Texture);
  TexturePtr texture2(new Texture);
  TexturePtr texture1_for_unit_1(new Texture);
  TexturePtr texture2_for_unit_1(new Texture);
  texture1->SetImage(0U, image);
  texture2->SetImage(0U, image);
  texture1_for_unit_1->SetImage(0U, image);
  texture2_for_unit_1->SetImage(0U, image);
  SamplerPtr sampler(new Sampler());
  texture1->SetSampler(sampler);
  texture2->SetSampler(sampler);
  texture1_for_unit_1->SetSampler(sampler);
  texture2_for_unit_1->SetSampler(sampler);

  // Ping-pong the textures so that texture1 and texture2 both use image unit 0.
  renderer->CreateOrUpdateResource(texture1.Get());
  renderer->CreateOrUpdateResource(texture1_for_unit_1.Get());
  renderer->CreateOrUpdateResource(texture2.Get());
  renderer->CreateOrUpdateResource(texture2_for_unit_1.Get());

  // Rebind texture2 on the main thread, so that it is associated with the main
  // GL context's ResourceBinder. It should be unbound from the background GL
  // context's ResourceBinder.
  portgfx::GlContext::MakeCurrent(gl_context_);
  renderer->CreateOrUpdateResource(texture2.Get());
  // Destroy texture2, calling OnDestroyed() in its resource.
  texture2.Reset(nullptr);
  // This will trigger the actual release.
  renderer->DrawScene(NodePtr());

  // Go back to the other GL context and bind texture1 there, which will replace
  // the resource at image unit 0.
  portgfx::GlContext::MakeCurrent(gl_context_background);
  renderer->CreateOrUpdateResource(texture1.Get());

  // Set back the original GlContext.
  portgfx::GlContext::MakeCurrent(gl_context_);
}

// The following multithreaded tests cannot run on asmjs, where there are no
// threads.
#if !defined(ION_PLATFORM_ASMJS)
TEST_F(RendererTest, MultiThreadedDataLoading) {
  // Test that resources can be uploaded on a separate thread using a share
  // context via a FakeGlContext.

  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);

  // AttributeArray (just binds attribute buffers).
  {
    GlContextPtr share_context = FakeGlContext::CreateShared(*gl_context_);
    // Updating the array will trigger any buffers it references.
    port::ThreadStdFunc func =
        std::bind(UploadThread<AttributeArray>, renderer, share_context,
                  data_->attribute_array.Get());
    base::ThreadSpawner spawner("worker", func);
  }
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));
  Reset();
  renderer->DrawScene(data_->rect);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));

  // BufferObject.
  renderer->ClearAllResources();
  Reset();
  {
    GlContextPtr share_context = FakeGlContext::CreateShared(*gl_context_);
    port::ThreadStdFunc func =
        std::bind(UploadThread<BufferObject>, renderer, share_context,
                  data_->vertex_buffer.Get());
    base::ThreadSpawner spawner("worker", func);
  }
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));
  Reset();
  renderer->DrawScene(data_->rect);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));

  // ShaderProgram.
  renderer->ClearAllResources();
  Reset();
  {
    GlContextPtr share_context = FakeGlContext::CreateShared(*gl_context_);
    port::ThreadStdFunc func = std::bind(UploadThread<ShaderProgram>, renderer,
                                         share_context, data_->shader.Get());
    base::ThreadSpawner spawner("worker", func);
  }
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("CreateProgram"));
  Reset();
  renderer->DrawScene(data_->rect);
  // Since the program is not marked as concurrent, it should only be created
  // once and shared between threads.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("CreateProgram"));

  // Texture.
  renderer->ClearAllResources();
  Reset();
  {
    GlContextPtr share_context = FakeGlContext::CreateShared(*gl_context_);
    port::ThreadStdFunc func = std::bind(UploadThread<Texture>, renderer,
                                         share_context, data_->texture.Get());
    base::ThreadSpawner spawner("worker", func);
  }
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexImage2D(GL_TEXTURE_2D"));
  Reset();
  renderer->DrawScene(data_->rect);
  // The texture gets bound twice, once for the resource change, and again for
  // the uniform binding.
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D(GL_TEXTURE_2D"));

  // Shape (the index buffer and the Shape's attribute array's buffers).
  renderer->ClearAllResources();
  Reset();
  {
    GlContextPtr share_context = FakeGlContext::CreateShared(*gl_context_);
    port::ThreadStdFunc func = std::bind(UploadThread<ShapePtr>, renderer,
                                         share_context, &data_->shape);
    base::ThreadSpawner spawner("worker", func);
  }
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf("BindBuffer(GL_ELEMENT_ARRAY_BUFFER"));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf("BufferData(GL_ELEMENT_ARRAY_BUFFER"));
  Reset();
  renderer->DrawScene(data_->rect);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf("BindBuffer(GL_ELEMENT_ARRAY_BUFFER"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BufferData"));
}
#endif

TEST_F(RendererTest, IndexBuffers32Bit) {
  base::LogChecker log_checker;
  gm_->EnableFeature(GraphicsManager::kElementIndex32Bit, false);
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight, true);
  Reset();

  renderer->DrawScene(root);
  EXPECT_TRUE(log_checker.HasMessage(
      "ERROR", "32-bit element indices are not supported"));

  gm_->EnableFeature(GraphicsManager::kElementIndex32Bit, true);
  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST_F(RendererTest, ResolveMultisampleFramebuffer) {
  RendererPtr renderer(new Renderer(gm_));

  int sample_size = 4;
  FramebufferObjectPtr ms_fbo(new FramebufferObject(128, 128));
  ms_fbo->SetColorAttachment(
      0U, FramebufferObject::Attachment::CreateMultisampled(Image::kRgba8888,
                                                            sample_size));
  FramebufferObject::Attachment ms_packed_depth_stencil =
      FramebufferObject::Attachment::CreateMultisampled(
          Image::kRenderbufferDepth24Stencil8, sample_size);
  ms_fbo->SetDepthAttachment(ms_packed_depth_stencil);
  ms_fbo->SetStencilAttachment(ms_packed_depth_stencil);
  FramebufferObjectPtr dest_fbo(new FramebufferObject(128, 128));
  dest_fbo->SetColorAttachment(
      0U, FramebufferObject::Attachment(Image::kRgba8888));
  FramebufferObject::Attachment dest_packed_depth_stencil =
      FramebufferObject::Attachment(Image::kRenderbufferDepth24Stencil8);
  dest_fbo->SetDepthAttachment(dest_packed_depth_stencil);
  dest_fbo->SetStencilAttachment(dest_packed_depth_stencil);

  // Perform tests for all (8) buffer-bit permutations.
  uint32_t all_buffer_bits = Renderer::kColorBufferBit |
                             Renderer::kDepthBufferBit |
                             Renderer::kStencilBufferBit;
  EXPECT_EQ(all_buffer_bits, 7U);
  // Also perform iterations over all masks containing a valid combination of
  // buffer bits plus one invalid bit.
  uint32_t mask_max = (all_buffer_bits+1) | all_buffer_bits;
  for (uint32_t mask = 0U; mask <= mask_max; mask++) {
    SCOPED_TRACE(::testing::Message() << "mask " << mask);
    gm_->EnableFeature(GraphicsManager::kFramebufferBlit, true);
    {
      base::LogChecker log_checker;
      Reset();
      renderer->ResolveMultisampleFramebuffer(ms_fbo, dest_fbo, mask);
      // Ensure that the FBOs are updated on the first valid call.
      EXPECT_EQ(mask == 1 ? 2U : 0U,
                trace_verifier_->GetCountOf("RenderbufferStorage("));
      EXPECT_EQ(mask == 1 ? 2U : 0U,
                trace_verifier_->GetCountOf("RenderbufferStorageMultisample"));
      if (mask == 0U) {
        EXPECT_EQ(0U, trace_verifier_->GetCountOf("BlitFramebuffer"));
      } else if (mask > all_buffer_bits) {
        EXPECT_EQ(0U, trace_verifier_->GetCountOf("BlitFramebuffer"));
        EXPECT_TRUE(log_checker.HasMessage(
            "ERROR", "Invalid mask argument. Must be a combination of "
                     "kColorBufferBit, kDepthBufferBit and kStencilBufferBit"));
      } else {
        EXPECT_EQ(1U, trace_verifier_->GetCountOf("BlitFramebuffer"));

        // Verify proper Buffer bits.
        const size_t index = trace_verifier_->GetNthIndexOf(0,
                                                            "BlitFramebuffer");
        std::string mask_argument =
            trace_verifier_->VerifyCallAt(index).GetArg(9);
        EXPECT_EQ((mask & Renderer::kColorBufferBit) != 0U,
                  mask_argument.find("GL_COLOR_BUFFER_BIT") !=
                      std::string::npos);
        EXPECT_EQ((mask & Renderer::kDepthBufferBit) != 0U,
                  mask_argument.find("GL_DEPTH_BUFFER_BIT") !=
                      std::string::npos);
        EXPECT_EQ((mask & Renderer::kStencilBufferBit) != 0U,
                  mask_argument.find("GL_STENCIL_BUFFER_BIT") !=
                      std::string::npos);

        // Verify the previous framebuffer (i.e., 0) is restored after the call.
        EXPECT_GE(1U, trace_verifier_->GetCountOf(
            "BindFramebuffer(GL_FRAMEBUFFER, 0x0)"));
      }
        EXPECT_EQ(0U,
                  trace_verifier_->GetCountOf("ResolveMultisampleFramebuffer"));
        EXPECT_EQ(0U, renderer->GetResourceGlId(
            renderer->GetCurrentFramebuffer().Get()));
    }

    gm_->EnableFeature(GraphicsManager::kFramebufferBlit, false);
    gm_->EnableFeature(GraphicsManager::kMultisampleFramebufferResolve, true);
    {
      base::LogChecker log_checker;
      Reset();
      renderer->ResolveMultisampleFramebuffer(ms_fbo, dest_fbo, mask);
      EXPECT_EQ(0U, trace_verifier_->GetCountOf("BlitFramebuffer"));
      if (mask == 0U) {
        EXPECT_EQ(0U,
                  trace_verifier_->GetCountOf("ResolveMultisampleFramebuffer"));
      } else if (mask > all_buffer_bits) {
        EXPECT_EQ(0U,
                  trace_verifier_->GetCountOf("ResolveMultisampleFramebuffer"));
        EXPECT_TRUE(log_checker.HasMessage(
            "ERROR", "Invalid mask argument. Must be a combination of "
                     "kColorBufferBit, kDepthBufferBit and kStencilBufferBit"));
      } else {
        EXPECT_EQ(1U,
                  trace_verifier_->GetCountOf("ResolveMultisampleFramebuffer"));
        // Verify the previous framebuffer (i.e., 0) is restored after the call.
        EXPECT_GE(1U, trace_verifier_->GetCountOf(
            "BindFramebuffer(GL_FRAMEBUFFER, 0x0)"));
        std::string warnings = log_checker.GetLogString();
        if (mask & Renderer::kDepthBufferBit) {
          EXPECT_TRUE(warnings.find("Multisampled depth buffer resolves are "
                                    "not supported by this platform.") !=
                      std::string::npos);
        }
        if (mask & Renderer::kStencilBufferBit) {
          EXPECT_TRUE(warnings.find("Multisampled stencil buffer resolves are "
                                    "not supported by this platform.") !=
                      std::string::npos);
        }
      }
      EXPECT_EQ(0U, renderer->GetResourceGlId(
          renderer->GetCurrentFramebuffer().Get()));
    }

    gm_->EnableFeature(GraphicsManager::kFramebufferBlit, false);
    gm_->EnableFeature(GraphicsManager::kMultisampleFramebufferResolve, false);
    {
      base::LogChecker log_checker;
      Reset();
      renderer->ResolveMultisampleFramebuffer(ms_fbo, dest_fbo, mask);
      EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindFramebuffer"));
      EXPECT_EQ(0U, trace_verifier_->GetCountOf("BlitFramebuffer"));
      EXPECT_EQ(0U,
                trace_verifier_->GetCountOf("ResolveMultisampleFramebuffer"));
      if (mask > all_buffer_bits) {
        EXPECT_TRUE(log_checker.HasMessage(
            "ERROR", "Invalid mask argument. Must be a combination of "
                     "kColorBufferBit, kDepthBufferBit and kStencilBufferBit"));
      } else if (mask != 0U) {
        EXPECT_TRUE(log_checker.HasMessage(
            "WARNING", "No multisampled framebuffer functions available."));
      }
    }
  }
}

TEST_F(RendererTest, ExternalFramebufferDestruction) {
  // Check whether dropping references to the bound framebuffer works correctly.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight, true);

  {
    FramebufferObjectPtr dest_fbo(new FramebufferObject(256, 256));
    dest_fbo->SetColorAttachment(
        0U, FramebufferObject::Attachment(Image::kRgba8888));
    renderer->BindFramebuffer(dest_fbo);
    renderer->DrawScene(root);
  }

  FramebufferObjectPtr fbo = renderer->GetCurrentFramebuffer();
  EXPECT_TRUE(fbo.Get() == nullptr);
  renderer->DrawScene(root);
}

TEST_F(RendererTest, FramebufferGracefulDegradation) {
  // SetDrawBuffer and SetReadBuffer calls that do not change the values from
  // OpenGL defaults should not trigger error messages.
  RendererPtr renderer(new Renderer(gm_));
  FramebufferObjectPtr fbo(new FramebufferObject(128, 128));
  fbo->SetColorAttachment(
      0U, FramebufferObject::Attachment(Image::kRgba8888));
  base::LogChecker log_checker;

  gm_->EnableFeature(GraphicsManager::kDrawBuffers, false);
  gm_->EnableFeature(GraphicsManager::kReadBuffer, false);

  fbo->SetDrawBuffers({0, -1, 1, -1});
  renderer->BindFramebuffer(fbo);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "DrawBuffers is not available"));

  fbo->SetDrawBuffers({1, -1, -1, -1});
  renderer->BindFramebuffer(fbo);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "DrawBuffers is not available"));

  fbo->SetDrawBuffer(0U, 0);
  fbo->SetDrawBuffer(1U, -1);
  fbo->SetDrawBuffer(2U, -1);
  fbo->SetDrawBuffer(3U, -1);
  renderer->BindFramebuffer(fbo);
  EXPECT_FALSE(log_checker.HasAnyMessages());

  fbo->SetDrawBuffers({0, -1, -1, -1});
  fbo->SetReadBuffer(2);
  renderer->BindFramebuffer(fbo);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "ReadBuffer is not available"));

  fbo->SetReadBuffer(0);
  renderer->BindFramebuffer(fbo);
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // Test that having only a depth attachment works.
  // We have to manually call ReadBuffer and DrawBuffers on the graphics
  // manager, since the underlying FakeGlContext will still enforce draw buffer
  // and read buffer incompleteness rules for framebuffer objects.
  GLenum buffer = GL_NONE;
  gm_->ReadBuffer(buffer);
  gm_->DrawBuffers(1, &buffer);
  fbo->ResetDrawBuffers();
  fbo->ResetReadBuffer();
  fbo->SetDepthAttachment(
      FramebufferObject::Attachment(Image::kRenderbufferDepth16));
  fbo->SetColorAttachment(0U, FramebufferObject::Attachment());
  renderer->BindFramebuffer(fbo);
  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST_F(RendererTest, ContextChangePolicy) {
  const Renderer::ContextChangePolicy kPolicies[] = {
    Renderer::kAbandonResources,
    Renderer::kIgnore,
  };
  for (Renderer::ContextChangePolicy policy : kPolicies) {
    RendererPtr renderer(new Renderer(gm_));
    renderer->SetContextChangePolicy(policy);
    NodePtr root = BuildGraph(data_, options_, kWidth, kHeight, true);
    renderer->DrawScene(root);
    ShaderProgramPtr program = root->GetShaderProgram();
    GLint shader_glid = renderer->GetResourceGlId(program.Get());
    EXPECT_NE(0, shader_glid);
    EXPECT_TRUE(gm_->IsProgram(shader_glid));

    base::LogChecker log_checker;
    testing::TraceVerifier trace_verifier(gm_.Get());
    GlContextPtr other_context = FakeGlContext::Create(500, 600);
    portgfx::GlContext::MakeCurrent(other_context);
    FramebufferObjectPtr fbo(new FramebufferObject(128, 128));
    fbo->SetDepthAttachment(
        FramebufferObject::Attachment(Image::kRenderbufferDepth16));
    renderer->BindFramebuffer(fbo);
    EXPECT_FALSE(log_checker.HasAnyMessages());
    portgfx::GlContext::MakeCurrent(gl_context_);
    EXPECT_TRUE(gm_->IsProgram(shader_glid));
    renderer.Reset();
    // Neither kAbandonResources nor kIgnore should result in Delete* calls
    // when the renderer is destroyed.
    EXPECT_EQ(0U, trace_verifier.GetCountOf("Delete"));
  }
}

TEST_F(RendererTest, TransformFeedback) {
  if (!gm_->IsFeatureAvailable(GraphicsManager::kTransformFeedback)) {
    return;
  }
  RendererPtr renderer(new Renderer(gm_));
  gm_->EnableErrorChecking(true);
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight, false, false);
  data_->shader->SetCapturedVaryings({"vTexCoords"});
  renderer->DrawScene(root);

  // Obviously, there should be no Begin/End if TF is not active.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BeginTransformFeedback"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("EndTransformFeedback"));

  // The selection of chosen varyings should be sent to the shader at link time,
  // regardless of whether a transform feedback object is active.
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("TransformFeedbackVaryings"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("LinkProgram"));
  trace_verifier_->Reset();

  // Create a buffer object to capture vertex data.
  BufferObjectPtr buffer(new BufferObject);
  const size_t vert_count = 4;
  using ion::math::Vector4f;
  Vector4f* verts = new Vector4f[vert_count];
  ion::base::DataContainerPtr container =
      ion::base::DataContainer::Create<Vector4f>(
          verts, ion::base::DataContainer::ArrayDeleter<Vector4f>,
          true, buffer->GetAllocator());
  buffer->SetData(container, sizeof(verts[0]), vert_count,
                  ion::gfx::BufferObject::kStreamDraw);
  TransformFeedbackPtr tf(new TransformFeedback(buffer));

  // The low-level call to BeginTransformFeedback is deferred until DrawNode
  // because the shader program can't be changed within a Begin/End.
  renderer->BeginTransformFeedback(tf);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BeginTransformFeedback"));

  // Drawing the scene should cause the GL transform feedback object to be
  // created, and include Begin/End calls.
  renderer->DrawScene(root);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BeginTransformFeedback"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenTransformFeedbacks"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindTransformFeedback"));
  renderer->EndTransformFeedback();
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("EndTransformFeedback"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("BindTransformFeedback"));

  // Ensure that TF is deactivated properly.
  trace_verifier_->Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BeginTransformFeedback"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("EndTransformFeedback"));
}

typedef RendererTest RendererDeathTest;
TEST_F(RendererDeathTest, AbortPolicy) {
  // Verify that reusing a renderer after changing the GL context aborts the
  // program.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight, true);
  renderer->DrawScene(root);

  GlContextPtr initial_context = portgfx::GlContext::GetCurrent();
  GlContextPtr other_context = FakeGlContext::Create(500, 600);
  portgfx::GlContext::MakeCurrent(other_context);
  FramebufferObjectPtr fbo(new FramebufferObject(128, 128));
  fbo->SetDepthAttachment(
      FramebufferObject::Attachment(Image::kRenderbufferDepth16));
  EXPECT_DEATH_IF_SUPPORTED(renderer->BindFramebuffer(fbo),
                            "OpenGL context has changed");

  // Restore the previous GL context to allow proper resource destruction.
  portgfx::GlContext::MakeCurrent(initial_context);
}

TEST_F(RendererTest, ManyRenderers) {
  base::LogChecker log_checker;
  std::vector<RendererPtr> renderers;
  for (size_t i = 0; i < ResourceHolder::kInlineResourceGroups + 1; ++i) {
    renderers.emplace_back(new Renderer(gm_));
  }
  EXPECT_TRUE(log_checker.HasMessage("WARNING",
                                     "Performance may be adversely affected"));
}

TEST_F(RendererTest, BufferlessShapeTest) {
  RendererPtr renderer(new Renderer(gm_));
  gfx::ShapePtr bufferless_shape =
      gfxutils::BuildPrimitivesList(gfx::Shape::kTriangles, 3);
  gfx::NodePtr root (new gfx::Node());
  root->AddShape(bufferless_shape);
  renderer->DrawScene(root);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawArrays(GL_TRIANGLES"));
}

}  // namespace gfx
}  // namespace ion

#endif  // ION_PRODUCTION
