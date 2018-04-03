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

#include "ion/gfx/statetable.h"

#include "ion/base/argcount.h"
#include "ion/base/enumhelper.h"
#include "ion/base/logging.h"
#include "ion/base/static_assert.h"
#include "ion/portgfx/glheaders.h"
#include "absl/base/macros.h"

namespace ion {
namespace gfx {

StateTable::~StateTable() {
}

void StateTable::Reset() {
  // Copy the default Data instance into this.
  data_ = GetDefaultData();

  // Update the size-dependent values.
  const math::Point2i max_point(default_width_, default_height_);
  data_.scissor_box.SetMaxPoint(max_point);
  data_.viewport.SetMaxPoint(max_point);
}

void StateTable::CopyFrom(const StateTable& other) {
  default_width_ = other.default_width_;
  default_height_ = other.default_height_;
  data_ = other.data_;
}

// Definitions for each number of arguments.
#define ION_UPDATE_VALUE1(n) data_.n = other.data_.n
#define ION_UPDATE_VALUE2(n1, n2) \
  ION_UPDATE_VALUE1(n1);          \
  data_.n2 = other.data_.n2
#define ION_UPDATE_VALUE3(n1, n2, n3) \
  ION_UPDATE_VALUE2(n1, n2);          \
  data_.n3 = other.data_.n3
#define ION_UPDATE_VALUE4(n1, n2, n3, n4) \
  ION_UPDATE_VALUE3(n1, n2, n3);          \
  data_.n4 = other.data_.n4
#define ION_UPDATE_VALUE5(n1, n2, n3, n4, n5) \
  ION_UPDATE_VALUE4(n1, n2, n3, n4);          \
  data_.n5 = other.data_.n5
#define ION_UPDATE_VALUE6(n1, n2, n3, n4, n5, n6) \
  ION_UPDATE_VALUE5(n1, n2, n3, n4, n5);          \
  data_.n6 = other.data_.n6
#define ION_UPDATE_VALUE_(UPDATE_VALUE_MACRO, ...) \
  UPDATE_VALUE_MACRO(__VA_ARGS__)

// This will call the right macro above by concatenating the macro name with
// the number of arguments.
#define ION_UPDATE_VALUE(enum_name, ...)                                   \
  if (state_to_test.IsValueSet(enum_name)) {                               \
    data_.values_set.set(enum_name);                                       \
    ION_UPDATE_VALUE_(                                                     \
        ION_ARGCOUNT_XCONCAT(ION_UPDATE_VALUE, ION_ARGCOUNT(__VA_ARGS__)), \
        __VA_ARGS__);                                                      \
  }

void StateTable::MergeValuesFrom(const StateTable& other,
                                 const StateTable& state_to_test) {
  MergeNonClearValuesFrom(other, state_to_test);

  // If any values are set in the other state, set their values here.
  if (state_to_test.GetSetValueCount()) {
    ION_UPDATE_VALUE(kClearColorValue, clear_color)
    ION_UPDATE_VALUE(kClearDepthValue, clear_depth_value)
    ION_UPDATE_VALUE(kClearStencilValue, clear_stencil_value)
  }
}

void StateTable::MergeNonClearValuesFrom(const StateTable& other,
                                         const StateTable& state_to_test) {
  // If any capability settings are set in the other state then set them here.
  if (state_to_test.GetSetCapabilityCount() &&
      (!AreCapabilitiesSame(*this, other) ||
       state_to_test.AreSettingsEnforced())) {
    const size_t num_capabilities = data_.capabilities_set.size();
    for (size_t i = 0; i < num_capabilities; ++i) {
      if (state_to_test.data_.capabilities_set.test(i)) {
        data_.capabilities.set(i, other.data_.capabilities.test(i));
        data_.capabilities_set.set(i);
      }
    }
  }

  // If any values are set in the other state, set their values here.
  if (state_to_test.GetSetValueCount()) {
    ION_UPDATE_VALUE(kBlendColorValue, blend_color)
    ION_UPDATE_VALUE(kBlendEquationsValue, rgb_blend_equation,
                     alpha_blend_equation)
    ION_UPDATE_VALUE(kBlendFunctionsValue,
                     rgb_blend_source_factor,
                     rgb_blend_destination_factor,
                     alpha_blend_source_factor,
                     alpha_blend_destination_factor)
    ION_UPDATE_VALUE(kColorWriteMasksValue,
                     color_write_masks[0],
                     color_write_masks[1],
                     color_write_masks[2],
                     color_write_masks[3])
    ION_UPDATE_VALUE(kCullFaceModeValue, cull_face_mode)
    ION_UPDATE_VALUE(kDepthWriteMaskValue, depth_write_mask)
    ION_UPDATE_VALUE(kFrontFaceModeValue, front_face_mode)
    ION_UPDATE_VALUE(kDefaultInnerTessellationLevelValue,
                     default_inner_tess_level)
    ION_UPDATE_VALUE(kDefaultOuterTessellationLevelValue,
                     default_outer_tess_level)
    ION_UPDATE_VALUE(kDepthFunctionValue, depth_function)
    ION_UPDATE_VALUE(kDepthRangeValue, depth_range)
    // Hints are an array of unknown size and so must be handled specially.
    if (other.IsValueSet(StateTable::kHintsValue)) {
      for (int i = 0; i < kNumHints; ++i)
        data_.hints[i] = other.data_.hints[i];
    }
    ION_UPDATE_VALUE(kLineWidthValue, line_width)
    ION_UPDATE_VALUE(kMinSampleShadingValue, min_sample_shading)
    ION_UPDATE_VALUE(
        kPolygonOffsetValue, polygon_offset_factor, polygon_offset_units)
    ION_UPDATE_VALUE(
        kSampleCoverageValue, sample_coverage_value, sample_coverage_inverted)
    ION_UPDATE_VALUE(kStencilFunctionsValue,
                     front_stencil_function,
                     back_stencil_function,
                     front_stencil_reference_value,
                     back_stencil_reference_value,
                     front_stencil_mask,
                     back_stencil_mask)
    ION_UPDATE_VALUE(kStencilOperationsValue,
                     front_stencil_fail_op,
                     front_stencil_depth_fail_op,
                     front_stencil_pass_op,
                     back_stencil_fail_op,
                     back_stencil_depth_fail_op,
                     back_stencil_pass_op)
    ION_UPDATE_VALUE(kViewportValue, viewport)
    ION_UPDATE_VALUE(kScissorBoxValue, scissor_box)
    ION_UPDATE_VALUE(kStencilWriteMasksValue,
                     front_stencil_write_mask,
                     back_stencil_write_mask)
  }
}

#undef ION_UPDATE_VALUE
#undef ION_UPDATE_VALUE_
#undef ION_UPDATE_VALUE1
#undef ION_UPDATE_VALUE2
#undef ION_UPDATE_VALUE3
#undef ION_UPDATE_VALUE4
#undef ION_UPDATE_VALUE5
#undef ION_UPDATE_VALUE6

//---------------------------------------------------------------------------
// Generic value item functions.
void StateTable::ResetValue(Value value) {
// Convenience macro.
#define ION_COPY_VAL(var) data_.var = default_data.var

  const Data& default_data = GetDefaultData();

  switch (value) {
    case kBlendColorValue:
      ION_COPY_VAL(blend_color);
      break;
    case kBlendEquationsValue:
      ION_COPY_VAL(rgb_blend_equation);
      ION_COPY_VAL(alpha_blend_equation);
      break;
    case kBlendFunctionsValue:
      ION_COPY_VAL(rgb_blend_source_factor);
      ION_COPY_VAL(rgb_blend_destination_factor);
      ION_COPY_VAL(alpha_blend_source_factor);
      ION_COPY_VAL(alpha_blend_destination_factor);
      break;
    case kClearColorValue:
      ION_COPY_VAL(clear_color);
      break;
    case kColorWriteMasksValue:
      for (int i = 0; i < 4; ++i) {
        ION_COPY_VAL(color_write_masks[i]);
      }
      break;
    case kCullFaceModeValue:
      ION_COPY_VAL(cull_face_mode);
      break;
    case kFrontFaceModeValue:
      ION_COPY_VAL(front_face_mode);
      break;
    case kClearDepthValue:
      ION_COPY_VAL(clear_depth_value);
      break;
    case kDefaultInnerTessellationLevelValue:
      ION_COPY_VAL(default_inner_tess_level);
      break;
    case kDefaultOuterTessellationLevelValue:
      ION_COPY_VAL(default_outer_tess_level);
      break;
    case kDepthFunctionValue:
      ION_COPY_VAL(depth_function);
      break;
    case kDepthRangeValue:
      ION_COPY_VAL(depth_range);
      break;
    case kDepthWriteMaskValue:
      ION_COPY_VAL(depth_write_mask);
      break;
    case kHintsValue:
      for (int i = 0; i < kNumHints; ++i) {
        ION_COPY_VAL(hints[i]);
      }
      break;
    case kLineWidthValue:
      ION_COPY_VAL(line_width);
      break;
    case kMinSampleShadingValue:
      ION_COPY_VAL(min_sample_shading);
      break;
    case kPolygonOffsetValue:
      ION_COPY_VAL(polygon_offset_factor);
      ION_COPY_VAL(polygon_offset_units);
      break;
    case kSampleCoverageValue:
      ION_COPY_VAL(sample_coverage_value);
      ION_COPY_VAL(sample_coverage_inverted);
      break;
    case kScissorBoxValue:
      data_.scissor_box.SetWithSize(
          default_data.scissor_box.GetMinPoint(),
          math::Vector2i(default_width_, default_height_));
      break;
    case kStencilFunctionsValue:
      ION_COPY_VAL(front_stencil_function);
      ION_COPY_VAL(back_stencil_function);
      ION_COPY_VAL(front_stencil_reference_value);
      ION_COPY_VAL(back_stencil_reference_value);
      ION_COPY_VAL(front_stencil_mask);
      ION_COPY_VAL(back_stencil_mask);
      break;
    case kStencilOperationsValue:
      ION_COPY_VAL(front_stencil_fail_op);
      ION_COPY_VAL(front_stencil_depth_fail_op);
      ION_COPY_VAL(front_stencil_pass_op);
      ION_COPY_VAL(back_stencil_fail_op);
      ION_COPY_VAL(back_stencil_depth_fail_op);
      ION_COPY_VAL(back_stencil_pass_op);
      break;
    case kClearStencilValue:
      ION_COPY_VAL(clear_stencil_value);
      break;
    case kStencilWriteMasksValue:
      ION_COPY_VAL(front_stencil_write_mask);
      ION_COPY_VAL(back_stencil_write_mask);
      break;
    case kViewportValue:
      data_.viewport.SetWithSize(
          default_data.viewport.GetMinPoint(),
          math::Vector2i(default_width_, default_height_));
      break;
    default:
      DCHECK(false) << "Invalid Value type";
      return;
  }

  // Indicate that the value is no longer set in the instance.
  data_.values_set.reset(value);

#undef ION_COPY_VAL
}

//---------------------------------------------------------------------------
// Blending state.

void StateTable::SetBlendColor(const math::Vector4f& color) {
  data_.blend_color = color;
  data_.values_set.set(kBlendColorValue);
}

void StateTable::SetBlendEquations(BlendEquation rgb_eq,
                                   BlendEquation alpha_eq) {
  data_.rgb_blend_equation = rgb_eq;
  data_.alpha_blend_equation = alpha_eq;
  data_.values_set.set(kBlendEquationsValue);
}

void StateTable::SetBlendFunctions(
    BlendFunctionFactor rgb_source_factor,
    BlendFunctionFactor rgb_destination_factor,
    BlendFunctionFactor alpha_source_factor,
    BlendFunctionFactor alpha_destination_factor) {
  data_.rgb_blend_source_factor = rgb_source_factor;
  data_.rgb_blend_destination_factor = rgb_destination_factor;
  data_.alpha_blend_source_factor = alpha_source_factor;
  data_.alpha_blend_destination_factor = alpha_destination_factor;
  data_.values_set.set(kBlendFunctionsValue);
}

//---------------------------------------------------------------------------
// Color state.

void StateTable::SetClearColor(const math::Vector4f& color) {
  data_.clear_color = color;
  data_.values_set.set(kClearColorValue);
}

void StateTable::SetColorWriteMasks(bool red, bool green,
                                    bool blue, bool alpha) {
  data_.color_write_masks[0] = red;
  data_.color_write_masks[1] = green;
  data_.color_write_masks[2] = blue;
  data_.color_write_masks[3] = alpha;
  data_.values_set.set(kColorWriteMasksValue);
}

//---------------------------------------------------------------------------
// Face culling state.

void StateTable::SetCullFaceMode(CullFaceMode mode) {
  data_.cull_face_mode = mode;
  data_.values_set.set(kCullFaceModeValue);
}

// Sets/returns which faces are considered front-facing. The default is
// kCounterClockwise.
void StateTable::SetFrontFaceMode(FrontFaceMode mode) {
  data_.front_face_mode = mode;
  data_.values_set.set(kFrontFaceModeValue);
}

//---------------------------------------------------------------------------
// Depth buffer state.

void StateTable::SetClearDepthValue(float value) {
  data_.clear_depth_value = value;
  data_.values_set.set(kClearDepthValue);
}

void StateTable::SetDepthFunction(DepthFunction func) {
  data_.depth_function = func;
  data_.values_set.set(kDepthFunctionValue);
}

void StateTable::SetDepthRange(const math::Range1f& range) {
  data_.depth_range = range;
  data_.values_set.set(kDepthRangeValue);
}

void StateTable::SetDepthWriteMask(bool mask) {
  data_.depth_write_mask = mask;
  data_.values_set.set(kDepthWriteMaskValue);
}

//---------------------------------------------------------------------------
// Hint state.

void StateTable::SetHint(HintTarget target, HintMode mode) {
  data_.hints[target] = mode;
  data_.values_set.set(kHintsValue);
}

//---------------------------------------------------------------------------
// Line width state.

void StateTable::SetLineWidth(float width) {
  data_.line_width = width;
  data_.values_set.set(kLineWidthValue);
}

//---------------------------------------------------------------------------
// Minimum sample shading fraction state.

void StateTable::SetMinSampleShading(float fraction) {
  data_.min_sample_shading = fraction;
  data_.values_set.set(kMinSampleShadingValue);
}

//---------------------------------------------------------------------------
// Polygon offset state.

void StateTable::SetPolygonOffset(float factor, float units) {
  data_.polygon_offset_factor = factor;
  data_.polygon_offset_units = units;
  data_.values_set.set(kPolygonOffsetValue);
}

//---------------------------------------------------------------------------
// Sample coverage state.

void StateTable::SetSampleCoverage(float value, bool is_inverted) {
  data_.sample_coverage_value = value;
  data_.sample_coverage_inverted = is_inverted;
  data_.values_set.set(kSampleCoverageValue);
}

//---------------------------------------------------------------------------
// Scissoring state.

void StateTable::SetScissorBox(const math::Range2i& box) {
  data_.scissor_box = box;
  data_.values_set.set(kScissorBoxValue);
}

//---------------------------------------------------------------------------
// Stenciling state.

void StateTable::SetStencilFunctions(
    StencilFunction front_func, int front_reference_value, uint32 front_mask,
    StencilFunction back_func, int back_reference_value, uint32 back_mask) {
  data_.front_stencil_function = front_func;
  data_.front_stencil_reference_value = front_reference_value;
  data_.front_stencil_mask = front_mask;
  data_.back_stencil_function = back_func;
  data_.back_stencil_reference_value = back_reference_value;
  data_.back_stencil_mask = back_mask;
  data_.values_set.set(kStencilFunctionsValue);
}

void StateTable::SetStencilOperations(StencilOperation front_stencil_fail,
                                      StencilOperation front_depth_fail,
                                      StencilOperation front_pass,
                                      StencilOperation back_stencil_fail,
                                      StencilOperation back_depth_fail,
                                      StencilOperation back_pass) {
  data_.front_stencil_fail_op = front_stencil_fail;
  data_.front_stencil_depth_fail_op = front_depth_fail;
  data_.front_stencil_pass_op = front_pass;
  data_.back_stencil_fail_op = back_stencil_fail;
  data_.back_stencil_depth_fail_op = back_depth_fail;
  data_.back_stencil_pass_op = back_pass;
  data_.values_set.set(kStencilOperationsValue);
}

// Sets/returns the value to clear stencil buffers to. The default is 0.
void StateTable::SetClearStencilValue(int value) {
  data_.clear_stencil_value = value;
  data_.values_set.set(kClearStencilValue);
}

void StateTable::SetStencilWriteMasks(uint32 front_mask, uint32 back_mask) {
  data_.front_stencil_write_mask = front_mask;
  data_.back_stencil_write_mask = back_mask;
  data_.values_set.set(kStencilWriteMasksValue);
}

//---------------------------------------------------------------------------
// Viewport state.

void StateTable::SetViewport(const math::Range2i& rect) {
  data_.viewport = rect;
  data_.values_set.set(kViewportValue);
}

void StateTable::SetViewport(int left, int bottom, int width, int height) {
  SetViewport(math::Range2i::BuildWithSize(math::Point2i(left, bottom),
                                           math::Vector2i(width, height)));
}

//---------------------------------------------------------------------------
// Enum to string utility functions.

#define ION_INSTANTIATE_GETENUMSTRING(type) \
  template <> ION_API const char* StateTable::GetEnumString(type value) { \
    return base::EnumHelper::GetString(value); \
  }

ION_INSTANTIATE_GETENUMSTRING(Capability);
ION_INSTANTIATE_GETENUMSTRING(BlendEquation);
ION_INSTANTIATE_GETENUMSTRING(BlendFunctionFactor);
ION_INSTANTIATE_GETENUMSTRING(CullFaceMode);
ION_INSTANTIATE_GETENUMSTRING(DepthFunction);
ION_INSTANTIATE_GETENUMSTRING(FrontFaceMode);
ION_INSTANTIATE_GETENUMSTRING(HintMode);
ION_INSTANTIATE_GETENUMSTRING(StencilFunction);
ION_INSTANTIATE_GETENUMSTRING(StencilOperation);

#undef ION_INSTANTIATE_GETENUMSTRING

}  // namespace gfx

//---------------------------------------------------------------------------
// EnumHelper instantiations. These must be in the ion::base namespace.

namespace base {

using gfx::StateTable;

#define ION_CHECK_ARRAYS(enums, strings)                        \
  ION_STATIC_ASSERT(ABSL_ARRAYSIZE(enums) == ABSL_ARRAYSIZE(strings),     \
                    "Wrong size for " #strings)

// The unspecialized version of GetEnumData() should never be called, as it
// will result in a link-time error.

// Specialize for StateTable::Capability.
template <> ION_API const EnumHelper::EnumData<StateTable::Capability>
EnumHelper::GetEnumData() {
  static const GLenum kCapabilities[] = {
      GL_BLEND,
      GL_CLIP_DISTANCE0,
      GL_CLIP_DISTANCE1,
      GL_CLIP_DISTANCE2,
      GL_CLIP_DISTANCE3,
      GL_CLIP_DISTANCE4,
      GL_CLIP_DISTANCE5,
      GL_CLIP_DISTANCE6,
      GL_CLIP_DISTANCE7,
      GL_CULL_FACE,
      GL_DEBUG_OUTPUT_SYNCHRONOUS,
      GL_DEPTH_TEST,
      GL_DITHER,
      GL_MULTISAMPLE,
      GL_POLYGON_OFFSET_FILL,
      GL_RASTERIZER_DISCARD,
      GL_SAMPLE_ALPHA_TO_COVERAGE,
      GL_SAMPLE_COVERAGE,
      GL_SAMPLE_SHADING,
      GL_SCISSOR_TEST,
      GL_STENCIL_TEST,
  };
  static const char* kCapabilityStrings[] = {
      "Blend",
      "ClipDistance0",
      "ClipDistance1",
      "ClipDistance2",
      "ClipDistance3",
      "ClipDistance4",
      "ClipDistance5",
      "ClipDistance6",
      "ClipDistance7",
      "CullFace",
      "DebugOutputSynchronous",
      "DepthTest",
      "Dither",
      "Multisample",
      "PolygonOffsetFill",
      "RasterizerDiscard",
      "SampleAlphaToCoverage",
      "SampleCoverage",
      "SampleShading",
      "ScissorTest",
      "StencilTest",
  };
  ION_CHECK_ARRAYS(kCapabilities, kCapabilityStrings);
  return EnumData<StateTable::Capability>(
      base::IndexMap<StateTable::Capability, GLenum>(
          kCapabilities, ABSL_ARRAYSIZE(kCapabilities)),
      kCapabilityStrings);
}

// Specialize for StateTable::BlendEquation.
template <> ION_API const EnumHelper::EnumData<StateTable::BlendEquation>
EnumHelper::GetEnumData() {
  static const GLenum kBlendEquations[] = {
    GL_FUNC_ADD, GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_SUBTRACT, GL_MIN, GL_MAX
  };
  static const char* kBlendEquationStrings[] = {
    "Add", "ReverseSubtract", "Subtract", "Min", "Max"
  };
  ION_CHECK_ARRAYS(kBlendEquations, kBlendEquationStrings);
  return EnumData<StateTable::BlendEquation>(
      base::IndexMap<StateTable::BlendEquation, GLenum>(
          kBlendEquations, ABSL_ARRAYSIZE(kBlendEquations)),
      kBlendEquationStrings);
}

// Specialize for StateTable::BlendFunctionFactor.
template <> ION_API
const EnumHelper::EnumData<StateTable::BlendFunctionFactor>
EnumHelper::GetEnumData() {
  static const GLenum kBlendFunctionFactors[] = {
    GL_CONSTANT_ALPHA, GL_CONSTANT_COLOR, GL_DST_ALPHA, GL_DST_COLOR, GL_ONE,
    GL_ONE_MINUS_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_COLOR,
    GL_ONE_MINUS_DST_ALPHA, GL_ONE_MINUS_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA,
    GL_ONE_MINUS_SRC_COLOR, GL_SRC_ALPHA, GL_SRC_ALPHA_SATURATE, GL_SRC_COLOR,
    GL_ZERO
  };
  static const char* kBlendFunctionFactorStrings[] = {
    "ConstantAlpha", "ConstantColor", "DstAlpha", "DstColor", "One",
    "OneMinusConstantAlpha", "OneMinusConstantColor", "OneMinusDstAlpha",
    "OneMinusDstColor", "OneMinusSrcAlpha", "OneMinusSrcColor", "SrcAlpha",
    "SrcAlphaSaturate", "SrcColor", "Zero"
  };
  ION_CHECK_ARRAYS(kBlendFunctionFactors, kBlendFunctionFactorStrings);
  return EnumData<StateTable::BlendFunctionFactor>(
      base::IndexMap<StateTable::BlendFunctionFactor, GLenum>(
          kBlendFunctionFactors, ABSL_ARRAYSIZE(kBlendFunctionFactors)),
      kBlendFunctionFactorStrings);
}

// Specialize for StateTable::CullFaceMode.
template <> ION_API const EnumHelper::EnumData<StateTable::CullFaceMode>
EnumHelper::GetEnumData() {
  static const GLenum kCullFaceModes[] = {
    GL_FRONT, GL_BACK, GL_FRONT_AND_BACK
  };
  static const char* kCullFaceModeStrings[] = {
    "CullFront", "CullBack", "CullFrontAndBack",
  };
  ION_CHECK_ARRAYS(kCullFaceModes, kCullFaceModeStrings);
  return EnumData<StateTable::CullFaceMode>(
      base::IndexMap<StateTable::CullFaceMode, GLenum>(
          kCullFaceModes, ABSL_ARRAYSIZE(kCullFaceModes)),
      kCullFaceModeStrings);
}

// Specialize for StateTable::DepthFunction.
template <> ION_API const EnumHelper::EnumData<StateTable::DepthFunction>
EnumHelper::GetEnumData() {
  static const GLenum kDepthFunctions[] = {
    GL_ALWAYS, GL_EQUAL, GL_GREATER, GL_GEQUAL,
    GL_LESS, GL_LEQUAL, GL_NEVER, GL_NOTEQUAL
  };
  static const char* kDepthFunctionStrings[] = {
    "DepthAlways", "DepthEqual", "DepthGreater", "DepthGreaterOrEqual",
    "DepthLess", "DepthLessOrEqual", "DepthNever", "DepthNotEqual"
  };
  ION_CHECK_ARRAYS(kDepthFunctions, kDepthFunctionStrings);
  return EnumData<StateTable::DepthFunction>(
      base::IndexMap<StateTable::DepthFunction, GLenum>(
          kDepthFunctions, ABSL_ARRAYSIZE(kDepthFunctions)),
      kDepthFunctionStrings);
}

// Specialize for StateTable::FrontFaceMode.
template <> ION_API const EnumHelper::EnumData<StateTable::FrontFaceMode>
EnumHelper::GetEnumData() {
  static const GLenum kFrontFaceModes[] = { GL_CW, GL_CCW };
  static const char* kFrontFaceModeStrings[] = {
    "Clockwise", "CounterClockwise"
  };
  ION_CHECK_ARRAYS(kFrontFaceModes, kFrontFaceModeStrings);
  return EnumData<StateTable::FrontFaceMode>(
      base::IndexMap<StateTable::FrontFaceMode, GLenum>(
          kFrontFaceModes, ABSL_ARRAYSIZE(kFrontFaceModes)),
      kFrontFaceModeStrings);
}

// Specialize for StateTable::HintMode.
template <> ION_API const EnumHelper::EnumData<StateTable::HintMode>
EnumHelper::GetEnumData() {
  static const GLenum kHintModes[] = { GL_FASTEST, GL_NICEST, GL_DONT_CARE };
  static const char* kHintModeStrings[] = {
    "HintFastest", "HintNicest", "HintDontCare"
  };
  ION_CHECK_ARRAYS(kHintModes, kHintModeStrings);
  return EnumData<StateTable::HintMode>(
      base::IndexMap<StateTable::HintMode, GLenum>(
          kHintModes, ABSL_ARRAYSIZE(kHintModes)),
      kHintModeStrings);
}

// Specialize for StateTable::StencilFunction.
template <> ION_API const EnumHelper::EnumData<StateTable::StencilFunction>
EnumHelper::GetEnumData() {
  static const GLenum kStencilFunctions[] = {
    GL_ALWAYS, GL_EQUAL, GL_GREATER, GL_GEQUAL,
    GL_LESS, GL_LEQUAL, GL_NEVER, GL_NOTEQUAL
  };
  static const char* kStencilFunctionStrings[] = {
    "StencilAlways", "StencilEqual", "StencilGreater", "StencilGreaterOrEqual",
    "StencilLess", "StencilLessOrEqual", "StencilNever", "StencilNotEqual"
  };
  ION_CHECK_ARRAYS(kStencilFunctions, kStencilFunctionStrings);
  return EnumData<StateTable::StencilFunction>(
      base::IndexMap<StateTable::StencilFunction, GLenum>(
          kStencilFunctions, ABSL_ARRAYSIZE(kStencilFunctions)),
      kStencilFunctionStrings);
}

// Specialize for StateTable::StencilOperation.
template <> ION_API const EnumHelper::EnumData<StateTable::StencilOperation>
EnumHelper::GetEnumData() {
  static const GLenum kStencilOperations[] = {
    GL_DECR, GL_DECR_WRAP, GL_INCR, GL_INCR_WRAP,
    GL_INVERT, GL_KEEP, GL_REPLACE, GL_ZERO
  };
  static const char* kStencilOperationStrings[] = {
    "StencilDecrement", "StencilDecrementAndWrap", "StencilIncrement",
    "StencilIncrementAndWrap", "StencilInvert", "StencilKeep",
    "StencilReplace", "StencilZero"
  };
  ION_CHECK_ARRAYS(kStencilOperations, kStencilOperationStrings);
  return EnumData<StateTable::StencilOperation>(
      base::IndexMap<StateTable::StencilOperation, GLenum>(
          kStencilOperations, ABSL_ARRAYSIZE(kStencilOperations)),
      kStencilOperationStrings);
}

#undef ION_CHECK_ARRAYS

}  // namespace base

namespace gfx {

//---------------------------------------------------------------------------
// Data struct functions.

const StateTable::Data& StateTable::GetDefaultData() {
  // Use the special private constructor that initializes everything properly.
  static const StateTable::Data default_data(true);
  return default_data;
}

StateTable::Data::Data(bool unused) {
  // Reset all the capability flags except kDither and kMultisample.
  capabilities.reset();
  capabilities.set(kDither);
  capabilities.set(kMultisample);

  // Reset all the is-set flags.
  capabilities_set.reset();
  values_set.reset();

  is_enforced = false;

  // Reset all the values to their defaults.
  blend_color.Set(0.0f, 0.0f, 0.0f, 0.0f);
  rgb_blend_equation = alpha_blend_equation = kAdd;
  rgb_blend_source_factor = alpha_blend_source_factor = kOne;
  rgb_blend_destination_factor = alpha_blend_destination_factor = kZero;
  clear_color.Set(0.0f, 0.0f, 0.0f, 0.0f);
  color_write_masks[0] = color_write_masks[1] =
      color_write_masks[2] = color_write_masks[3] = true;
  cull_face_mode = kCullBack;
  front_face_mode = kCounterClockwise;
  clear_depth_value = 1.0f;
  depth_function = kDepthLess;
  depth_range.Set(0.0f, 1.0f);
  depth_write_mask = true;
  hints[kGenerateMipmapHint] = kHintDontCare;
  line_width = 1.0f;
  polygon_offset_factor = 0.0f;
  polygon_offset_units = 0.0f;
  sample_coverage_value = 1.0f;
  sample_coverage_inverted = false;
  min_sample_shading = 0.0f;
  scissor_box.Set(math::Point2i::Zero(), math::Point2i::Zero());
  front_stencil_function = back_stencil_function = kStencilAlways;
  front_stencil_reference_value = back_stencil_reference_value = 0;
  front_stencil_mask = back_stencil_mask = 0xffffffff;
  front_stencil_fail_op = front_stencil_depth_fail_op =
      front_stencil_pass_op = back_stencil_fail_op =
      back_stencil_depth_fail_op = back_stencil_pass_op = kStencilKeep;
  clear_stencil_value = 0;
  front_stencil_write_mask = back_stencil_write_mask = 0xffffffff;
  viewport.Set(math::Point2i::Zero(), math::Point2i::Zero());
}

}  // namespace gfx
}  // namespace ion
