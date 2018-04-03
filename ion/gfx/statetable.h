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

#ifndef ION_GFX_STATETABLE_H_
#define ION_GFX_STATETABLE_H_

#include <bitset>

#include "base/integral_types.h"
#include "ion/base/referent.h"
#include "ion/math/range.h"
#include "ion/math/vector.h"

namespace ion {
namespace gfx {

// A StateTable represents a collection of graphical state items that affect
// OpenGL rendering.
//
// State items are divided into two broad categories: capabilities and values.
// Capabilities are Boolean flags that are set in OpenGL with glEnable() or
// glDisable(). Values are all other global state items, arranged into
// meaningful categories.
//
// Each item in the state table stores its value and an is-set flag. An unset
// item is not applied, and when state tables are used in a Node tree, unset
// items are interpreted as "inherit from parent". Calling any function that
// sets an item causes that item's is-set flag to be set to true.
//
// Capabilities are set with Enable() and Disable() functions and unset with
// ResetCapability(). Values are set with a dedicated function for each value
// and reset with ResetValue().
//
// A default-constructed StateTable has each item initialized to its OpenGL
// default state and its is-set flag initialized to false. Calling
// ResetSetState() will clear the set flags for all items.
class ION_API StateTable : public base::Referent {
 public:
  //---------------------------------------------------------------------------
  // Enumerated types for StateTable items.

  // The number of independent user-defined clipping distances.
  static const size_t kClipDistanceCount = 8;

  // OpenGL capability items. Each can be enabled or disabled.
  enum Capability {
    kBlend,                   // Corresponds to GL_BLEND.
    kClipDistance0,           // Corresponds to GL_CLIP_DISTANCE0.
    kClipDistance1,           // Corresponds to GL_CLIP_DISTANCE1.
    kClipDistance2,           // Corresponds to GL_CLIP_DISTANCE2.
    kClipDistance3,           // Corresponds to GL_CLIP_DISTANCE3.
    kClipDistance4,           // Corresponds to GL_CLIP_DISTANCE4.
    kClipDistance5,           // Corresponds to GL_CLIP_DISTANCE5.
    kClipDistance6,           // Corresponds to GL_CLIP_DISTANCE6.
    kClipDistance7,           // Corresponds to GL_CLIP_DISTANCE7.
    kCullFace,                // Corresponds to GL_CULL_FACE.
    kDebugOutputSynchronous,  // Corresponds to GL_DEBUG_OUTPUT_SYNCHRONOUS
    kDepthTest,               // Corresponds to GL_DEPTH_TEST.
    kDither,                  // Corresponds to GL_DITHER.
    kMultisample,             // Corresponds to GL_MULTISAMPLE.
    kPolygonOffsetFill,       // Corresponds to GL_POLYGON_OFFSET_FILL.
    kRasterizerDiscard,       // Corresponds to GL_RASTERIZER_DISCARD.
    kSampleAlphaToCoverage,   // Corresponds to GL_SAMPLE_ALPHA_TO_COVERAGE.
    kSampleCoverage,          // Corresponds to GL_SAMPLE_COVERAGE.
    kSampleShading,           // Corresponds to GL_SAMPLE_SHADING.
    kScissorTest,             // Corresponds to GL_SCISSOR_TEST.
    kStencilTest,             // Corresponds to GL_STENCIL_TEST.
    kNumCapabilities,         // The number of supported capabilities.
  };

  // OpenGL state value items. Each of these may actually represent multiple
  // values that are passed in unison to a single OpenGL function.
  enum Value {
    kBlendColorValue,
    kBlendEquationsValue,
    kBlendFunctionsValue,
    kClearColorValue,
    kClearDepthValue,
    kClearStencilValue,
    kColorWriteMasksValue,
    kCullFaceModeValue,
    kFrontFaceModeValue,
    kDefaultInnerTessellationLevelValue,
    kDefaultOuterTessellationLevelValue,
    kDepthFunctionValue,
    kDepthRangeValue,
    kDepthWriteMaskValue,
    kHintsValue,
    kLineWidthValue,
    kMinSampleShadingValue,
    kPolygonOffsetValue,
    kSampleCoverageValue,
    kScissorBoxValue,
    kStencilFunctionsValue,
    kStencilOperationsValue,
    kStencilWriteMasksValue,
    kViewportValue,
    kNumValues,
  };

