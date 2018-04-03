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

#include "ion/base/enumhelper.h"
#include "ion/base/invalid.h"
#include "ion/math/vector.h"
#include "ion/port/nullptr.h"  // For kNullFunction.
#include "ion/portgfx/glheaders.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfx {

namespace {

//-----------------------------------------------------------------------------
// Helper functions.

static void TestDefaultStateTable(const StateTable& st,
                                  int default_width, int default_height) {
  // All items are reset by default.
  EXPECT_FALSE(st.IsCapabilitySet(StateTable::kBlend));
  for (GLenum i = 0; i < StateTable::kClipDistanceCount; ++i)
    EXPECT_FALSE(st.IsCapabilitySet(static_cast<StateTable::Capability>(
        StateTable::kClipDistance0 + i)));
  EXPECT_FALSE(st.IsCapabilitySet(StateTable::kCullFace));
  EXPECT_FALSE(st.IsCapabilitySet(StateTable::kDebugOutputSynchronous));
  EXPECT_FALSE(st.IsCapabilitySet(StateTable::kDepthTest));
  EXPECT_FALSE(st.IsCapabilitySet(StateTable::kDither));
  EXPECT_FALSE(st.IsCapabilitySet(StateTable::kMultisample));
  EXPECT_FALSE(st.IsCapabilitySet(StateTable::kPolygonOffsetFill));
  EXPECT_FALSE(st.IsCapabilitySet(StateTable::kRasterizerDiscard));
  EXPECT_FALSE(st.IsCapabilitySet(StateTable::kSampleAlphaToCoverage));
  EXPECT_FALSE(st.IsCapabilitySet(StateTable::kSampleCoverage));
  EXPECT_FALSE(st.IsCapabilitySet(StateTable::kScissorTest));
  EXPECT_FALSE(st.IsCapabilitySet(StateTable::kStencilTest));
  EXPECT_FALSE(st.IsValueSet(StateTable::kBlendColorValue));
  EXPECT_FALSE(st.IsValueSet(StateTable::kBlendEquationsValue));
  EXPECT_FALSE(st.IsValueSet(StateTable::kBlendFunctionsValue));
  EXPECT_FALSE(st.IsValueSet(StateTable::kClearColorValue));
  EXPECT_FALSE(st.IsValueSet(StateTable::kColorWriteMasksValue));
  EXPECT_FALSE(st.IsValueSet(StateTable::kCullFaceModeValue));
  EXPECT_FALSE(st.IsValueSet(StateTable::kFrontFaceModeValue));
  EXPECT_FALSE(st.IsValueSet(StateTable::kClearDepthValue));
  EXPECT_FALSE(st.IsValueSet(StateTable::kDefaultInnerTessellationLevelValue));
  EXPECT_FALSE(st.IsValueSet(StateTable::kDefaultOuterTessellationLevelValue));
  EXPECT_FALSE(st.IsValueSet(StateTable::kDepthFunctionValue));
  EXPECT_FALSE(st.IsValueSet(StateTable::kDepthRangeValue));
  EXPECT_FALSE(st.IsValueSet(StateTable::kDepthWriteMaskValue));
  EXPECT_FALSE(st.IsValueSet(StateTable::kHintsValue));
  EXPECT_FALSE(st.IsValueSet(StateTable::kLineWidthValue));
  EXPECT_FALSE(st.IsValueSet(StateTable::kMinSampleShadingValue));
  EXPECT_FALSE(st.IsValueSet(StateTable::kPolygonOffsetValue));
  EXPECT_FALSE(st.IsValueSet(StateTable::kSampleCoverageValue));
  EXPECT_FALSE(st.IsValueSet(StateTable::kScissorBoxValue));
  EXPECT_FALSE(st.IsValueSet(StateTable::kStencilFunctionsValue));
  EXPECT_FALSE(st.IsValueSet(StateTable::kStencilOperationsValue));
  EXPECT_FALSE(st.IsValueSet(StateTable::kClearStencilValue));
  EXPECT_FALSE(st.IsValueSet(StateTable::kStencilWriteMasksValue));
  EXPECT_FALSE(st.IsValueSet(StateTable::kViewportValue));
  EXPECT_EQ(0U, st.GetSetCapabilityCount());
  EXPECT_EQ(0U, st.GetSetValueCount());

  // All capabilities except dithering are disabled by default.
  EXPECT_FALSE(st.IsEnabled(StateTable::kBlend));
  for (GLenum i = 0; i < StateTable::kClipDistanceCount; ++i)
    EXPECT_FALSE(st.IsEnabled(static_cast<StateTable::Capability>(
        StateTable::kClipDistance0 + i)));
  EXPECT_FALSE(st.IsEnabled(StateTable::kCullFace));
  EXPECT_FALSE(st.IsEnabled(StateTable::kDebugOutputSynchronous));
  EXPECT_FALSE(st.IsEnabled(StateTable::kDepthTest));
  EXPECT_TRUE(st.IsEnabled(StateTable::kDither));
  EXPECT_TRUE(st.IsEnabled(StateTable::kMultisample));
  EXPECT_FALSE(st.IsEnabled(StateTable::kPolygonOffsetFill));
  EXPECT_FALSE(st.IsEnabled(StateTable::kRasterizerDiscard));
  EXPECT_FALSE(st.IsEnabled(StateTable::kSampleAlphaToCoverage));
  EXPECT_FALSE(st.IsEnabled(StateTable::kSampleCoverage));
  EXPECT_FALSE(st.IsEnabled(StateTable::kScissorTest));
  EXPECT_FALSE(st.IsEnabled(StateTable::kStencilTest));
  EXPECT_EQ(2U, st.GetEnabledCount());

  // All other state values have documented defaults.
  EXPECT_EQ(math::Vector4f(0, 0, 0, 0), st.GetBlendColor());
  EXPECT_EQ(StateTable::kAdd, st.GetRgbBlendEquation());
  EXPECT_EQ(StateTable::kAdd, st.GetAlphaBlendEquation());
  EXPECT_EQ(StateTable::kOne, st.GetRgbBlendFunctionSourceFactor());
  EXPECT_EQ(StateTable::kOne, st.GetAlphaBlendFunctionSourceFactor());
  EXPECT_EQ(StateTable::kZero, st.GetRgbBlendFunctionDestinationFactor());
  EXPECT_EQ(StateTable::kZero, st.GetAlphaBlendFunctionDestinationFactor());
  EXPECT_EQ(math::Vector4f(0, 0, 0, 0), st.GetClearColor());
  EXPECT_TRUE(st.GetRedColorWriteMask());
  EXPECT_TRUE(st.GetGreenColorWriteMask());
  EXPECT_TRUE(st.GetBlueColorWriteMask());
  EXPECT_TRUE(st.GetAlphaColorWriteMask());
  EXPECT_EQ(StateTable::kCullBack, st.GetCullFaceMode());
  EXPECT_EQ(StateTable::kCounterClockwise, st.GetFrontFaceMode());
  EXPECT_EQ(1.0f, st.GetClearDepthValue());
  EXPECT_EQ(StateTable::kDepthLess, st.GetDepthFunction());
  EXPECT_EQ(math::Range1f(0.0f, 1.0f), st.GetDepthRange());
  EXPECT_TRUE(st.GetDepthWriteMask());
  EXPECT_EQ(StateTable::kHintDontCare,
            st.GetHint(StateTable::kGenerateMipmapHint));
  EXPECT_EQ(1.0f, st.GetLineWidth());
  EXPECT_EQ(0.0f, st.GetMinSampleShading());
  EXPECT_EQ(0.0f, st.GetPolygonOffsetFactor());
  EXPECT_EQ(0.0f, st.GetPolygonOffsetUnits());
  EXPECT_EQ(1.0f, st.GetSampleCoverageValue());
  EXPECT_FALSE(st.IsSampleCoverageInverted());
  EXPECT_EQ(math::Range2i(math::Point2i(0, 0),
                          math::Point2i(default_width, default_height)),
            st.GetScissorBox());
  EXPECT_EQ(StateTable::kStencilAlways, st.GetFrontStencilFunction());
  EXPECT_EQ(StateTable::kStencilAlways, st.GetBackStencilFunction());
  EXPECT_EQ(0, st.GetFrontStencilReferenceValue());
  EXPECT_EQ(0, st.GetBackStencilReferenceValue());
  EXPECT_EQ(static_cast<uint32>(-1), st.GetFrontStencilMask());
  EXPECT_EQ(static_cast<uint32>(-1), st.GetBackStencilMask());
  EXPECT_EQ(math::Range2i(math::Point2i(0, 0),
                          math::Point2i(default_width, default_height)),
            st.GetViewport());
  // Settings are not enforced by default.
  EXPECT_FALSE(st.AreSettingsEnforced());
}

static void TestCapability(StateTable* st, StateTable::Capability cap) {
  const bool initial_value =
      cap == StateTable::kDither || cap == StateTable::kMultisample;
  const size_t num_other_enabled =
      (cap == StateTable::kDither || cap == StateTable::kMultisample) ? 1U : 2U;

  std::cout << base::EnumHelper::GetString(cap) << std::endl;

  // Verify that the capability has the correct initial value and is not set.
  EXPECT_EQ(initial_value, st->IsEnabled(cap));
  EXPECT_FALSE(st->IsCapabilitySet(cap));

  // Enable it. It should now be enabled and set.
  st->Enable(cap, true);
  EXPECT_TRUE(st->IsEnabled(cap));
  EXPECT_TRUE(st->IsCapabilitySet(cap));

  // Should have no effect on values.
  EXPECT_EQ(0U, st->GetSetValueCount());

  // Verify that no other capabilities are enabled or set.
  EXPECT_EQ(num_other_enabled + 1U, st->GetEnabledCount());
  EXPECT_EQ(1U, st->GetSetCapabilityCount());

  // Disable it. It should now be disabled and set.
  st->Enable(cap, false);
  EXPECT_FALSE(st->IsEnabled(cap));
  EXPECT_TRUE(st->IsCapabilitySet(cap));
  EXPECT_EQ(num_other_enabled, st->GetEnabledCount());
  EXPECT_EQ(1U, st->GetSetCapabilityCount());

  // Reset it. It should now have the initial value and not be set.
  st->ResetCapability(cap);
  EXPECT_EQ(initial_value, st->IsEnabled(cap));
  EXPECT_FALSE(st->IsCapabilitySet(cap));
  EXPECT_EQ(2U, st->GetEnabledCount());
  EXPECT_EQ(0U, st->GetSetCapabilityCount());

  // Enable it again, then reset again. It should still be disabled and not set.
  st->Enable(cap, true);
  st->ResetCapability(cap);
  EXPECT_EQ(initial_value, st->IsEnabled(cap));
  EXPECT_FALSE(st->IsCapabilitySet(cap));
  EXPECT_EQ(2U, st->GetEnabledCount());
  EXPECT_EQ(0U, st->GetSetCapabilityCount());

  // Enable it again, then reset the instance. The capability should still have
  // its initial value and not be set.
  st->Enable(cap, true);
  st->Reset();
  EXPECT_EQ(initial_value, st->IsEnabled(cap));
  EXPECT_FALSE(st->IsCapabilitySet(cap));
  EXPECT_EQ(2U, st->GetEnabledCount());
  EXPECT_EQ(0U, st->GetSetCapabilityCount());
}

// Compares all value items in two tables except for the given item, expecting
// all of them to be equal.
static void CompareTableValues(const StateTable& st0, const StateTable& st1,
                               StateTable::Value except_val) {
// Convenience typedef.
#define ION_COMPARE_VALUES(func) EXPECT_EQ(st0.func, st1.func)

  if (except_val != StateTable::kBlendColorValue) {
    ION_COMPARE_VALUES(GetBlendColor());
  }
  if (except_val != StateTable::kBlendEquationsValue) {
    ION_COMPARE_VALUES(GetRgbBlendEquation());
    ION_COMPARE_VALUES(GetAlphaBlendEquation());
  }
  if (except_val != StateTable::kBlendFunctionsValue) {
    ION_COMPARE_VALUES(GetRgbBlendFunctionSourceFactor());
    ION_COMPARE_VALUES(GetRgbBlendFunctionDestinationFactor());
    ION_COMPARE_VALUES(GetAlphaBlendFunctionSourceFactor());
    ION_COMPARE_VALUES(GetAlphaBlendFunctionSourceFactor());
  }
  if (except_val != StateTable::kClearColorValue) {
    ION_COMPARE_VALUES(GetClearColor());
  }
  if (except_val != StateTable::kColorWriteMasksValue) {
    ION_COMPARE_VALUES(GetRedColorWriteMask());
    ION_COMPARE_VALUES(GetGreenColorWriteMask());
    ION_COMPARE_VALUES(GetBlueColorWriteMask());
    ION_COMPARE_VALUES(GetAlphaColorWriteMask());
  }
  if (except_val != StateTable::kCullFaceModeValue) {
    ION_COMPARE_VALUES(GetCullFaceMode());
  }
  if (except_val != StateTable::kFrontFaceModeValue) {
    ION_COMPARE_VALUES(GetFrontFaceMode());
  }
  if (except_val != StateTable::kClearDepthValue) {
    ION_COMPARE_VALUES(GetClearDepthValue());
  }
  if (except_val != StateTable::kDefaultInnerTessellationLevelValue) {
    ION_COMPARE_VALUES(GetDefaultInnerTessellationLevel());
  }
  if (except_val != StateTable::kDefaultOuterTessellationLevelValue) {
    ION_COMPARE_VALUES(GetDefaultOuterTessellationLevel());
  }
  if (except_val != StateTable::kDepthFunctionValue) {
    ION_COMPARE_VALUES(GetDepthFunction());
  }
  if (except_val != StateTable::kDepthRangeValue) {
    ION_COMPARE_VALUES(GetDepthRange());
  }
  if (except_val != StateTable::kDepthWriteMaskValue) {
    ION_COMPARE_VALUES(GetDepthWriteMask());
  }
  if (except_val != StateTable::kHintsValue) {
    ION_COMPARE_VALUES(GetHint(StateTable::kGenerateMipmapHint));
  }
  if (except_val != StateTable::kLineWidthValue) {
    ION_COMPARE_VALUES(GetLineWidth());
  }
  if (except_val != StateTable::kMinSampleShadingValue) {
    ION_COMPARE_VALUES(GetMinSampleShading());
  }
  if (except_val != StateTable::kPolygonOffsetValue) {
    ION_COMPARE_VALUES(GetPolygonOffsetFactor());
    ION_COMPARE_VALUES(GetPolygonOffsetUnits());
  }
  if (except_val != StateTable::kSampleCoverageValue) {
    ION_COMPARE_VALUES(GetSampleCoverageValue());
    ION_COMPARE_VALUES(IsSampleCoverageInverted());
  }
  if (except_val != StateTable::kScissorBoxValue) {
    ION_COMPARE_VALUES(GetScissorBox());
  }
  if (except_val != StateTable::kStencilFunctionsValue) {
    ION_COMPARE_VALUES(GetFrontStencilFunction());
    ION_COMPARE_VALUES(GetFrontStencilReferenceValue());
    ION_COMPARE_VALUES(GetFrontStencilMask());
    ION_COMPARE_VALUES(GetBackStencilFunction());
    ION_COMPARE_VALUES(GetBackStencilReferenceValue());
    ION_COMPARE_VALUES(GetBackStencilMask());
  }
  if (except_val != StateTable::kStencilOperationsValue) {
    ION_COMPARE_VALUES(GetFrontStencilFailOperation());
    ION_COMPARE_VALUES(GetFrontStencilPassOperation());
    ION_COMPARE_VALUES(GetFrontStencilDepthFailOperation());
    ION_COMPARE_VALUES(GetBackStencilFailOperation());
    ION_COMPARE_VALUES(GetBackStencilPassOperation());
    ION_COMPARE_VALUES(GetBackStencilDepthFailOperation());
  }
  if (except_val != StateTable::kClearStencilValue) {
    ION_COMPARE_VALUES(GetClearStencilValue());
  }
  if (except_val != StateTable::kStencilWriteMasksValue) {
    ION_COMPARE_VALUES(GetFrontStencilWriteMask());
    ION_COMPARE_VALUES(GetBackStencilWriteMask());
  }
  if (except_val != StateTable::kViewportValue) {
    ION_COMPARE_VALUES(GetViewport());
  }

#undef ION_COMPARE_VALUES
}

// Tests changes to a value that has one parameter to set and get.
template <typename T>
static void TestValue1(const StateTable& default_st, StateTable* st,
                       StateTable::Value val,
                       T sample_value,
                       void (StateTable::* set_func)(T),
                       T (StateTable::* get_func)() const) {
  SCOPED_TRACE(val);
  const T default_value = (default_st.*get_func)();

  // Verify that the value item has the default value and is not set.
  EXPECT_EQ(default_value, (st->*get_func)());
  EXPECT_FALSE(st->IsValueSet(val));

  (st->*set_func)(sample_value);
  EXPECT_EQ(sample_value, (st->*get_func)());
  EXPECT_TRUE(st->IsValueSet(val));

  // Verify that no other values are set or incorrect.
  EXPECT_EQ(1U, st->GetSetValueCount());
  CompareTableValues(default_st, *st, val);

  // Reset the value.
  st->ResetValue(val);
  EXPECT_EQ(default_value, (st->*get_func)());
  EXPECT_FALSE(st->IsValueSet(val));
  EXPECT_EQ(0U, st->GetSetValueCount());
}

// Tests changes to a value that has two parameters to set and get.
template <typename T1, typename T2>
static void TestValue2(const StateTable& default_st, StateTable* st,
                       StateTable::Value val,
                       T1 sample_value1, T2 sample_value2,
                       void (StateTable::* set_func)(T1, T2),
                       T1 (StateTable::* get_func1)() const,
                       T2 (StateTable::* get_func2)() const) {
  SCOPED_TRACE(val);
  const T1 default_value1 = (default_st.*get_func1)();
  const T2 default_value2 = (default_st.*get_func2)();

  // Verify that the value item has the default values and is not set.
  EXPECT_EQ(default_value1, (st->*get_func1)());
  EXPECT_EQ(default_value2, (st->*get_func2)());
  EXPECT_FALSE(st->IsValueSet(val));

  (st->*set_func)(sample_value1, sample_value2);
  EXPECT_EQ(sample_value1, (st->*get_func1)());
  EXPECT_EQ(sample_value2, (st->*get_func2)());
  EXPECT_TRUE(st->IsValueSet(val));

  // Verify that no other values are set or incorrect.
  EXPECT_EQ(1U, st->GetSetValueCount());
  CompareTableValues(default_st, *st, val);

  // Reset the value.
  st->ResetValue(val);
  EXPECT_EQ(default_value1, (st->*get_func1)());
  EXPECT_EQ(default_value2, (st->*get_func2)());
  EXPECT_FALSE(st->IsValueSet(val));
  EXPECT_EQ(0U, st->GetSetValueCount());
}

// Tests changes to a value that has four parameters to set and get.
template <typename T1, typename T2, typename T3, typename T4>
static void TestValue4(const StateTable& default_st, StateTable* st,
                       StateTable::Value val,
                       T1 sample_value1, T2 sample_value2,
                       T3 sample_value3, T4 sample_value4,
                       void (StateTable::* set_func)(T1, T2, T3, T4),
                       T1 (StateTable::* get_func1)() const,
                       T2 (StateTable::* get_func2)() const,
                       T3 (StateTable::* get_func3)() const,
                       T4 (StateTable::* get_func4)() const) {
  SCOPED_TRACE(val);
  const T1 default_value1 = (default_st.*get_func1)();
  const T2 default_value2 = (default_st.*get_func2)();
  const T3 default_value3 = (default_st.*get_func3)();
  const T4 default_value4 = (default_st.*get_func4)();

  // Verify that the value item has the default values and is not set.
  EXPECT_EQ(default_value1, (st->*get_func1)());
  EXPECT_EQ(default_value2, (st->*get_func2)());
  EXPECT_EQ(default_value3, (st->*get_func3)());
  EXPECT_EQ(default_value4, (st->*get_func4)());
  EXPECT_FALSE(st->IsValueSet(val));

  (st->*set_func)(sample_value1, sample_value2, sample_value3, sample_value4);
  EXPECT_EQ(sample_value1, (st->*get_func1)());
  EXPECT_EQ(sample_value2, (st->*get_func2)());
  EXPECT_EQ(sample_value3, (st->*get_func3)());
  EXPECT_EQ(sample_value4, (st->*get_func4)());
  EXPECT_TRUE(st->IsValueSet(val));

  // Verify that no other values are set or incorrect.
  EXPECT_EQ(1U, st->GetSetValueCount());
  CompareTableValues(default_st, *st, val);

  // Reset the value.
  st->ResetValue(val);
  EXPECT_EQ(default_value1, (st->*get_func1)());
  EXPECT_EQ(default_value2, (st->*get_func2)());
  EXPECT_EQ(default_value3, (st->*get_func3)());
  EXPECT_EQ(default_value4, (st->*get_func4)());
  EXPECT_FALSE(st->IsValueSet(val));
  EXPECT_EQ(0U, st->GetSetValueCount());
}

// Tests changes to a value that has six parameters to set and get.
template <typename T1, typename T2, typename T3,
          typename T4, typename T5, typename T6>
static void TestValue6(const StateTable& default_st, StateTable* st,
                       StateTable::Value val,
                       T1 sample_value1, T2 sample_value2,
                       T3 sample_value3, T4 sample_value4,
                       T5 sample_value5, T6 sample_value6,
                       void (StateTable::* set_func)(T1, T2, T3, T4, T5, T6),
                       T1 (StateTable::* get_func1)() const,
                       T2 (StateTable::* get_func2)() const,
                       T3 (StateTable::* get_func3)() const,
                       T4 (StateTable::* get_func4)() const,
                       T5 (StateTable::* get_func5)() const,
                       T6 (StateTable::* get_func6)() const) {
  SCOPED_TRACE(val);
  const T1 default_value1 = (default_st.*get_func1)();
  const T2 default_value2 = (default_st.*get_func2)();
  const T3 default_value3 = (default_st.*get_func3)();
  const T4 default_value4 = (default_st.*get_func4)();
  const T5 default_value5 = (default_st.*get_func5)();
  const T6 default_value6 = (default_st.*get_func6)();

  // Verify that the value item has the default values and is not set.
  EXPECT_EQ(default_value1, (st->*get_func1)());
  EXPECT_EQ(default_value2, (st->*get_func2)());
  EXPECT_EQ(default_value3, (st->*get_func3)());
  EXPECT_EQ(default_value4, (st->*get_func4)());
  EXPECT_EQ(default_value5, (st->*get_func5)());
  EXPECT_EQ(default_value6, (st->*get_func6)());
  EXPECT_FALSE(st->IsValueSet(val));

  (st->*set_func)(sample_value1, sample_value2, sample_value3,
                  sample_value4, sample_value5, sample_value6);
  EXPECT_EQ(sample_value1, (st->*get_func1)());
  EXPECT_EQ(sample_value2, (st->*get_func2)());
  EXPECT_EQ(sample_value3, (st->*get_func3)());
  EXPECT_EQ(sample_value4, (st->*get_func4)());
  EXPECT_EQ(sample_value5, (st->*get_func5)());
  EXPECT_EQ(sample_value6, (st->*get_func6)());
  EXPECT_TRUE(st->IsValueSet(val));

  // Verify that no other values are set or incorrect.
  EXPECT_EQ(1U, st->GetSetValueCount());
  CompareTableValues(default_st, *st, val);

  // Reset the value.
  st->ResetValue(val);
  EXPECT_EQ(default_value1, (st->*get_func1)());
  EXPECT_EQ(default_value2, (st->*get_func2)());
  EXPECT_EQ(default_value3, (st->*get_func3)());
  EXPECT_EQ(default_value4, (st->*get_func4)());
  EXPECT_EQ(default_value5, (st->*get_func5)());
  EXPECT_EQ(default_value6, (st->*get_func6)());
  EXPECT_FALSE(st->IsValueSet(val));
  EXPECT_EQ(0U, st->GetSetValueCount());
}

// Special function for testing hints, which can't be done with a templated
// function.
static void TestHints(const StateTable& default_st, StateTable* st) {
  static const int kMaxHintTarget =
      static_cast<int>(StateTable::kGenerateMipmapHint);

  // Verify that the hints item is not set.
  EXPECT_FALSE(st->IsValueSet(StateTable::kHintsValue));

  // Verify that each hint has the default value.
  for (int i = 0; i <= kMaxHintTarget; ++i) {
    const StateTable::HintTarget ht = static_cast<StateTable::HintTarget>(i);
    EXPECT_EQ(default_st.GetHint(ht), st->GetHint(ht));
  }

  for (int i = 0; i <= kMaxHintTarget; ++i) {
    const StateTable::HintTarget ht = static_cast<StateTable::HintTarget>(i);

    // Change a hint and verify that the item is set and the hint has the
    // proper value.
    st->SetHint(ht, StateTable::kHintNicest);
    EXPECT_EQ(StateTable::kHintNicest, st->GetHint(ht));
    EXPECT_TRUE(st->IsValueSet(StateTable::kHintsValue));

    // Other hints should not have changed.
    for (int j = 0; j <= kMaxHintTarget; ++j) {
      if (j == i)
        continue;
      const StateTable::HintTarget ht2 = static_cast<StateTable::HintTarget>(j);
      EXPECT_EQ(default_st.GetHint(ht2), st->GetHint(ht2));
    }

    // Verify that no other values are set or incorrect.
    EXPECT_EQ(1U, st->GetSetValueCount());
    CompareTableValues(default_st, *st, StateTable::kHintsValue);

    // Reset the value.
    st->ResetValue(StateTable::kHintsValue);
    for (int j = 0; j <= kMaxHintTarget; ++j) {
      const StateTable::HintTarget ht2 = static_cast<StateTable::HintTarget>(j);
      EXPECT_EQ(default_st.GetHint(ht2), st->GetHint(ht2));
    }
    EXPECT_FALSE(st->IsValueSet(StateTable::kHintsValue));
    EXPECT_EQ(0U, st->GetSetValueCount());
  }
}

}  // anonymous namespace

