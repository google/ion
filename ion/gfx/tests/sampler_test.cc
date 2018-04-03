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

#include "ion/gfx/sampler.h"

#include <memory>

#include "ion/base/logchecker.h"
#include "ion/gfx/tests/mockresource.h"
#include "ion/math/vector.h"

#include "absl/memory/memory.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfx {

using math::Point2ui;

typedef testing::MockResource<Sampler::kNumChanges> MockSamplerResource;

// This is used for testing invalid modes.
int bad_mode = 5123;

class SamplerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    sampler_.Reset(new Sampler());
    resource_ = absl::make_unique<MockSamplerResource>();
    EXPECT_FALSE(resource_->AnyModifiedBitsSet());
    sampler_->SetResource(0U, 0, resource_.get());
    EXPECT_EQ(resource_.get(), sampler_->GetResource(0U, 0));
    EXPECT_TRUE(resource_->AnyModifiedBitsSet());
    resource_->ResetModifiedBits();
    EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  }

  // This is to ensure that the resource holder goes away before the resource.
  void TearDown() override { sampler_.Reset(nullptr); }

  SamplerPtr sampler_;
  std::unique_ptr<MockSamplerResource> resource_;
};

TEST_F(SamplerTest, DefaultModes) {
  // Check that mipmapping is disabled by default.
  EXPECT_FALSE(sampler_->IsAutogenerateMipmapsEnabled());

  // Check the the max anisotropy is 1 by default.
  EXPECT_EQ(1.f, sampler_->GetMaxAnisotropy());

  // Check that filter modes are NEAREST by default.
  EXPECT_EQ(Sampler::kNearest, sampler_->GetMinFilter());
  EXPECT_EQ(Sampler::kNearest, sampler_->GetMagFilter());

  // Check that wrap modes are REPEAT by default.
  EXPECT_EQ(Sampler::kRepeat, sampler_->GetWrapS());
  EXPECT_EQ(Sampler::kRepeat, sampler_->GetWrapT());

  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
}