  //---------------------------------------------------------------------------
  // Other enumerated types.

  // OpenGL blend equations.
  enum BlendEquation {
    kAdd,                    // Corresponds to GL_FUNC_ADD.
    kReverseSubtract,        // Corresponds to GL_FUNC_REVERSE_SUBTRACT.
    kSubtract,               // Corresponds to GL_FUNC_SUBTRACT
    kMin,                    // Corresponds to GL_MIN
    kMax,                    // Corresponds to GL_MAX
  };

  // OpenGL blend function factors.
  enum BlendFunctionFactor {
    kConstantAlpha,          // Corresponds to GL_CONSTANT_ALPHA.
    kConstantColor,          // Corresponds to GL_CONSTANT_COLOR.
    kDstAlpha,               // Corresponds to GL_DST_ALPHA.
    kDstColor,               // Corresponds to GL_DST_COLOR.
    kOne,                    // Corresponds to GL_ONE.
    kOneMinusConstantAlpha,  // Corresponds to GL_ONE_MINUS_CONSTANT_ALPHA.
    kOneMinusConstantColor,  // Corresponds to GL_ONE_MINUS_CONSTANT_COLOR.
    kOneMinusDstAlpha,       // Corresponds to GL_ONE_MINUS_DST_ALPHA.
    kOneMinusDstColor,       // Corresponds to GL_ONE_MINUS_DST_COLOR.
    kOneMinusSrcAlpha,       // Corresponds to GL_ONE_MINUS_SRC_ALPHA.
    kOneMinusSrcColor,       // Corresponds to GL_ONE_MINUS_SRC_COLOR.
    kSrcAlpha,               // Corresponds to GL_SRC_ALPHA.
    kSrcAlphaSaturate,       // Corresponds to GL_SRC_ALPHA_SATURATE.
    kSrcColor,               // Corresponds to GL_SRC_COLOR.
    kZero,                   // Corresponds to GL_ZERO.
  };

  // OpenGL clear mask bits.
  enum ClearMaskBit {
    kClearColorBufferBit,    // Corresponds to GL_COLOR_BUFFER_BIT.
    kClearDepthBufferBit,    // Corresponds to GL_DEPTH_BUFFER_BIT.
    kClearStencilBufferBit,  // Corresponds to GL_STENCIL_BUFFER_BIT.
  };

  // OpenGL cull face modes.
  enum CullFaceMode {
    kCullFront,              // Corresponds to GL_FRONT.
    kCullBack,               // Corresponds to GL_BACK.
    kCullFrontAndBack,       // Corresponds to GL_FRONT_AND_BACK.
  };

  // OpenGL depth test functions.
  enum DepthFunction {
    kDepthAlways,            // Corresponds to GL_ALWAYS.
    kDepthEqual,             // Corresponds to GL_EQUAL.
    kDepthGreater,           // Corresponds to GL_GREATER.
    kDepthGreaterOrEqual,    // Corresponds to GL_GEQUAL.
    kDepthLess,              // Corresponds to GL_LESS.
    kDepthLessOrEqual,       // Corresponds to GL_LEQUAL.
    kDepthNever,             // Corresponds to GL_NEVER.
    kDepthNotEqual,          // Corresponds to GL_NOTEQUAL.
  };

  // OpenGL front face modes.
  enum FrontFaceMode {
    kClockwise,              // Corresponds to GL_CW.
    kCounterClockwise,       // Corresponds to GL_CCW.
  };

  // OpenGL hint modes.
  enum HintMode {
    kHintFastest,            // Corresponds to GL_FASTEST.
    kHintNicest,             // Corresponds to GL_NICEST.
    kHintDontCare,           // Corresponds to GL_DONT_CARE.
  };

