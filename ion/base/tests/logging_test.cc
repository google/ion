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

#include <functional>
#include <iostream>
#include <sstream>
#include <string>

#include "ion/base/logchecker.h"
#include "ion/base/logging.h"
#include "ion/base/nulllogentrywriter.h"
#include "ion/base/tests/logging_test_util.h"
#include "ion/port/fileutils.h"
#include "ion/port/logging.h"
#include "ion/port/timer.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

using ion::port::GetCanonicalFilePath;

namespace {

#if !ION_PRODUCTION || ION_ALWAYS_LOG
// Helper function that builds a line number into a string.
static int LogMessageOnce() {
  LOG_ONCE(INFO) << "This message should be printed once";
  return __LINE__ - 1;
}

static int LogAnotherMessageOnce() {
  LOG_ONCE(INFO) << "This message should also be printed once";
  return __LINE__ - 1;
}

static int LogMessageEverySecond() {
  LOG_EVERY_N_SEC(INFO, 1) << "This message should be printed no more than "
                              "once per second";
  return __LINE__ - 2;
}
#endif

// Helper class to test check operations with staticly initialized variables
// which have no address.
class ClassWithStaticInitializers {
 public:
  ClassWithStaticInitializers() {}
  ~ClassWithStaticInitializers() {}

  enum Enum {
    kValue1,
    kValue2
  };

  static const int kInt = 1;
  static const int kSizeT = 3U;
  static const Enum kEnum = kValue2;
};

}  // namespace

// Helper function that builds a line number into a string.
static const std::string BuildMessage(const char* severity, int line,
                                      const char* after) {
  std::ostringstream s;
  s << severity << " [" << GetCanonicalFilePath(__FILE__) << ":" << line << "] "
    << after;
  return s.str();
}

TEST(Logging, SetWriter) {
  // Expect the default log-writer to be used before we
  // replace it with our own.
  EXPECT_EQ(ion::base::GetDefaultLogEntryWriter(),
            ion::base::GetLogEntryWriter());

  ion::base::NullLogEntryWriter null_logger;

  EXPECT_EQ(&null_logger, ion::base::GetLogEntryWriter());
}

TEST(Logging, BadSeverity) {
  ion::base::LogChecker checker;

  using ion::base::logging_internal::Logger;
  using ion::port::LogSeverity;

  // Can't use LOG macro because the Severity is not one of the supported ones.
  int severity_as_int = 123;
  const LogSeverity severity = static_cast<LogSeverity>(severity_as_int);
  Logger(__FILE__, __LINE__, severity).GetStream() << "Blah";
  const int line = __LINE__ - 1;
  EXPECT_EQ(BuildMessage("<Unknown severity>", line, "Blah\n"),
            GetCanonicalFilePath(checker.GetLogString()));
  checker.ClearLog();
}

TEST(Logging, CheckMessage) {
  std::string message =
      ion::base::logging_internal::Logger::CheckMessage("check", "expr");
  EXPECT_EQ(0, strcmp("check failed: expression='expr' ", message.c_str()));
}

TEST(Logging, NullLogger) {
  ion::base::logging_internal::NullLogger null_logger;
  // Test that NullLogger can handle std::endl.
  null_logger.GetStream() << std::endl;
}

#if !ION_PRODUCTION || ION_ALWAYS_LOG
TEST(Logging, OneInfo) {
  ion::base::LogChecker checker;

  LOG(INFO) << "Test string";
  const int line = __LINE__ - 1;
  EXPECT_EQ(BuildMessage("INFO", line, "Test string\n"),
            GetCanonicalFilePath(checker.GetLogString()));
  checker.ClearLog();
}

TEST(Logging, Multiple) {
  ion::base::LogChecker checker;

  LOG(WARNING) << "This is a warning!";
  const int line0 = __LINE__ - 1;
  LOG(ERROR) << "And an error!";
  const int line1 = __LINE__ - 1;
  EXPECT_EQ(BuildMessage("WARNING", line0, "This is a warning!\n") +
            BuildMessage("ERROR", line1, "And an error!\n"),
            GetCanonicalFilePath(checker.GetLogString()));
  checker.ClearLog();
}

