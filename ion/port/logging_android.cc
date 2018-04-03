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

#include "ion/port/logging.h"

#include <android/log.h>
#include <cstdio>
#include <vector>

namespace {

// Customized from ion/base/stringutils SplitStringWithoutSkipping.
std::vector<std::string> SplitStringOnLineBreaks(const std::string& str) {
  std::vector<std::string> strings;

  size_t end_pos = 0;
  const size_t length = str.length();
  while (end_pos != std::string::npos && end_pos < length) {
    const size_t start_pos = end_pos;
    // Find the end of the string of non-delimiter characters.
    end_pos = str.find_first_of('\n', start_pos);

    strings.push_back(str.substr(start_pos, end_pos - start_pos));
    // Move to the next character if we are not already at the end of the
    // string.
    if (end_pos != std::string::npos)
      end_pos++;
  }
  return strings;
}

class AndroidLogEntryWriter : public ion::port::LogEntryWriter {
 public:
  AndroidLogEntryWriter() {}
  virtual ~AndroidLogEntryWriter() {}

  // LogEntryWriter impl.
  virtual void Write(ion::port::LogSeverity severity,
                     const std::string& message) {
    android_LogPriority priority;
    switch (severity) {
      default:
      case ion::port::INFO:
        priority = ANDROID_LOG_INFO;
        break;
      case ion::port::WARNING:
        priority = ANDROID_LOG_WARN;
        break;
      case ion::port::ERROR:
        priority = ANDROID_LOG_ERROR;
        break;
      case ion::port::FATAL:
        priority = ANDROID_LOG_FATAL;
        break;
      case ion::port::DFATAL:
        priority = ANDROID_LOG_FATAL;
        break;
    }

    // Split message at line breaks to avoid truncated output.
    const std::vector<std::string> lines = SplitStringOnLineBreaks(message);
    for (auto it = lines.begin(); it < lines.end(); ++it)
      __android_log_write(priority, tag_, it->c_str());
    // Also write to stderr for terminal applications. Note: use stderr instead
    // of std::cerr here to avoid potential problems with static initialization
    // order when called before main().
    fprintf(stderr, "%s %s\n", GetSeverityName(severity), message.c_str());
  }

  // Sets the logging tag.  Android supports a maximum of 23 characters.
  static void SetTag(const char* tag) {
    strncpy(tag_, tag, 23U);
  }

 private:
  static char tag_[24];
};

char AndroidLogEntryWriter::tag_[24] = "Ion";

}  // namespace

ion::port::LogEntryWriter* ion::port::CreateDefaultLogEntryWriter() {
  return new AndroidLogEntryWriter();
}

void ion::port::SetLoggingTag(const char* tag) {
  // Do not allow nullptr tags.
  if (tag) AndroidLogEntryWriter::SetTag(tag);
}