  // OpenGL hint targets.
  enum HintTarget {
    kGenerateMipmapHint,     // Corresponds to GL_GENERATE_MIPMAP_HINT.
  };

  // OpenGL stencil functions.
  enum StencilFunction {
    kStencilAlways,          // Corresponds to GL_ALWAYS.
    kStencilEqual,           // Corresponds to GL_EQUAL.
    kStencilGreater,         // Corresponds to GL_GREATER.
    kStencilGreaterOrEqual,  // Corresponds to GL_GEQUAL.
    kStencilLess,            // Corresponds to GL_LESS.
    kStencilLessOrEqual,     // Corresponds to GL_LEQUAL.
    kStencilNever,           // Corresponds to GL_NEVER.
    kStencilNotEqual,        // Corresponds to GL_NOTEQUAL.
  };

  // OpenGL stencil operations.
  enum StencilOperation {
    kStencilDecrement,         // Corresponds to GL_DECR.
    kStencilDecrementAndWrap,  // Corresponds to GL_DECR_WRAP.
    kStencilIncrement,         // Corresponds to GL_INCR.
    kStencilIncrementAndWrap,  // Corresponds to GL_INCR_WRAP.
    kStencilInvert,            // Corresponds to GL_INVERT.
    kStencilKeep,              // Corresponds to GL_KEEP.
    kStencilReplace,           // Corresponds to GL_REPLACE.
    kStencilZero,              // Corresponds to GL_ZERO.
  };

  //---------------------------------------------------------------------------

  // The constructor initializes the instance to contain all default values. It
  // is passed the default width and height that are used to initialize the
  // viewport and scissor box correctly. If the default constructor is used the
  // default width and height are initialized to zero.
  StateTable()
      : default_width_(0),
        default_height_(0) {
    Reset();
  }
  StateTable(int default_width, int default_height)
      : default_width_(default_width),
        default_height_(default_height) {
    Reset();
  }

  // Resets all items to their default values. All capabilities are disabled
  // except for kDither, which is enabled, and all other values are set to
  // their documented defaults. The window size is used to set the scissor box
  // and viewport values.
  void Reset();

  // Resets the "set" state of the StateTable; future calls to IsValueSet() or
  // IsCapabilitySet() will return false until another setting is changed.
  void ResetSetState() {
    data_.capabilities_set.reset();
    data_.values_set.reset();
  }

  // Sets the "set" state to true for all capabilities and values of the
  // StateTable; future calls to IsValueSet() or IsCapabilitySet() will return
  // true until another setting is changed.
  void MarkAllSet() {
    data_.capabilities_set.set();
    data_.values_set.set();
  }

  // Copies all state (including the default width and height) from another
  // instance.
  void CopyFrom(const StateTable& other);

  // Merges all state that has been set in another instance into this one, using
  // the test bits in state_to_test. State will be changed in this only if it is
  // marked as changed in state_to_test. This is useful for only copying partial
  // StateTables and for undoing changes made by a StateTable, for example:
  //
  // saved_st->CopyFrom(current_st);  // Save the current state.
  // current_st->MergeValuesFrom(new_st, new_st);  // Make some changes.
  // ...
  // current_st->MergeValuesFrom(saved_st, new_st);  // Restore original state
  //                                                 // while properly setting
  //                                                 // set bits.
  void MergeValuesFrom(const StateTable& other,
                       const StateTable& state_to_test);

  // The same as MergeValuesFrom() except that clear-related flags (clear color,
  // depth and stencil values, write masks, and scissor box) are not merged.
  void MergeNonClearValuesFrom(const StateTable& other,
                               const StateTable& state_to_test);


  //---------------------------------------------------------------------------
  // Capability item functions.

  // Sets a flag indicating whether a capability is enabled.
  void Enable(Capability capability, bool is_enabled) {
    data_.capabilities.set(capability, is_enabled);
    data_.capabilities_set.set(capability);
  }