TEST(Logging, SingleLogger) {
  ion::base::LogChecker checker;
  int line = LogMessageOnce();
  EXPECT_EQ(BuildMessage("INFO", line, "This message should be printed once\n"),
            GetCanonicalFilePath(checker.GetLogString()));
  checker.ClearLog();
  LogMessageOnce();
  EXPECT_FALSE(checker.HasAnyMessages());

  line = LogAnotherMessageOnce();
  EXPECT_EQ(
      BuildMessage("INFO", line, "This message should also be printed once\n"),
      GetCanonicalFilePath(checker.GetLogString()));
  checker.ClearLog();
  LogAnotherMessageOnce();
  EXPECT_FALSE(checker.HasAnyMessages());

  // Clear the set of logged messages.
  ion::base::logging_internal::SingleLogger::ClearMessages();
  line = LogAnotherMessageOnce();
  EXPECT_EQ(
      BuildMessage("INFO", line, "This message should also be printed once\n"),
      GetCanonicalFilePath(checker.GetLogString()));
  checker.ClearLog();
  LogAnotherMessageOnce();
  EXPECT_FALSE(checker.HasAnyMessages());
}

TEST(Logging, ThrottledLogger) {
  ion::base::LogChecker checker;
  int line = LogMessageEverySecond();
  EXPECT_EQ(BuildMessage("INFO", line, "This message should be printed no more "
                              "than once per second\n"),
            GetCanonicalFilePath(checker.GetLogString()));
  checker.ClearLog();
  LogMessageEverySecond();
  EXPECT_FALSE(checker.HasAnyMessages());
  ion::port::Timer::SleepNSeconds(2);
  line = LogMessageEverySecond();
  EXPECT_EQ(BuildMessage("INFO", line, "This message should be printed no more "
                                       "than once per second\n"),
            GetCanonicalFilePath(checker.GetLogString()));
  EXPECT_TRUE(checker.HasMessage("INFO", "This message should be printed"));
}

TEST(Logging, SetLoggingTag) {
  ion::base::LogChecker checker;

  LOG(INFO) << "Test string";
  int line = __LINE__ - 1;
  EXPECT_EQ(BuildMessage("INFO", line, "Test string\n"),
            GetCanonicalFilePath(checker.GetLogString()));
  ion::port::SetLoggingTag("LoggingTest");
  checker.ClearLog();
  LOG(INFO) << "Test string";
  line = __LINE__ - 1;
  EXPECT_EQ(BuildMessage("INFO", line, "Test string\n"),
            GetCanonicalFilePath(checker.GetLogString()));
  checker.ClearLog();
}
#endif

// This is intentially outside of the #if !ION_PRODUCTION block as LOG_PROD
// should log messages independently of whether ION_PRODUCTION is defined.
TEST(Logging, LogProd) {
  ion::base::LogChecker checker;

  LOG_PROD(INFO) << "Test string";
  const int line = __LINE__ - 1;
  EXPECT_EQ(BuildMessage("INFO", line, "Test string\n"),
            GetCanonicalFilePath(checker.GetLogString()));
  checker.ClearLog();
}

TEST(Logging, DcheckSyntax) {
  // Make sure that CHECK and DCHECK parenthesize expressions properly.
  CHECK_EQ(0x1, 0x1 & 0x3);
  CHECK_NE(0x0, 0x1 & 0x3);
  CHECK_LE(0x1, 0x1 & 0x3);
  CHECK_LT(0x0, 0x1 & 0x3);
  CHECK_GE(0x1, 0x1 & 0x3);
  CHECK_GT(0x2, 0x1 & 0x3);

  DCHECK_EQ(0x1, 0x1 & 0x3);
  DCHECK_NE(0x0, 0x1 & 0x3);
  DCHECK_LE(0x1, 0x1 & 0x3);
  DCHECK_LT(0x0, 0x1 & 0x3);
  DCHECK_GE(0x1, 0x1 & 0x3);
  DCHECK_GT(0x2, 0x1 & 0x3);

  // Make sure that CHECK_NOTNULL returns the argument value.
  int some_int = 0;
  int* some_int_ptr = CHECK_NOTNULL(&some_int);
  CHECK_EQ(&some_int, some_int_ptr);

  // Check nullptr specialization.
  CHECK_NE(nullptr, some_int_ptr);
  CHECK_NE(some_int_ptr, nullptr);
  DCHECK_NE(nullptr, some_int_ptr);
  DCHECK_NE(some_int_ptr, nullptr);

  size_t some_size_t = 2U;
  CHECK_EQ(2U, some_size_t);
  CHECK_EQ(some_size_t, 2U);
}

