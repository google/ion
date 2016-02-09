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


#ifndef ION_BASE_LOGGING_H_
#define ION_BASE_LOGGING_H_

#include <functional>
#include <memory>
#include <sstream>
#include <string>

#include "ion/port/logging.h"

// @file
//
// Macros for displaying logging messages and checking assertions.  Logging
// and checking facilities are each available in fatal and non-fatal variants,
// and variants that log only in debug mode vs. always.
//
// @internal

namespace ion {
namespace base {

// See ion::port::LogEntryWriter, an abstract class which can be overridden to
// integrate Ion Logging with other logging systems.
//
// Sets the log-writer to log messages to, instead of the default for the
// current platform (passing NULL causes the default writer to be used).
// It is assumed that the writer's lifetime is long enough to process all
// subsequent messages.
ION_API void SetLogEntryWriter(port::LogEntryWriter* writer);

// Returns the log-writer that messages are currently logged to.
ION_API port::LogEntryWriter* GetLogEntryWriter();

// Returns the log-writer that messages will be logged to if
// if another is not explicitly specified via SetLogEntryWriter().
ION_API port::LogEntryWriter* GetDefaultLogEntryWriter();

// Sets a custom break handler that gets invoked by Logger::CheckMessage.
// If break_handler is an empty function (e.g. a std::function<void()>
// without any function assigned to it), fatal errors will not invoke any
// break handler and will not abort program execution.
ION_API void SetBreakHandler(const std::function<void()>& break_handler);

// Restores the default break handler (port::BreakAndAbort).
ION_API void RestoreDefaultBreakHandler();

// Internal logging implementation.  Should not be used by client code.  Classes
// and variables are in an internal namespace to avoid polluting the Ion
// namespace.
namespace logging_internal {

// This class is used for regular logging. It sends messages to std::cerr by
// default, but that can be changed with SetLogEntryWriter(), which is a global
// override.
class ION_API Logger {
 public:
  Logger(const char* file_name, int line_number, port::LogSeverity severity);
  ~Logger();

  // Return the stream to which output is sent (or accumulated).
  std::ostream& GetStream();

  // Returns a message that can be used in CHECK or DCHECK output.
  static const std::string CheckMessage(const char* check_string,
                                        const char* expr_string);

 private:
  port::LogSeverity severity_;
  std::ostringstream stream_;
};

// This class is used to disable logging, while still allowing for log messages
// to contain '<<' expressions. Fatal messages still call the registered break
// handler.
class ION_API NullLogger {
 public:
  // Constructs a NullLogger that does nothing.
  NullLogger() {}

  // Since NullLogger generally does nothing, the severity is necessary so that
  // it can break on fatal errors.
  explicit NullLogger(port::LogSeverity severity);
  ~NullLogger() {}

  // Returns the stream to which output is sent.
  std::ostream& GetStream();
};

// This class prints a message only the first time it is created for the passed
// |file_name| and |line_number|. Subsequent creations with the same parameters
// (regardless of severity) will not print a message.
class ION_API SingleLogger {
 public:
  SingleLogger(const char* file_name, int line_number,
               port::LogSeverity severity);
  ~SingleLogger();

  // Return the stream to which output is sent (or accumulated).
  std::ostream& GetStream();

  // Clears the set of messages that have been logged. This means that the next
  // LOG_ONCE calls will succeed, once.
  static void ClearMessages();

 private:
  static bool HasLoggedMessageAt(const char* file_name, int line_number);

  std::unique_ptr<Logger> logger_;
};

// This class prints a message only if the passed |file_name| and |line_number|
// has not printed a message in a certain amount of time. Any creations with the
// same parameters (regardless of severity) within that time window will not
// print a message.
class ION_API ThrottledLogger {
 public:
  ThrottledLogger(const char* file_name, int line_number,
                  port::LogSeverity severity, float seconds);
  ~ThrottledLogger();

  // Return the stream to which output is sent (or accumulated).
  std::ostream& GetStream();

 private:
  std::unique_ptr<Logger> logger_;
};

}  // namespace logging_internal
}  // namespace base
}  // namespace ion

// @name Internal LOG macro implementation
//
// These macros implement Ion's LOG statement.  Use the non-prefixed LOG macros
// in your code instead.
// @{

// Completely disable logging in production builds.
#if ION_PRODUCTION

// Macro that actually causes logging to happen.
#define ION_LOG(severity) \
  ::ion::base::logging_internal::NullLogger(::ion::port::severity).GetStream()

// Macro that ignores logging.
#define ION_IGNORE_LOG(severity) \
  ::ion::base::logging_internal::NullLogger(::ion::port::severity).GetStream()

// Macro that logs a message only once.
#define ION_LOG_ONCE(severity) \
  ::ion::base::logging_internal::NullLogger(::ion::port::severity).GetStream()

