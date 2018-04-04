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

#include "ion/gfx/tests/traceverifier.h"

#include <algorithm>

#include "ion/base/invalid.h"
#include "ion/base/logging.h"
#include "ion/base/stringutils.h"
#include "ion/gfx/graphicsmanager.h"
#include "ion/gfx/tracecallextractor.h"
#include "absl/memory/memory.h"

namespace ion {
namespace gfx {
namespace testing {

namespace {

// Strips an address 0x[0-9a-f]* -> tag from an argument and returns the result.
static std::string StripAddressFieldFromArg(const std::string& arg) {
  std::string trimmed_arg = arg;
  if (base::StartsWith(trimmed_arg, "0x")) {
    const size_t pos = trimmed_arg.find("-> ");
    if (pos != std::string::npos && pos + 3U < trimmed_arg.size())
      trimmed_arg = trimmed_arg.substr(pos + 3U);
  }
  if (base::StartsWith(trimmed_arg, "[") && trimmed_arg.length() > 2U &&
      trimmed_arg.find(',') == std::string::npos)
    trimmed_arg = trimmed_arg.substr(1U, trimmed_arg.length() - 2U);
  return trimmed_arg;
}

}  // anonymous namespace

TraceVerifier::Call::Call(const std::string& call)
    : call_(call),
      result_(new ::testing::AssertionResult(true)) {
  // Split the call into the function name and arguments.
  args_ = base::SplitString(call, "(),\t");
  const size_t count = args_.size();
  for (size_t i = 0; i < count; ++i)
    args_[i] = base::TrimStartAndEndWhitespace(args_[i]);
}

TraceVerifier::Call::Call(const Call& other)
    : call_(other.call_),
      args_(other.args_),
      result_(new ::testing::AssertionResult(*other.result_)) {}

TraceVerifier::Call& TraceVerifier::Call::HasArg(
    size_t index, const std::string& arg_start) {
  if (index >= args_.size()) {
    result_ = absl::make_unique<::testing::AssertionResult>(false);
    *result_ << "Expected call \"" << base::JoinStrings(args_, ", ")
             << "\" to have arg " << index << ", but it only has "
             << args_.size() << "; call was " << call_;
  } else {
    // If the arg start begins with an address, ignore it.
    const std::string stripped_arg = StripAddressFieldFromArg(args_[index]);
    const std::string stripped_arg_start = StripAddressFieldFromArg(arg_start);
    if (!base::StartsWith(stripped_arg, stripped_arg_start)) {
      result_ = absl::make_unique<::testing::AssertionResult>(false);
      *result_ << "Expected arg " << index << " to be " << arg_start << " ("
               << stripped_arg_start << "), but it is " << args_[index] << " ("
               << stripped_arg << "); call was " << call_;
    }
  }
  return *this;
}

TraceVerifier::TraceVerifier(GraphicsManager* graphics_manager)
    : tracing_stream_(graphics_manager->GetTracingStream()) {
  tracing_stream_.SetForwardedStream(&trace_stream_);
  tracing_stream_.StartTracing();
}

TraceVerifier::~TraceVerifier() {
  tracing_stream_.StopTracing();
  tracing_stream_.SetForwardedStream(nullptr);
}

size_t TraceVerifier::GetCallCount() const {
  return TraceCallExtractor(trace_stream_.str()).GetCallCount();
}

size_t TraceVerifier::GetCountOf(const std::string& start) const {
  return TraceCallExtractor(trace_stream_.str()).GetCountOf(start);
}

size_t TraceVerifier::GetCountOf(const ArgSpec& arg_spec) const {
  return TraceCallExtractor(trace_stream_.str()).GetCountOf(arg_spec);
}

size_t TraceVerifier::GetNthIndexOf(size_t n, const std::string& start) const {
  return TraceCallExtractor(trace_stream_.str()).GetNthIndexOf(n, start);
}

size_t TraceVerifier::GetNthIndexOf(size_t n, const ArgSpec& arg_spec) const {
  return TraceCallExtractor(trace_stream_.str()).GetNthIndexOf(n, arg_spec);
}

::testing::AssertionResult TraceVerifier::VerifySortedCalls(
    const std::vector<std::string>& expected_starts) const {
  std::vector<std::string> calls = GetCalls();
  std::sort(calls.begin(), calls.end());
  const size_t num_expected_calls = expected_starts.size();
  if (num_expected_calls != calls.size())
    return ::testing::AssertionFailure() << "Expected " << num_expected_calls
                                         << " calls, but found "
                                         << calls.size();
  const size_t check_count = std::min(num_expected_calls, calls.size());
  for (size_t i = 0; i < check_count; ++i) {
    if (!base::StartsWith(calls[i], expected_starts[i]))
      return ::testing::AssertionFailure()
             << "Expected call " << i << " to start"
             << " with " << expected_starts[i] << ", but it is " << calls[i];
  }
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult TraceVerifier::VerifySomeCalls(
    const std::vector<std::string>& expected_starts) const {
  std::vector<std::string> calls = GetCalls();
  std::sort(calls.begin(), calls.end());
  const size_t num_calls = calls.size();
  const size_t num_expected_calls = expected_starts.size();
  for (size_t j = 0; j < num_expected_calls; ++j) {
    bool found = false;
    for (size_t i = 0; i < num_calls; ++i) {
      int comp_val = expected_starts[j].compare(
          calls[i].substr(0, expected_starts[j].size()));
      // If comp_val is < 0 then, since calls is sorted, expected_starts[j] does
      // not appear in calls.
      if (comp_val < 0) {
        break;
      } else if (comp_val == 0) {
        found = true;
        break;
      }
    }
    if (!found)
      return ::testing::AssertionFailure() << "Expected call to start with "
                                           << expected_starts[j];
  }
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult TraceVerifier::VerifyCallAtIndex(size_t index,
                                      const std::string& expected_start) const {
  const std::vector<std::string> calls = GetCalls();
  if (index >= calls.size())
    return ::testing::AssertionFailure() << "Could not find call " << index
                                         << " in vector of " << calls.size()
                                         << " calls";
  if (base::StartsWith(calls[index], expected_start))
    return ::testing::AssertionSuccess();
  else
    return ::testing::AssertionFailure() << "Expected call " << index
                                         << " to start with " << expected_start
                                         << ", but it is " << calls[index];
}

::testing::AssertionResult TraceVerifier::VerifyNoCalls() const {
  const std::vector<std::string> calls = GetCalls();
  if (calls.empty())
    return ::testing::AssertionSuccess();
  else
    return ::testing::AssertionFailure() << "Expected no calls, but found "
                                         << calls.size();
}

::testing::AssertionResult TraceVerifier::VerifyOneCall(
    const std::string& expected_start) const {
  const std::vector<std::string> calls = GetCalls();
  if (calls.size() != 1U)
    return ::testing::AssertionFailure()
           << "Expected only a single call but found " << calls.size();
  if (base::StartsWith(calls[0], expected_start))
    return ::testing::AssertionSuccess();
  else
    return ::testing::AssertionFailure() << "Expected call to start with "
                                         << expected_start << ", but it is "
                                         << calls[0];
}

::testing::AssertionResult TraceVerifier::VerifyTwoCalls(
    const std::string& expected_start0,
    const std::string& expected_start1) const {
  std::vector<std::string> expected_starts;
  expected_starts.push_back(expected_start0);
  expected_starts.push_back(expected_start1);
  return VerifySortedCalls(expected_starts);
}

void TraceVerifier::Reset() {
  trace_stream_.str("");
}

const std::vector<std::string> TraceVerifier::GetCalls() const {
  TraceCallExtractor extractor(trace_stream_.str());
  return TraceCallExtractor(trace_stream_.str()).GetCalls();
}

TraceVerifier::Call TraceVerifier::VerifyCallAt(size_t index) {
  const std::vector<std::string> calls = GetCalls();
  EXPECT_LT(index, calls.size());
  if (index < calls.size())
    return Call(calls[index]);
  else
    return Call("invalid invalid invalid invalid");
}

}  // namespace testing
}  // namespace gfx
}  // namespace ion
