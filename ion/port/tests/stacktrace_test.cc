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

#include "ion/port/stacktrace.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

#if defined(ION_PLATFORM_LINUX) || defined(ION_PLATFORM_MAC) ||   \
    defined(ION_PLATFORM_ANDROID) || defined(ION_PLATFORM_IOS) || \
    defined(ION_PLATFORM_WINDOWS)
#  define ION_TEST_STACKTRACE
#endif

// The sanitizers seem to inline methods in a way that makes these tests fail.
#if !defined(ADDRESS_SANITIZER) && !defined(THREAD_SANITIZER) && \
    !defined(MEMORY_SANITIZER)
#  define ION_TEST_STACKTRACE_NAMES
#endif

#if ION_DEBUG

void Recursive(size_t caller_stack_depth, size_t recursion_depth) {
  static const size_t kMaxRecursionDepth = 4;
  ion::port::StackTrace stack_trace;
  const std::vector<void*>& stack = stack_trace.GetAddresses();
  std::string stack_string = stack_trace.GetSymbolString();

#if defined(ION_TEST_STACKTRACE)

  EXPECT_EQ(caller_stack_depth + 1, stack.size());
#if defined(ION_TEST_STACKTRACE_NAMES)
  for (size_t i = 0, pos = 0; i <= recursion_depth + 1; ++i, ++pos) {
    pos = stack_string.find(std::string("Recursive"), pos);
    if (i <= recursion_depth)
      EXPECT_TRUE(pos != std::string::npos);
    else
      EXPECT_TRUE(pos == std::string::npos);
  }
#endif
#else
  // StackTrace not implemented.
  EXPECT_EQ(0U, stack.size());
  EXPECT_EQ(0U, stack_string.length());
#endif

  if (recursion_depth < kMaxRecursionDepth)
    Recursive(stack.size(), recursion_depth + 1);
}

void Inner() {
  ion::port::StackTrace stack_trace;
  const std::vector<void*>& stack = stack_trace.GetAddresses();
  std::string stack_string = stack_trace.GetSymbolString();

#if defined(ION_TEST_STACKTRACE)

  EXPECT_GT(stack.size(), 3U);
#if defined(ION_TEST_STACKTRACE_NAMES)
  size_t inner = stack_string.find(std::string("Inner"));
  EXPECT_TRUE(inner != std::string::npos);
  size_t nested = stack_string.find(std::string("Nested"));
  size_t outer = stack_string.find(std::string("Outer"));
  EXPECT_TRUE(nested != std::string::npos);
  EXPECT_TRUE(outer != std::string::npos);
  EXPECT_TRUE(nested > outer);
  EXPECT_TRUE(outer > inner);
#endif
#else
  // StackTrace not implemented.
  EXPECT_EQ(0U, stack.size());
  EXPECT_EQ(0U, stack_string.length());
#endif
}

void Outer() {
  Inner();
}

TEST(StackTrace, Basic) {
  ion::port::StackTrace stack_trace;
  const std::vector<void*>& stack = stack_trace.GetAddresses();
  std::string stack_string = stack_trace.GetSymbolString();

#if defined(ION_TEST_STACKTRACE)

  EXPECT_GT(stack.size(), 1U);
#if defined(ION_TEST_STACKTRACE_NAMES)
  EXPECT_TRUE(
      stack_string.find(std::string("Basic")) != std::string::npos);
#endif
#else
  // StackTrace not implemented.
  EXPECT_EQ(0U, stack.size());
  EXPECT_EQ(0U, stack_string.length());
#endif
}

TEST(StackTrace, Nested) {
  Outer();
}

TEST(StackTrace, Recursion) {
  ion::port::StackTrace stack_trace;
  const std::vector<void*>& stack = stack_trace.GetAddresses();
  std::string stack_string = stack_trace.GetSymbolString();

#if defined(ION_TEST_STACKTRACE)

  EXPECT_GT(stack.size(), 1U);
#if defined(ION_TEST_STACKTRACE_NAMES)
  EXPECT_TRUE(
      stack_string.find(std::string("Recursion")) != std::string::npos);
#endif
#else
  // StackTrace not implemented.
  EXPECT_EQ(0U, stack.size());
  EXPECT_EQ(0U, stack_string.length());
#endif

  Recursive(stack.size(), 0);
}

#undef ION_TEST_STACKTRACE

#endif  // ION_DEBUG