// Macro that logs a message at most every N seconds.
#define ION_LOG_EVERY_N_SEC(severity, time) \
  ::ion::base::logging_internal::NullLogger(::ion::port::severity).GetStream()

#else

// Macro that actually causes logging to happen.
#define ION_LOG(severity)                                   \
  ::ion::base::logging_internal::Logger(__FILE__, __LINE__, \
                                        ::ion::port::severity).GetStream()

// Macro that ignores logging.
#define ION_IGNORE_LOG(severity) \
  ::ion::base::logging_internal::NullLogger().GetStream()

// Macro that logs a message only once.
#define ION_LOG_ONCE(severity)                 \
  ::ion::base::logging_internal::SingleLogger( \
      __FILE__, __LINE__, ::ion::port::severity).GetStream()

// Macro that logs a message at most every N seconds.
#define ION_LOG_EVERY_N_SEC(severity, time)       \
  ::ion::base::logging_internal::ThrottledLogger( \
      __FILE__, __LINE__, ::ion::port::severity, time).GetStream()

#endif

// @}

// @name Main logging macros
//
// These macros log a message to the console in a platform-specific way.  The
// severity argument determines the severity of the logging message.  `DFATAL`
// and `FATAL` severities will also trigger a breakpoint and abort, either in
// debug mode (`DFATAL`) or unconditionally (`FATAL`).
//
// All versions of these macros use streaming syntax, like std::cerr.  That is,
// you invoke them like:
//
//     LOG(WARNING) << "Something mildly concerning happened.";
//     LOG(FATAL) << "The " << device << " is literally on fire.";
//
// @{

// Logs the streamed message unconditionally with a severity of |severity|.
#define LOG(severity) ION_LOG(severity)

// Logs the streamed message once per process run with a severity of |severity|.
#define LOG_ONCE(severity) ION_LOG_ONCE(severity)

// Logs the streamed message at most once every |time| seconds with a severity
// of |severity|.
#define LOG_EVERY_N_SEC(severity, time) ION_LOG_EVERY_N_SEC(severity, time)
#if ION_DEBUG
#define DLOG(severity) ION_LOG(severity)
#define DLOG_ONCE(severity) ION_LOG_ONCE(severity)
#define DLOG_EVERY_N_SEC(severity, time) ION_LOG_EVERY_N_SEC(severity, time)
#else
// Same as LOG(severity), but only logs in debug mode.
#define DLOG(severity) ION_IGNORE_LOG(severity)

// Same as LOG_ONCE(severity), but only logs in debug mode.
#define DLOG_ONCE(severity) ION_IGNORE_LOG(severity)

// Same as LOG_EVERY_N_SEC(severity, time), but only logs in debug mode.
#define DLOG_EVERY_N_SEC(severity, time) ION_IGNORE_LOG(severity)
#endif
// @}