  // Returns a flag indicating whether a capability is enabled.
  bool IsEnabled(Capability capability) const {
    return data_.capabilities.test(capability);
  }

  // Returns the number of capabilities that are enabled in the instance.
  size_t GetEnabledCount() const {
    return data_.capabilities.count();
  }

  // Resets a capability flag to its default state.
  void ResetCapability(Capability capability) {
    if (capability == kDither || capability == kMultisample)
      data_.capabilities.set(capability);
    else
      data_.capabilities.reset(capability);
    data_.capabilities_set.reset(capability);
  }

  // Returns a flag indicating whether a capability was set since the
  // StateTable was constructed or since the last call to ResetCapability() for
  // that capability.
  bool IsCapabilitySet(Capability capability) const {
    return data_.capabilities_set.test(capability);
  }

  // Returns the number of capabilities that are set in the instance.
  size_t GetSetCapabilityCount() const {
    return data_.capabilities_set.count();
  }

  // Returns true if the capabilities set in two instances are the same.
  static bool AreCapabilitiesSame(const StateTable& st0,
                                  const StateTable& st1) {
    return st0.data_.capabilities == st1.data_.capabilities;
  }

  // Returns the number of Capabilities.
  static int GetCapabilityCount() { return kNumCapabilities; }

  //---------------------------------------------------------------------------
  // Generic value item functions.

  // Resets a value item to its default state.
  void ResetValue(Value value);

  // Returns a flag indicating whether a value was set since the StateTable was
  // constructed or since the last call to ResetValue() for that value.
  bool IsValueSet(Value value) const {
    return data_.values_set.test(value);
  }

  // Returns the number of values that are set in the instance.
  size_t GetSetValueCount() const {
    return data_.values_set.count();
  }

  // Returns the number of Values.
  static int GetValueCount() { return kNumValues; }

  //---------------------------------------------------------------------------
  // Sets/returns whether enforcement is enabled. When enforcement is enabled,
  // GL calls that set the items will be made even if the item matches the
  // state cache. (Normally, GL calls that don't change existing state are
  // elided.)
  void SetEnforceSettings(bool enforced) { data_.is_enforced = enforced; }
  bool AreSettingsEnforced() const { return data_.is_enforced; }

  //---------------------------------------------------------------------------
  // Blending state.

  // Sets/returns the blend color. The default is (0,0,0,0).
  void SetBlendColor(const math::Vector4f& color);
  const math::Vector4f& GetBlendColor() const { return data_.blend_color; }

  // Sets/returns the RGB and alpha blend equations. The default is kAdd for
  // both.
  void SetBlendEquations(BlendEquation rgb_eq, BlendEquation alpha_eq);
  BlendEquation GetRgbBlendEquation() const { return data_.rgb_blend_equation; }
  BlendEquation GetAlphaBlendEquation() const {
    return data_.alpha_blend_equation;
  }

  // Sets/returns the source and destination factors for the RGB and alpha
  // blend functions. The default is kOne for source and kZero for destination.
  void SetBlendFunctions(BlendFunctionFactor rgb_source_factor,
                         BlendFunctionFactor rgb_destination_factor,
                         BlendFunctionFactor alpha_source_factor,
                         BlendFunctionFactor alpha_destination_factor);
  BlendFunctionFactor GetRgbBlendFunctionSourceFactor() const {
    return data_.rgb_blend_source_factor;
  }
  BlendFunctionFactor GetRgbBlendFunctionDestinationFactor() const {
    return data_.rgb_blend_destination_factor;
  }
  BlendFunctionFactor GetAlphaBlendFunctionSourceFactor() const {
    return data_.alpha_blend_source_factor;
  }
  BlendFunctionFactor GetAlphaBlendFunctionDestinationFactor() const {
    return data_.alpha_blend_destination_factor;
  }

  //---------------------------------------------------------------------------
  // Clear state.

