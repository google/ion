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

#ifndef ION_ANALYTICS_BENCHMARKUTILS_H_
#define ION_ANALYTICS_BENCHMARKUTILS_H_

#include <iostream>  // NOLINT
#include <string>

#include "ion/analytics/benchmark.h"

namespace ion {
namespace analytics {

// Merges one Benchmark instance into another. If any constant or variable is
// present in both instances, this logs an error message and leaves the
// constant or variable untouched in the "to" instance. This returns the number
// of such conflicts.
ION_API size_t MergeBenchmarks(const Benchmark& from, Benchmark* to);

// Outputs benchmark results as CSV (comma-separated values), suitable for use
// in performance dashboards. Note that SampledVariables are converted to
// AccumulatedVariables for CSV output.
ION_API void OutputBenchmarkAsCsv(const Benchmark& benchmark,
                                  std::ostream& out);  // NOLINT

// Outputs a Constant as JSON. See below for the output format.
ION_API void OutputConstantAsJson(const Benchmark::Constant& c,
                                  const std::string& indent,
                                  std::ostream& out);  // NOLINT

// Outputs an AccumulatedVariable as JSON. See below for the output format.
ION_API void OutputAccumulatedVariableAsJson(
    const Benchmark::AccumulatedVariable& v, const std::string& indent,
    std::ostream& out);  // NOLINT

// Outputs benchmark results as JSON, suitable for serialization and use in
// performance dashboards. Note that SampledVariables are converted to
// AccumulatedVariables for JSON output. Pass the proper indentation depending
// on the hierarchy of objects you wish to insert the JSON into. The JSON output
// to the stream is an object of lists, for example:
//   {
//     "constants": [
//       {
//         "id": "Const1",
//         "description": "CDesc1",
//         "group": "Group1",
//         "value": 1,
//         "units": "Units1"
//       },
//     ],
//     "sampled_variables": [
//       {
//         "id": "SVar2",
//         "description": "SVDesc2",
//         "group": "Group2",
//         "mean": 200,
//         "units": "Units2",
//         "minimum": 100,
//         "maximum": 300,
//         "standard_deviation": 20000,
//         "variation": 10000
//       }
//     ],
//     "accumulated_variables": [
//       {
//         "id": "AVar3",
//         "description": "AVDesc3",
//         "group": "Group3",
//         "mean": 2000,
//         "units": "Units3"
//       }
//     ]
//   }
ION_API void OutputBenchmarkAsJson(const Benchmark& benchmark,
                                   const std::string& indent_in,
                                   std::ostream& out);  // NOLINT

// Outputs benchmark results in a pretty format. Note that SampledVariables are
// converted to AccumulatedVariables for pretty output.
ION_API void OutputBenchmarkPretty(const std::string& id_string,
                                   bool print_descriptions,
                                   const Benchmark& benchmark,
                                   std::ostream& out);  // NOLINT

}  // namespace analytics
}  // namespace ion

#endif  // ION_ANALYTICS_BENCHMARKUTILS_H_
