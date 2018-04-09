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

#ifndef ION_BASE_NULLLOGENTRYWRITER_H_
#define ION_BASE_NULLLOGENTRYWRITER_H_

#include <ostream>  // NOLINT

#include "ion/base/logging.h"

namespace ion {
namespace base {

//
// A NullLogEntryWriter can be used completely disable all logging
// programmatically as long as it exists. In this way it is similar to a
// LogChecker, but does not actually send log messages anywhere. Note that this
// does NOT prevent FATAL, DFATAL, or DCHECKs from calling the currently
// installed break handler.
//
// You can disable logging for a scope like this:
//   {
//     NullLogEntryWriter null_logger;
//     ...  // Very verbose code.
//   }  // Old writer is restored when the null logger passes out of scope.
//
// Alternatively, managing the life of the instance can disable logging for
// longer periods, for example:
//     #if ION_PRODUCTION
//     std::unique_ptr<NullLogEntryWriter> null_logger(new NullLogEntryWriter);
//     #endif
// could be used to completely disable logging in an application when building
// for production mode.
//
class ION_API NullLogEntryWriter : public port::LogEntryWriter {
 public:
  NullLogEntryWriter() : previous_writer_(GetLogEntryWriter()) {
    SetLogEntryWriter(this);
  }
  ~NullLogEntryWriter() override {
    // Restore the old log-writer.
    SetLogEntryWriter(previous_writer_);
  }

  // LogEntryWriter impl.
  void Write(port::LogSeverity severity, const std::string& message) override {}

 private:
  port::LogEntryWriter* previous_writer_;
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_NULLLOGENTRYWRITER_H_
