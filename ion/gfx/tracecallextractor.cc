/**
Copyright 2016 Google Inc. All Rights Reserved.

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

#include "ion/gfx/tracecallextractor.h"

#include <algorithm>

#include "ion/base/invalid.h"
#include "ion/base/logging.h"
#include "ion/base/stringutils.h"

namespace ion {
namespace gfx {

TraceCallExtractor::TraceCallExtractor() {
  SetTrace("");
}

TraceCallExtractor::TraceCallExtractor(const std::string& trace) {
  SetTrace(trace);
}

void TraceCallExtractor::SetTrace(const std::string& trace) {
  trace_ = trace;
  CreateCallVector();
}

size_t TraceCallExtractor::GetCallCount() const {
  return calls_.size();
}

size_t TraceCallExtractor::GetCountOf(const std::string& call_prefix) const {
  size_t count = 0;
  for (size_t i = 0, n = calls_.size(); i < n; ++i) {
    if (base::StartsWith(calls_[i], call_prefix))
      count++;
  }
  return count;
}

size_t TraceCallExtractor::GetNthIndexOf(
    size_t n, const std::string& call_prefix) const {
  for (size_t i = 0, num_calls = calls_.size(); i < num_calls; ++i) {
    if (base::StartsWith(calls_[i], call_prefix)) {
      if (n == 0)
        return i;
      else
        --n;
    }
  }
  return base::kInvalidIndex;
}

void TraceCallExtractor::CreateCallVector() {
  const std::vector<std::string> raw_calls =
      base::SplitString(trace_, "\n");
  const size_t count = raw_calls.size();
  calls_.clear();
  calls_.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    if (!base::StartsWith(raw_calls[i], ">") &&
        !base::StartsWith(raw_calls[i], "-")) {
      // Strip out all "<name> = " references.
      std::string call;
      const std::vector<std::string> args =
          base::SplitString(base::TrimStartWhitespace(raw_calls[i]), "(),");
      const size_t arg_count = args.size();
      for (size_t j = 0; j < arg_count; ++j) {
        const size_t pos = args[j].find(" = ");
        std::string arg = args[j];
        if (pos != std::string::npos)
          arg = arg.substr(pos + 3, std::string::npos);
        call += arg;
        if (j == 0)
          call += "(";
        else if (j < arg_count - 1)
          call += ", ";
      }
      call += ")";
      calls_.push_back(call);
    }
  }
}

}  // namespace gfx
}  // namespace ion
