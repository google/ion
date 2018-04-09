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

#include "ion/port/timer.h"

#if defined(ION_PLATFORM_WINDOWS)
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <ctime>

#include "third_party/googletest/googletest/include/gtest/gtest.h"

TEST(Timer, GetTimes) {
  ion::port::Timer timer;

  EXPECT_GE(timer.GetInMs(), 0.);
  EXPECT_GE(timer.GetInS(), 0.);

  // Check that timer checks wall clock time by sleeping.
  timer.Reset();
  ion::port::Timer::SleepNSeconds(1);

  // Test GetInS() method.
  double time_seconds = timer.GetInS();
  EXPECT_GT(time_seconds, 0.9);

  timer.Reset();
  ion::port::Timer::SleepNSeconds(1);

  time_seconds = timer.GetInS();
  EXPECT_GT(time_seconds, 0.9);

  // Test GetInMs() method.
  timer.Reset();
  ion::port::Timer::SleepNSeconds(1);

  const double time_milliseconds = timer.GetInMs();
  EXPECT_GT(time_milliseconds, 900.0);
}

// Tests that the epoch used by std::chrono::system_clock is the Unix epoch of
// 01 January 1970, as this is an assumption made in many places by Ion users.
// We convert the date of the Unix epoch to a
// std::chrono::system_clock::time_point, and then verify that the number of
// clock periods since the clock epoch is 0.
TEST(Timer, SystemClockEpochIsUnixEpoch) {
  // A std::tm representing the Unix epoch.
  std::tm unix_epoch_tm{};
  unix_epoch_tm.tm_mday = 1;
  unix_epoch_tm.tm_year = 70;
  unix_epoch_tm.tm_isdst = -1;

  // Convert |unix_epoch_tm| to a time since the (unspecified) system clock
  // epoch, in the local timezone.
  const std::time_t unix_epoch_time_t_local = std::mktime(&unix_epoch_tm);

  // Interpret |unix_epoch_time_t_local| as a std::tm representing the time, but
  // in UTC.  |unix_epoch_tm_utc| will then be offset by the difference between
  // UTC and the local timezone.
  std::tm unix_epoch_tm_utc = *std::gmtime(&unix_epoch_time_t_local);

  // Convert |unix_epoch_tm_utc| to a a time since the (unspecified) system
  // clock epoch.
  const std::time_t unix_epoch_time_t_utc = std::mktime(&unix_epoch_tm_utc);

  // Add the offset between local time and UTC to the computed local time Unix
  // epoch, to get the UTC Unix epoch as a time_t.
  const std::time_t unix_epoch_time_t =
      unix_epoch_time_t_local +
      (unix_epoch_time_t_local - unix_epoch_time_t_utc);

  // Convert this to std::chrono::system_clock::time_point.
  const std::chrono::system_clock::time_point unix_epoch_time_point =
      std::chrono::system_clock::from_time_t(unix_epoch_time_t);

  // Verify that the unix_epoch_time_point is our actual epoch.
  EXPECT_EQ(0, unix_epoch_time_point.time_since_epoch().count());
}

TEST(Timer, SleepNSeconds) {
  const ion::port::Timer::steady_clock::time_point t0 =
      ion::port::Timer::steady_clock::now();
  ion::port::Timer::SleepNSeconds(1);
  const ion::port::Timer::steady_clock::time_point t1 =
      ion::port::Timer::steady_clock::now();
  EXPECT_LE(std::chrono::milliseconds(900), t1 - t0);
}

TEST(Timer, SleepNMilliseconds) {
  const ion::port::Timer::steady_clock::time_point t0 =
      ion::port::Timer::steady_clock::now();
  ion::port::Timer::SleepNMilliseconds(100);
  const ion::port::Timer::steady_clock::time_point t1 =
      ion::port::Timer::steady_clock::now();
  EXPECT_LE(std::chrono::milliseconds(90), t1 - t0);
}

// These tests are specific only to test the correctness of the Windows
// implementation of a monotonic clock. Can be safely deleted once Windows
// moves back to the standard chrono implementations.
#if defined(ION_PLATFORM_WINDOWS)
TEST(Timer, PositiveTimestamp) {
  using Clock = ion::port::Timer::steady_clock;
  EXPECT_LE(0, Clock::now().time_since_epoch().count());
}

#endif

// 