TEST(Logging, ClassStaticConstInitializers) {
  CHECK_EQ(1, ClassWithStaticInitializers::kInt);
  CHECK_EQ(ClassWithStaticInitializers::kInt, 1U);
  CHECK_EQ(3U, ClassWithStaticInitializers::kSizeT);
  CHECK_EQ(ClassWithStaticInitializers::kSizeT, 3U);
  CHECK_EQ(ClassWithStaticInitializers::kValue2,
           ClassWithStaticInitializers::kEnum);
  CHECK_EQ(ClassWithStaticInitializers::kEnum,
           ClassWithStaticInitializers::kValue2);
}

TEST(Logging, QcheckGeneratesCode) {
  // 1. QCHECK exists and compiles.
  // 2. The expression in QCHECK executes the expression at runtime.
  // 3. QCHECK produces an assert on false expressions.
  const int initial_value = 1;
  int final_value = 0;
  // Note: using assignment expression to verify code generation.
  QCHECK((final_value = initial_value));
  EXPECT_EQ(initial_value, final_value);

  constexpr char kCheckFailMessage[] = "CHECK failed";
  // Testing QCHECK so suppress QCHECK_EQ suggestion lint.
  EXPECT_DEATH_IF_SUPPORTED(QCHECK(final_value == 0), kCheckFailMessage);
}

TEST(Logging, QcheckComparisonTests) {
  // Run through QCHECK comparison forms. Break handler and TestInt
  // instrumentation used to validate against silent failure (e.g. no code
  // generated at all).

  using TestInt = ion::base::testing::TestInt;
  const TestInt kZero(0);
  const TestInt kOne(1);

  // Ensure comparison QCHECKS can pass, while still generating code.
  // The expression forms normally have no side effects, but TestInt has
  // instrumentation.
  QCHECK_EQ(kZero, kZero);
  EXPECT_EQ(2, kZero.GetComparisonCount());
  EXPECT_EQ(0, kOne.GetComparisonCount());
  QCHECK_NE(kZero, kOne);
  QCHECK_LE(kZero, kOne);
  QCHECK_LE(kOne, kOne);
  QCHECK_LT(kZero, kOne);
  QCHECK_GE(kOne, kZero);
  QCHECK_GE(kOne, kOne);
  QCHECK_GT(kOne, kZero);
  EXPECT_EQ(7, kZero.GetComparisonCount());
  EXPECT_EQ(9, kOne.GetComparisonCount());

  // Ensure assert production. This versions uses the Ion break handler, which
  // is more portable than EXPECT_DEATH.
  EXPECT_DEATH_IF_SUPPORTED(QCHECK_EQ(kZero, kOne), "CHECK");
  EXPECT_DEATH_IF_SUPPORTED(QCHECK_NE(kZero, kZero), "CHECK");
  EXPECT_DEATH_IF_SUPPORTED(QCHECK_LE(kOne, kZero), "CHECK");
  EXPECT_DEATH_IF_SUPPORTED(QCHECK_LT(kOne, kZero), "CHECK");
  EXPECT_DEATH_IF_SUPPORTED(QCHECK_GE(kZero, kOne), "CHECK");
  EXPECT_DEATH_IF_SUPPORTED(QCHECK_GT(kZero, kOne), "CHECK");
}

// Verify that log messages don't interleave.
TEST(Logging, NoInterleaving) {
  ion::base::LogChecker checker;

  using ion::base::logging_internal::Logger;
  std::unique_ptr<Logger> logger1(new Logger("file1", 42, ion::port::INFO));
  std::unique_ptr<Logger> logger2(new Logger("file2", 24, ion::port::INFO));
  logger1->GetStream() << "logger1 message";
  logger2->GetStream() << "logger2 message";
  // This is the key to this test; logger1 needs to be freed before logger2 to
  // demonstrate that messages don't get interleaved.
  logger1.reset();
  ASSERT_EQ("INFO [file1:42] logger1 message\n", checker.GetLogString());
  checker.ClearLog();

  logger2.reset();
  ASSERT_EQ("INFO [file2:24] logger2 message\n", checker.GetLogString());
  checker.ClearLog();

  EXPECT_FALSE(checker.HasAnyMessages());
}