//-----------------------------------------------------------------------------
// The tests.

TEST(StateTable, Default) {
  StateTablePtr st(new StateTable(300, 200));
  TestDefaultStateTable(*st, 300, 200);

  // Test that the default constructor initializes the width and height to 0.
  StateTablePtr st2(new StateTable());
  TestDefaultStateTable(*st2, 0, 0);
}

TEST(StateTable, Capabilities) {
  StateTablePtr st(new StateTable(100, 100));
  TestCapability(st.Get(), StateTable::kBlend);
  for (GLenum i = 0; i < StateTable::kClipDistanceCount; ++i)
    TestCapability(st.Get(), static_cast<StateTable::Capability>(
        StateTable::kClipDistance0 + i));
  TestCapability(st.Get(), StateTable::kCullFace);
  TestCapability(st.Get(), StateTable::kDepthTest);
  TestCapability(st.Get(), StateTable::kDither);
  TestCapability(st.Get(), StateTable::kMultisample);
  TestCapability(st.Get(), StateTable::kPolygonOffsetFill);
  TestCapability(st.Get(), StateTable::kRasterizerDiscard);
  TestCapability(st.Get(), StateTable::kSampleAlphaToCoverage);
  TestCapability(st.Get(), StateTable::kSampleCoverage);
  TestCapability(st.Get(), StateTable::kScissorTest);
  TestCapability(st.Get(), StateTable::kStencilTest);

  EXPECT_EQ(StateTable::kStencilTest + 1, StateTable::GetCapabilityCount());

  // Check a few strings.
  EXPECT_EQ(std::string("Blend"),
            StateTable::GetEnumString(StateTable::kBlend));
  EXPECT_EQ(std::string("Dither"),
            StateTable::GetEnumString(StateTable::kDither));
  EXPECT_EQ(std::string("ScissorTest"),
            StateTable::GetEnumString(StateTable::kScissorTest));
}

