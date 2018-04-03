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

#include <iostream>  // NOLINT
#include <sstream>

#include "ppapi/c/ppb_console.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

#include "ion/port/logging.h"

namespace pp {
class Instance;
}  // namespace pp

namespace {

static PP_LogLevel GetPPLogLevelFromSeverity(ion::port::LogSeverity severity) {
  switch (severity) {
    case ion::port::INFO:
      return PP_LOGLEVEL_LOG;
    case ion::port::WARNING:
      return PP_LOGLEVEL_WARNING;
    case ion::port::ERROR:
    case ion::port::FATAL:
    case ion::port::DFATAL:
      return PP_LOGLEVEL_ERROR;
  }
}

// A logger class that pipes messages logged using the LOG macros to the Chrome
// developer console.
class NaClLogEntryWriter : public ion::port::LogEntryWriter {
 public:
  NaClLogEntryWriter() {}
  virtual ~NaClLogEntryWriter() {}

  // LogEntryWriter impl.
  virtual void Write(ion::port::LogSeverity severity,
                     const std::string& message) {
    // Don't cache the module so we aren't dependent on initialization order.
    pp::Module* module = pp::Module::Get();
    if (module) {
      PP_LogLevel level = GetPPLogLevelFromSeverity(severity);
      const pp::Module::InstanceMap& instances = module->current_instances();
      for (pp::Module::InstanceMap::const_iterator iter = instances.begin();
           iter != instances.end(); ++iter) {
        pp::Instance* instance = iter->second;
        if (instance) {
          instance->LogToConsole(level, message);
        }
      }
    }

    // Also log to stderr.
    std::cerr << GetSeverityName(severity) << " " << message << std::endl;
  }
};

}  // namespace

ion::port::LogEntryWriter* ion::port::CreateDefaultLogEntryWriter() {
  return new NaClLogEntryWriter();
}

void ion::port::SetLoggingTag(const char* tag) {}
