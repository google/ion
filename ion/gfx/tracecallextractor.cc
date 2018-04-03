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

#include "ion/gfx/tracecallextractor.h"

#include <algorithm>

#include "ion/base/invalid.h"
#include "ion/base/logging.h"
#include "ion/base/stringutils.h"

namespace ion {
namespace gfx {

namespace {

// Check whether a single argument entry matches the given call.
bool ArgumentMatches(const std::vector<std::string>& call_args,
                     const std::pair<int, std::string>& arg_entry) {
  return static_cast<size_t>(arg_entry.first) < call_args.size() &&
         call_args[arg_entry.first] == arg_entry.second;
}

}  // anonymous namespace

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
      ++count;
  }
  return count;
}

size_t TraceCallExtractor::GetCountOf(const ArgSpec& name_and_args) const {
  size_t count = 0;
  for (size_t i = 0, n = calls_.size(); i < n; ++i) {
    ++count;
    for (const auto& entry : name_and_args) {
      if (!ArgumentMatches(args_[i], entry)) {
        --count;
        break;
      }
    }
  }
  return count;
}

size_t TraceCallExtractor::GetNthIndexOf(size_t n,
                                         const std::string& call_prefix) const {
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

size_t TraceCallExtractor::GetNthIndexOf(size_t n,
                                         const ArgSpec& name_and_args) const {
  for (size_t i = 0, num_calls = calls_.size(); i < num_calls; ++i) {
    for (const auto& entry : name_and_args) {
      if (!ArgumentMatches(args_[i], entry)) {
        ++n;
        break;
      }
    }
    if (n == 0)
      return i;
    else
      --n;
  }
  return base::kInvalidIndex;
}

void TraceCallExtractor::CreateCallVector() {
  const std::vector<std::string> raw_calls =
      base::SplitString(trace_, "\n");
  const size_t count = raw_calls.size();
  calls_.clear();
  calls_.reserve(count);
  args_.clear();
  args_.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    if (base::StartsWith(raw_calls[i], "GetError() returned ")) continue;
    if (!base::StartsWith(raw_calls[i], ">") &&
        !base::StartsWith(raw_calls[i], "-")) {
      // Strip out all "<name> = " references.
      std::string call;
      std::vector<std::string> args =
          base::SplitString(base::TrimStartWhitespace(raw_calls[i]), "(),");
      const size_t arg_count = args.size();
      for (size_t j = 0; j < arg_count; ++j) {
        size_t pos = args[j].find(" = ");
        if (pos != std::string::npos) {
          // Skip array pointer and to go the actual value(s).
          size_t actual_pos = args[j].find(" -> ");
          if (actual_pos != std::string::npos) pos = actual_pos + 1U;
          args[j] = args[j].substr(pos + 3, std::string::npos);
        }
        call += args[j];
        if (j == 0)
          call += "(";
        else if (j < arg_count - 1)
          call += ", ";
      }
      call += ")";
      calls_.push_back(call);
      args_.push_back(args);
    }
  }
}

}  // namespace gfx
}  // namespace ion