TEST(StateTable, Values) {
  StateTablePtr default_st(new StateTable(600, 400));
  StateTablePtr st(new StateTable(600, 400));

  TestValue1<const math::Vector4f&>(
      *default_st, st.Get(), StateTable::kBlendColorValue,
      math::Vector4f(0.1f, 0.2f, 0.3f, 0.4f),
      &StateTable::SetBlendColor, &StateTable::GetBlendColor);

  TestValue2<StateTable::BlendEquation, StateTable::BlendEquation>(
      *default_st, st.Get(), StateTable::kBlendEquationsValue,
      StateTable::kSubtract, StateTable::kReverseSubtract,
      &StateTable::SetBlendEquations,
      &StateTable::GetRgbBlendEquation, &StateTable::GetAlphaBlendEquation);

  TestValue4<StateTable::BlendFunctionFactor, StateTable::BlendFunctionFactor,
             StateTable::BlendFunctionFactor, StateTable::BlendFunctionFactor>(
      *default_st, st.Get(), StateTable::kBlendFunctionsValue,
      StateTable::kSrcColor, StateTable::kOneMinusDstColor,
      StateTable::kOneMinusConstantAlpha, StateTable::kDstColor,
      &StateTable::SetBlendFunctions,
      &StateTable::GetRgbBlendFunctionSourceFactor,
      &StateTable::GetRgbBlendFunctionDestinationFactor,
      &StateTable::GetAlphaBlendFunctionSourceFactor,
      &StateTable::GetAlphaBlendFunctionDestinationFactor);

  TestValue1<const math::Vector4f&>(
      *default_st, st.Get(), StateTable::kClearColorValue,
      math::Vector4f(0.4f, 0.5f, 0.6f, 0.7f),
      &StateTable::SetClearColor, &StateTable::GetClearColor);

  TestValue4<bool, bool, bool, bool>(
      *default_st, st.Get(), StateTable::kColorWriteMasksValue,
      false, true, true, false,
      &StateTable::SetColorWriteMasks,
      &StateTable::GetRedColorWriteMask, &StateTable::GetGreenColorWriteMask,
      &StateTable::GetBlueColorWriteMask, &StateTable::GetAlphaColorWriteMask);

  TestValue1<StateTable::CullFaceMode>(
      *default_st, st.Get(), StateTable::kCullFaceModeValue,
      StateTable::kCullFrontAndBack,
      &StateTable::SetCullFaceMode, &StateTable::GetCullFaceMode);

  TestValue1<StateTable::FrontFaceMode>(
      *default_st, st.Get(), StateTable::kFrontFaceModeValue,
      StateTable::kClockwise,
      &StateTable::SetFrontFaceMode, &StateTable::GetFrontFaceMode);

  TestValue1<float>(
      *default_st, st.Get(), StateTable::kClearDepthValue,
      0.2f,
      &StateTable::SetClearDepthValue, &StateTable::GetClearDepthValue);

  TestValue1<const math::Vector2f&>(
      *default_st, st.Get(), StateTable::kDefaultInnerTessellationLevelValue,
      math::Vector2f(1.0, 2.0),
      &StateTable::SetDefaultInnerTessellationLevel,
      &StateTable::GetDefaultInnerTessellationLevel);

  TestValue1<const math::Vector4f&>(
      *default_st, st.Get(), StateTable::kDefaultOuterTessellationLevelValue,
      math::Vector4f(1.0, 2.0, 3.0, 4.0),
      &StateTable::SetDefaultOuterTessellationLevel,
      &StateTable::GetDefaultOuterTessellationLevel);

  TestValue1<StateTable::DepthFunction>(
      *default_st, st.Get(), StateTable::kDepthFunctionValue,
      StateTable::kDepthNotEqual,
      &StateTable::SetDepthFunction, &StateTable::GetDepthFunction);

  TestValue1<const math::Range1f&>(
      *default_st, st.Get(), StateTable::kDepthRangeValue,
      math::Range1f(0.2f, 0.6f),
      &StateTable::SetDepthRange, &StateTable::GetDepthRange);

  TestValue1<bool>(
      *default_st, st.Get(), StateTable::kDepthWriteMaskValue,
      false,
      &StateTable::SetDepthWriteMask, &StateTable::GetDepthWriteMask);

  // Hints are a special case that don't work with the templated functions.
  TestHints(*default_st, st.Get());

  TestValue1<float>(
      *default_st, st.Get(), StateTable::kLineWidthValue,
      0.25f,
      &StateTable::SetLineWidth, &StateTable::GetLineWidth);

  TestValue1<float>(
      *default_st, st.Get(), StateTable::kMinSampleShadingValue,
      0.5f,
      &StateTable::SetMinSampleShading, &StateTable::GetMinSampleShading);

  TestValue2<float, float>(
      *default_st, st.Get(), StateTable::kPolygonOffsetValue,
      0.5f, 2.0f,
      &StateTable::SetPolygonOffset,
      &StateTable::GetPolygonOffsetFactor, &StateTable::GetPolygonOffsetUnits);

  TestValue2<float, bool>(
      *default_st, st.Get(), StateTable::kSampleCoverageValue,
      0.4f, true,
      &StateTable::SetSampleCoverage,
      &StateTable::GetSampleCoverageValue,
      &StateTable::IsSampleCoverageInverted);

  TestValue1<const math::Range2i&>(
      *default_st, st.Get(), StateTable::kScissorBoxValue,
      math::Range2i(math::Point2i(10, 20), math::Point2i(210, 320)),
      &StateTable::SetScissorBox, &StateTable::GetScissorBox);

  TestValue6<StateTable::StencilFunction, int, uint32,
             StateTable::StencilFunction, int, uint32>(
      *default_st, st.Get(), StateTable::kStencilFunctionsValue,
      StateTable::kStencilNever, 10, 0x40404040,
      StateTable::kStencilLess, 5, 0x12345678,
      &StateTable::SetStencilFunctions,
      &StateTable::GetFrontStencilFunction,
      &StateTable::GetFrontStencilReferenceValue,
      &StateTable::GetFrontStencilMask,
      &StateTable::GetBackStencilFunction,
      &StateTable::GetBackStencilReferenceValue,
      &StateTable::GetBackStencilMask);

  TestValue6<StateTable::StencilOperation, StateTable::StencilOperation,
             StateTable::StencilOperation, StateTable::StencilOperation,
             StateTable::StencilOperation, StateTable::StencilOperation>(
      *default_st, st.Get(), StateTable::kStencilOperationsValue,
      StateTable::kStencilDecrement, StateTable::kStencilDecrementAndWrap,
      StateTable::kStencilIncrement, StateTable::kStencilIncrementAndWrap,
      StateTable::kStencilInvert, StateTable::kStencilReplace,
      &StateTable::SetStencilOperations,
      &StateTable::GetFrontStencilFailOperation,
      &StateTable::GetFrontStencilDepthFailOperation,
      &StateTable::GetFrontStencilPassOperation,
      &StateTable::GetBackStencilFailOperation,
      &StateTable::GetBackStencilDepthFailOperation,
      &StateTable::GetBackStencilPassOperation);

  TestValue1<int>(
      *default_st, st.Get(), StateTable::kClearStencilValue,
      152,
      &StateTable::SetClearStencilValue, &StateTable::GetClearStencilValue);

  TestValue2<uint32, uint32>(
      *default_st, st.Get(), StateTable::kStencilWriteMasksValue,
      0x12349876, 0xbeefface,
      &StateTable::SetStencilWriteMasks,
      &StateTable::GetFrontStencilWriteMask,
      &StateTable::GetBackStencilWriteMask);

  TestValue1<const math::Range2i&>(
      *default_st, st.Get(), StateTable::kViewportValue,
      math::Range2i(math::Point2i(10, 20), math::Point2i(210, 320)),
      &StateTable::SetViewport, &StateTable::GetViewport);

  // Try to set an invalid value.
#if !ION_PRODUCTION
  EXPECT_DEATH_IF_SUPPORTED(
      st->ResetValue(static_cast<StateTable::Value>(base::kInvalidIndex)),
      "Invalid Value type");
#endif
}

