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

#ifndef ION_PROFILE_TIMELINEMETRIC_H_
#define ION_PROFILE_TIMELINEMETRIC_H_

#include "ion/analytics/benchmark.h"
#include "ion/profile/timeline.h"

namespace ion {
namespace profile {

// A timeline metric processes a timeline, computes a set of metrics and adds
// them to a Benchmark object. Instances of this class need to be registerd with
// the CallTraceManager.
class TimelineMetric {
 public:
  virtual ~TimelineMetric() {}
  // Run the metric on the |timeline| and add results to |benchmark|.
  virtual void Run(const Timeline& timeline,
                   analytics::Benchmark* benchmark) const = 0;
};

}  // namespace profile
}  // namespace ion

#endif  // ION_PROFILE_TIMELINEMETRIC_H_
