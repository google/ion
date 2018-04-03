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
#include "third_party/googletest/googletest/include/gtest/gtest.h"

// Logging is disabled in production mode.
#if !ION_PRODUCTION
TEST(LogChecker, Basic) {
  ion::base::LogChecker log_checker;
  EXPECT_FALSE(log_checker.HasAnyMessages());

  LOG(ERROR) << "This is an error";
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "This is"));
  EXPECT_FALSE(log_checker.HasAnyMessages());

  LOG(WARNING) << "A warning with some stuff in it";
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "some stuff"));
  EXPECT_FALSE(log_checker.HasAnyMessages());

  LOG(ERROR) << "Another error";
  // Bad severity or string matches should leave the message alone.
  EXPECT_FALSE(log_checker.HasMessage("ERROR", "another"));
  EXPECT_FALSE(log_checker.HasMessage("WARNING", "Another"));
  EXPECT_FALSE(log_checker.HasNoMessage("ERROR", "Another"));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Another"));
  EXPECT_TRUE(log_checker.HasNoMessage("ERROR", "another"));
  EXPECT_TRUE(log_checker.HasNoMessage("WARNING", "Another"));
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // Note there is no message since we are using a LogChecker.
  EXPECT_DEATH_IF_SUPPORTED(LOG(FATAL) << "Fatal error", "");
  // With EXPECT_DEATH, we will get no messages.
  EXPECT_FALSE(log_checker.HasMessage("ERROR", "Fatal"));
  EXPECT_FALSE(log_checker.HasMessage("FATAL", "fatal"));
  EXPECT_FALSE(log_checker.HasMessage("FATAL", "Fatal"));
  EXPECT_TRUE(log_checker.HasNoMessage("FATAL", "Fatal"));
  EXPECT_TRUE(log_checker.HasNoMessage("ERROR", "Fatal"));
  EXPECT_TRUE(log_checker.HasNoMessage("FATAL", "fatal"));

  EXPECT_DEATH_IF_SUPPORTED(LOG(DFATAL) << "DFatal error", "");
  // With EXPECT_DEATH, we will get no messages.
  EXPECT_FALSE(log_checker.HasMessage("ERROR", "Fatal"));
  EXPECT_FALSE(log_checker.HasMessage("FATAL", "fatal"));
  EXPECT_FALSE(log_checker.HasMessage("DFATAL", "DFatal"));
  EXPECT_TRUE(log_checker.HasNoMessage("DFATAL", "DFatal"));
  EXPECT_TRUE(log_checker.HasNoMessage("ERROR", "Fatal"));
  EXPECT_TRUE(log_checker.HasNoMessage("FATAL", "fatal"));

  const int* null_int_ptr = nullptr;
  const int* null_int_ptr_result = nullptr;
  EXPECT_DEATH_IF_SUPPORTED(null_int_ptr_result = CHECK_NOTNULL(null_int_ptr),
                            "");
  EXPECT_FALSE(log_checker.HasMessage("ERROR", "NOTNULL"));
  EXPECT_FALSE(log_checker.HasMessage("FATAL", "Notnull"));
  EXPECT_FALSE(log_checker.HasMessage("FATAL", "NOTNULL"));
  EXPECT_TRUE(log_checker.HasNoMessage("FATAL", "NOTNULL"));
  EXPECT_TRUE(log_checker.HasNoMessage("ERROR", "NOTNULL"));
  EXPECT_TRUE(log_checker.HasNoMessage("FATAL", "Notnull"));
  EXPECT_TRUE(null_int_ptr_result == nullptr);
}

TEST(LogChecker, GetAllMessages) {
  ion::base::LogChecker log_checker;
  EXPECT_TRUE(log_checker.GetAllMessages().empty());

  LOG(ERROR) << "This is a single error";
  std::vector<std::string> messages;
  messages = log_checker.GetAllMessages();
  EXPECT_EQ(1U, messages.size());
  EXPECT_TRUE(ion::base::EndsWith(messages[0], "This is a single error"));
  EXPECT_FALSE(log_checker.HasAnyMessages());

  LOG(ERROR) << "Error 1";
  LOG(ERROR) << "Error 2";
  LOG(ERROR) << "Error 3";
  messages = log_checker.GetAllMessages();
  EXPECT_EQ(3U, messages.size());
  EXPECT_TRUE(ion::base::EndsWith(messages[0], "Error 1"));
  EXPECT_TRUE(ion::base::EndsWith(messages[1], "Error 2"));
  EXPECT_TRUE(ion::base::EndsWith(messages[2], "Error 3"));
  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST(LogChecker, HasSecondMessage) {
  ion::base::LogChecker log_checker;
  EXPECT_TRUE(log_checker.GetAllMessages().empty());

  LOG(INFO) << "Message 1";
  LOG(WARNING) << "Message 2";
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "Message 2"));
  EXPECT_FALSE(log_checker.HasAnyMessages());

  LOG(INFO) << "Message 1";
  LOG(WARNING) << "Message 2";
  EXPECT_FALSE(log_checker.HasMessage("WARNING", "Message 1"));
  EXPECT_FALSE(log_checker.HasMessage("INFO", "Message 2"));
  EXPECT_TRUE(log_checker.HasMessage("INFO", "Message 1"));
  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST(LogChecker, DestroyedWithMessages) {
  // Test that destroying a LogChecker when it contains error messages will
  // produce an error message. This uses an outer LogChecker to trap the
  // message produced by the inner one.
  ion::base::LogChecker outer_log_checker;
  {
    ion::base::LogChecker inner_log_checker;
    LOG(ERROR) << "Untracked error";
  }
  EXPECT_TRUE(outer_log_checker.HasMessage("ERROR", "destroyed with messages"));
}
#endif

TEST(LogChecker, UninstallsWhenDestroyed) {
  {
    ion::base::LogChecker checker;
    EXPECT_EQ(&checker, ion::base::GetLogEntryWriter());
  }
  EXPECT_EQ(ion::base::GetDefaultLogEntryWriter(),
            ion::base::GetLogEntryWriter());
}