  // Sets/returns the color to clear color buffers to. The default is (0,0,0,0).
  void SetClearColor(const math::Vector4f& color);
  const math::Vector4f& GetClearColor() const { return data_.clear_color; }

  // Sets/returns the value to clear depth buffers to. The default is 1.
  void SetClearDepthValue(float value);
  float GetClearDepthValue() const { return data_.clear_depth_value; }

  // Sets/returns the value to clear stencil buffers to. The default is 0.
  void SetClearStencilValue(int value);
  int GetClearStencilValue() const { return data_.clear_stencil_value; }

  //---------------------------------------------------------------------------
  // Color state.

  // Sets/returns the mask to use when writing colors. The default is true for
  // all components.
  void SetColorWriteMasks(bool red, bool green, bool blue, bool alpha);
  bool GetRedColorWriteMask() const { return data_.color_write_masks[0]; }
  bool GetGreenColorWriteMask() const { return data_.color_write_masks[1]; }
  bool GetBlueColorWriteMask() const { return data_.color_write_masks[2]; }
  bool GetAlphaColorWriteMask() const { return data_.color_write_masks[3]; }

  //---------------------------------------------------------------------------
  // Face culling state.

  // Sets/returns which faces are culled when culling is enabled. The default
  // is kCullBack.
  void SetCullFaceMode(CullFaceMode mode);
  CullFaceMode GetCullFaceMode() const { return data_.cull_face_mode; }

  // Sets/returns which faces are considered front-facing. The default is
  // kCounterClockwise.
  void SetFrontFaceMode(FrontFaceMode mode);
  FrontFaceMode GetFrontFaceMode() const { return data_.front_face_mode; }

  void SetDefaultInnerTessellationLevel(const math::Vector2f& value) {
    data_.default_inner_tess_level = value;
    data_.values_set.set(kDefaultInnerTessellationLevelValue);
  }
  const math::Vector2f& GetDefaultInnerTessellationLevel() const {
    return data_.default_inner_tess_level;
  }
  void SetDefaultOuterTessellationLevel(const math::Vector4f& value) {
    data_.default_outer_tess_level = value;
    data_.values_set.set(kDefaultOuterTessellationLevelValue);
  }
  const math::Vector4f& GetDefaultOuterTessellationLevel() const {
    return data_.default_outer_tess_level;
  }

  //---------------------------------------------------------------------------
  // Depth buffer state.

  // Sets/returns the function to use for depth testing when enabled. The
  // default is kDepthLess.
  void SetDepthFunction(DepthFunction func);
  DepthFunction GetDepthFunction() const { return data_.depth_function; }

  // Sets/returns the range to use for mapping depth values. The default is 0
  // for near_value and 1 for far_value.
  void SetDepthRange(const math::Range1f& range);
  const math::Range1f& GetDepthRange() const { return data_.depth_range; }

  // Sets/returns whether depth values will be written. The default is true.
  void SetDepthWriteMask(bool mask);
  bool GetDepthWriteMask() const { return data_.depth_write_mask; }

  //---------------------------------------------------------------------------
  // Hint state.

  // Sets/returns a hint value. The default is kHintDontCare for all hints.
  void SetHint(HintTarget target, HintMode mode);
  HintMode GetHint(HintTarget target) const { return data_.hints[target]; }

  //---------------------------------------------------------------------------
  // Line width state.

  // Sets/returns the width of rasterized lines, in pixels. The default is 1.
  void SetLineWidth(float width);
  float GetLineWidth() const { return data_.line_width; }

  //---------------------------------------------------------------------------
  // Mininum sample shading fraction state.

  // Sets/returns the minimum fraction of samples for which the fragment
  // program will be executed. 0.0 means the fragment program will be executed
  // only once for each pixel, while 1.0 means the fragment program will be
  // executed for every sample.
  void SetMinSampleShading(float fraction);
  float GetMinSampleShading() const { return data_.min_sample_shading; }

  //---------------------------------------------------------------------------
  // Polygon offset state.

