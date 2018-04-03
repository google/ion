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

#include "ion/base/nulllogentrywriter.h"

#include "ion/base/logchecker.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

TEST(NullLogEntryWriter, Basic) {
  ion::base::LogChecker log_checker;
  EXPECT_FALSE(log_checker.HasAnyMessages());

  LOG(ERROR) << "This is an error";
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "This is"));
  EXPECT_FALSE(log_checker.HasAnyMessages());
  {
    ion::base::NullLogEntryWriter null_logger;
    LOG(ERROR) << "This is an error";
    EXPECT_FALSE(log_checker.HasAnyMessages());
  }

  LOG(WARNING) << "A warning with some stuff in it";
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "some stuff"));
  EXPECT_FALSE(log_checker.HasAnyMessages());
  {
    ion::base::NullLogEntryWriter null_logger;
    LOG(WARNING) << "A warning with some stuff in it";
    EXPECT_FALSE(log_checker.HasAnyMessages());
  }
  ion::base::NullLogEntryWriter null_logger;

  LOG(ERROR) << "Another error";
#if !ION_PRODUCTION
  EXPECT_DEATH_IF_SUPPORTED(LOG(FATAL) << "Fatal error", "");
  EXPECT_DEATH_IF_SUPPORTED(LOG(DFATAL) << "Fatal error", "");
#endif
  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST(NullLogEntryWriter, UninstallsWhenDestroyed) {
  {
    ion::base::NullLogEntryWriter logger;
    EXPECT_EQ(&logger, ion::base::GetLogEntryWriter());
  }
  EXPECT_EQ(ion::base::GetDefaultLogEntryWriter(),
            ion::base::GetLogEntryWriter());
}