// @name Internal CHECK macro implementation
//
// These macros implement Ion's CHECK and DCHECK statements.  Use the non-
// prefixed CHECK and DCHECK macros in your code instead.
// @{
#define ION_LOG_CHECK_MESSAGE(severity, check_type, expr)               \
  LOG(severity) << ::ion::base::logging_internal::Logger::CheckMessage( \
                       check_type, #expr)

#define ION_CHECK(expr) \
  if (expr)             \
    ;                   \
  else                  \
  ION_LOG_CHECK_MESSAGE(FATAL, "CHECK", #expr)

// ION_DCHECK is defined normally in debug builds.
#if ION_DEBUG
#define ION_DCHECK(expr) \
  if (expr)              \
    ;                    \
  else                   \
    ION_LOG_CHECK_MESSAGE(DFATAL, "DCHECK", #expr)
#else
// In optimized and production builds, we still want to compile expr in order
// to avoid unused variable warnings and to ensure that expr is valid code.
//
// We do not evaluate expr at runtime, though (the compiler optimizes it away),
// hence we also never use any strings that might get passed to ION_DCHECK
// using operator<< and we can discard them by passing them into a NullLogger.
// Using a NullLogger instead of a regular Logger results in fewer instructions
// after preprocessing, and increases the likelihood for small functions to be
// inlined in compilers that are sensitive to that (such as Visual C++).
//
// An efficient way to achieve this is to let a call such as
//   DCHECK(expr) << "foo"
//
// to be expanded to
//   (true) ?
//   (void)(expr) :
//   ::ion::base::logging_internal::NullLogger().GetStream() << "foo"
//
// This ensures three things:
//   (1) expr gets compiled, but its generated code gets thrown away.
//   (2) Callers can still pass in a debug string that then gets thrown away.
//   (3) The entire expression is a no-op.
//
// Some toolchains do not support the ternary operator syntax since they require
// the second and third operand to be of the same type. Hence by default we use
// an if/else clause instead. Visual Studio seems to have a higher likelihood of
// inlining for small functions that use ION_DCHECKs if they expand to the
// ternary operator, so we use the ternary operator on Windows.
#if defined(ION_PLATFORM_WINDOWS)
#define ION_DCHECK(expr)                                                    \
  (true) ?                                                                  \
  (void)(expr) :                                                            \
  ::ion::base::logging_internal::NullLogger().GetStream()
#else  // defined(ION_PLATFORM_WINDOWS)
#define ION_DCHECK(expr)                                                    \
  if (true || (false && (expr)))                                            \
    ;                                                                       \
  else                                                                      \
    ::ion::base::logging_internal::NullLogger().GetStream()
#endif  // defined(ION_PLATFORM_WINDOWS)
#endif  // ION_DEBUG

#define ION_CHECK_OP(op, val1, val2)                                        \
  ION_CHECK((val1) op (val2)) << "(" << (val1) << " " << #op  /* NOLINT */  \
                              << " " << (val2) << ")\n"
#define ION_DCHECK_OP(op, val1, val2)                                       \
  ION_DCHECK((val1) op (val2)) << "(" << (val1) << " " << #op  /* NOLINT */ \
                               << " " << (val2) << ")\n"
// @}

// @name CHECK and DCHECK macros
//
// These macros will assert that the value they are given satisfies some
// predicate (for CHECK, the expression must evaluate to true). If it does not,
// the process will be forcibly killed.  In the DCHECK variants, the check will
// only be performed in debug mode.
//
// Warning: Do not place code with side effects inside a DCHECK, or the
// behavior of your program will differ between debug and release mode!
// @{
#define CHECK(expr) ION_CHECK(expr)
#define CHECK_EQ(val1, val2) ION_CHECK_OP(==, val1, val2)  // NOLINT
#define CHECK_NE(val1, val2) ION_CHECK_OP(!=, val1, val2)  // NOLINT
#define CHECK_LE(val1, val2) ION_CHECK_OP(<=, val1, val2)  // NOLINT
#define CHECK_LT(val1, val2) ION_CHECK_OP(<, val1, val2)   // NOLINT
#define CHECK_GE(val1, val2) ION_CHECK_OP(>=, val1, val2)  // NOLINT
#define CHECK_GT(val1, val2) ION_CHECK_OP(>, val1, val2)   // NOLINT

#define DCHECK(expr) ION_DCHECK(expr)
#define DCHECK_EQ(val1, val2) ION_DCHECK_OP(==, val1, val2)  // NOLINT
#define DCHECK_NE(val1, val2) ION_DCHECK_OP(!=, val1, val2)  // NOLINT
#define DCHECK_LE(val1, val2) ION_DCHECK_OP(<=, val1, val2)  // NOLINT
#define DCHECK_LT(val1, val2) ION_DCHECK_OP(<, val1, val2)   // NOLINT
#define DCHECK_GE(val1, val2) ION_DCHECK_OP(>=, val1, val2)  // NOLINT
#define DCHECK_GT(val1, val2) ION_DCHECK_OP(>, val1, val2)   // NOLINT

namespace ion {
namespace base {
namespace logging_internal {

// Helpers for CHECK_NOTNULL(). Two are necessary to support both raw pointers
// and smart pointers.
template <typename T>
T& CheckNotNullCommon(const char* expr_string, T& t) {  // NOLINT
  if (t == NULL) {
    ION_LOG(FATAL) << Logger::CheckMessage("CHECK_NOTNULL", expr_string);
  }
  return t;
}

template <typename T>
T* CheckNotNull(const char* expr_string, T* t) {  // NOLINT
  return CheckNotNullCommon(expr_string, t);
}

template <typename T>
T& CheckNotNull(const char* expr_string, T& t) {  // NOLINT
  return CheckNotNullCommon(expr_string, t);
}

// This extra overload is needed to support non-const rvalues of non-pointer
// type.
//
// For example, it enables the following use-case:
//   CHECK_NOTNULL(make_scoped_ptr<T>(MakeT()));
//
// Without this overload, the above code wouldn't compile.
template <typename T>
const T& CheckNotNull(const char* expr_string, const T& t) {  // NOLINT
  return CheckNotNullCommon(expr_string, t);
}

}  // namespace logging_internal
}  // namespace base
}  // namespace ion

// Check that the input is not NULL.  Unlike other CHECK macros, this one
// returns val, so it can be used in initializer lists.  Outside initializers,
// prefer CHECK.
#define CHECK_NOTNULL(val) \
  ::ion::base::logging_internal::CheckNotNull(\
      "'" #val "' Must be non NULL", (val))

// @}

#endif  // ION_BASE_LOGGING_H_
