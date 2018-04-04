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

#ifndef ION_ANALYTICS_BENCHMARK_H_
#define ION_ANALYTICS_BENCHMARK_H_

#include <string>
#include <vector>

#include "base/integral_types.h"
#include "ion/port/timer.h"

namespace ion {
namespace analytics {

// The Benchmark class provides types and utilities to make it easier to create
// performance benchmarks. It facilitates tracking constant values (such as
// number of frames) and accumulation of per-sample variables (such as
// triangles per frames or frames per second).
class ION_API Benchmark {
 public:
  // This struct stores information about a measurement computed by
  // benchmarking. It is used to describe the value in benchmark reports.
  struct Descriptor {
    Descriptor(std::string id_in,
               std::string group_in,
               std::string description_in,
               std::string units_in)
        : id(std::move(id_in)),
          group(std::move(group_in)),
          description(std::move(description_in)),
          units(std::move(units_in)) {}
    std::string id;            // (Unique) identifying name.
    std::string group;         // Group the measurement belongs to.
    std::string description;   // Readable description.
    std::string units;         // Description of units.
  };

  // A variant of the above structure that can be trivially destructed. Only use
  // this when all parameters are string constants.
  struct StaticDescriptor {
    StaticDescriptor(const char* id_in,
               const char* group_in,
               const char* description_in,
               const char* units_in)
        : id(id_in),
          group(group_in),
          description(description_in),
          units(units_in) {}
    // Silence ClangTidy, since we actually want this conversion to be implicit.
    operator Descriptor() const {  // NOLINT
      return Descriptor(id, group, description, units);
    }
    const char* id;
    const char* group;
    const char* description;
    const char* units;
  };

  // This struct represents a number that is constant over all samples.
  struct Constant {
    Constant(const Descriptor& descriptor_in, double value_in)
        : descriptor(descriptor_in),
          value(value_in) {}
    Descriptor descriptor;
    double value;
  };

  // This struct represents a single timestamped value of a variable. To save
  // space, the timestamp (in milliseconds) is relative to an initial timestamp
  // so that it can be stored in 32 bits.
  struct Sample {
    Sample(uint32 time_offset_ms_in, double value_in)
        : time_offset_ms(time_offset_ms_in),
          value(value_in) {}
    uint32 time_offset_ms;
    double value;
  };

  // This struct represents a variable: a number that may vary over samples,
  // such as a count or timing. It stores all of the samples.
  struct SampledVariable {
    explicit SampledVariable(const Descriptor& descriptor_in)
        : descriptor(descriptor_in) {}
    Descriptor descriptor;
    std::vector<Sample> samples;
  };

  // This struct represents accumulated values for a variable. It uses less
  // space than a SampledVariable.
  struct AccumulatedVariable {
    AccumulatedVariable(const Descriptor& descriptor_in, size_t samples_in,
                        double minimum_in, double maximum_in,
                        double mean_in, double standard_deviation_in)
        : descriptor(descriptor_in),
          samples(samples_in),
          minimum(minimum_in),
          maximum(maximum_in),
          mean(mean_in),
          standard_deviation(standard_deviation_in) {}
    Descriptor descriptor;
    size_t samples;             // Number of samples taken.
    double minimum;             // Minimum value.
    double maximum;             // Maximum value.
    double mean;                // Average (mean) value.
    double standard_deviation;  // Standard deviation of value.
  };

  // This class aids in creation of a benchmarked SampledVariable.
  class ION_API VariableSampler {
   public:
    explicit VariableSampler(const Descriptor& descriptor)
        : variable_(descriptor) {}

    // Adds one sample of the SampledVariable's value.
    void AddSample(double value);

    // Returns the resulting SampledVariable.
    const SampledVariable Get() { return variable_; }

   private:
    SampledVariable variable_;

    // This times the samples.
    ion::port::Timer timer_;
  };

  // This class aids in accumulation of a benchmarked AccumulatedVariable.
  class ION_API VariableAccumulator {
   public:
    explicit VariableAccumulator(const Descriptor& descriptor);

    // Adds one sample of the Variable's value.
    void AddSample(double value);

    // Returns the resulting Variable.
    const AccumulatedVariable Get();

   private:
    AccumulatedVariable variable_;
    double m2_;  // Needed by Welford's algorithm.
  };

  // Each of these adds a measurement of a specific type to the benchmark
  // results.
  void AddConstant(const Constant& constant) { constants_.push_back(constant); }
  void AddSampledVariable(const SampledVariable& variable) {
    sampled_variables_.push_back(variable);
  }
  void AddAccumulatedVariable(const AccumulatedVariable& variable) {
    accumulated_variables_.push_back(variable);
  }

  // Each of these returns the results for a given type of measurement.
  const std::vector<Constant>& GetConstants() const { return constants_; }
  const std::vector<SampledVariable>& GetSampledVariables() const {
    return sampled_variables_;
  }
  const std::vector<AccumulatedVariable>& GetAccumulatedVariables() const {
    return accumulated_variables_;
  }

  // Converts a SampledVariable to an AccumulatedVariable by accumulating all
  // of the samples.
  static const AccumulatedVariable AccumulateSampledVariable(
      const SampledVariable& sampled_variable);

 private:
  std::vector<Constant> constants_;
  std::vector<SampledVariable> sampled_variables_;
  std::vector<AccumulatedVariable> accumulated_variables_;
};

}  // namespace analytics
}  // namespace ion

#endif  // ION_ANALYTICS_BENCHMARK_H_
