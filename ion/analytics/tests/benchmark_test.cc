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

#include "ion/analytics/benchmark.h"

#include <string>
#include <vector>

#include "third_party/googletest/googletest/include/gtest/gtest.h"

using ion::analytics::Benchmark;

TEST(Benchmark, Empty) {
  Benchmark b;
  EXPECT_TRUE(b.GetConstants().empty());
  EXPECT_TRUE(b.GetSampledVariables().empty());
  EXPECT_TRUE(b.GetAccumulatedVariables().empty());
}

TEST(Benchmark, AddConstant) {
  Benchmark b;

  b.AddConstant(Benchmark::Constant(
      Benchmark::Descriptor("Foo", "Group", "Desc", "Liters"), 127.2));
  ASSERT_EQ(1U, b.GetConstants().size());
  {
    const Benchmark::Constant& c = b.GetConstants()[0];
    EXPECT_EQ("Foo", c.descriptor.id);
    EXPECT_EQ("Group", c.descriptor.group);
    EXPECT_EQ("Desc", c.descriptor.description);
    EXPECT_EQ("Liters", c.descriptor.units);
    EXPECT_EQ(127.2, c.value);
  }

  b.AddConstant(Benchmark::Constant(
      Benchmark::Descriptor("Bar", "Group 2", "Desc 2", "Puppies"), 14.5));
  ASSERT_EQ(2U, b.GetConstants().size());
  {
    const Benchmark::Constant& c = b.GetConstants()[1];
    EXPECT_EQ("Bar", c.descriptor.id);
    EXPECT_EQ("Group 2", c.descriptor.group);
    EXPECT_EQ("Desc 2", c.descriptor.description);
    EXPECT_EQ("Puppies", c.descriptor.units);
    EXPECT_EQ(14.5, c.value);
  }
}

TEST(Benchmark, AddSampledVariable) {
  Benchmark b;

  b.AddSampledVariable(Benchmark::SampledVariable(
      Benchmark::Descriptor("Foo", "Group", "Desc", "Liters/s")));
  ASSERT_EQ(1U, b.GetSampledVariables().size());
  {
    const Benchmark::SampledVariable& v = b.GetSampledVariables()[0];
    EXPECT_EQ("Foo", v.descriptor.id);
    EXPECT_EQ("Group", v.descriptor.group);
    EXPECT_EQ("Desc", v.descriptor.description);
    EXPECT_EQ("Liters/s", v.descriptor.units);
    EXPECT_TRUE(v.samples.empty());
  }

  b.AddSampledVariable(Benchmark::SampledVariable(
      Benchmark::Descriptor("Bar", "Group 2", "Desc 2", "Puppies/s")));
  ASSERT_EQ(2U, b.GetSampledVariables().size());
  {
    const Benchmark::SampledVariable& v = b.GetSampledVariables()[1];
    EXPECT_EQ("Bar", v.descriptor.id);
    EXPECT_EQ("Group 2", v.descriptor.group);
    EXPECT_EQ("Desc 2", v.descriptor.description);
    EXPECT_EQ("Puppies/s", v.descriptor.units);
    EXPECT_TRUE(v.samples.empty());
  }

  // Add a variable with samples.
  Benchmark::SampledVariable sv(
      Benchmark::Descriptor("Blah", "Group3", "Desc3", "Units3"));
  sv.samples.push_back(Benchmark::Sample(10, 100.0));
  sv.samples.push_back(Benchmark::Sample(20, 200.0));
  sv.samples.push_back(Benchmark::Sample(30, 300.0));
  b.AddSampledVariable(sv);
  ASSERT_EQ(3U, b.GetSampledVariables().size());
  {
    const Benchmark::SampledVariable& v = b.GetSampledVariables()[2];
    EXPECT_EQ("Blah", v.descriptor.id);
    EXPECT_EQ("Group3", v.descriptor.group);
    EXPECT_EQ("Desc3", v.descriptor.description);
    EXPECT_EQ("Units3", v.descriptor.units);
    ASSERT_EQ(3U, v.samples.size());
    EXPECT_EQ(10U, v.samples[0].time_offset_ms);
    EXPECT_EQ(20U, v.samples[1].time_offset_ms);
    EXPECT_EQ(30U, v.samples[2].time_offset_ms);
    EXPECT_EQ(100.0, v.samples[0].value);
    EXPECT_EQ(200.0, v.samples[1].value);
    EXPECT_EQ(300.0, v.samples[2].value);
  }
}