TEST(StateTable, AreCapabilitiesSame) {
  StateTablePtr st0(new StateTable(300, 200));
  StateTablePtr st1(new StateTable(300, 200));

  // Default state.
  EXPECT_TRUE(StateTable::AreCapabilitiesSame(*st0, *st1));

  // One capability change.
  st0->Enable(StateTable::kCullFace, true);
  EXPECT_FALSE(StateTable::AreCapabilitiesSame(*st0, *st1));
  st1->Enable(StateTable::kCullFace, true);
  EXPECT_TRUE(StateTable::AreCapabilitiesSame(*st0, *st1));

  // Reset.
  st0->Reset();
  EXPECT_FALSE(StateTable::AreCapabilitiesSame(*st0, *st1));
  st1->Reset();
  EXPECT_TRUE(StateTable::AreCapabilitiesSame(*st0, *st1));

  // Multiple capabilities.
  st0->Enable(StateTable::kDither, false);
  EXPECT_FALSE(StateTable::AreCapabilitiesSame(*st0, *st1));
  st0->Enable(StateTable::kScissorTest, true);
  EXPECT_FALSE(StateTable::AreCapabilitiesSame(*st0, *st1));
  st0->Enable(StateTable::kDither, true);
  EXPECT_FALSE(StateTable::AreCapabilitiesSame(*st0, *st1));
  st0->Enable(StateTable::kScissorTest, false);
  EXPECT_TRUE(StateTable::AreCapabilitiesSame(*st0, *st1));
}

