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

#ifndef ION_GFX_TESTS_TRACEVERIFIER_H_
#define ION_GFX_TESTS_TRACEVERIFIER_H_

#include <memory>
#include <sstream>  // NOLINT
#include <string>
#include <vector>

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfx {

class GraphicsManager;
class TracingStream;

namespace testing {

// The TraceVerifier class can be used in graphics tests to verify that certain
// OpenGL calls were made by a GraphicsManager by examining the tracing output.
// The verification functions use EXPECT() macros to test that the expected
// calls were made.
//
// Calls are tested using one or more strings that must match the beginnings of
// the resulting trace strings. This scheme is used, rather than exact
// full-string matches, to allow for differences in trace formatting on
// different platforms.
class TraceVerifier {
 public:
  using ArgSpec = std::vector<std::pair<int, std::string>>;
  // The constructor is passed a GraphicsManager instance that is used for
  // verification. It calls GraphicsManager::SetTracingStream() to send tracing
  // information to an internal string stream, which is used by the
  // verification functions.
  explicit TraceVerifier(GraphicsManager* graphics_manager);
  ~TraceVerifier();

  // Returns the number of calls in the stream.
  size_t GetCallCount() const;

  // Returns the number of times the passed call start occurs in the trace
  // stream.
  size_t GetCountOf(const std::string& start) const;
  size_t GetCountOf(const ArgSpec& arg_spec) const;

  // Returns the index of the nth call in the trace stream beginning with
  // start, if it exists, otherwise returns base::kInvalidIndex. Note that
  // n == 0 returns the first index, n == 1 returns the second index, and so on.
  size_t GetNthIndexOf(size_t n, const std::string& start) const;
  size_t GetNthIndexOf(size_t n, const ArgSpec& arg_spec) const;

  // Verifies that one or more OpenGL calls were made in arbitrary order, using
  // the trace strings from the GraphicsManager.  This assumes the expected
  // strings are sorted alphabetically, as it sorts the resulting call strings
  // and compares them in the same order.
  ::testing::AssertionResult VerifySortedCalls(
      const std::vector<std::string>& expected_starts) const;

  // Verifies that one or more OpenGL calls were made in arbitrary order, using
  // the trace strings from the GraphicsManager.  Multiple identical strings
  // will all be matched.  expected_starts does not have to be a complete set
  // of calls.
  ::testing::AssertionResult VerifySomeCalls(
      const std::vector<std::string>& expected_starts) const;

  // Verifies that an OpenGL call was made at a given index among the trace
  // strings, in their original order.
  ::testing::AssertionResult VerifyCallAtIndex(
      size_t index,
      const std::string& expected_start) const;

  // Verifies that no OpenGL calls were made.
  ::testing::AssertionResult VerifyNoCalls() const;

  // Verifies that a single OpenGL call was made, using the trace strings.
  ::testing::AssertionResult VerifyOneCall(
      const std::string& expected_start) const;

  // Verifies that exactly two OpenGL calls were made in either order, using
  // the trace strings.
  ::testing::AssertionResult VerifyTwoCalls(
      const std::string& expected_start0,
      const std::string& expected_start1) const;

  // Returns the entire trace output contents, which can be useful for
  // debugging test failures.
  const std::string GetTraceString() const { return trace_stream_.str(); }

  // Resets the current trace output for a new test.
  void Reset();

  // A wrapper class around a single call, a Call provides a declarative way to
  // determine if a call with specific arguments occurs in the trace stream.
  // For example, to see if a buffer with a particular pointer value was bound,
  // e.g., BindBuffer(GL_ARRAY_BUFFER, <some size>, 0xff0522ca, GL_STREAM_DRAW)
  // occurs, use the following sequence of calls:
  //
  // const size_t index = verifier.GetNthIndexOf(0,
  //   "BufferData(GL_ARRAY_BUFFER");
  // if (verifier.VerifyCallAt(index).HasArg(3, "0xff0522ca")
  //     .HasArg(4, "GL_STREAM_DRAW"))
  // ...  // The call occurred.
  //
  // Note that in the above example, the arguments are 1-based; the argument at
  // index 0 is the function name. Also not all arguments were checked, allowing
  // imprecise specification of a particular call.
  class Call {
   public:
    explicit Call(const std::string& call);
    Call(const Call& other);
    Call& HasArg(size_t index, const std::string& arg_start);
    std::string GetArg(size_t index) { return args_.at(index); }
    operator ::testing::AssertionResult() const { return *result_; }
   private:
    std::string call_;
    std::vector<std::string> args_;
    std::unique_ptr< ::testing::AssertionResult> result_;
  };

  // Returns a call object (see above) containing the call at the specified
  // index. Use GetNthIndexOf() to get the index of a call.
  Call VerifyCallAt(size_t index);

 private:
  // Parses the trace stream and returns a vector of calls, removing leading
  // whitespace from each call and ignoring markers that start with - or >.
  const std::vector<std::string> GetCalls() const;

  TracingStream& tracing_stream_;

  // String stream used to save the tracing results.
  std::ostringstream trace_stream_;
};

}  // namespace testing
}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_TESTS_TRACEVERIFIER_H_
