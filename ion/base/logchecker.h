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

#ifndef ION_BASE_LOGCHECKER_H_
#define ION_BASE_LOGCHECKER_H_

#include <sstream>
#include <string>
#include <vector>

#include "ion/base/logging.h"

namespace ion {
namespace base {

// The LogChecker class can be used inside unit tests to trap all log output
// and verify that it matches what is expected. The destructor will log an
// error (to the regular stream) that no unexpected messages were logged, but
// it is up to the client to test for this.
//
// For example, if you have a test that is known to generate two log messages,
// you can set up the test like this:
//
//   TEST(MyTest, Something) {
//     LogChecker log_checker;
//
//     // This generates an error that contains the substring "invalid type".
//     SomeFunction("hello");
//     EXPECT_TRUE(log_checker.HasMessage("ERROR", "invalid type"));
//
//     // This generates a warning that contains the substring "zero size".
//     SomeOtherFunction(0);
//     EXPECT_TRUE(log_checker.HasMessage("WARNING", "zero size"));
//
//     // The log_checker goes out of scope here and will log an error if
//     // other messages were logged. It is good practice for the test to fail
//     // in that case by checking explicitly.
//     EXPECT_FALSE(log_checker.HasAnyMessages());
//   }
class ION_API LogChecker : public port::LogEntryWriter {
 public:
  // The constructor sets up to trap all log output.
  LogChecker();

  // The destructor logs an error if unexpected messages were logged, and
  // unconditionally restores the previous log writer.
  ~LogChecker() override;

  // LogEntryWriter impl.
  void Write(port::LogSeverity severity, const std::string& message)
      override;

  // Returns true if exactly one message was logged since the LogChecker was
  // constructed or since the last call to HasMessage(). The message must be of
  // the given severity and must contain the given substring. If this returns
  // true, it clears the stream for the next call. If it returns false, it
  // leaves the stream alone so the caller can use GetLogString() to examine
  // the log.
  bool HasMessage(const std::string& severity_string,
                  const std::string& substring);

  // Returns true if no message of the given severity and containing the given
  // substring was logged since the LogChecker was constructed or since the last
  // call to HasMessage(). This leaves the stream alone so the caller can use
  // GetLogString() to examine the log.
  bool HasNoMessage(const std::string& severity_string,
                    const std::string& substring);

  // Returns true if any messages were logged since the instance was
  // constructed or since the last call to CheckLog().
  bool HasAnyMessages() const {
    return !GetLogString().empty();
  }

  // This function is useful for testing code that produces more than one log
  // message at a time. It returns a vector containing all message strings
  // (split at newlines) logged since the LogChecker was constructed or the log
  // was last cleared. This clears the log afterwards.
  const std::vector<std::string> GetAllMessages();

  // Clears any messages that may be in the log.
  void ClearLog() {
    stream_.str("");
  }

  // Returns a string containing all current logged messages.
  const std::string GetLogString() const {
    return stream_.str();
  }

 private:
  port::LogEntryWriter* previous_writer_;
  std::ostringstream stream_;
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_LOGCHECKER_H_