TEST(StateTable, CopyFrom) {
  StateTablePtr st0(new StateTable(300, 200));
  StateTablePtr st1(new StateTable(500, 100));

  // Set a few things in the state.
  st0->Enable(StateTable::kBlend, true);
  st0->Enable(StateTable::kCullFace, true);
  st0->Enable(StateTable::kSampleShading, true);
  st0->SetBlendColor(math::Vector4f(.2f, .3f, .4f, .5f));
  st0->SetBlendEquations(StateTable::kReverseSubtract, StateTable::kSubtract);
  st0->SetBlendFunctions(StateTable::kDstColor, StateTable::kOne,
                         StateTable::kSrcAlpha, StateTable::kZero);
  st0->SetClearColor(math::Vector4f(.6f, .7f, .8f, .9f));
  st0->SetColorWriteMasks(true, false, false, true);
  st0->SetCullFaceMode(StateTable::kCullFrontAndBack);
  st0->SetFrontFaceMode(StateTable::kClockwise);
  st0->SetClearDepthValue(0.8f);
  st0->SetDepthRange(math::Range1f(.2f, .4f));
  st0->SetDepthWriteMask(false);
  st0->SetHint(StateTable::kGenerateMipmapHint, StateTable::kHintNicest);
  st0->SetLineWidth(.4f);
  st0->SetMinSampleShading(.7f);
  st0->SetPolygonOffset(.5f, .2f);
  st0->SetSampleCoverage(.6f, true);
  st0->SetScissorBox(math::Range2i::BuildWithSize(math::Point2i(10, 20),
                                                  math::Vector2i(30, 40)));
  st0->SetClearStencilValue(123456);
  st0->SetViewport(math::Range2i::BuildWithSize(math::Point2i(50, 60),
                                                math::Vector2i(70, 80)));
  st1->SetViewport(50, 60, 70, 80);
  st1->SetStencilFunctions(StateTable::kStencilNotEqual, 42, 0xbabebabe,
                           StateTable::kStencilLess, 155, 0x87654321);
  st1->SetStencilWriteMasks(0x13572468, 0xfeebbeef);

  // Copy and test.
  st1->CopyFrom(*st0);
  EXPECT_TRUE(st1->IsEnabled(StateTable::kBlend));
  for (size_t i = 0; i < StateTable::kClipDistanceCount; ++i)
    EXPECT_FALSE(st1->IsEnabled(
        static_cast<StateTable::Capability>(StateTable::kClipDistance0 + i)));
  EXPECT_TRUE(st1->IsEnabled(StateTable::kCullFace));
  EXPECT_FALSE(st1->IsEnabled(StateTable::kDepthTest));
  EXPECT_TRUE(st1->IsEnabled(StateTable::kDither));
  EXPECT_FALSE(st1->IsEnabled(StateTable::kPolygonOffsetFill));
  EXPECT_FALSE(st1->IsEnabled(StateTable::kRasterizerDiscard));
  EXPECT_FALSE(st1->IsEnabled(StateTable::kSampleAlphaToCoverage));
  EXPECT_FALSE(st1->IsEnabled(StateTable::kSampleCoverage));
  EXPECT_TRUE(st1->IsEnabled(StateTable::kSampleShading));
  EXPECT_FALSE(st1->IsEnabled(StateTable::kScissorTest));
  EXPECT_FALSE(st1->IsEnabled(StateTable::kStencilTest));
  EXPECT_EQ(5U, st1->GetEnabledCount());
  // Test BlendColor and Viewport explicitly.
  EXPECT_EQ(st0->GetBlendColor(), st1->GetBlendColor());
  EXPECT_EQ(st0->GetViewport(), st1->GetViewport());
  // Test all the other values using the helper function.
  CompareTableValues(*st0, *st1, StateTable::kBlendColorValue);
}

TEST(StateTable, ResetSetState) {
  StateTablePtr st(new StateTable(300, 200));

  // Default state.
  EXPECT_EQ(0U, st->GetSetCapabilityCount());
  EXPECT_EQ(0U, st->GetSetValueCount());

  // One capability change.
  st->Enable(StateTable::kCullFace, true);
  EXPECT_EQ(1U, st->GetSetCapabilityCount());
  EXPECT_EQ(0U, st->GetSetValueCount());

  // Reset the set state.
  st->ResetSetState();
  EXPECT_EQ(0U, st->GetSetCapabilityCount());
  // The value is unchanged.
  EXPECT_TRUE(st->IsEnabled(StateTable::kCullFace));

  // Set a value.
  st->SetCullFaceMode(StateTable::kCullFrontAndBack);
  EXPECT_EQ(0U, st->GetSetCapabilityCount());
  EXPECT_EQ(1U, st->GetSetValueCount());
  st->ResetSetState();
  EXPECT_EQ(0U, st->GetSetValueCount());
  EXPECT_EQ(StateTable::kCullFrontAndBack, st->GetCullFaceMode());

  // Multiple capabilities and values.
  st->Enable(StateTable::kDither, false);
  st->Enable(StateTable::kScissorTest, true);
  st->Enable(StateTable::kDither, true);
  st->Enable(StateTable::kScissorTest, false);
  st->SetDepthFunction(StateTable::kDepthLess);
  st->SetFrontFaceMode(StateTable::kClockwise);
  st->SetDepthFunction(StateTable::kDepthGreater);
  EXPECT_EQ(2U, st->GetSetCapabilityCount());
  EXPECT_EQ(2U, st->GetSetValueCount());
  st->ResetSetState();
  EXPECT_EQ(0U, st->GetSetCapabilityCount());
  EXPECT_EQ(0U, st->GetSetValueCount());
}

