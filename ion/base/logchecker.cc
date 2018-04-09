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

#include "ion/base/logchecker.h"

#include "ion/base/stringutils.h"

namespace ion {
namespace base {

LogChecker::LogChecker()
    : previous_writer_(base::GetLogEntryWriter()) {
  SetLogEntryWriter(this);
}

LogChecker::~LogChecker() {
  // Restore the old log-writer.
  SetLogEntryWriter(previous_writer_);

  // Make sure there are no unexpected logged messages.
  if (HasAnyMessages()) {
    LOG(ERROR) << "LogChecker destroyed with messages: " << GetLogString();
  }
}

bool LogChecker::HasMessage(const std::string& severity_string,
                            const std::string& substring) {
#if ION_PRODUCTION
  // In production mode assume the checker has the given message.
  return true;
#else
  // The stream should start with the severity and contain the substring.
  const std::string log = GetLogString();
  const std::vector<std::string> messages = SplitString(log, "\n");
  const size_t count = messages.size();
  for (size_t i = 0; i < count; ++i) {
    // DCHECKs output the error message on the next line; treat them as a single
    // message.
    const bool check_dcheck = severity_string == "DFATAL" &&
        messages[i].find("DCHECK failed") != std::string::npos &&
        i < count - 1U;
    if ((messages[i].find(severity_string) == 0 &&
        messages[i].find(substring) != std::string::npos) ||
        (check_dcheck &&
         messages[i + 1U].find(substring) != std::string::npos)) {
      // Clear the stream for next time.
      ClearLog();
      return true;
    }
  }
  return false;
#endif
}

bool LogChecker::HasNoMessage(const std::string& severity_string,
                              const std::string& substring) {
#if ION_PRODUCTION
  // In production mode assume the checker lacks the given message.
  return true;
#else
  // No message in the stream should start with the severity and contain the
  // substring.
  const std::string log = GetLogString();
  const std::vector<std::string> messages = SplitString(log, "\n");
  const size_t count = messages.size();
  for (size_t i = 0; i < count; ++i) {
    if (messages[i].find(severity_string) == 0 &&
        messages[i].find(substring) != std::string::npos)
      return false;
  }
  return true;
#endif
}

const std::vector<std::string> LogChecker::GetAllMessages() {
  const std::string log = GetLogString();
  ClearLog();
  return SplitString(log, "\n");
}

void LogChecker::Write(port::LogSeverity severity, const std::string& message) {
  stream_ << GetSeverityName(severity) << " " << message << std::endl;
}

}  // namespace base
}  // namespace ion