  // Sets/returns the polygon offset factor and units. The default is 0 for
  // both.
  void SetPolygonOffset(float factor, float units);
  float GetPolygonOffsetFactor() const { return data_.polygon_offset_factor; }
  float GetPolygonOffsetUnits() const { return data_.polygon_offset_units; }

  //---------------------------------------------------------------------------
  // Sample coverage state.

  // Sets/returns the sample coverage factor and inversion flag. The default is
  // 1 for value and false for is_inverted.
  void SetSampleCoverage(float value, bool is_inverted);
  float GetSampleCoverageValue() const { return data_.sample_coverage_value; }
  bool IsSampleCoverageInverted() const {
    return data_.sample_coverage_inverted;
  }

  //---------------------------------------------------------------------------
  // Scissoring state.

  // Sets the scissor box. The default state is all zeroes.
  void SetScissorBox(const math::Range2i& box);
  const math::Range2i& GetScissorBox() const { return data_.scissor_box; }

  //---------------------------------------------------------------------------
  // Stenciling state.

  // Sets/returns the function and related values to use for front and back
  // faces when stencils are enabled. The default functions are kStencilAlways,
  // the default reference values are 0, and the default masks are all ones.
  void SetStencilFunctions(
      StencilFunction front_func, int front_reference_value, uint32 front_mask,
      StencilFunction back_func, int back_reference_value, uint32 back_mask);
  StencilFunction GetFrontStencilFunction() const {
    return data_.front_stencil_function;
  }
  StencilFunction GetBackStencilFunction() const {
    return data_.back_stencil_function;
  }
  int GetFrontStencilReferenceValue() const {
    return data_.front_stencil_reference_value;
  }
  int GetBackStencilReferenceValue() const {
    return data_.back_stencil_reference_value;
  }
  uint32 GetFrontStencilMask() const { return data_.front_stencil_mask; }
  uint32 GetBackStencilMask() const { return data_.back_stencil_mask; }

  // Sets/returns the stencil test actions for front and back faces. The
  // default is kStencilKeep for all actions.
  void SetStencilOperations(StencilOperation front_stencil_fail,
                            StencilOperation front_depth_fail,
                            StencilOperation front_pass,
                            StencilOperation back_stencil_fail,
                            StencilOperation back_depth_fail,
                            StencilOperation back_pass);
  StencilOperation GetFrontStencilFailOperation() const {
    return data_.front_stencil_fail_op;
  }
  StencilOperation GetFrontStencilDepthFailOperation() const {
    return data_.front_stencil_depth_fail_op;
  }
  StencilOperation GetFrontStencilPassOperation() const {
    return data_.front_stencil_pass_op;
  }
  StencilOperation GetBackStencilFailOperation() const {
    return data_.back_stencil_fail_op;
  }
  StencilOperation GetBackStencilDepthFailOperation() const {
    return data_.back_stencil_depth_fail_op;
  }
  StencilOperation GetBackStencilPassOperation() const {
    return data_.back_stencil_pass_op;
  }

  // Sets/returns a mask indicating which stencil bits will be written for
  // front and back faces. The default is all ones for both.
  void SetStencilWriteMasks(uint32 front_mask, uint32 back_mask);
  uint32 GetFrontStencilWriteMask() const {
    return data_.front_stencil_write_mask;
  }
  uint32 GetBackStencilWriteMask() const {
    return data_.back_stencil_write_mask;
  }

  //---------------------------------------------------------------------------
  // Viewport state.

  // Sets the viewport rectangle. The default state is all zeroes.
  void SetViewport(const math::Range2i& rect);
  void SetViewport(int left, int bottom, int width, int height);
  // Returns the viewport rectangle.
  const math::Range2i& GetViewport() const { return data_.viewport; }

  //---------------------------------------------------------------------------
  // Returns a string representation of a StateTable enum. For example,
  // passsing kPolygonOffsetFill will return "PolygonOffsetFill".
  template <typename EnumType> static const char* GetEnumString(EnumType value);

