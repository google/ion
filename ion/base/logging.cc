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

#include "ion/base/logging.h"

#include <functional>
#include <map>
#include <mutex>  // NOLINT(build/c++11)
#include <set>

#include "ion/base/staticsafedeclare.h"
#include "ion/port/atomic.h"
#include "ion/port/break.h"
#include "ion/port/logging.h"
#include "ion/port/stacktrace.h"
#include "ion/port/timer.h"

namespace ion {
namespace base {

namespace logging_internal {
namespace {

// Stores the LogEntryWriter set by the user.
port::LogEntryWriter* s_writer = nullptr;

// Logging mutexes. Since std::mutex constructors are constexpr, they are
// effectively loaded in an already initialized state from the data segment of
// the binary, and their destructors run after any other statics.
std::mutex s_logger_mutex;
std::mutex s_message_set_mutex;
std::mutex s_logged_messages_mutex;

std::set<std::string>& GetSingleLoggerMessageSet() {
  ION_DECLARE_SAFE_STATIC_POINTER(std::set<std::string>, logged_messages);
  return *logged_messages;
}

void BreakOnFatalSeverity(port::LogSeverity severity) {
  bool is_fatal = (severity == port::FATAL);
#if ION_DEBUG
  is_fatal |= (severity == port::DFATAL);
#endif
  if (is_fatal) {
    // Log a stacktrace for debugging.
    {
      std::lock_guard<std::mutex> guard(s_logger_mutex);
      port::StackTrace stacktrace;
      GetDefaultLogEntryWriter()->Write(
          severity,
          "Dumping stack:\n" + stacktrace.GetSymbolString() + "\n");
    }

    port::BreakOrAbort();
  }
}

bool HasLoggedMessageSince(const char* file_name, int line_number,
                           float past_seconds) {
  const port::Timer::steady_clock::time_point now =
      port::Timer::steady_clock::now();
  const port::Timer::steady_clock::time_point when =
      now - std::chrono::duration_cast<port::Timer::steady_clock::duration>(
                std::chrono::duration<float>(past_seconds));
  static std::map<std::string, port::Timer::steady_clock::time_point>
      logged_messages;
  std::stringstream str;
  str << file_name << ":" << line_number;
  const std::string key = str.str();

  std::lock_guard<std::mutex> guard(s_logged_messages_mutex);
  const auto& insert_pair = logged_messages.insert(std::make_pair(key, now));
  if (insert_pair.second) {
    // Entry was inserted with a new timestamp.
    return false;
  } else {
    if (insert_pair.first->second >= when) {
      // There has been a log entry since |when|.  Skip the timestamp update.
      return true;
    }
    // There has not been a log entry since |when|.  Update the latest entry to
    // the |now| time.
    insert_pair.first->second = now;
    return false;
  }
}

}  // anonymous namespace

Logger::Logger(const char* filename, int line_number,
               port::LogSeverity severity)
    : severity_(severity) {
  stream_ << "[" << filename << ":" << line_number << "] ";
}

Logger::~Logger() {
  {
    std::lock_guard<std::mutex> guard(s_logger_mutex);
    GetLogEntryWriter()->Write(severity_, stream_.str());
  }

  // Having written the log entry, we now break (and perhaps abort) if the error
  // is sufficiently severe.
  BreakOnFatalSeverity(severity_);
}

NullLogger::NullLogger(port::LogSeverity severity) {
  BreakOnFatalSeverity(severity);
}

std::ostream& NullLogger::GetStream() {
  static std::ostream null_stream(nullptr);
  return null_stream;
}

std::ostream& Logger::GetStream() { return stream_; }

const std::string Logger::CheckMessage(const char* check_string,
                                       const char* expr_string) {
  return std::string(check_string) + " failed: expression='" + expr_string +
         "' ";
}

static std::ostream& GetNullStream() {
  static std::ostream null_stream(nullptr);
  return null_stream;
}

SingleLogger::SingleLogger(const char* file_name, int line_number,
                           port::LogSeverity severity)
    : logger_(HasLoggedMessageAt(file_name, line_number)
                  ? nullptr
                  : new Logger(file_name, line_number, severity)) {}

SingleLogger::~SingleLogger() {}

void SingleLogger::ClearMessages() {
  std::lock_guard<std::mutex> guard(s_message_set_mutex);
  GetSingleLoggerMessageSet().clear();
}

std::ostream& SingleLogger::GetStream() {
  return logger_ ? logger_->GetStream() : GetNullStream();
}

bool SingleLogger::HasLoggedMessageAt(const char* file_name, int line_number) {
  std::lock_guard<std::mutex> guard(s_message_set_mutex);
  std::set<std::string>& logged_messages = GetSingleLoggerMessageSet();
  std::stringstream str;
  str << file_name << ":" << line_number;
  return !(logged_messages.insert(str.str()).second);
}

ThrottledLogger::ThrottledLogger(const char* file_name, int line_number,
                                 port::LogSeverity severity, float seconds)
    : logger_(HasLoggedMessageSince(file_name, line_number, seconds)
                  ? nullptr
                  : new Logger(file_name, line_number, severity)) {}

ThrottledLogger::~ThrottledLogger() {}

std::ostream& ThrottledLogger::GetStream() {
  return logger_ ? logger_->GetStream() : GetNullStream();
}

void InitializeLogging() {
  GetSingleLoggerMessageSet();
  GetDefaultLogEntryWriter();
}

}  // namespace logging_internal

//-----------------------------------------------------------------------------
// Public functions.

void SetLogEntryWriter(port::LogEntryWriter* writer) {
  logging_internal::s_writer = writer;
}

port::LogEntryWriter* GetLogEntryWriter() {
  return logging_internal::s_writer ? logging_internal::s_writer
                                    : GetDefaultLogEntryWriter();
}

port::LogEntryWriter* GetDefaultLogEntryWriter() {
  ION_DECLARE_SAFE_STATIC_POINTER_WITH_CONSTRUCTOR(
      port::LogEntryWriter, default_writer,
      (port::CreateDefaultLogEntryWriter()));

  return default_writer;
}

}  // namespace base
}  // namespace ion