TEST(Benchmark, AddAccumulatedVariable) {
  Benchmark b;

  b.AddAccumulatedVariable(Benchmark::AccumulatedVariable(
      Benchmark::Descriptor("Foo", "Group", "Desc", "Liters/s"),
      12U, 100.0, 106.5, 102.5, 1.5));
  ASSERT_EQ(1U, b.GetAccumulatedVariables().size());
  {
    const Benchmark::AccumulatedVariable& v = b.GetAccumulatedVariables()[0];
    EXPECT_EQ("Foo", v.descriptor.id);
    EXPECT_EQ("Group", v.descriptor.group);
    EXPECT_EQ("Desc", v.descriptor.description);
    EXPECT_EQ("Liters/s", v.descriptor.units);
    EXPECT_EQ(12U, v.samples);
    EXPECT_EQ(100.0, v.minimum);
    EXPECT_EQ(106.5, v.maximum);
    EXPECT_EQ(102.5, v.mean);
    EXPECT_EQ(1.5, v.standard_deviation);
  }

  b.AddAccumulatedVariable(Benchmark::AccumulatedVariable(
      Benchmark::Descriptor("Bar", "Group 2", "Desc 2", "Puppies/s"),
      42U, 1008.2, 2011.4, 1400.0, 43.2));
  ASSERT_EQ(2U, b.GetAccumulatedVariables().size());
  {
    const Benchmark::AccumulatedVariable& v = b.GetAccumulatedVariables()[1];
    EXPECT_EQ("Bar", v.descriptor.id);
    EXPECT_EQ("Group 2", v.descriptor.group);
    EXPECT_EQ("Desc 2", v.descriptor.description);
    EXPECT_EQ("Puppies/s", v.descriptor.units);
    EXPECT_EQ(42U, v.samples);
    EXPECT_EQ(1008.2, v.minimum);
    EXPECT_EQ(2011.4, v.maximum);
    EXPECT_EQ(1400.0, v.mean);
    EXPECT_EQ(43.2, v.standard_deviation);
  }
}

TEST(Benchmark, VariableSampler) {
  Benchmark b;
  Benchmark::VariableSampler vs(
      Benchmark::Descriptor("Foo", "Group", "Desc", "Liters"));

  // No samples yet.
  {
    const Benchmark::SampledVariable v = vs.Get();
    EXPECT_EQ("Foo", v.descriptor.id);
    EXPECT_EQ("Group", v.descriptor.group);
    EXPECT_EQ("Desc", v.descriptor.description);
    EXPECT_EQ("Liters", v.descriptor.units);
    EXPECT_TRUE(v.samples.empty());
  }

  // Add some samples.
  static const size_t kNumSamples = 10;
  for (size_t i = 0; i < kNumSamples; ++i)
    vs.AddSample(100.0 * static_cast<double>(i));
  {
    const Benchmark::SampledVariable v = vs.Get();
    EXPECT_EQ("Foo", v.descriptor.id);
    EXPECT_EQ("Group", v.descriptor.group);
    EXPECT_EQ("Desc", v.descriptor.description);
    EXPECT_EQ("Liters", v.descriptor.units);
    EXPECT_EQ(kNumSamples, v.samples.size());
    EXPECT_EQ(0U, v.samples[0].time_offset_ms);
    EXPECT_EQ(0., v.samples[0].value);
    EXPECT_LE(v.samples[0].time_offset_ms, v.samples[1].time_offset_ms);
    EXPECT_EQ(100., v.samples[1].value);
    EXPECT_LE(v.samples[1].time_offset_ms, v.samples[2].time_offset_ms);
    EXPECT_EQ(200., v.samples[2].value);
    EXPECT_EQ((kNumSamples - 1) * 100.0, v.samples[kNumSamples - 1].value);
  }
}

TEST(Benchmark, VariableAccumulator) {
  Benchmark b;
  Benchmark::VariableAccumulator va(
      Benchmark::Descriptor("Foo", "Group", "Desc", "Liters"));

  // No samples yet.
  {
    const Benchmark::AccumulatedVariable v = va.Get();
    EXPECT_EQ("Foo", v.descriptor.id);
    EXPECT_EQ("Group", v.descriptor.group);
    EXPECT_EQ("Desc", v.descriptor.description);
    EXPECT_EQ("Liters", v.descriptor.units);
    EXPECT_EQ(0U, v.samples);
    EXPECT_EQ(std::numeric_limits<double>::max(), v.minimum);
    EXPECT_EQ(std::numeric_limits<double>::min(), v.maximum);
    EXPECT_EQ(0.0, v.mean);
    EXPECT_EQ(0.0, v.standard_deviation);
  }

  // Add some samples.
  static const size_t kNumSamples = 1000;
  static const double kBaseValue = 10000.0;
  for (size_t i = 0; i < kNumSamples; ++i) {
    va.AddSample(kBaseValue + static_cast<double>(i % 10));
  }
  {
    const Benchmark::AccumulatedVariable v = va.Get();
    EXPECT_EQ("Foo", v.descriptor.id);
    EXPECT_EQ("Group", v.descriptor.group);
    EXPECT_EQ("Desc", v.descriptor.description);
    EXPECT_EQ("Liters", v.descriptor.units);
    EXPECT_EQ(kNumSamples, v.samples);
    EXPECT_EQ(kBaseValue, v.minimum);
    EXPECT_EQ(kBaseValue + 9, v.maximum);
    EXPECT_NEAR(kBaseValue + 4.5, v.mean, 1e-10);
    EXPECT_NEAR(2.873719, v.standard_deviation, 1e-6);
  }

  // Add another sample.
  va.AddSample(kBaseValue + 100.0);
  {
    const Benchmark::AccumulatedVariable v = va.Get();
    EXPECT_EQ("Foo", v.descriptor.id);
    EXPECT_EQ("Group", v.descriptor.group);
    EXPECT_EQ("Desc", v.descriptor.description);
    EXPECT_EQ("Liters", v.descriptor.units);
    EXPECT_EQ(kNumSamples + 1, v.samples);
    EXPECT_EQ(kBaseValue, v.minimum);
    EXPECT_EQ(kBaseValue + 100, v.maximum);
    EXPECT_NEAR(kBaseValue + 4.5954, v.mean, 1e-4);
    EXPECT_NEAR(4.166670, v.standard_deviation, 1e-6);
  }
}