 protected:
  // The destructor is protected because all base::Referent classes must have
  // protected or private destructors.
  ~StateTable() override;

 private:
  // The number of state values.
  static const int kNumStateValues = kViewportValue + 1;

  // The number of supported hints.
  static const int kNumHints = kGenerateMipmapHint + 1;

  // This nested class contains all of the data held by a StateTable. It exists
  // so that a default instance can be created and used to initialize and reset
  // StateTable instances. The StateTable class is derived from base::Referent
  // and is therefore not copyable.
  struct Data {
    // The default constructor leaves all values uninitialized, since the
    // StateTable will call Reset() to set them in its constructor.
    Data() {}

    // This constructor is used only for building the static default Data
    // instance returned by GetDefaultData(). The parameter is unused and
    // exists only to create a unique function signature.
    explicit Data(bool unused);

    // Bitset indicating which capabilities have been set in this instance.
    std::bitset<kNumCapabilities> capabilities_set;

    // Bitset indicating which values have been set in this instance.
    std::bitset<kNumStateValues> values_set;

    // Capability flags stored as a bitset.
    std::bitset<kNumCapabilities> capabilities;

    // Enforce boolean.
    // Forces the capabilities/values to be set no matter what the old state is.
    // GL state may have changed and enabling the is_enforced boolean ensures
    // that those values that this has will actually be set.
    bool is_enforced;

    // Blending state.
    math::Vector4f blend_color;
    BlendEquation rgb_blend_equation;
    BlendEquation alpha_blend_equation;
    BlendFunctionFactor rgb_blend_source_factor;
    BlendFunctionFactor rgb_blend_destination_factor;
    BlendFunctionFactor alpha_blend_source_factor;
    BlendFunctionFactor alpha_blend_destination_factor;

    // Clear state.
    math::Vector4f clear_color;
    float clear_depth_value;
    int clear_stencil_value;

    // Color state.
    bool color_write_masks[4];  // Red, green, blue, alpha.

    // Face culling state.
    CullFaceMode cull_face_mode;
    FrontFaceMode front_face_mode;

    // Default tess levels state.
    math::Vector2f default_inner_tess_level;
    math::Vector4f default_outer_tess_level;

    // Depth buffer state.
    DepthFunction depth_function;
    math::Range1f depth_range;
    bool depth_write_mask;

    // Hint state.
    HintMode hints[kNumHints];

    // Line width state.
    float line_width;

    // Polygon offset state.
    float polygon_offset_factor;
    float polygon_offset_units;

    // Sample coverage state.
    float sample_coverage_value;
    bool sample_coverage_inverted;

    // Sample shading state.
    float min_sample_shading;

    // Scissoring state.
    math::Range2i scissor_box;

    // Stenciling state.
    StencilFunction front_stencil_function;
    StencilFunction back_stencil_function;
    int front_stencil_reference_value;
    int back_stencil_reference_value;
    uint32 front_stencil_mask;
    uint32 back_stencil_mask;
    StencilOperation front_stencil_fail_op;
    StencilOperation front_stencil_depth_fail_op;
    StencilOperation front_stencil_pass_op;
    StencilOperation back_stencil_fail_op;
    StencilOperation back_stencil_depth_fail_op;
    StencilOperation back_stencil_pass_op;
    uint32 front_stencil_write_mask;
    uint32 back_stencil_write_mask;

    // Viewport state.
    math::Range2i viewport;
  };

  // Returns a Data instance containing all default settings. This is used to
  // very quickly initialize a StateTable instance to all default values. Note
  // that the default scissor box and viewport settings do not take the window
  // size into account; they are all zeroes.
  static const Data& GetDefaultData();

  // Default width and height passed to the constructor.
  int default_width_;
  int default_height_;

  // Data for this instance.
  Data data_;
};

// Convenience typedef for shared pointer to a StateTable.
using StateTablePtr = base::SharedPtr<StateTable>;

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_STATETABLE_H_
