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

#ifndef ION_PORT_LOGGING_H_
#define ION_PORT_LOGGING_H_

#include <string>

// This appears to be a macro in some versions of Visual Studio.
#ifdef ERROR
#  undef ERROR
#endif

namespace ion {
namespace port {

enum LogSeverity {
  INFO,
  WARNING,
  ERROR,
  FATAL,
  DFATAL,
};

// Abstract class which can be overridden to integrate Ion logging with other
// logging systems.
class ION_API LogEntryWriter {
 public:
  virtual ~LogEntryWriter() {}

  virtual void Write(LogSeverity severity, const std::string& message) = 0;

 protected:
  // Convenient way to map a severity-code to a printable represenation.
  const char* GetSeverityName(LogSeverity severity) const;
};

// Instantiate a *new* LogEntryWriter of the default type for the current
// platform... don't call this repeatedly or you'll leak memory!
// Note that this function must be safe to execute before the entry to main(),
// and thus the constructor must not perform any nontrivial initialization.
ION_API LogEntryWriter* CreateDefaultLogEntryWriter();

// Sets the tag prepended to all logging messages on supported platforms. This
// function is not guaranteed to be thread-safe and should not be called while
// another thread is writing a log message.
ION_API void SetLoggingTag(const char* tag);

}  // namespace port
}  // namespace ion

#endif  // ION_PORT_LOGGING_H_