TEST(StateTable, MarkAllSet) {
  StateTablePtr st(new StateTable(300, 200));

  // Default state.
  EXPECT_EQ(0U, st->GetSetCapabilityCount());
  EXPECT_EQ(0U, st->GetSetValueCount());

  // Mark all capabilities and values as set.
  st->MarkAllSet();

  // Verify that all capabilities and values have been set.
  EXPECT_EQ(static_cast<size_t>(StateTable::GetCapabilityCount()),
            st->GetSetCapabilityCount());
  EXPECT_EQ(static_cast<size_t>(StateTable::GetValueCount()),
            st->GetSetValueCount());
}

TEST(StateTable, MergeValues) {
  StateTablePtr st0(new StateTable(300, 200));
  StateTablePtr st1(new StateTable(500, 100));
  StateTablePtr st2(new StateTable(500, 100));

  // Set a few things in the state.
  st0->Enable(StateTable::kBlend, true);
  st0->Enable(StateTable::kClipDistance3, true);
  st0->Enable(StateTable::kCullFace, true);
  st0->SetBlendColor(math::Vector4f(.2f, .3f, .4f, .5f));
  st0->SetBlendEquations(StateTable::kReverseSubtract, StateTable::kSubtract);
  st0->SetBlendFunctions(StateTable::kDstColor, StateTable::kOne,
                         StateTable::kSrcAlpha, StateTable::kZero);
  st0->SetClearColor(math::Vector4f(.6f, .7f, .8f, .9f));
  st0->SetColorWriteMasks(true, false, false, true);
  st0->SetCullFaceMode(StateTable::kCullFrontAndBack);
  st0->SetFrontFaceMode(StateTable::kClockwise);
  st0->SetClearDepthValue(0.8f);
  st0->SetDepthRange(math::Range1f(.2f, .4f));
  st0->SetDepthWriteMask(false);
  st0->SetHint(StateTable::kGenerateMipmapHint, StateTable::kHintNicest);
  st0->SetLineWidth(.4f);
  st0->SetPolygonOffset(.5f, .2f);
  st0->SetSampleCoverage(.6f, true);
  st0->SetScissorBox(math::Range2i::BuildWithSize(math::Point2i(10, 20),
                                                  math::Vector2i(30, 40)));
  st0->SetClearStencilValue(123456);
  st0->SetViewport(math::Range2i::BuildWithSize(math::Point2i(50, 60),
                                                math::Vector2i(70, 80)));
  st1->SetSampleCoverage(.21f, false);
  st1->SetLineWidth(.111f);
  st1->SetStencilFunctions(StateTable::kStencilNotEqual, 42, 0xbabebabe,
                           StateTable::kStencilLess, 155, 0x87654321);
  st1->SetStencilWriteMasks(0x13572468, 0xfeebbeef);

  // Set st2 from st1.
  st2->CopyFrom(*st1.Get());
  st2->SetDepthFunction(StateTable::kDepthLess);

  // Merge and test.
  st1->MergeNonClearValuesFrom(*st0, *st0);
  EXPECT_TRUE(st1->IsEnabled(StateTable::kBlend));
  for (size_t i = 0; i < StateTable::kClipDistanceCount; ++i)
    EXPECT_EQ(i == 3, st1->IsEnabled(
        static_cast<StateTable::Capability>(StateTable::kClipDistance0 + i)));
  EXPECT_TRUE(st1->IsEnabled(StateTable::kCullFace));
  EXPECT_FALSE(st1->IsEnabled(StateTable::kDepthTest));
  EXPECT_TRUE(st1->IsEnabled(StateTable::kDither));
  EXPECT_FALSE(st1->IsEnabled(StateTable::kPolygonOffsetFill));
  EXPECT_FALSE(st1->IsEnabled(StateTable::kRasterizerDiscard));
  EXPECT_FALSE(st1->IsEnabled(StateTable::kSampleAlphaToCoverage));
  EXPECT_FALSE(st1->IsEnabled(StateTable::kSampleCoverage));
  EXPECT_FALSE(st1->IsEnabled(StateTable::kScissorTest));
  EXPECT_FALSE(st1->IsEnabled(StateTable::kStencilTest));
  EXPECT_EQ(5U, st1->GetEnabledCount());
  // Test values.
  EXPECT_EQ(st0->GetBlendColor(), st1->GetBlendColor());
  EXPECT_EQ(st0->GetRgbBlendEquation(), st1->GetRgbBlendEquation());
  EXPECT_EQ(st0->GetAlphaBlendEquation(), st1->GetAlphaBlendEquation());
  EXPECT_EQ(st0->GetRedColorWriteMask(), st1->GetRedColorWriteMask());
  EXPECT_EQ(st0->GetAlphaColorWriteMask(), st1->GetAlphaColorWriteMask());
  EXPECT_EQ(st0->GetFrontFaceMode(), st1->GetFrontFaceMode());
  EXPECT_EQ(st0->GetDepthRange(), st1->GetDepthRange());
  EXPECT_EQ(st0->GetHint(StateTable::kGenerateMipmapHint),
            st1->GetHint(StateTable::kGenerateMipmapHint));
  // This should have been overwritten from st0.
  EXPECT_EQ(st0->GetLineWidth(), st1->GetLineWidth());
  EXPECT_EQ(st0->GetPolygonOffsetFactor(), st1->GetPolygonOffsetFactor());
  EXPECT_EQ(st0->GetPolygonOffsetUnits(), st1->GetPolygonOffsetUnits());
  // This should have been overwritten from st0.
  EXPECT_EQ(st0->GetSampleCoverageValue(), st1->GetSampleCoverageValue());
  EXPECT_EQ(st0->IsSampleCoverageInverted(), st1->IsSampleCoverageInverted());
  EXPECT_EQ(st0->GetViewport(), st1->GetViewport());
  EXPECT_EQ(st0->GetScissorBox(), st1->GetScissorBox());
  EXPECT_EQ(st0->GetGreenColorWriteMask(), st1->GetGreenColorWriteMask());
  EXPECT_EQ(st0->GetBlueColorWriteMask(), st1->GetBlueColorWriteMask());
  EXPECT_EQ(st0->GetDepthWriteMask(), st1->GetDepthWriteMask());

  // Clear values should not have been merged.
  EXPECT_NE(st0->GetClearStencilValue(), st1->GetClearStencilValue());
  EXPECT_NE(st0->GetClearColor(), st1->GetClearColor());
  EXPECT_NE(st0->GetClearDepthValue(), st1->GetClearDepthValue());

  // Merge in the clear flags.
  st1->MergeValuesFrom(*st0, *st0);
  EXPECT_EQ(st0->GetScissorBox(), st1->GetScissorBox());
  EXPECT_EQ(st0->GetClearStencilValue(), st1->GetClearStencilValue());
  EXPECT_EQ(st0->GetClearColor(), st1->GetClearColor());
  EXPECT_EQ(st0->GetGreenColorWriteMask(), st1->GetGreenColorWriteMask());
  EXPECT_EQ(st0->GetBlueColorWriteMask(), st1->GetBlueColorWriteMask());
  EXPECT_EQ(st0->GetDepthWriteMask(), st1->GetDepthWriteMask());
  EXPECT_EQ(st0->GetClearDepthValue(), st1->GetClearDepthValue());

  // st1's original values should still be set.
  EXPECT_EQ(StateTable::kStencilNotEqual, st1->GetFrontStencilFunction());
  EXPECT_EQ(42, st1->GetFrontStencilReferenceValue());
  EXPECT_EQ(0xbabebabe, st1->GetFrontStencilMask());
  EXPECT_EQ(StateTable::kStencilLess, st1->GetBackStencilFunction());
  EXPECT_EQ(155, st1->GetBackStencilReferenceValue());
  EXPECT_EQ(0x87654321, st1->GetBackStencilMask());
  EXPECT_EQ(0x13572468u, st1->GetFrontStencilWriteMask());
  EXPECT_EQ(0xfeebbeefu, st1->GetBackStencilWriteMask());

  // Merge and test, only copying values that are set in st1.
  st2->MergeValuesFrom(*st0, *st1);

  // Only the overrides should have changed.
  // This should have been overwritten from st0.
  EXPECT_EQ(st0->GetLineWidth(), st2->GetLineWidth());
  // This should have been overwritten from st0.
  EXPECT_EQ(st0->GetSampleCoverageValue(), st2->GetSampleCoverageValue());

  // Original state overriden by values in st0.
  EXPECT_EQ(st0->GetFrontStencilFunction(), st2->GetFrontStencilFunction());
  EXPECT_EQ(st0->GetFrontStencilReferenceValue(),
            st2->GetFrontStencilReferenceValue());
  EXPECT_EQ(st0->GetFrontStencilMask(), st2->GetFrontStencilMask());
  EXPECT_EQ(st0->GetBackStencilFunction(), st2->GetBackStencilFunction());
  EXPECT_EQ(st0->GetBackStencilReferenceValue(),
            st2->GetBackStencilReferenceValue());
  EXPECT_EQ(st0->GetBackStencilMask(), st2->GetBackStencilMask());
  EXPECT_EQ(st0->GetFrontStencilWriteMask(), st2->GetFrontStencilWriteMask());
  EXPECT_EQ(st0->GetBackStencilWriteMask(), st2->GetBackStencilWriteMask());
  EXPECT_TRUE(st2->IsEnabled(StateTable::kBlend));
  for (size_t i = 0; i < StateTable::kClipDistanceCount; ++i)
    EXPECT_EQ(i == 3, st1->IsEnabled(
        static_cast<StateTable::Capability>(StateTable::kClipDistance0 + i)));
  EXPECT_TRUE(st2->IsEnabled(StateTable::kCullFace));
  EXPECT_FALSE(st2->IsEnabled(StateTable::kDepthTest));
  EXPECT_TRUE(st2->IsEnabled(StateTable::kDither));
  EXPECT_FALSE(st2->IsEnabled(StateTable::kPolygonOffsetFill));
  EXPECT_FALSE(st2->IsEnabled(StateTable::kRasterizerDiscard));
  EXPECT_FALSE(st2->IsEnabled(StateTable::kSampleAlphaToCoverage));
  EXPECT_FALSE(st2->IsEnabled(StateTable::kSampleCoverage));
  EXPECT_FALSE(st2->IsEnabled(StateTable::kScissorTest));
  EXPECT_FALSE(st2->IsEnabled(StateTable::kStencilTest));
  EXPECT_EQ(5U, st2->GetEnabledCount());

  // Since st1 did not set a depth function, the st2 value should be unchanged.
  EXPECT_EQ(StateTable::kDepthLess, st2->GetDepthFunction());
}

