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

#include "ion/gfx/updatestatetable.h"

#include "ion/base/enumhelper.h"
#include "ion/base/indexmap.h"
#include "ion/base/logging.h"
#include "ion/gfx/graphicsmanager.h"
#include "ion/gfx/statetable.h"
#include "ion/math/vector.h"

namespace ion {
namespace gfx {

namespace {

//-----------------------------------------------------------------------------
//
// Helper functions.
//
//-----------------------------------------------------------------------------

// Clear buffers if st has values set for Clear*Value, and saves any changed
// state into the passed save_state.
static void ClearBuffers(const StateTable& st, StateTable* save_state,
                         GraphicsManager* gm) {
  GLbitfield mask = 0;
  if (st.IsValueSet(StateTable::kClearColorValue)) {
    const math::Vector4f& color = st.GetClearColor();
    if (st.AreSettingsEnforced() || color != save_state->GetClearColor()) {
      save_state->SetClearColor(color);
      gm->ClearColor(color[0], color[1], color[2], color[3]);
    }
    mask |= GL_COLOR_BUFFER_BIT;
  }
  if (st.IsValueSet(StateTable::kClearDepthValue)) {
    const float value = st.GetClearDepthValue();
    if (st.AreSettingsEnforced() || value != save_state->GetClearDepthValue()) {
      save_state->SetClearDepthValue(value);
      gm->ClearDepthf(value);
    }
    mask |= GL_DEPTH_BUFFER_BIT;
  }
  if (st.IsValueSet(StateTable::kClearStencilValue)) {
    const int value = st.GetClearStencilValue();
    if (st.AreSettingsEnforced() ||
        value != save_state->GetClearStencilValue()) {
      save_state->SetClearStencilValue(value);
      gm->ClearStencil(value);
    }
    mask |= GL_STENCIL_BUFFER_BIT;
  }
  if (mask)
    gm->Clear(mask);
}

// Makes GraphicsManager calls to update capability settings that differ
// between two StateTable instances.
static void UpdateAndSetCapability(StateTable::Capability cap,
                                   const StateTable& new_state,
                                   StateTable* save_state,
                                   GraphicsManager* gm) {
  // Guard against use of invalid capability.
  if (!gm->IsValidStateTableCapability(cap)) {
    return;
  }
  const base::IndexMap<StateTable::Capability, GLenum> capability_map =
      base::EnumHelper::GetIndexMap<StateTable::Capability>();
  if (new_state.IsCapabilitySet(cap)) {
    const bool enabled = new_state.IsEnabled(cap);
    if (new_state.AreSettingsEnforced() ||
        enabled != save_state->IsEnabled(cap)) {
      const GLenum value = capability_map.GetUnorderedIndex(cap);
      if (enabled)
        gm->Enable(value);
      else
        gm->Disable(value);
      save_state->Enable(cap, enabled);
    }
  }
}

// Makes GraphicsManager calls to update capability settings that differ
// between two StateTable instances, but only for capabilities that are set in
// state_to_test.
static void UpdateCapabilities(const StateTable& st0, const StateTable& st1,
                               const StateTable& state_to_test,
                               GraphicsManager* gm) {
  const base::IndexMap<StateTable::Capability, GLenum> capability_map =
      base::EnumHelper::GetIndexMap<StateTable::Capability>();
  const size_t num_capabilities = capability_map.GetCount();
  for (size_t i = 0; i < num_capabilities; ++i) {
    const StateTable::Capability st_cap =
        static_cast<StateTable::Capability>(i);
    // Guard against use of invalid capability.
    if (!gm->IsValidStateTableCapability(st_cap)) {
      continue;
    }
    if (state_to_test.IsCapabilitySet(st_cap)) {
      const bool enabled = st1.IsEnabled(st_cap);
      if (state_to_test.AreSettingsEnforced() ||
          enabled != st0.IsEnabled(st_cap)) {
        const GLenum gl_cap = capability_map.GetUnorderedIndex(st_cap);
        if (enabled)
          gm->Enable(gl_cap);
        else
          gm->Disable(gl_cap);
      }
    }
  }
}

//
// Each of these makes a GraphicsManager call to update a single type of value
// that differs between two StateTable instances.
//

static void UpdateBlendColor(
    const StateTable& st0, const StateTable& st1, GraphicsManager* gm) {
  const math::Vector4f& color = st1.GetBlendColor();
  if (st1.AreSettingsEnforced() || color != st0.GetBlendColor())
    gm->BlendColor(color[0], color[1], color[2], color[3]);
}

static void UpdateBlendEquations(
    const StateTable& st0, const StateTable& st1, GraphicsManager* gm) {
  const StateTable::BlendEquation rgb = st1.GetRgbBlendEquation();
  const StateTable::BlendEquation alpha = st1.GetAlphaBlendEquation();
  if (st1.AreSettingsEnforced() || rgb != st0.GetRgbBlendEquation() ||
      alpha != st0.GetAlphaBlendEquation()) {
    gm->BlendEquationSeparate(base::EnumHelper::GetConstant(rgb),
                              base::EnumHelper::GetConstant(alpha));
  }
}

static void UpdateBlendFunctions(
    const StateTable& st0, const StateTable& st1, GraphicsManager* gm) {
  // A typedef for brevity.
  typedef StateTable::BlendFunctionFactor Factor;
  const Factor rgb_src = st1.GetRgbBlendFunctionSourceFactor();
  const Factor rgb_dst = st1.GetRgbBlendFunctionDestinationFactor();
  const Factor alpha_src = st1.GetAlphaBlendFunctionSourceFactor();
  const Factor alpha_dst = st1.GetAlphaBlendFunctionDestinationFactor();
  if (st1.AreSettingsEnforced() ||
      rgb_src != st0.GetRgbBlendFunctionSourceFactor() ||
      rgb_dst != st0.GetRgbBlendFunctionDestinationFactor() ||
      alpha_src != st0.GetAlphaBlendFunctionSourceFactor() ||
      alpha_dst != st0.GetAlphaBlendFunctionDestinationFactor()) {
    gm->BlendFuncSeparate(base::EnumHelper::GetConstant(rgb_src),
                          base::EnumHelper::GetConstant(rgb_dst),
                          base::EnumHelper::GetConstant(alpha_src),
                          base::EnumHelper::GetConstant(alpha_dst));
  }
}

static void UpdateColorWriteMasks(
    StateTable* st0, const StateTable& st1, GraphicsManager* gm) {
  const bool red = st1.GetRedColorWriteMask();
  const bool green = st1.GetGreenColorWriteMask();
  const bool blue = st1.GetBlueColorWriteMask();
  const bool alpha = st1.GetAlphaColorWriteMask();
  if (st1.AreSettingsEnforced() || red != st0->GetRedColorWriteMask() ||
      green != st0->GetGreenColorWriteMask() ||
      blue != st0->GetBlueColorWriteMask() ||
      alpha != st0->GetAlphaColorWriteMask()) {
    gm->ColorMask(red, green, blue, alpha);
    st0->SetColorWriteMasks(red, green, blue, alpha);
  }
}

static void UpdateCullFaceMode(
    const StateTable& st0, const StateTable& st1, GraphicsManager* gm) {
  const StateTable::CullFaceMode mode = st1.GetCullFaceMode();
  if (st1.AreSettingsEnforced() || mode != st0.GetCullFaceMode())
    gm->CullFace(base::EnumHelper::GetConstant(mode));
}

static void UpdateFrontFaceMode(
    const StateTable& st0, const StateTable& st1, GraphicsManager* gm) {
  const StateTable::FrontFaceMode mode = st1.GetFrontFaceMode();
  if (st1.AreSettingsEnforced() || mode != st0.GetFrontFaceMode())
    gm->FrontFace(base::EnumHelper::GetConstant(mode));
}

static void UpdateDefaultInnerTessLevelFunction(const StateTable& st0,
                                                const StateTable& st1,
                                                GraphicsManager* gm) {
  if (gm->IsFeatureAvailable(GraphicsManager::kDefaultTessellationLevels)) {
    math::Vector2f levels = st1.GetDefaultInnerTessellationLevel();
    if (st1.AreSettingsEnforced() ||
        levels != st0.GetDefaultInnerTessellationLevel()) {
      gm->PatchParameterfv(GL_PATCH_DEFAULT_INNER_LEVEL, levels.Data());
    }
  }
}

static void UpdateDefaultOuterTessLevelFunction(const StateTable& st0,
                                                const StateTable& st1,
                                                GraphicsManager* gm) {
  if (gm->IsFeatureAvailable(GraphicsManager::kDefaultTessellationLevels)) {
    math::Vector4f levels = st1.GetDefaultOuterTessellationLevel();
    if (st1.AreSettingsEnforced() ||
        levels != st0.GetDefaultOuterTessellationLevel()) {
      gm->PatchParameterfv(GL_PATCH_DEFAULT_OUTER_LEVEL, levels.Data());
    }
  }
}

static void UpdateDepthFunction(
    const StateTable& st0, const StateTable& st1, GraphicsManager* gm) {
  const StateTable::DepthFunction func = st1.GetDepthFunction();
  if (st1.AreSettingsEnforced() || func != st0.GetDepthFunction())
    gm->DepthFunc(base::EnumHelper::GetConstant(func));
}

static void UpdateDepthRange(
    const StateTable& st0, const StateTable& st1, GraphicsManager* gm) {
  const math::Range1f& range = st1.GetDepthRange();
  if (st1.AreSettingsEnforced() || range != st0.GetDepthRange())
    gm->DepthRangef(range.GetMinPoint(), range.GetMaxPoint());
}

static void UpdateDepthWriteMask(
    StateTable* st0, const StateTable& st1, GraphicsManager* gm) {
  const bool mask = st1.GetDepthWriteMask();
  if (st1.AreSettingsEnforced() || mask != st0->GetDepthWriteMask()) {
    gm->DepthMask(mask);
    st0->SetDepthWriteMask(mask);
  }
}

static void UpdateHints(
    const StateTable& st0, const StateTable& st1, GraphicsManager* gm) {
  const StateTable::HintMode mipmap_hint =
      st1.GetHint(StateTable::kGenerateMipmapHint);
  if (st1.AreSettingsEnforced() ||
      mipmap_hint != st0.GetHint(StateTable::kGenerateMipmapHint))
    gm->Hint(GL_GENERATE_MIPMAP_HINT,
             base::EnumHelper::GetConstant(mipmap_hint));
}

static void UpdateLineWidth(
    const StateTable& st0, const StateTable& st1, GraphicsManager* gm) {
  const float width = st1.GetLineWidth();
  if (st1.AreSettingsEnforced() || width != st0.GetLineWidth())
    gm->LineWidth(width);
}

static void UpdateMinSampleShading(
    const StateTable& st0, const StateTable& st1, GraphicsManager* gm) {
  if (gm->IsFeatureAvailable(GraphicsManager::kSampleShading)) {
    const float fraction = st1.GetMinSampleShading();
    if (st1.AreSettingsEnforced() || fraction != st0.GetMinSampleShading())
      gm->MinSampleShading(fraction);
  }
}

static void UpdatePolygonOffset(
    const StateTable& st0, const StateTable& st1, GraphicsManager* gm) {
  const float factor = st1.GetPolygonOffsetFactor();
  const float units = st1.GetPolygonOffsetUnits();
  if (st1.AreSettingsEnforced() || factor != st0.GetPolygonOffsetFactor() ||
      units != st0.GetPolygonOffsetUnits()) {
    gm->PolygonOffset(factor, units);
  }
}

static void UpdateSampleCoverage(
    const StateTable& st0, const StateTable& st1, GraphicsManager* gm) {
  const float value = st1.GetSampleCoverageValue();
  const bool is_inverted = st1.IsSampleCoverageInverted();
  if (st1.AreSettingsEnforced() || value != st0.GetSampleCoverageValue() ||
      is_inverted != st0.IsSampleCoverageInverted()) {
    gm->SampleCoverage(value, is_inverted);
  }
}

static void UpdateScissorBox(
    StateTable* st0, const StateTable& st1, GraphicsManager* gm) {
  const math::Range2i& box = st1.GetScissorBox();
  if (st1.AreSettingsEnforced() || box != st0->GetScissorBox()) {
    const math::Point2i& min_point = box.GetMinPoint();
    const math::Vector2i size = box.GetSize();
    gm->Scissor(min_point[0], min_point[1], size[0], size[1]);
    st0->SetScissorBox(box);
  }
}

static void UpdateStencilFunctions(
    const StateTable& st0, const StateTable& st1, GraphicsManager* gm) {
  const StateTable::StencilFunction front_func = st1.GetFrontStencilFunction();
  const int front_ref = st1.GetFrontStencilReferenceValue();
  const uint32 front_mask = st1.GetFrontStencilMask();
  if (st1.AreSettingsEnforced() ||
      front_func != st0.GetFrontStencilFunction() ||
      front_ref != st0.GetFrontStencilReferenceValue() ||
      front_mask != st0.GetFrontStencilMask()) {
    gm->StencilFuncSeparate(GL_FRONT,
                            base::EnumHelper::GetConstant(front_func),
                            front_ref, front_mask);
  }

  const StateTable::StencilFunction back_func = st1.GetBackStencilFunction();
  const int back_ref = st1.GetBackStencilReferenceValue();
  const uint32 back_mask = st1.GetBackStencilMask();
  if (st1.AreSettingsEnforced() || back_func != st0.GetBackStencilFunction() ||
      back_ref != st0.GetBackStencilReferenceValue() ||
      back_mask != st0.GetBackStencilMask()) {
    gm->StencilFuncSeparate(GL_BACK,
                            base::EnumHelper::GetConstant(back_func),
                            back_ref, back_mask);
  }
}

static void UpdateStencilOperations(
    const StateTable& st0, const StateTable& st1, GraphicsManager* gm) {
  // A typedef for brevity.
  typedef StateTable::StencilOperation Op;

  const Op front_fail = st1.GetFrontStencilFailOperation();
  const Op front_depth_fail = st1.GetFrontStencilDepthFailOperation();
  const Op front_pass = st1.GetFrontStencilPassOperation();
  if (st1.AreSettingsEnforced() ||
      front_fail != st0.GetFrontStencilFailOperation() ||
      front_depth_fail != st0.GetFrontStencilDepthFailOperation() ||
      front_pass != st0.GetFrontStencilPassOperation()) {
    gm->StencilOpSeparate(GL_FRONT,
                          base::EnumHelper::GetConstant(front_fail),
                          base::EnumHelper::GetConstant(front_depth_fail),
                          base::EnumHelper::GetConstant(front_pass));
  }

  const Op back_fail = st1.GetBackStencilFailOperation();
  const Op back_depth_fail = st1.GetBackStencilDepthFailOperation();
  const Op back_pass = st1.GetBackStencilPassOperation();
  if (st1.AreSettingsEnforced() ||
      back_fail != st0.GetBackStencilFailOperation() ||
      back_depth_fail != st0.GetBackStencilDepthFailOperation() ||
      back_pass != st0.GetBackStencilPassOperation()) {
    gm->StencilOpSeparate(GL_BACK, base::EnumHelper::GetConstant(back_fail),
                          base::EnumHelper::GetConstant(back_depth_fail),
                          base::EnumHelper::GetConstant(back_pass));
  }
}

static void UpdateStencilWriteMasks(
    StateTable* st0, const StateTable& st1, GraphicsManager* gm) {
  const uint32 front_mask = st1.GetFrontStencilWriteMask();
  bool do_set = false;
  if (st1.AreSettingsEnforced() ||
      front_mask != st0->GetFrontStencilWriteMask()) {
    gm->StencilMaskSeparate(GL_FRONT, front_mask);
    do_set = true;
  }

  const uint32 back_mask = st1.GetBackStencilWriteMask();
  if (st1.AreSettingsEnforced() ||
      back_mask != st0->GetBackStencilWriteMask()) {
    gm->StencilMaskSeparate(GL_BACK, back_mask);
    do_set = true;
  }

  if (do_set)
    st0->SetStencilWriteMasks(front_mask, back_mask);
}

static void UpdateViewport(
    const StateTable& st0, const StateTable& st1, GraphicsManager* gm) {
  const math::Range2i& viewport = st1.GetViewport();
  if (st1.AreSettingsEnforced() || viewport != st0.GetViewport()) {
    const math::Point2i& min_point = viewport.GetMinPoint();
    const math::Vector2i size = viewport.GetSize();
    gm->Viewport(min_point[0], min_point[1], size[0], size[1]);
  }
}

//-----------------------------------------------------------------------------
//
// GraphicsManager access convenience functions.
//
//-----------------------------------------------------------------------------

static GLint GetInt(GraphicsManager* gm, GLenum what) {
  GLint i;
  gm->GetIntegerv(what, &i);
  return i;
}

static math::Vector2f GetFloat2(GraphicsManager* gm, GLenum what) {
  math::Vector2f v;
  gm->GetFloatv(what, v.Data());
  return v;
}

static math::Vector4f GetFloat4(GraphicsManager* gm, GLenum what) {
  math::Vector4f v;
  gm->GetFloatv(what, v.Data());
  return v;
}

static GLfloat GetFloat(GraphicsManager* gm, GLenum what) {
  GLfloat f;
  gm->GetFloatv(what, &f);
  return f;
}

static bool GetBool(GraphicsManager* gm, GLenum what) {
  return GetInt(gm, what) != 0;
}

template <typename EnumType>
static EnumType GetEnum(GraphicsManager* gm, GLenum what) {
  // Typically, -1 is not an invalid value for glGetIntegerv to find. In this
  // case, however, we are only ever checking for values that should never be
  // -1.  That is, when querying the value of a capability or another enum,
  // the value should always be another valid GL enum.
  static const GLint kInvalidValue = -1;
  GLint value = GetInt(gm, what);
  if (value == kInvalidValue) {
    LOG(ERROR)
        << "GL returned an invalid value (" << value << ") while glGet*()ing 0x"
        << std::hex << what
        << ". This may indicate a GPU driver bug or an unsupported GL enum"
           " value being used on this platform. Unexpected results may occur.";
    // Return the "default" value for this enum, which may be wrong, but we have
    // no other information to go by.
    return EnumType{};
  }
  return base::EnumHelper::GetEnum<EnumType>(value);
}

//-----------------------------------------------------------------------------
//
// Other helper functions.
//
//-----------------------------------------------------------------------------

// Copies the current capability settings from a GraphicsManager into a
// StateTable. Only the settings that differ from the current state in the
// StateTable are copied.
static void CopyCapabilities(GraphicsManager* gm, StateTable* st) {
  const base::IndexMap<StateTable::Capability, GLenum> capability_map =
      base::EnumHelper::GetIndexMap<StateTable::Capability>();

  const size_t num_capabilities = capability_map.GetCount();
  for (size_t i = 0; i < num_capabilities; ++i) {
    const StateTable::Capability st_cap =
        static_cast<StateTable::Capability>(i);
    if (gm->IsValidStateTableCapability(st_cap)) {
      const GLenum gl_cap = capability_map.GetUnorderedIndex(st_cap);
      const bool gl_state = gm->IsEnabled(gl_cap);
      if (st->IsEnabled(st_cap) != gl_state)
        st->Enable(st_cap, gl_state);
    }
  }
}

// Copies capability settings and values from the GraphicsManager to the
// StateTable. Only capabilities that are already set are queried and set.
static void CopySetCapabilities(GraphicsManager* gm, StateTable* st) {
  const base::IndexMap<StateTable::Capability, GLenum> capability_map =
      base::EnumHelper::GetIndexMap<StateTable::Capability>();

  const size_t num_capabilities = capability_map.GetCount();
  for (size_t i = 0; i < num_capabilities; ++i) {
    const StateTable::Capability st_cap =
        static_cast<StateTable::Capability>(i);
    if (st->IsCapabilitySet(st_cap)) {
      const GLenum gl_cap = capability_map.GetUnorderedIndex(st_cap);
      if (gm->IsValidStateTableCapability(st_cap)) {
        const bool gl_state = gm->IsEnabled(gl_cap);
        if (st->IsEnabled(st_cap) != gl_state)
          st->Enable(st_cap, gl_state);
      }
    }
  }
}

// For brevity and clarity in the below functions.
#define ION_GET_ENUM(st_type, gl_enum) GetEnum<StateTable::st_type>(gm, gl_enum)

// Copies all current values from the GraphicsManager into the StateTable.
static void CopyValues(GraphicsManager* gm, StateTable* st) {
  st->SetBlendColor(GetFloat4(gm, GL_BLEND_COLOR));
  st->SetBlendEquations(ION_GET_ENUM(BlendEquation, GL_BLEND_EQUATION_RGB),
                        ION_GET_ENUM(BlendEquation, GL_BLEND_EQUATION_ALPHA));
  st->SetBlendFunctions(ION_GET_ENUM(BlendFunctionFactor, GL_BLEND_SRC_RGB),
                        ION_GET_ENUM(BlendFunctionFactor, GL_BLEND_DST_RGB),
                        ION_GET_ENUM(BlendFunctionFactor, GL_BLEND_SRC_ALPHA),
                        ION_GET_ENUM(BlendFunctionFactor, GL_BLEND_DST_ALPHA));
  st->SetClearColor(GetFloat4(gm, GL_COLOR_CLEAR_VALUE));
  {
    GLint mask[4];
    gm->GetIntegerv(GL_COLOR_WRITEMASK, mask);
    st->SetColorWriteMasks(mask[0], mask[1], mask[2], mask[3]);
  }
  st->SetCullFaceMode(ION_GET_ENUM(CullFaceMode, GL_CULL_FACE_MODE));
  st->SetFrontFaceMode(ION_GET_ENUM(FrontFaceMode, GL_FRONT_FACE));
  st->SetClearDepthValue(GetFloat(gm, GL_DEPTH_CLEAR_VALUE));
  st->SetDepthFunction(ION_GET_ENUM(DepthFunction, GL_DEPTH_FUNC));
  {
    GLfloat range[2];
    gm->GetFloatv(GL_DEPTH_RANGE, range);
    st->SetDepthRange(math::Range1f(range[0], range[1]));
  }
  st->SetDepthWriteMask(GetBool(gm, GL_DEPTH_WRITEMASK));
  st->SetHint(StateTable::kGenerateMipmapHint,
              ION_GET_ENUM(HintMode, GL_GENERATE_MIPMAP_HINT));
  st->SetLineWidth(GetFloat(gm, GL_LINE_WIDTH));
  st->SetPolygonOffset(GetFloat(gm, GL_POLYGON_OFFSET_FACTOR),
                       GetFloat(gm, GL_POLYGON_OFFSET_UNITS));
  st->SetSampleCoverage(GetFloat(gm, GL_SAMPLE_COVERAGE_VALUE),
                        GetBool(gm, GL_SAMPLE_COVERAGE_INVERT));
  {
    GLint box[4];
    gm->GetIntegerv(GL_SCISSOR_BOX, box);
    st->SetScissorBox(
        math::Range2i::BuildWithSize(math::Point2i(box[0], box[1]),
                                     math::Vector2i(box[2], box[3])));
  }
  st->SetStencilFunctions(ION_GET_ENUM(StencilFunction, GL_STENCIL_FUNC),
                          GetInt(gm, GL_STENCIL_REF),
                          GetInt(gm, GL_STENCIL_VALUE_MASK),
                          ION_GET_ENUM(StencilFunction, GL_STENCIL_BACK_FUNC),
                          GetInt(gm, GL_STENCIL_BACK_REF),
                          GetInt(gm, GL_STENCIL_BACK_VALUE_MASK));
  st->SetStencilOperations(
      ION_GET_ENUM(StencilOperation, GL_STENCIL_FAIL),
      ION_GET_ENUM(StencilOperation, GL_STENCIL_PASS_DEPTH_FAIL),
      ION_GET_ENUM(StencilOperation, GL_STENCIL_PASS_DEPTH_PASS),
      ION_GET_ENUM(StencilOperation, GL_STENCIL_BACK_FAIL),
      ION_GET_ENUM(StencilOperation, GL_STENCIL_BACK_PASS_DEPTH_FAIL),
      ION_GET_ENUM(StencilOperation, GL_STENCIL_BACK_PASS_DEPTH_PASS));
  st->SetClearStencilValue(GetInt(gm, GL_STENCIL_CLEAR_VALUE));
  st->SetStencilWriteMasks(GetInt(gm, GL_STENCIL_WRITEMASK),
                           GetInt(gm, GL_STENCIL_BACK_WRITEMASK));
  {
    GLint viewport[4];
    gm->GetIntegerv(GL_VIEWPORT, viewport);
    st->SetViewport(
        math::Range2i::BuildWithSize(math::Point2i(viewport[0], viewport[1]),
                                     math::Vector2i(viewport[2], viewport[3])));
  }
}

// Copies the set of values already set in the StateTable from the
// GraphicsManager into the StateTable.
static void CopySetValues(GraphicsManager* gm, StateTable* st) {
  if (st->IsValueSet(StateTable::kBlendColorValue)) {
    st->SetBlendColor(GetFloat4(gm, GL_BLEND_COLOR));
  }
  if (st->IsValueSet(StateTable::kBlendEquationsValue)) {
    st->SetBlendEquations(ION_GET_ENUM(BlendEquation, GL_BLEND_EQUATION_RGB),
                          ION_GET_ENUM(BlendEquation, GL_BLEND_EQUATION_ALPHA));
  }
  if (st->IsValueSet(StateTable::kBlendFunctionsValue)) {
    st->SetBlendFunctions(
        ION_GET_ENUM(BlendFunctionFactor, GL_BLEND_SRC_RGB),
        ION_GET_ENUM(BlendFunctionFactor, GL_BLEND_DST_RGB),
        ION_GET_ENUM(BlendFunctionFactor, GL_BLEND_SRC_ALPHA),
        ION_GET_ENUM(BlendFunctionFactor, GL_BLEND_DST_ALPHA));
  }
  if (st->IsValueSet(StateTable::kClearColorValue)) {
    st->SetClearColor(GetFloat4(gm, GL_COLOR_CLEAR_VALUE));
  }
  if (st->IsValueSet(StateTable::kColorWriteMasksValue)) {
    GLint mask[4];
    gm->GetIntegerv(GL_COLOR_WRITEMASK, mask);
    st->SetColorWriteMasks(mask[0], mask[1], mask[2], mask[3]);
  }
  if (st->IsValueSet(StateTable::kCullFaceModeValue)) {
    st->SetCullFaceMode(ION_GET_ENUM(CullFaceMode, GL_CULL_FACE_MODE));
  }
  if (st->IsValueSet(StateTable::kFrontFaceModeValue)) {
    st->SetFrontFaceMode(ION_GET_ENUM(FrontFaceMode, GL_FRONT_FACE));
  }
  if (st->IsValueSet(StateTable::kClearDepthValue)) {
    st->SetClearDepthValue(GetFloat(gm, GL_DEPTH_CLEAR_VALUE));
  }
  if (st->IsValueSet(StateTable::kDefaultInnerTessellationLevelValue)) {
    st->SetDefaultInnerTessellationLevel(
        GetFloat2(gm, GL_PATCH_DEFAULT_INNER_LEVEL));
  }
  if (st->IsValueSet(StateTable::kDefaultOuterTessellationLevelValue)) {
    st->SetDefaultOuterTessellationLevel(
        GetFloat4(gm, GL_PATCH_DEFAULT_OUTER_LEVEL));
  }
  if (st->IsValueSet(StateTable::kDepthFunctionValue)) {
    st->SetDepthFunction(ION_GET_ENUM(DepthFunction, GL_DEPTH_FUNC));
  }
  if (st->IsValueSet(StateTable::kDepthRangeValue)) {
    GLfloat range[2];
    gm->GetFloatv(GL_DEPTH_RANGE, range);
    st->SetDepthRange(math::Range1f(range[0], range[1]));
  }
  if (st->IsValueSet(StateTable::kDepthWriteMaskValue)) {
    st->SetDepthWriteMask(GetBool(gm, GL_DEPTH_WRITEMASK));
  }
  if (st->IsValueSet(StateTable::kLineWidthValue)) {
    st->SetLineWidth(GetFloat(gm, GL_LINE_WIDTH));
  }
  if (st->IsValueSet(StateTable::kPolygonOffsetValue)) {
    st->SetPolygonOffset(GetFloat(gm, GL_POLYGON_OFFSET_FACTOR),
                         GetFloat(gm, GL_POLYGON_OFFSET_UNITS));
  }
  if (st->IsValueSet(StateTable::kSampleCoverageValue)) {
    st->SetSampleCoverage(GetFloat(gm, GL_SAMPLE_COVERAGE_VALUE),
                          GetBool(gm, GL_SAMPLE_COVERAGE_INVERT));
  }
  if (st->IsValueSet(StateTable::kScissorBoxValue)) {
    GLint box[4];
    gm->GetIntegerv(GL_SCISSOR_BOX, box);
    st->SetScissorBox(math::Range2i::BuildWithSize(
        math::Point2i(box[0], box[1]), math::Vector2i(box[2], box[3])));
  }
  if (st->IsValueSet(StateTable::kStencilFunctionsValue)) {
    st->SetStencilFunctions(ION_GET_ENUM(StencilFunction, GL_STENCIL_FUNC),
                            GetInt(gm, GL_STENCIL_REF),
                            GetInt(gm, GL_STENCIL_VALUE_MASK),
                            ION_GET_ENUM(StencilFunction, GL_STENCIL_BACK_FUNC),
                            GetInt(gm, GL_STENCIL_BACK_REF),
                            GetInt(gm, GL_STENCIL_BACK_VALUE_MASK));
  }
  if (st->IsValueSet(StateTable::kStencilOperationsValue)) {
    st->SetStencilOperations(
        ION_GET_ENUM(StencilOperation, GL_STENCIL_FAIL),
        ION_GET_ENUM(StencilOperation, GL_STENCIL_PASS_DEPTH_FAIL),
        ION_GET_ENUM(StencilOperation, GL_STENCIL_PASS_DEPTH_PASS),
        ION_GET_ENUM(StencilOperation, GL_STENCIL_BACK_FAIL),
        ION_GET_ENUM(StencilOperation, GL_STENCIL_BACK_PASS_DEPTH_FAIL),
        ION_GET_ENUM(StencilOperation, GL_STENCIL_BACK_PASS_DEPTH_PASS));
  }
  if (st->IsValueSet(StateTable::kClearStencilValue)) {
    st->SetClearStencilValue(GetInt(gm, GL_STENCIL_CLEAR_VALUE));
  }
  if (st->IsValueSet(StateTable::kStencilWriteMasksValue)) {
    st->SetStencilWriteMasks(GetInt(gm, GL_STENCIL_WRITEMASK),
                             GetInt(gm, GL_STENCIL_BACK_WRITEMASK));
  }
  if (st->IsValueSet(StateTable::kViewportValue)) {
    GLint viewport[4];
    gm->GetIntegerv(GL_VIEWPORT, viewport);
    st->SetViewport(
        math::Range2i::BuildWithSize(math::Point2i(viewport[0], viewport[1]),
                                     math::Vector2i(viewport[2], viewport[3])));
  }
}
#undef ION_GET_ENUM

// Resets all StateTable values that are now the same as the defaults.
static void ResetValues(const StateTable& default_st, StateTable* st) {
#define ION_IS_SAME(func) st->func == default_st.func
#define ION_RESET(condition, value_enum)                                  \
  if (condition)                                                          \
    st->ResetValue(StateTable::value_enum)
#define ION_RESET1(value_enum, func)                                      \
  ION_RESET(ION_IS_SAME(func), value_enum)
#define ION_RESET2(value_enum, func1, func2)                              \
  ION_RESET(ION_IS_SAME(func1) && ION_IS_SAME(func2), value_enum)
#define ION_RESET4(value_enum, func1, func2, func3, func4)                \
  ION_RESET(ION_IS_SAME(func1) && ION_IS_SAME(func2) &&                   \
            ION_IS_SAME(func3) && ION_IS_SAME(func4), value_enum)
#define ION_RESET6(value_enum, func1, func2, func3, func4, func5, func6)  \
  ION_RESET(ION_IS_SAME(func1) && ION_IS_SAME(func2) &&                   \
            ION_IS_SAME(func3) && ION_IS_SAME(func4) &&                   \
            ION_IS_SAME(func5) && ION_IS_SAME(func6), value_enum)

  ION_RESET1(kBlendColorValue, GetBlendColor());
  ION_RESET2(kBlendEquationsValue,
             GetRgbBlendEquation(), GetAlphaBlendEquation());
  ION_RESET4(kBlendFunctionsValue,
             GetRgbBlendFunctionSourceFactor(),
             GetAlphaBlendFunctionSourceFactor(),
             GetRgbBlendFunctionDestinationFactor(),
             GetAlphaBlendFunctionDestinationFactor());
  ION_RESET1(kClearColorValue, GetClearColor());
  ION_RESET4(kColorWriteMasksValue,
             GetRedColorWriteMask(), GetBlueColorWriteMask(),
             GetGreenColorWriteMask(), GetAlphaColorWriteMask());
  ION_RESET1(kCullFaceModeValue, GetCullFaceMode());
  ION_RESET1(kFrontFaceModeValue, GetFrontFaceMode());
  ION_RESET1(kFrontFaceModeValue, GetFrontFaceMode());
  ION_RESET1(kClearDepthValue, GetClearDepthValue());
  ION_RESET1(kDefaultInnerTessellationLevelValue,
             GetDefaultInnerTessellationLevel());
  ION_RESET1(kDefaultOuterTessellationLevelValue,
             GetDefaultOuterTessellationLevel());
  ION_RESET1(kDepthFunctionValue, GetDepthFunction());
  ION_RESET1(kDepthRangeValue, GetDepthRange());
  ION_RESET1(kDepthWriteMaskValue, GetDepthWriteMask());
  ION_RESET1(kHintsValue, GetHint(StateTable::kGenerateMipmapHint));
  ION_RESET1(kLineWidthValue, GetLineWidth());
  ION_RESET1(kMinSampleShadingValue, GetMinSampleShading());
  ION_RESET2(kPolygonOffsetValue,
             GetPolygonOffsetFactor(), GetPolygonOffsetUnits());
  ION_RESET2(kSampleCoverageValue,
             GetSampleCoverageValue(), IsSampleCoverageInverted());
  ION_RESET1(kScissorBoxValue, GetScissorBox());
  ION_RESET6(kStencilFunctionsValue,
             GetFrontStencilFunction(), GetBackStencilFunction(),
             GetFrontStencilReferenceValue(), GetBackStencilReferenceValue(),
             GetFrontStencilMask(), GetBackStencilMask());
  ION_RESET6(kStencilOperationsValue,
             GetFrontStencilFailOperation(), GetBackStencilFailOperation(),
             GetFrontStencilDepthFailOperation(),
             GetBackStencilDepthFailOperation(),
             GetFrontStencilPassOperation(), GetBackStencilPassOperation());
  ION_RESET1(kClearStencilValue, GetClearStencilValue());
  ION_RESET2(kStencilWriteMasksValue,
             GetFrontStencilWriteMask(), GetBackStencilWriteMask());
  ION_RESET1(kViewportValue, GetViewport());

#undef ION_RESET4
#undef ION_RESET2
#undef ION_RESET1
#undef ION_RESET
#undef ION_IS_SAME
}

}  // anonymous namespace

//-----------------------------------------------------------------------------
//
// Public functions.
//
//-----------------------------------------------------------------------------

void UpdateStateTable(int default_width, int default_height,
                      GraphicsManager* gm, StateTable* st) {
  DCHECK(gm);
  DCHECK(st);

  // Reset the StateTable to default settings.
  st->Reset();

  // Copy capability settings and values from the GraphicsManager to the
  // StateTable. Only non-default capabilities are copied.
  CopyCapabilities(gm, st);
  CopyValues(gm, st);

  // Use a default StateTable to reset the values that are now the same as the
  // defaults.
  StateTablePtr default_st(new(st->GetAllocatorForLifetime(base::kShortTerm))
                           StateTable(default_width, default_height));
  ResetValues(*default_st, st);
}

#define ION_UPDATE_CLEAR_VALUE(enum_name, update_func) \
  if (new_state.IsValueSet(StateTable::enum_name))     \
  update_func(save_state, new_state, gm)
#define ION_UPDATE_CLEAR_ONLY_VALUE(enum_name, clear_enum_name, update_func) \
  if (new_state.IsValueSet(StateTable::enum_name) && \
      new_state.IsValueSet(StateTable::clear_enum_name))     \
  update_func(save_state, new_state, gm)
#define ION_UPDATE_VALUE(enum_name, update_func)   \
  if (new_state.IsValueSet(StateTable::enum_name)) \
  update_func(*save_state, new_state, gm)

void ClearFromStateTable(const StateTable& new_state,
                         StateTable* save_state,
                         GraphicsManager* gm) {
  UpdateAndSetCapability(StateTable::kDither, new_state, save_state, gm);
  UpdateAndSetCapability(StateTable::kScissorTest, new_state, save_state, gm);
  UpdateAndSetCapability(StateTable::kRasterizerDiscard, new_state, save_state,
                         gm);
  if (new_state.GetSetValueCount()) {
    // Write masks, the scissor box, rasterizer discard, and dithering affect
    // Clear(). Everything but the write masks are handled above.
    ION_UPDATE_CLEAR_VALUE(kScissorBoxValue, UpdateScissorBox);
    // Only send the write mask values if we are actually going to do a clear.
    // Otherwise, they will be sent via UpdateFromStateTable before drawing
    // geometry.
    ION_UPDATE_CLEAR_ONLY_VALUE(kColorWriteMasksValue, kClearColorValue,
                                UpdateColorWriteMasks);
    ION_UPDATE_CLEAR_ONLY_VALUE(kDepthWriteMaskValue, kClearDepthValue,
                                UpdateDepthWriteMask);
    ION_UPDATE_CLEAR_ONLY_VALUE(kStencilWriteMasksValue, kClearStencilValue,
                                UpdateStencilWriteMasks);
    ClearBuffers(new_state, save_state, gm);
  }
}

void UpdateSettingsInStateTable(StateTable* st, GraphicsManager* gm) {
  DCHECK(gm);
  DCHECK(st);

  CopySetCapabilities(gm, st);
  CopySetValues(gm, st);
}

void UpdateFromStateTable(const StateTable& new_state,
                          StateTable* save_state,
                          GraphicsManager* gm) {
  DCHECK(gm);

  // If any capability settings were modified in the state and differ between
  // the two states, enable/disable them.
  if (new_state.GetSetCapabilityCount() &&
      (new_state.AreSettingsEnforced() ||
       !StateTable::AreCapabilitiesSame(*save_state, new_state)))
    UpdateCapabilities(*save_state, new_state, new_state, gm);

  // If any values have been modified in the state, update the differences.
  if (new_state.GetSetValueCount()) {
    ION_UPDATE_VALUE(kBlendColorValue, UpdateBlendColor);
    ION_UPDATE_VALUE(kBlendEquationsValue, UpdateBlendEquations);
    ION_UPDATE_VALUE(kBlendFunctionsValue, UpdateBlendFunctions);
    ION_UPDATE_CLEAR_VALUE(kColorWriteMasksValue, UpdateColorWriteMasks);
    ION_UPDATE_VALUE(kCullFaceModeValue, UpdateCullFaceMode);
    ION_UPDATE_VALUE(kDefaultInnerTessellationLevelValue,
                     UpdateDefaultInnerTessLevelFunction);
    ION_UPDATE_VALUE(kDefaultInnerTessellationLevelValue,
                     UpdateDefaultOuterTessLevelFunction);
    ION_UPDATE_VALUE(kDepthFunctionValue, UpdateDepthFunction);
    ION_UPDATE_VALUE(kDepthRangeValue, UpdateDepthRange);
    ION_UPDATE_CLEAR_VALUE(kDepthWriteMaskValue, UpdateDepthWriteMask);
    ION_UPDATE_VALUE(kFrontFaceModeValue, UpdateFrontFaceMode);
    ION_UPDATE_VALUE(kHintsValue, UpdateHints);
    ION_UPDATE_VALUE(kLineWidthValue, UpdateLineWidth);
    ION_UPDATE_VALUE(kMinSampleShadingValue, UpdateMinSampleShading);
    ION_UPDATE_VALUE(kPolygonOffsetValue, UpdatePolygonOffset);
    ION_UPDATE_VALUE(kSampleCoverageValue, UpdateSampleCoverage);
    ION_UPDATE_CLEAR_VALUE(kScissorBoxValue, UpdateScissorBox);
    ION_UPDATE_VALUE(kStencilFunctionsValue, UpdateStencilFunctions);
    ION_UPDATE_VALUE(kStencilOperationsValue, UpdateStencilOperations);
    ION_UPDATE_CLEAR_VALUE(kStencilWriteMasksValue, UpdateStencilWriteMasks);
    ION_UPDATE_VALUE(kViewportValue, UpdateViewport);
  }
}

#undef ION_UPDATE_CLEAR_VALUE
#undef ION_UPDATE_CLEAR_ONLY_VALUE
#undef ION_UPDATE_VALUE

}  // namespace gfx
}  // namespace ion
