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

#include "ion/base/logging.h"

#include <functional>
#include <map>
#include <set>

#include "ion/base/staticsafedeclare.h"
#include "ion/port/atomic.h"
#include "ion/port/break.h"
#include "ion/port/logging.h"
#include "ion/port/timer.h"

namespace ion {
namespace base {

namespace logging_internal {

// Stores the LogEntryWriter set by the user.
static port::LogEntryWriter* s_writer_ = NULL;

// Stores the break handler that gets invoked by Logger::CheckMessage.
static std::function<void()> s_break_handler_;

static const std::function<void()> GetBreakHandler() {
  static std::atomic<int> has_been_initialized_(0);
  if (has_been_initialized_.exchange(1) == 0) {
    RestoreDefaultBreakHandler();
  }
  return s_break_handler_;
}

static port::Mutex* GetSingleLoggerMutex() {
  ION_DECLARE_SAFE_STATIC_POINTER(port::Mutex, mutex);
  return mutex;
}

static std::set<std::string>& GetSingleLoggerMessageSet() {
  ION_DECLARE_SAFE_STATIC_POINTER(std::set<std::string>, logged_messages);
  return *logged_messages;
}

static void BreakOnFatalSeverity(port::LogSeverity severity) {
  bool is_fatal = (severity == port::FATAL);
#if ION_DEBUG
  is_fatal |= (severity == port::DFATAL);
#endif
  if (is_fatal) {
    // Call the break handler unless it has specifically been set to NULL
    // using SetBreakHandler(NULL).
    std::function<void()> break_handler = GetBreakHandler();
    if (break_handler) {
      break_handler();
    }
  }
}

static bool HasLoggedMessageSince(const char* file_name, int line_number,
                                  float past_seconds) {
  ION_DECLARE_SAFE_STATIC_POINTER(port::Mutex, mutex);
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

  LockGuard guard(mutex);
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

Logger::Logger(const char* filename, int line_number,
               port::LogSeverity severity)
    : severity_(severity) {
  stream_ << "[" << filename << ":" << line_number << "] ";
}

Logger::~Logger() {
  ION_DECLARE_SAFE_STATIC_POINTER(port::Mutex, mutex);
  {
    LockGuard guard(mutex);
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
  static std::ostream null_stream(NULL);
  return null_stream;
}

std::ostream& Logger::GetStream() { return stream_; }

const std::string Logger::CheckMessage(const char* check_string,
                                       const char* expr_string) {
  return std::string(check_string) + " failed: expression='" + expr_string +
         "' ";
}

static std::ostream& GetNullStream() {
  static std::ostream null_stream(NULL);
  return null_stream;
}

SingleLogger::SingleLogger(const char* file_name, int line_number,
                           port::LogSeverity severity)
    : logger_(HasLoggedMessageAt(file_name, line_number)
                  ? NULL
                  : new Logger(file_name, line_number, severity)) {}

SingleLogger::~SingleLogger() {}

void SingleLogger::ClearMessages() {
  LockGuard guard(GetSingleLoggerMutex());
  GetSingleLoggerMessageSet().clear();
}

std::ostream& SingleLogger::GetStream() {
  return logger_ ? logger_->GetStream() : GetNullStream();
}

bool SingleLogger::HasLoggedMessageAt(const char* file_name, int line_number) {
  LockGuard guard(GetSingleLoggerMutex());
  std::set<std::string>& logged_messages = GetSingleLoggerMessageSet();
  std::stringstream str;
  str << file_name << ":" << line_number;
  return !(logged_messages.insert(str.str()).second);
}

ThrottledLogger::ThrottledLogger(const char* file_name, int line_number,
                                 port::LogSeverity severity, float seconds)
    : logger_(HasLoggedMessageSince(file_name, line_number, seconds)
                  ? NULL
                  : new Logger(file_name, line_number, severity)) {}

ThrottledLogger::~ThrottledLogger() {}

std::ostream& ThrottledLogger::GetStream() {
  return logger_ ? logger_->GetStream() : GetNullStream();
}

}  // namespace logging_internal

//-----------------------------------------------------------------------------
// Public functions.

void SetLogEntryWriter(port::LogEntryWriter* w) {
  logging_internal::s_writer_ = w;
}

port::LogEntryWriter* GetLogEntryWriter() {
  return logging_internal::s_writer_ ? logging_internal::s_writer_
                                     : GetDefaultLogEntryWriter();
}

port::LogEntryWriter* GetDefaultLogEntryWriter() {
  ION_DECLARE_SAFE_STATIC_POINTER_WITH_CONSTRUCTOR(
      port::LogEntryWriter, default_writer,
      (port::CreateDefaultLogEntryWriter()));

  return default_writer;
}

void SetBreakHandler(const std::function<void()>& break_handler) {
  // Ensure the break handler has been initialized.
  logging_internal::GetBreakHandler();
  logging_internal::s_break_handler_ = break_handler;
}

void RestoreDefaultBreakHandler() {
  logging_internal::s_break_handler_ = port::BreakOrAbort;
}

}  // namespace base
}  // namespace ion