TEST_F(SamplerTest, CompareFunction) {
  base::LogChecker log_checker;

  EXPECT_EQ(Sampler::kLess, sampler_->GetCompareFunction());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  sampler_->SetCompareFunction(Sampler::kAlways);
  EXPECT_EQ(Sampler::kAlways, sampler_->GetCompareFunction());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Sampler::kCompareFunctionChanged));
  resource_->ResetModifiedBit(Sampler::kCompareFunctionChanged);
  sampler_->SetCompareFunction(Sampler::kAlways);
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  // Check that there weren't any state changing side effects.
  EXPECT_FALSE(sampler_->IsAutogenerateMipmapsEnabled());
  EXPECT_EQ(Sampler::kNearest, sampler_->GetMagFilter());
  EXPECT_EQ(Sampler::kRepeat, sampler_->GetWrapS());
  EXPECT_EQ(Sampler::kRepeat, sampler_->GetWrapT());
  EXPECT_EQ(Sampler::kAlways, sampler_->GetCompareFunction());

  // Set an invalid mode.
  sampler_->SetCompareFunction(static_cast<Sampler::CompareFunction>(bad_mode));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "invalid value"));

  // Check that nothing has changed.
  EXPECT_EQ(Sampler::kNearest, sampler_->GetMinFilter());
  EXPECT_FALSE(sampler_->IsAutogenerateMipmapsEnabled());
  EXPECT_EQ(Sampler::kNearest, sampler_->GetMagFilter());
  EXPECT_EQ(Sampler::kRepeat, sampler_->GetWrapS());
  EXPECT_EQ(Sampler::kRepeat, sampler_->GetWrapT());
  EXPECT_EQ(Sampler::kNone, sampler_->GetCompareMode());
  EXPECT_EQ(Sampler::kAlways, sampler_->GetCompareFunction());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST_F(SamplerTest, CompareMode) {
  base::LogChecker log_checker;

  EXPECT_EQ(Sampler::kNone, sampler_->GetCompareMode());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  sampler_->SetCompareMode(Sampler::kCompareToTexture);
  EXPECT_EQ(Sampler::kCompareToTexture, sampler_->GetCompareMode());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Sampler::kCompareModeChanged));
  resource_->ResetModifiedBit(Sampler::kCompareModeChanged);
  sampler_->SetCompareMode(Sampler::kCompareToTexture);
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  // Check that there weren't any state changing side effects.
  EXPECT_FALSE(sampler_->IsAutogenerateMipmapsEnabled());
  EXPECT_EQ(Sampler::kNearest, sampler_->GetMagFilter());
  EXPECT_EQ(Sampler::kRepeat, sampler_->GetWrapS());
  EXPECT_EQ(Sampler::kRepeat, sampler_->GetWrapT());
  EXPECT_EQ(Sampler::kLess, sampler_->GetCompareFunction());

  // Set an invalid mode.
  sampler_->SetCompareMode(static_cast<Sampler::CompareMode>(bad_mode));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "invalid value"));

  // Check that nothing has changed.
  EXPECT_EQ(Sampler::kNearest, sampler_->GetMinFilter());
  EXPECT_FALSE(sampler_->IsAutogenerateMipmapsEnabled());
  EXPECT_EQ(Sampler::kNearest, sampler_->GetMagFilter());
  EXPECT_EQ(Sampler::kRepeat, sampler_->GetWrapS());
  EXPECT_EQ(Sampler::kRepeat, sampler_->GetWrapT());
  EXPECT_EQ(Sampler::kCompareToTexture, sampler_->GetCompareMode());
  EXPECT_EQ(Sampler::kLess, sampler_->GetCompareFunction());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST_F(SamplerTest, Lod) {
  EXPECT_EQ(-1000.f, sampler_->GetMinLod());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  sampler_->SetMinLod(12.3f);
  EXPECT_EQ(12.3f, sampler_->GetMinLod());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Sampler::kMinLodChanged));
  resource_->ResetModifiedBit(Sampler::kMinLodChanged);
  sampler_->SetMinLod(12.3f);
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  EXPECT_EQ(1000.f, sampler_->GetMaxLod());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  sampler_->SetMaxLod(12.34f);
  EXPECT_EQ(12.34f, sampler_->GetMaxLod());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Sampler::kMaxLodChanged));
  resource_->ResetModifiedBit(Sampler::kMaxLodChanged);
  sampler_->SetMaxLod(12.34f);
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
}

TEST_F(SamplerTest, MaxAnisotropy) {
  base::LogChecker log_checker;
  EXPECT_EQ(1.f, sampler_->GetMaxAnisotropy());

  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  // Check that get returns what was set for each mode.
  sampler_->SetMaxAnisotropy(5.5f);
  EXPECT_EQ(5.5f, sampler_->GetMaxAnisotropy());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Sampler::kMaxAnisotropyChanged));
  resource_->ResetModifiedBit(Sampler::kMaxAnisotropyChanged);

  // Check that an invalid value logs an error and does not change.
  sampler_->SetMaxAnisotropy(0.9f);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "invalid value passed"));
  EXPECT_EQ(5.5f, sampler_->GetMaxAnisotropy());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  // Check that there weren't any state changing side effects.
  EXPECT_FALSE(sampler_->IsAutogenerateMipmapsEnabled());
  EXPECT_EQ(Sampler::kNearest, sampler_->GetMagFilter());
  EXPECT_EQ(Sampler::kRepeat, sampler_->GetWrapS());
  EXPECT_EQ(Sampler::kRepeat, sampler_->GetWrapT());
}

