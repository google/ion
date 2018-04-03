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

#include <algorithm>
#include <chrono>  // NOLINT
#include <limits>

#include "ion/math/utils.h"

namespace ion {
namespace analytics {

//-----------------------------------------------------------------------------
//
// Benchmark::VariableSampler functions.
//
//-----------------------------------------------------------------------------

void Benchmark::VariableSampler::AddSample(double value) {
  // If this is the first sample, use the current time as the base time.
  if (variable_.samples.empty()) {
    timer_.Reset();
    variable_.samples.push_back(Sample(0, value));
  } else {
    variable_.samples.push_back(Sample(
        std::chrono::duration_cast<std::chrono::duration<uint32, std::milli>>(
            timer_.Get())
            .count(),
        value));
  }
}

//-----------------------------------------------------------------------------
//
// Benchmark::VariableAccumulator functions.
//
//-----------------------------------------------------------------------------

Benchmark::VariableAccumulator::VariableAccumulator(
    const Descriptor& descriptor)
    : variable_(descriptor, 0,
                std::numeric_limits<double>::max(),
                std::numeric_limits<double>::min(),
                0.0, 0.0),
      m2_(0.0) {
}

void Benchmark::VariableAccumulator::AddSample(double value) {
  ++variable_.samples;
  variable_.minimum = std::min(variable_.minimum, value);
  variable_.maximum = std::max(variable_.maximum, value);

  // Use Welford's algorithm to compute the mean and standard deviation.
  const double delta = value - variable_.mean;
  variable_.mean += delta / static_cast<double>(variable_.samples);
  m2_ += delta * (value - variable_.mean);
}

const Benchmark::AccumulatedVariable Benchmark::VariableAccumulator::Get() {
  // Finish Welford's algorithm, which computes the variance. Take the square
  // root to get the standard deviation.
  variable_.standard_deviation =
      variable_.samples ?
      math::Sqrt(m2_ / static_cast<double>(variable_.samples - 1U)) : 0.0;

  return variable_;
}

//-----------------------------------------------------------------------------
//
// Benchmark functions.
//
//-----------------------------------------------------------------------------

const Benchmark::AccumulatedVariable Benchmark::AccumulateSampledVariable(
    const Benchmark::SampledVariable& sampled_variable) {
  VariableAccumulator va(sampled_variable.descriptor);
  const size_t num_samples = sampled_variable.samples.size();
  for (size_t i = 0; i < num_samples; ++i)
    va.AddSample(sampled_variable.samples[i].value);
  return va.Get();
}

}  // namespace analytics
}  // namespace ion
