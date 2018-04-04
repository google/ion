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

#include <Foundation/Foundation.h>

namespace {

// A logger class that passes log messages to NSLog.
class NsLogEntryWriter : public ion::port::LogEntryWriter {
 public:
  NsLogEntryWriter() {}
  ~NsLogEntryWriter() override {}

  // LogEntryWriter impl.
  void Write(ion::port::LogSeverity severity,
             const std::string& message) override {
    NSString* severityName = [NSString stringWithUTF8String:GetSeverityName(severity)];
    NSString* errorMessage = [NSString stringWithUTF8String:message.c_str()];
    NSLog(@"%@ %@\n", severityName, errorMessage);
  }
};

}  // namespace

ion::port::LogEntryWriter* ion::port::CreateDefaultLogEntryWriter() {
  return new NsLogEntryWriter();
}

void ion::port::SetLoggingTag(const char* tag) {}