TEST(StateTable, SetEnforceSettings) {
  StateTablePtr st(new StateTable(300, 200));
  st->SetEnforceSettings(true);
  EXPECT_TRUE(st->AreSettingsEnforced());
  st->SetEnforceSettings(false);
  EXPECT_FALSE(st->AreSettingsEnforced());
}

//-----------------------------------------------------------------------------
//
// Some macros to make this much clearer and easier to read.
//
//-----------------------------------------------------------------------------

#define TEST_COUNT(type, num)                                               \
  EXPECT_EQ(num, base::EnumHelper::GetCount<StateTable::type>());

#define TEST_CONSTANT(type, val, gl_val)                                    \
  EXPECT_EQ(static_cast<GLenum>(gl_val),                                    \
            base::EnumHelper::GetConstant(StateTable::val))

#define TEST_STRING(type, val)                                              \
  EXPECT_EQ(std::string(#val),                                              \
            std::string(base::EnumHelper::GetString(StateTable::k ## val)))

// Helper function for creating invalid enum values. Some compilers choke when
// casting a const invalid value to an enum.
template <typename Type> static Type GetTooBigEnum(int max_value) {
  return static_cast<Type>(max_value + 1);
}

#define TEST_INVALID_STRINGS(type, max_val)                                 \
  EXPECT_EQ(std::string("<INVALID>"),                                       \
            std::string(base::EnumHelper::GetString(                        \
                base::InvalidEnumValue<StateTable::type>())));              \
  EXPECT_EQ(std::string("<INVALID>"),                                       \
            std::string(base::EnumHelper::GetString(                        \
                GetTooBigEnum<StateTable::type>(StateTable::max_val))))

//-----------------------------------------------------------------------------
//
// EnumHelper tests.
//
//-----------------------------------------------------------------------------

TEST(StateTable, Capability) {
  TEST_COUNT(Capability, 21U);
  TEST_CONSTANT(Capability, kBlend, GL_BLEND);
  TEST_CONSTANT(Capability, kClipDistance0, GL_CLIP_DISTANCE0);
  TEST_CONSTANT(Capability, kClipDistance1, GL_CLIP_DISTANCE1);
  TEST_CONSTANT(Capability, kClipDistance2, GL_CLIP_DISTANCE2);
  TEST_CONSTANT(Capability, kClipDistance3, GL_CLIP_DISTANCE3);
  TEST_CONSTANT(Capability, kClipDistance4, GL_CLIP_DISTANCE4);
  TEST_CONSTANT(Capability, kClipDistance5, GL_CLIP_DISTANCE5);
  TEST_CONSTANT(Capability, kClipDistance6, GL_CLIP_DISTANCE6);
  TEST_CONSTANT(Capability, kClipDistance7, GL_CLIP_DISTANCE7);
  TEST_CONSTANT(Capability, kCullFace, GL_CULL_FACE);
  TEST_CONSTANT(Capability, kDebugOutputSynchronous,
                GL_DEBUG_OUTPUT_SYNCHRONOUS);
  TEST_CONSTANT(Capability, kDepthTest, GL_DEPTH_TEST);
  TEST_CONSTANT(Capability, kDither, GL_DITHER);
  TEST_CONSTANT(Capability, kMultisample, GL_MULTISAMPLE);
  TEST_CONSTANT(Capability, kPolygonOffsetFill, GL_POLYGON_OFFSET_FILL);
  TEST_CONSTANT(Capability, kRasterizerDiscard, GL_RASTERIZER_DISCARD);
  TEST_CONSTANT(Capability, kSampleAlphaToCoverage,
                GL_SAMPLE_ALPHA_TO_COVERAGE);
  TEST_CONSTANT(Capability, kSampleCoverage, GL_SAMPLE_COVERAGE);
  TEST_CONSTANT(Capability, kSampleShading, GL_SAMPLE_SHADING);
  TEST_CONSTANT(Capability, kScissorTest, GL_SCISSOR_TEST);
  TEST_CONSTANT(Capability, kStencilTest, GL_STENCIL_TEST);

  TEST_STRING(Capability, Blend);
  TEST_STRING(Capability, CullFace);
  TEST_STRING(Capability, DebugOutputSynchronous);
  TEST_STRING(Capability, DepthTest);
  TEST_STRING(Capability, Dither);
  TEST_STRING(Capability, Multisample);
  TEST_STRING(Capability, PolygonOffsetFill);
  TEST_STRING(Capability, RasterizerDiscard);
  TEST_STRING(Capability, SampleAlphaToCoverage);
  TEST_STRING(Capability, SampleCoverage);
  TEST_STRING(Capability, ScissorTest);
  TEST_STRING(Capability, StencilTest);
  TEST_INVALID_STRINGS(Capability, kStencilTest);
}

TEST(StateTable, BlendEquation) {
  TEST_COUNT(BlendEquation, 5U);
  TEST_CONSTANT(BlendEquation, kAdd, GL_FUNC_ADD);
  TEST_CONSTANT(BlendEquation, kReverseSubtract, GL_FUNC_REVERSE_SUBTRACT);
  TEST_CONSTANT(BlendEquation, kSubtract, GL_FUNC_SUBTRACT);
  TEST_CONSTANT(BlendEquation, kMin, GL_MIN);
  TEST_CONSTANT(BlendEquation, kMax, GL_MAX);

  TEST_STRING(BlendEquation, Add);
  TEST_STRING(BlendEquation, ReverseSubtract);
  TEST_STRING(BlendEquation, Subtract);
  TEST_STRING(BlendEquation, Min);
  TEST_STRING(BlendEquation, Max);
  TEST_INVALID_STRINGS(BlendEquation, kMax);
}

TEST(StateTable, BlendFunctionFactor) {
  TEST_COUNT(BlendFunctionFactor, 15U);
  TEST_CONSTANT(BlendFunctionFactor, kConstantAlpha, GL_CONSTANT_ALPHA);
  TEST_CONSTANT(BlendFunctionFactor, kConstantColor, GL_CONSTANT_COLOR);
  TEST_CONSTANT(BlendFunctionFactor, kDstAlpha, GL_DST_ALPHA);
  TEST_CONSTANT(BlendFunctionFactor, kDstColor, GL_DST_COLOR);
  TEST_CONSTANT(BlendFunctionFactor, kOne, GL_ONE);
  TEST_CONSTANT(BlendFunctionFactor, kOneMinusConstantAlpha,
                GL_ONE_MINUS_CONSTANT_ALPHA);
  TEST_CONSTANT(BlendFunctionFactor, kOneMinusConstantColor,
                GL_ONE_MINUS_CONSTANT_COLOR);
  TEST_CONSTANT(BlendFunctionFactor, kOneMinusDstAlpha, GL_ONE_MINUS_DST_ALPHA);
  TEST_CONSTANT(BlendFunctionFactor, kOneMinusDstColor, GL_ONE_MINUS_DST_COLOR);
  TEST_CONSTANT(BlendFunctionFactor, kOneMinusSrcAlpha, GL_ONE_MINUS_SRC_ALPHA);
  TEST_CONSTANT(BlendFunctionFactor, kOneMinusSrcColor, GL_ONE_MINUS_SRC_COLOR);
  TEST_CONSTANT(BlendFunctionFactor, kSrcAlpha, GL_SRC_ALPHA);
  TEST_CONSTANT(BlendFunctionFactor, kSrcAlphaSaturate, GL_SRC_ALPHA_SATURATE);
  TEST_CONSTANT(BlendFunctionFactor, kSrcColor, GL_SRC_COLOR);
  TEST_CONSTANT(BlendFunctionFactor, kZero, GL_ZERO);

  TEST_STRING(BlendFunctionFactor, ConstantAlpha);
  TEST_STRING(BlendFunctionFactor, ConstantColor);
  TEST_STRING(BlendFunctionFactor, DstAlpha);
  TEST_STRING(BlendFunctionFactor, DstColor);
  TEST_STRING(BlendFunctionFactor, One);
  TEST_STRING(BlendFunctionFactor, OneMinusConstantAlpha);
  TEST_STRING(BlendFunctionFactor, OneMinusConstantColor);
  TEST_STRING(BlendFunctionFactor, OneMinusDstAlpha);
  TEST_STRING(BlendFunctionFactor, OneMinusDstColor);
  TEST_STRING(BlendFunctionFactor, OneMinusSrcAlpha);
  TEST_STRING(BlendFunctionFactor, OneMinusSrcColor);
  TEST_STRING(BlendFunctionFactor, SrcAlpha);
  TEST_STRING(BlendFunctionFactor, SrcAlphaSaturate);
  TEST_STRING(BlendFunctionFactor, SrcColor);
  TEST_STRING(BlendFunctionFactor, Zero);
  TEST_INVALID_STRINGS(BlendFunctionFactor, kZero);
}

TEST(StateTable, CullFaceMode) {
  TEST_COUNT(CullFaceMode, 3U);
  TEST_CONSTANT(CullFaceMode, kCullFront, GL_FRONT);
  TEST_CONSTANT(CullFaceMode, kCullBack, GL_BACK);
  TEST_CONSTANT(CullFaceMode, kCullFrontAndBack, GL_FRONT_AND_BACK);

  TEST_STRING(CullFaceMode, CullFront);
  TEST_STRING(CullFaceMode, CullBack);
  TEST_STRING(CullFaceMode, CullFrontAndBack);
  TEST_INVALID_STRINGS(CullFaceMode, kCullFrontAndBack);
}

TEST(StateTable, DepthFunction) {
  TEST_COUNT(DepthFunction, 8U);
  TEST_CONSTANT(DepthFunction, kDepthAlways, GL_ALWAYS);
  TEST_CONSTANT(DepthFunction, kDepthEqual, GL_EQUAL);
  TEST_CONSTANT(DepthFunction, kDepthGreater, GL_GREATER);
  TEST_CONSTANT(DepthFunction, kDepthGreaterOrEqual, GL_GEQUAL);
  TEST_CONSTANT(DepthFunction, kDepthLess, GL_LESS);
  TEST_CONSTANT(DepthFunction, kDepthLessOrEqual, GL_LEQUAL);
  TEST_CONSTANT(DepthFunction, kDepthNever, GL_NEVER);
  TEST_CONSTANT(DepthFunction, kDepthNotEqual, GL_NOTEQUAL);

  TEST_STRING(DepthFunction, DepthAlways);
  TEST_STRING(DepthFunction, DepthEqual);
  TEST_STRING(DepthFunction, DepthGreater);
  TEST_STRING(DepthFunction, DepthGreaterOrEqual);
  TEST_STRING(DepthFunction, DepthLess);
  TEST_STRING(DepthFunction, DepthLessOrEqual);
  TEST_STRING(DepthFunction, DepthNever);
  TEST_STRING(DepthFunction, DepthNotEqual);
  TEST_INVALID_STRINGS(DepthFunction, kDepthNotEqual);
}

TEST(StateTable, FrontFaceMode) {
  TEST_COUNT(FrontFaceMode, 2U);
  TEST_CONSTANT(FrontFaceMode, kClockwise, GL_CW);
  TEST_CONSTANT(FrontFaceMode, kCounterClockwise, GL_CCW);

  TEST_STRING(FrontFaceMode, Clockwise);
  TEST_STRING(FrontFaceMode, CounterClockwise);
  TEST_INVALID_STRINGS(FrontFaceMode, kCounterClockwise);
}

TEST(StateTable, HintMode) {
  TEST_COUNT(HintMode, 3U);
  TEST_CONSTANT(HintMode, kHintFastest, GL_FASTEST);
  TEST_CONSTANT(HintMode, kHintNicest, GL_NICEST);
  TEST_CONSTANT(HintMode, kHintDontCare, GL_DONT_CARE);

  TEST_STRING(HintMode, HintFastest);
  TEST_STRING(HintMode, HintNicest);
  TEST_STRING(HintMode, HintDontCare);
  TEST_INVALID_STRINGS(HintMode, kHintDontCare);
}

TEST(StateTable, StencilFunction) {
  TEST_COUNT(StencilFunction, 8U);
  TEST_CONSTANT(StencilFunction, kStencilAlways, GL_ALWAYS);
  TEST_CONSTANT(StencilFunction, kStencilEqual, GL_EQUAL);
  TEST_CONSTANT(StencilFunction, kStencilGreater, GL_GREATER);
  TEST_CONSTANT(StencilFunction, kStencilGreaterOrEqual, GL_GEQUAL);
  TEST_CONSTANT(StencilFunction, kStencilLess, GL_LESS);
  TEST_CONSTANT(StencilFunction, kStencilLessOrEqual, GL_LEQUAL);
  TEST_CONSTANT(StencilFunction, kStencilNever, GL_NEVER);
  TEST_CONSTANT(StencilFunction, kStencilNotEqual, GL_NOTEQUAL);

  TEST_STRING(StencilFunction, StencilAlways);
  TEST_STRING(StencilFunction, StencilEqual);
  TEST_STRING(StencilFunction, StencilGreater);
  TEST_STRING(StencilFunction, StencilGreaterOrEqual);
  TEST_STRING(StencilFunction, StencilLess);
  TEST_STRING(StencilFunction, StencilLessOrEqual);
  TEST_STRING(StencilFunction, StencilNever);
  TEST_STRING(StencilFunction, StencilNotEqual);
  TEST_INVALID_STRINGS(StencilFunction, kStencilNotEqual);
}

TEST(StateTable, StencilOperation) {
  TEST_COUNT(StencilOperation, 8U);
  TEST_CONSTANT(StencilOperation, kStencilDecrement, GL_DECR);
  TEST_CONSTANT(StencilOperation, kStencilDecrementAndWrap, GL_DECR_WRAP);
  TEST_CONSTANT(StencilOperation, kStencilIncrement, GL_INCR);
  TEST_CONSTANT(StencilOperation, kStencilIncrementAndWrap, GL_INCR_WRAP);
  TEST_CONSTANT(StencilOperation, kStencilInvert, GL_INVERT);
  TEST_CONSTANT(StencilOperation, kStencilKeep, GL_KEEP);
  TEST_CONSTANT(StencilOperation, kStencilReplace, GL_REPLACE);
  TEST_CONSTANT(StencilOperation, kStencilZero, GL_ZERO);

  TEST_STRING(StencilOperation, StencilDecrement);
  TEST_STRING(StencilOperation, StencilDecrementAndWrap);
  TEST_STRING(StencilOperation, StencilIncrement);
  TEST_STRING(StencilOperation, StencilIncrementAndWrap);
  TEST_STRING(StencilOperation, StencilInvert);
  TEST_STRING(StencilOperation, StencilKeep);
  TEST_STRING(StencilOperation, StencilReplace);
  TEST_STRING(StencilOperation, StencilZero);
  TEST_INVALID_STRINGS(StencilOperation, kStencilZero);
}

}  // namespace gfx
}  // namespace ion