TEST_F(SamplerTest, SetMinFilter) {
  base::LogChecker log_checker;

  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  // Check that get returns what was set for each mode.
  sampler_->SetMinFilter(Sampler::kLinear);
  EXPECT_EQ(Sampler::kLinear, sampler_->GetMinFilter());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Sampler::kMinFilterChanged));
  resource_->ResetModifiedBit(Sampler::kMinFilterChanged);

  // Check that there weren't any state changing side effects.
  EXPECT_FALSE(sampler_->IsAutogenerateMipmapsEnabled());
  EXPECT_EQ(Sampler::kNearest, sampler_->GetMagFilter());
  EXPECT_EQ(Sampler::kRepeat, sampler_->GetWrapS());
  EXPECT_EQ(Sampler::kRepeat, sampler_->GetWrapT());

  // Set an invalid mode.
  sampler_->SetMinFilter(static_cast<Sampler::FilterMode>(bad_mode));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "invalid value"));

  // Check that nothing has changed.
  EXPECT_EQ(Sampler::kLinear, sampler_->GetMinFilter());
  EXPECT_FALSE(sampler_->IsAutogenerateMipmapsEnabled());
  EXPECT_EQ(Sampler::kNearest, sampler_->GetMagFilter());
  EXPECT_EQ(Sampler::kRepeat, sampler_->GetWrapS());
  EXPECT_EQ(Sampler::kRepeat, sampler_->GetWrapT());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST_F(SamplerTest, SetMagFilter) {
  base::LogChecker log_checker;

  // Check that get returns what was set for each mode.
  sampler_->SetMagFilter(Sampler::kLinear);
  EXPECT_EQ(Sampler::kLinear, sampler_->GetMagFilter());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Sampler::kMagFilterChanged));
  resource_->ResetModifiedBit(Sampler::kMagFilterChanged);

  // Check that there weren't any state changing side effects.
  EXPECT_FALSE(sampler_->IsAutogenerateMipmapsEnabled());
  EXPECT_EQ(Sampler::kNearest, sampler_->GetMinFilter());
  EXPECT_EQ(Sampler::kRepeat, sampler_->GetWrapS());
  EXPECT_EQ(Sampler::kRepeat, sampler_->GetWrapT());

  // Set an invalid mode.
  sampler_->SetMagFilter(static_cast<Sampler::FilterMode>(bad_mode));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "invalid value"));

  // Check that nothing has changed.
  EXPECT_EQ(Sampler::kLinear, sampler_->GetMagFilter());
  EXPECT_FALSE(sampler_->IsAutogenerateMipmapsEnabled());
  EXPECT_EQ(Sampler::kNearest, sampler_->GetMinFilter());
  EXPECT_EQ(Sampler::kRepeat, sampler_->GetWrapS());
  EXPECT_EQ(Sampler::kRepeat, sampler_->GetWrapT());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  // Set another invalid mode (generates an error message).
  sampler_->SetMagFilter(Sampler::kNearestMipmapNearest);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "invalid value"));

  // Check that nothing has changed.
  EXPECT_EQ(Sampler::kLinear, sampler_->GetMagFilter());
  EXPECT_FALSE(sampler_->IsAutogenerateMipmapsEnabled());
  EXPECT_EQ(Sampler::kNearest, sampler_->GetMinFilter());
  EXPECT_EQ(Sampler::kRepeat, sampler_->GetWrapS());
  EXPECT_EQ(Sampler::kRepeat, sampler_->GetWrapT());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST_F(SamplerTest, SetWrapR) {
  base::LogChecker log_checker;

  // Check that get returns what was set for each mode.
  sampler_->SetWrapR(Sampler::kMirroredRepeat);
  EXPECT_EQ(Sampler::kMirroredRepeat, sampler_->GetWrapR());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Sampler::kWrapRChanged));
  resource_->ResetModifiedBit(Sampler::kWrapRChanged);

  // Check that there weren't any state changing side effects.
  EXPECT_FALSE(sampler_->IsAutogenerateMipmapsEnabled());
  EXPECT_EQ(Sampler::kNearest, sampler_->GetMinFilter());
  EXPECT_EQ(Sampler::kNearest, sampler_->GetMagFilter());
  EXPECT_EQ(Sampler::kRepeat, sampler_->GetWrapT());

  // Set an invalid mode.
  sampler_->SetWrapR(static_cast<Sampler::WrapMode>(bad_mode));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "invalid value"));

  // Check that nothing has changed.
  EXPECT_EQ(Sampler::kMirroredRepeat, sampler_->GetWrapR());
  EXPECT_FALSE(sampler_->IsAutogenerateMipmapsEnabled());
  EXPECT_EQ(Sampler::kNearest, sampler_->GetMinFilter());
  EXPECT_EQ(Sampler::kNearest, sampler_->GetMagFilter());
  EXPECT_EQ(Sampler::kRepeat, sampler_->GetWrapT());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST_F(SamplerTest, SetWrapS) {
  base::LogChecker log_checker;

  // Check that get returns what was set for each mode.
  sampler_->SetWrapS(Sampler::kMirroredRepeat);
  EXPECT_EQ(Sampler::kMirroredRepeat, sampler_->GetWrapS());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Sampler::kWrapSChanged));
  resource_->ResetModifiedBit(Sampler::kWrapSChanged);

  // Check that there weren't any state changing side effects.
  EXPECT_FALSE(sampler_->IsAutogenerateMipmapsEnabled());
  EXPECT_EQ(Sampler::kNearest, sampler_->GetMinFilter());
  EXPECT_EQ(Sampler::kNearest, sampler_->GetMagFilter());
  EXPECT_EQ(Sampler::kRepeat, sampler_->GetWrapT());

  // Set an invalid mode.
  sampler_->SetWrapS(static_cast<Sampler::WrapMode>(bad_mode));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "invalid value"));

  // Check that nothing has changed.
  EXPECT_EQ(Sampler::kMirroredRepeat, sampler_->GetWrapS());
  EXPECT_FALSE(sampler_->IsAutogenerateMipmapsEnabled());
  EXPECT_EQ(Sampler::kNearest, sampler_->GetMinFilter());
  EXPECT_EQ(Sampler::kNearest, sampler_->GetMagFilter());
  EXPECT_EQ(Sampler::kRepeat, sampler_->GetWrapT());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST_F(SamplerTest, SetWrapT) {
  base::LogChecker log_checker;

  // Check that get returns what was set for each mode.
  sampler_->SetWrapT(Sampler::kMirroredRepeat);
  EXPECT_EQ(Sampler::kMirroredRepeat, sampler_->GetWrapT());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Sampler::kWrapTChanged));
  resource_->ResetModifiedBit(Sampler::kWrapTChanged);

  // Check that there weren't any state changing side effects.
  EXPECT_FALSE(sampler_->IsAutogenerateMipmapsEnabled());
  EXPECT_EQ(Sampler::kNearest, sampler_->GetMinFilter());
  EXPECT_EQ(Sampler::kNearest, sampler_->GetMagFilter());
  EXPECT_EQ(Sampler::kRepeat, sampler_->GetWrapS());

  // Set an invalid mode.
  sampler_->SetWrapT(static_cast<Sampler::WrapMode>(bad_mode));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "invalid value"));

  // Check that nothing has changed.
  EXPECT_EQ(Sampler::kMirroredRepeat, sampler_->GetWrapT());
  EXPECT_FALSE(sampler_->IsAutogenerateMipmapsEnabled());
  EXPECT_EQ(Sampler::kNearest, sampler_->GetMinFilter());
  EXPECT_EQ(Sampler::kNearest, sampler_->GetMagFilter());
  EXPECT_EQ(Sampler::kRepeat, sampler_->GetWrapS());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST_F(SamplerTest, AutoMipmapping) {
  // Check that get returns what was set for each mode.
  sampler_->SetAutogenerateMipmapsEnabled(true);
  EXPECT_TRUE(sampler_->IsAutogenerateMipmapsEnabled());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Sampler::kAutoMipmappingChanged));
  resource_->ResetModifiedBit(Sampler::kAutoMipmappingChanged);

  // Check that there weren't any state changing side effects.
  EXPECT_EQ(Sampler::kNearest, sampler_->GetMinFilter());
  EXPECT_EQ(Sampler::kNearest, sampler_->GetMagFilter());
  EXPECT_EQ(Sampler::kRepeat, sampler_->GetWrapS());
  EXPECT_EQ(Sampler::kRepeat, sampler_->GetWrapT());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
}

}  // namespace gfx
}  // namespace ion
