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

// Copyright 2007 Google Inc.
// Author: fbailly@google.com (Francois Bailly)
// Author: quarup@google.com (Quarup Barreirinhas)

#include <array>
#include <chrono>  // NOLINT
#include <cmath>
#include <ctime>
#include <string>

#include "base/integral_types.h"
#include "ion/base/datetime.h"
#include "ion/base/logchecker.h"
#include "absl/base/macros.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace {
// Tolerable relative error when interpolating dates.
const double kDateTimeTolerance = 1e-10;

// Number of nanoseconds in one second.
static const int kNanoSecondsPerSecond = 1000000000;
}  // anonymous namespace

namespace ion {
namespace base {

class TestableDateTime : public DateTime {
 public:
  TestableDateTime() : DateTime(), use_24_hour_time_(false) {}

  void SetUse24HourTime(bool enabled) { use_24_hour_time_ = enabled; }

  // Implements DateTime.
  bool Use24HourTime() const override { return use_24_hour_time_; }

 private:
  bool use_24_hour_time_;
};

// Simple data structure to hold input test cases and expected output
struct DateTimeTestInfo {
  // Input test data to init a DateTime instance
  int64 year;
  uint8 month;
  uint8 day;
  uint8 hour;
  uint8 minute;
  uint8 second;
  int8 zone_hours;
  int8 zone_minutes;
  // Expected string output
  const std::string expected_date_string;
  const std::string expected_time_string;
};

// This function performs a series of equality tests needed repeatedly for
// testing the ComputeDurationString function.
static void COHelper(const DateTime& a, const DateTime& b) {
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a > b);
  EXPECT_TRUE(a >= b);
  EXPECT_TRUE(b < a);
  EXPECT_TRUE(b <= a);
}

// This function is the analogue of std::mktime, but returning a std::time_t in
// UTC rather than the unspecified local timezone.
static std::time_t MktimeUtc(std::tm tm) {
  const std::time_t local_time_t = std::mktime(&tm);
  std::tm utc_tm = *std::gmtime(&local_time_t);
  const std::time_t delta_time_t = std::mktime(&utc_tm);

  return local_time_t + (local_time_t - delta_time_t);
}

TEST(DateTime, StdChronoSystemTimeConversion) {
  // The DateTime conversion from std::chrono::system_clock::time_point assumes
  // that the system_clock has the POSIX epoch of 01 January 1970.  Verify this
  // assumption.
  const std::time_t epoch_time_t = MktimeUtc({0, 0, 0, 1, 0, 70});
  const std::chrono::system_clock::time_point epoch_time_point =
      std::chrono::system_clock::from_time_t(epoch_time_t);
  EXPECT_EQ(0, epoch_time_point.time_since_epoch().count());

  // Verify conversion for a range of dates.  Note that dates before 1970 or
  // "too far in the future" are problematic or treated as error on some
  // platforms (Windows, OSX, iOS, Android).
  std::array<std::tm, 4> test_times = {{
      {0, 0, 0, 1, 0, 70},        // 01 January 1970
      {59, 59, 23, 31, 11, 70},   // 31 December 1970, 23:59:59
      {0, 0, 0, 1, 0, 71},        // 01 January 1971
      {59, 59, 23, 31, 11, 115},  // 31 December 2015, 23:59:59
  }};

  for (const std::tm& time_tm : test_times) {
    const std::time_t time_time_t = MktimeUtc(time_tm);
    const std::chrono::system_clock::time_point time_time_point =
        std::chrono::system_clock::from_time_t(time_time_t);
    DateTime time_date_time(time_time_point, 0, 0);

    // Verify that the broken-down date is correct.
    EXPECT_EQ(time_tm.tm_year + 1900, time_date_time.GetYear());
    EXPECT_EQ(time_tm.tm_mon + 1, time_date_time.GetMonth());
    EXPECT_EQ(time_tm.tm_mday, time_date_time.GetDay());
    EXPECT_EQ(time_tm.tm_hour, time_date_time.GetHour());
    EXPECT_EQ(time_tm.tm_min, time_date_time.GetMinute());
    EXPECT_EQ(time_tm.tm_sec, time_date_time.GetSecond());

    // Verify GetPosixSeconds() works.
    const int64 time_since_epoch =
        std::chrono::duration_cast<std::chrono::seconds>(
            time_time_point.time_since_epoch())
            .count();
    EXPECT_EQ(time_since_epoch, time_date_time.GetPosixSeconds<int64>());
  }
}

// Verify the different templated conversions to/from the POSIX epoch.
TEST(DateTime, TemplatedPosixSecondsConversions) {
  DateTime d;
  d = DateTime::CreateFromPosixSeconds<int>(-1);
  EXPECT_EQ(59, d.GetSecond());
  EXPECT_EQ(0U, d.GetNanosecond());
  EXPECT_EQ(-1, d.GetPosixSeconds<int>());
  EXPECT_EQ(-1.0, d.GetPosixSeconds<double>());

  d = DateTime::CreateFromPosixSeconds<int>(0);
  EXPECT_EQ(0, d.GetSecond());
  EXPECT_EQ(0U, d.GetNanosecond());
  EXPECT_EQ(0, d.GetPosixSeconds<int>());
  EXPECT_EQ(0.0, d.GetPosixSeconds<double>());

  d = DateTime::CreateFromPosixSeconds<int>(1);
  EXPECT_EQ(1, d.GetSecond());
  EXPECT_EQ(0U, d.GetNanosecond());
  EXPECT_EQ(1, d.GetPosixSeconds<int>());
  EXPECT_EQ(1.0, d.GetPosixSeconds<double>());

  d = DateTime::CreateFromPosixSeconds<double>(-1.000000001);
  EXPECT_EQ(58, d.GetSecond());
  EXPECT_EQ(999999999U, d.GetNanosecond());
  EXPECT_EQ(-2, d.GetPosixSeconds<int>());
  EXPECT_EQ(-1.000000001, d.GetPosixSeconds<double>());

  d = DateTime::CreateFromPosixSeconds<double>(-1.000000000);
  EXPECT_EQ(59, d.GetSecond());
  EXPECT_EQ(0U, d.GetNanosecond());
  EXPECT_EQ(-1, d.GetPosixSeconds<int>());
  EXPECT_EQ(-1.0, d.GetPosixSeconds<double>());

  d = DateTime::CreateFromPosixSeconds<double>(-0.999999999);
  EXPECT_EQ(59, d.GetSecond());
  EXPECT_EQ(1U, d.GetNanosecond());
  EXPECT_EQ(-1, d.GetPosixSeconds<int>());
  EXPECT_EQ(-0.999999999, d.GetPosixSeconds<double>());

  d = DateTime::CreateFromPosixSeconds<double>(0.999999999);
  EXPECT_EQ(0, d.GetSecond());
  EXPECT_EQ(999999999U, d.GetNanosecond());
  EXPECT_EQ(0, d.GetPosixSeconds<int>());
  EXPECT_EQ(0.999999999, d.GetPosixSeconds<double>());

  d = DateTime::CreateFromPosixSeconds<double>(1.000000000);
  EXPECT_EQ(1, d.GetSecond());
  EXPECT_EQ(0U, d.GetNanosecond());
  EXPECT_EQ(1, d.GetPosixSeconds<int>());
  EXPECT_EQ(1.000, d.GetPosixSeconds<double>());

  d = DateTime::CreateFromPosixSeconds<double>(1.000000001);
  EXPECT_EQ(1, d.GetSecond());
  EXPECT_EQ(1U, d.GetNanosecond());
  EXPECT_EQ(1, d.GetPosixSeconds<int>());
  EXPECT_EQ(1.000000001, d.GetPosixSeconds<double>());
}

// Different representations of zero seconds from the Posix epoch.
TEST(DateTime, PosixZeroSecondsRepresentation) {
  // Zero seconds at UTC:+0.
  const DateTime dtime1 = DateTime::CreateFromPosixSeconds(0, 0, 0);
  EXPECT_EQ(0LL, dtime1.GetPosixSeconds<int64>());

  // Zero seconds at UTC:+0.
  const DateTime dtime2(1970LL, 1, 1, 0, 0, 0, 0, 0, 0);
  EXPECT_EQ(0LL, dtime2.GetPosixSeconds<int64>());

  // Zero seconds in UTC:+1:30.
  const DateTime dtime3 = DateTime::CreateFromPosixSeconds(0, 1, 30);
  EXPECT_EQ(1, dtime3.GetZoneHours());
  EXPECT_EQ(30, dtime3.GetZoneMinutes());
  EXPECT_EQ(0LL, dtime3.GetPosixSeconds<int64>());

  // Zero seconds in UTC:-3:30.
  const DateTime dtime4 = DateTime::CreateFromPosixSeconds(0, -3, -30);
  EXPECT_EQ(-3, dtime4.GetZoneHours());
  EXPECT_EQ(-30, dtime4.GetZoneMinutes());
  EXPECT_EQ(0LL, dtime4.GetPosixSeconds<int64>());
}

TEST(DateTime, DateFieldSetters) {
  LogChecker log_checker;
  DateTime dtime;

  // The year field can take any int64 value, so no testing is necessary.

  // Month valid range is 0-12.
  dtime.SetMonth(0);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(0, dtime.GetMonth());
  dtime.SetMonth(12);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(12, dtime.GetMonth());

  // Test for error and no edit on invalid input.
  // SetMonth argument is a uint, so no test for <0.
  dtime.SetMonth(6);
  dtime.SetMonth(13);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Invalid month"));
  EXPECT_EQ(6, dtime.GetMonth());

  // Set the date to a non-leap year first.
  dtime.SetYear(399LL);

  // Test 31-day month (January).
  dtime.SetMonth(1);
  dtime.SetDay(0);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(0, dtime.GetDay());
  dtime.SetDay(31);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(31, dtime.GetDay());

  dtime.SetDay(15);
  dtime.SetDay(32);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Invalid day"));
  EXPECT_EQ(15, dtime.GetDay());

  // Test 30-day month (April).
  dtime.SetMonth(4);
  dtime.SetDay(0);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(0, dtime.GetDay());
  dtime.SetDay(30);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(30, dtime.GetDay());

  dtime.SetDay(16);
  dtime.SetDay(31);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Invalid day"));
  EXPECT_EQ(16, dtime.GetDay());

  // Test February on a non-leap year.
  dtime.SetMonth(2);
  dtime.SetDay(0);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(0, dtime.GetDay());
  dtime.SetDay(28);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(28, dtime.GetDay());

  dtime.SetDay(14);
  dtime.SetDay(29);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Invalid day"));
  EXPECT_EQ(14, dtime.GetDay());

  // Test February on a leap year.
  dtime.SetYear(400LL);
  dtime.SetDay(0);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(0, dtime.GetDay());
  dtime.SetDay(29);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(29, dtime.GetDay());

  dtime.SetDay(13);
  dtime.SetDay(30);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Invalid day"));
  EXPECT_EQ(13, dtime.GetDay());
}

TEST(DateTime, TimeFieldSetters) {
  LogChecker log_checker;
  DateTime dtime;

  // Test hour setter.
  dtime.SetHour(0);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(0, dtime.GetHour());
  dtime.SetHour(23);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(23, dtime.GetHour());

  dtime.SetHour(10);
  dtime.SetHour(24);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Invalid hour"));
  EXPECT_EQ(10, dtime.GetHour());

  // Test minute setter.
  dtime.SetMinute(0);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(0, dtime.GetMinute());
  dtime.SetMinute(59);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(59, dtime.GetMinute());

  dtime.SetMinute(32);
  dtime.SetMinute(60);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Invalid minute"));
  EXPECT_EQ(32, dtime.GetMinute());

  // Test second setter.
  dtime.SetSecond(0);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(0, dtime.GetSecond());
  dtime.SetSecond(59);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(59, dtime.GetSecond());

  dtime.SetSecond(28);
  dtime.SetSecond(60);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Invalid second"));
  EXPECT_EQ(28, dtime.GetSecond());

  // Test nanosecond setter.
  dtime.SetNanosecond(0);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(0U, dtime.GetNanosecond());
  dtime.SetNanosecond(kNanoSecondsPerSecond - 1);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(kNanoSecondsPerSecond - 1U, dtime.GetNanosecond());

  dtime.SetNanosecond(29999);
  dtime.SetNanosecond(kNanoSecondsPerSecond);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Invalid nanosecond"));
  EXPECT_EQ(29999U, dtime.GetNanosecond());
}

TEST(DateTime, TimeZoneFieldSetters) {
  LogChecker log_checker;
  DateTime dtime;

  // Time zone hour field.
  dtime.SetZoneHours(-12);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(-12, dtime.GetZoneHours());
  dtime.SetZoneHours(14);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(14, dtime.GetZoneHours());

  dtime.SetZoneHours(2);
  dtime.SetZoneHours(-13);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Invalid time zone hour"));
  EXPECT_EQ(2, dtime.GetZoneHours());
  dtime.SetZoneHours(15);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Invalid time zone hour"));
  EXPECT_EQ(2, dtime.GetZoneHours());

  // Time zone minute field.
  dtime.SetZoneMinutes(-59);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(-59, dtime.GetZoneMinutes());
  dtime.SetZoneMinutes(59);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(59, dtime.GetZoneMinutes());

  dtime.SetZoneMinutes(15);
  dtime.SetZoneMinutes(60);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Invalid time zone minute"));
  EXPECT_EQ(15, dtime.GetZoneMinutes());
  dtime.SetZoneMinutes(-60);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Invalid time zone minute"));
  EXPECT_EQ(15, dtime.GetZoneMinutes());
}

TEST(DateTime, ToStringBranches) {
  DateTime dtime(1LL, 1, 1, 0, 0, 0, 0, 0, 0);
  EXPECT_EQ(dtime.ToString(), "0001");

  dtime.SetYear(12345LL);
  EXPECT_EQ(dtime.ToString(), "12345");

  dtime.SetYear(2LL);
  dtime.SetMonth(2);
  EXPECT_EQ(dtime.ToString(), "0002-02");

  dtime.SetYear(3LL);
  dtime.SetMonth(3);
  dtime.SetDay(3);
  EXPECT_EQ(dtime.ToString(), "0003-03-03");

  dtime.Set(1LL, 1, 1, 1, 1, 1, 12345, 0, 0);
  EXPECT_EQ(dtime.ToString(), "0001-01-01T01:01:01.12345Z");

  dtime.SetZoneHours(1);
  EXPECT_EQ(dtime.ToString(), "0001-01-01T01:01:01.12345+01:00");

  dtime.SetZoneHours(-2);
  EXPECT_EQ(dtime.ToString(), "0001-01-01T01:01:01.12345-02:00");

  dtime.SetZoneMinutes(-10);
  EXPECT_EQ(dtime.ToString(), "0001-01-01T01:01:01.12345-02:10");

  dtime.SetZoneHours(3);
  dtime.SetZoneMinutes(25);
  EXPECT_EQ(dtime.ToString(), "0001-01-01T01:01:01.12345+03:25");

  dtime.SetZoneHours(0);
  EXPECT_EQ(dtime.ToString(), "0001-01-01T01:01:01.12345+00:25");

  dtime.SetZoneMinutes(-32);
  EXPECT_EQ(dtime.ToString(), "0001-01-01T01:01:01.12345-00:32");
}

TEST(DateTime, OutputStreamOperator) {
  std::stringstream ss;
  DateTime d(2015, 8, 21, 12, 0, 0, 0, -8, 0);
  ss << d;
  EXPECT_EQ(d.ToString(), ss.str());
}

TEST(DateTime, InputStreamOperator) {
  DateTime d1(2015, 8, 21, 12, 0, 0, 0, -8, 0);
  DateTime d2;
  std::stringstream ss1(d1.ToString());
  ss1 >> d2;
  EXPECT_TRUE(d1 == d2);

  // Test bad input is handled properly.
  std::stringstream ss2("foobar");
  DateTime d3;
  ss2 >> d3;
  EXPECT_FALSE(d1 == d3);
  EXPECT_TRUE(ss2.fail());
  ss2.clear(std::ios_base::goodbit);
  string s;
  ss2 >> s;
  EXPECT_EQ("foobar", s);
}

// Representations of times close to year zero.
TEST(DateTime, NearYearZeroRepresentation) {
  const int64 kYear1970Seconds = 62167219200LL;
  DateTime dtime;

  // Jan 1, 0 (0:00:52 at UTC:+1:20).
  dtime.Set(0LL, 1, 1, 0, 0, 52, 1, 20);
  EXPECT_EQ((-1LL * 60 - 20) * 60 + 52,
            dtime.GetPosixSeconds<int64>() + kYear1970Seconds);

  // Jan 1, 0 (1:10:31 at UTC-2:20).
  dtime.Set(0LL, 1, 1, 1, 10, 31, -2, -20);
  EXPECT_EQ(((1LL + 2) * 60 + 10 + 20) * 60 + 31,
            dtime.GetPosixSeconds<int64>() + kYear1970Seconds);

  // Jan 1, 1 (0:00:12 at UTC:-3:30).
  dtime.Set(1LL, 1, 1, 0, 0, 12, -3, -30);
  EXPECT_EQ(((366LL * 24 + 3) * 60 + 30) * 60 + 12,
            dtime.GetPosixSeconds<int64>() + kYear1970Seconds);

  // Jan 1, -1 (10:01:01 at UTC:+3:30).
  dtime.Set(-1LL, 1, 1, 10, 1, 1, 3, 30);
  EXPECT_EQ(((-365LL * 24 + 10 - 3) * 60 + 1 - 30) * 60 + 1,
            dtime.GetPosixSeconds<int64>() + kYear1970Seconds);

  // Dec 31, -1 (23:59:59 at UTC:+0).
  dtime.Set(-1LL, 12, 31, 23, 59, 59, 0, 0);
  EXPECT_EQ(-1LL, dtime.GetPosixSeconds<int64>() + kYear1970Seconds);
}

// Makes sure arithmetic with big times work.
TEST(DateTime, BigTimeArithmetic) {
  // February 29, 2008 (leap year) (23:12:32 at UTC:+0:30).
  const DateTime a(2008LL, 2, 29, 23, 12, 32, 0, 0, 30);
  // August 27, -4590284194 (14:29:02 at UTC:-5).
  const DateTime b(-4590284194LL, 8, 27, 14, 29, 2, 0, -5, 0);
  // October 12, 2013 (08:43:28 at UTC:-3:30).
  const DateTime c(2013LL, 10, 12, 8, 43, 28, 0, -3, -30);
  // July 12, -7992 (14:24:34 at UTC:0).
  const DateTime d(-7992LL, 7, 12, 14, 24, 34, 0, 0, 0);
  DateTime sum;

  sum = a;  // a
  EXPECT_EQ(a.GetPosixSeconds<int64>(), sum.GetPosixSeconds<int64>());
  sum += b.GetPosixSeconds<int64>();  // a + b
  EXPECT_EQ(a.GetPosixSeconds<int64>() + b.GetPosixSeconds<int64>(),
            sum.GetPosixSeconds<int64>());
  sum += c.GetPosixSeconds<int64>();  // a + b + c
  EXPECT_EQ(a.GetPosixSeconds<int64>() + b.GetPosixSeconds<int64>() +
                c.GetPosixSeconds<int64>(),
            sum.GetPosixSeconds<int64>());
  sum -= d.GetPosixSeconds<int64>();  // a + b + c - d
  EXPECT_EQ(a.GetPosixSeconds<int64>() + b.GetPosixSeconds<int64>() +
                c.GetPosixSeconds<int64>() - d.GetPosixSeconds<int64>(),
            sum.GetPosixSeconds<int64>());
  sum += a.GetPosixSeconds<int64>();  // 2*a + b + c - d
  EXPECT_EQ(2LL * a.GetPosixSeconds<int64>() + b.GetPosixSeconds<int64>() +
                c.GetPosixSeconds<int64>() - d.GetPosixSeconds<int64>(),
            sum.GetPosixSeconds<int64>());
  sum -= b.GetPosixSeconds<int64>();  // 2*a + c - d
  EXPECT_EQ(2LL * a.GetPosixSeconds<int64>() + c.GetPosixSeconds<int64>() -
                d.GetPosixSeconds<int64>(),
            sum.GetPosixSeconds<int64>());
  sum -= 2LL * a.GetPosixSeconds<int64>();  // c - d
  EXPECT_EQ(c.GetPosixSeconds<int64>() - d.GetPosixSeconds<int64>(),
            sum.GetPosixSeconds<int64>());
  sum -= c.GetPosixSeconds<int64>() - d.GetPosixSeconds<int64>();  // zero
  EXPECT_EQ(0LL, sum.GetPosixSeconds<int64>());
}

// Tests geologic times (e.g. 4 billion BC).
TEST(DateTime, GeologicTimes) {
  DateTime a;
  DateTime b;

  // March 1, -400000001 (10:03:10 UTC:+4:05).
  a.Set(-4000000001LL, 3, 1, 10, 3, 10, 4, 5);
  // March 1, -400000000 (10:03:10 UTC:+4:05).
  b.Set(-4000000000LL, 3, 1, 10, 3, 10, 4, 5);
  // Difference between a and b is exactly one leap year.
  EXPECT_EQ(366LL * 24 * 60 * 60,
            b.GetPosixSeconds<int64>() - a.GetPosixSeconds<int64>());
  a.SetYear(-3999999999LL);
  // Difference between a and b is exactly one regular year.
  EXPECT_EQ(365LL * 24 * 60 * 60,
            a.GetPosixSeconds<int64>() - b.GetPosixSeconds<int64>());
}

// Tests times in different time zones.
TEST(DateTime, AdjustingTimeZones) {
  DateTime a;
  DateTime b;

  // March 1, -400000001 (10:03:10 UTC:+4:05).
  a.Set(-4000000001LL, 3, 1, 10, 3, 10, 4, 5);
  EXPECT_EQ(4, a.GetZoneHours());
  EXPECT_EQ(5, a.GetZoneMinutes());

  // Convert to UTC:0.
  b = a;
  b.AdjustTimeZone(0, 0);  // Equivalent to Normalize() function.
  EXPECT_EQ(0, b.GetZoneHours());
  EXPECT_EQ(0, b.GetZoneMinutes());
  EXPECT_EQ(b.GetPosixSeconds<int64>(), a.GetPosixSeconds<int64>());

  // Convert to UTC:-5:30.
  b = a;
  b.AdjustTimeZone(-5, -30);
  EXPECT_EQ(-5, b.GetZoneHours());
  EXPECT_EQ(-30, b.GetZoneMinutes());
  EXPECT_TRUE(b == a);  // NOLINT: EXPECT_EQ breaks on Windows due to alignment

  // A clock shows a certain time at UTC:+4:05 exactly 4:05 hours before a
  // clock shows the same time at UTC:+0.
  b = a;
  b.SetZoneHours(0);
  b.SetZoneMinutes(0);
  EXPECT_EQ((4LL * 60 + 5) * 60,
            b.GetPosixSeconds<int64>() - a.GetPosixSeconds<int64>());
}

// Compares two dates (converted to seconds) with respect to a reference value.
// Relative error (with respect to reference) must be below given tolerance.
// Using EXPECT_DOUBLE_EQ is no good because it expects perfect precision
// when comparing to zero.
// Reference value cannot be zero.
#define EXPECT_INT64_NEAR(val1, val2, ref, tolerance)                       \
  EXPECT_NEAR(fabs(static_cast<double>(val1) - static_cast<double>(val2)) / \
                  fabs(static_cast<double>(ref)),                           \
              0.0,                                                          \
              tolerance)

// Expect that datetimes are near within a given number of
// seconds. Tolerance can be a double, allowing for nanosecond
// comparisons.
void ExpectDateTimesNear(const DateTime& a,
                         const DateTime& b,
                         double tolerance_secs) {
  // Convert a and b to seconds without rounding in the nanoseconds.
  int64 a_secs = a.GetPosixSeconds<int64>();
  int64 b_secs = b.GetPosixSeconds<int64>();

  double diff = static_cast<double>(a_secs - b_secs);
  diff += a.GetNanosecond() / static_cast<double>(kNanoSecondsPerSecond);
  diff -= b.GetNanosecond() / static_cast<double>(kNanoSecondsPerSecond);

  char buf[256];
  snprintf(buf,
           256,
           "Comparing DateTimes (%s)  and (%s)",
           a.ToString().c_str(),
           b.ToString().c_str());
  std::string test_string = buf;

  EXPECT_NEAR(diff, 0.0, tolerance_secs);
}

// Interpolating between two different times.
TEST(DateTime, InterpolatingTimes) {
  DateTime a;
  DateTime b;
  DateTime c;
  DateTime d;

  static const double kHalfSecond = 0.5;

  // March 1, -400000001 (10:03:10 UTC:+4:05).
  a.Set(-4000000001LL, 3, 1, 10, 3, 10, 4, 5);

  // January 1, 2000 (leap year) (01:01:01 at UTC:+5).
  b.Set(2000LL, 1, 1, 1, 1, 1, 5, 0);

  // Left endpoint.
  c = DateTime::Interpolate(a, b, 0.0);
  EXPECT_INT64_NEAR(a.GetPosixSeconds<int64>(), c.GetPosixSeconds<int64>(),
                    a.GetPosixSeconds<int64>() - b.GetPosixSeconds<int64>(),
                    kDateTimeTolerance);

  // Right endpoint.
  c = DateTime::Interpolate(a, b, 1.0);
  EXPECT_INT64_NEAR(b.GetPosixSeconds<int64>(), c.GetPosixSeconds<int64>(),
                    a.GetPosixSeconds<int64>() - b.GetPosixSeconds<int64>(),
                    kDateTimeTolerance);

  // Interpolating from a date to itself should always return itself.
  c = DateTime::Interpolate(a, a, 0.3875);
  EXPECT_EQ(c.GetPosixSeconds<int64>(), a.GetPosixSeconds<int64>());

  // Convert a's exact number of seconds from BC to AD at UTC:-3:30.
  b = DateTime::CreateFromPosixSeconds(-a.GetPosixSeconds<int64>(), -3, -30);
  // The average of a and b should be zero.
  c = DateTime::Interpolate(a, b, 0.5);
  EXPECT_INT64_NEAR(c.GetPosixSeconds<int64>(), 0LL,
                    a.GetPosixSeconds<int64>() - b.GetPosixSeconds<int64>(),
                    kDateTimeTolerance);

  // Test regular year.
  //
  // February 28, 2007 (23:12:32 at UTC:+0).
  a.Set(2007LL, 2, 28, 23, 12, 32, 0, 0);
  // March 3, 2007 (23:12:32 at UTC:+0). Three days later.
  b.Set(2007LL, 3, 3, 23, 12, 32, 0, 0);
  // March 4, 2007 (23:12:32 at UTC:+0). Four days later.
  c.Set(2007LL, 3, 4, 23, 12, 32, 0, 0);
  d = DateTime::Interpolate(a, c, 0.75);
  ExpectDateTimesNear(b, d, kHalfSecond);

  // Test leap year.
  //
  // February 28, 2008 (23:12:32 at UTC:+0).
  a.Set(2008LL, 2, 28, 23, 12, 32, 0, 0);
  // March 3, 2008 (23:12:32 at UTC:+0). Four days later.
  b.Set(2008LL, 3, 3, 23, 12, 32, 0, 0);
  // March 4, 2008 (23:12:32 at UTC:+0). Five days later.
  c.Set(2008LL, 3, 4, 23, 12, 32, 0, 0);
  // Interpolate from c backwards to a.
  d = DateTime::Interpolate(c, a, 0.2);
  ExpectDateTimesNear(b, d, 0.5);

  // Test High-precision interpolation in nanoseconds.

  // test two values that are apart by one second.
  a.Set(2008LL, 4, 5, 23, 12, 31, 0, 0);
  b.Set(2008LL, 4, 5, 23, 12, 32, 0, 0);

  static const double kNanoSecondRoundingTolerance =
      2.0 / kNanoSecondsPerSecond;

  c = DateTime::Interpolate(a, b, 0.0);
  EXPECT_TRUE(c == a);  // NOLINT: EXPECT_EQ breaks on Windows due to alignment
  c = DateTime::Interpolate(a, b, 1.0);
  EXPECT_TRUE(c == b);  // NOLINT: EXPECT_EQ breaks on Windows due to alignment

  c = DateTime::Interpolate(a, b, 0.1);
  d.Set(2008LL,
        4,
        5,
        23,
        12,
        31,
        static_cast<int>(kNanoSecondsPerSecond * 0.1),
        0,
        0);
  ExpectDateTimesNear(c, d, kNanoSecondRoundingTolerance);

  c = DateTime::Interpolate(a, b, 0.04);
  d.Set(2008LL,
        4,
        5,
        23,
        12,
        31,
        static_cast<int>(kNanoSecondsPerSecond * 0.04),
        0,
        0);
  ExpectDateTimesNear(c, d, kNanoSecondRoundingTolerance);

  c = DateTime::Interpolate(a, b, 0.0005);
  d.Set(2008LL,
        4,
        5,
        23,
        12,
        31,
        static_cast<uint32>(kNanoSecondsPerSecond * 0.0005),
        0,
        0);
  ExpectDateTimesNear(c, d, kNanoSecondRoundingTolerance);

  // Test high-precision interpolation of values that are a
  // non-trivial distance apart. For example, test values that are ten
  // thousand seconds apart.
  static const int kNumSecondsApart = 1000;

  // Expect an accuracy of at least one tenth of one millisecond on this test.
  static const double kSecondTolerance = 0.0001;
  b = a;
  b += kNumSecondsApart;

  // Try interpolating 2 seconds in.
  c = DateTime::Interpolate(a, b, 2.0 / kNumSecondsApart);
  d = a;
  d += 2;
  ExpectDateTimesNear(c, d, kSecondTolerance);

  // Try interpolating 100 seconds in.
  c = DateTime::Interpolate(a, b, 100.0 / kNumSecondsApart);
  d = a;
  d += 100;
  ExpectDateTimesNear(c, d, kSecondTolerance);

  // Interpolate one tenth of one second in.
  c = DateTime::Interpolate(a, b, 0.1 / kNumSecondsApart);
  d = a;
  d.SetNanosecond(static_cast<uint32>(0.1 * kNanoSecondsPerSecond));
  ExpectDateTimesNear(c, d, kSecondTolerance);

  // Interpolate one thousanth of one second in.
  c = DateTime::Interpolate(a, b, 0.001 / kNumSecondsApart);
  d = a;
  d.SetNanosecond(static_cast<uint32>(0.001 * kNanoSecondsPerSecond));
  ExpectDateTimesNear(c, d, kSecondTolerance);
}

TEST(DateTime, EndpointInterpolation) {
  // This test case is ripped from http://b/2930972.
  DateTime a, b, c;
  a.Set(2010, 9, 20, 23, 53, 35, 826902334, 0, 0);
  b.Set(1946, 7, 26, 0, 0, 0, 0, 0, 0);
  c = DateTime::Interpolate(a, b, 0.0);
  EXPECT_TRUE(c == a);  // NOLINT: EXPECT_EQ breaks on Windows due to alignment
  c = DateTime::Interpolate(a, b, 1.0);
  EXPECT_TRUE(c == b);  // NOLINT: EXPECT_EQ breaks on Windows due to alignment
}

TEST(DateTime, GetTimeAsDecimal) {
  DateTime a;

  // noon
  a.Set(0LL, 0, 0, 12, 0, 0, 0, 0);
  EXPECT_EQ(a.GetTimeAsDecimal(), 0.5);

  // midnight
  a.Set(0LL, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_EQ(a.GetTimeAsDecimal(), 0.0);

  // ignore weird dates
  a.Set(-400000LL, 2, 29, 12, 0, 0, 0, 0);
  EXPECT_EQ(a.GetTimeAsDecimal(), 0.5);

  // one second after midnight
  a.Set(0LL, 0, 0, 0, 0, 1, 0, 0);
  double past_midnight = 1.0 / (60 * 60 * 24);
  EXPECT_NEAR(past_midnight, a.GetTimeAsDecimal(), 1e-9);

  // one second before midnight
  a.Set(0LL, 0, 0, 23, 59, 59, 0, 0);
  double almost_midnight = 1.0 - 1.0 / (60 * 60 * 24);
  EXPECT_NEAR(almost_midnight, a.GetTimeAsDecimal(), 1e-9);
}

// Get Julian Calendar Date representation.
TEST(DateTime, JulianCalendarConvert) {
  DateTime a;

  // November 16, 1858 at noon
  a.Set(1858LL, 11, 16, 12, 0, 0, 0, 0);
  int date = static_cast<int>(floor(a.GetJulianDate()));
  EXPECT_EQ(date, 2400000);

  // Example 7.a from "Astonomical Algorithms".
  // The launch of Sputnik
  a.Set(1957LL, 10, 4, 19, 28, 34, 0, 0);
  EXPECT_NEAR(2436116.31, a.GetJulianDate(), 1e-2);

  // Y2k at midnight
  a.Set(2000LL, 1, 1, 0, 0, 0, 0, 0);
  EXPECT_EQ(a.GetJulianDate(), 2451544.5);

  // J2000
  a.Set(2000LL, 1, 1, 12, 0, 0, 0, 0);
  EXPECT_EQ(a.GetJulianDate(), 2451545.0);

  // Y1.9k, at 7:12am (.3 decimal time)
  a.Set(1900LL, 1, 1, 7, 12, 0, 0, 0);
  EXPECT_DOUBLE_EQ(2415020.8, a.GetJulianDate());
}

// Get J2000 epoch representation.
TEST(DateTime, GetJ2000Date) {
  DateTime date;

  // J2000 should be 0 relative to itself.
  date.Set(2000LL, 1, 1, 12, 0, 0, 0, 0);
  EXPECT_EQ(date.GetJ2000Date(), 0.0);

  // Compare to known value: 15:30 UT, 4th April 2008.
  // J2000 is 3016.1458 (precision: 4 decimal places).
  date.Set(2008LL, 4, 4, 15, 30, 0, 0, 0);
  EXPECT_NEAR(3016.1458, date.GetJ2000Date(), 1e-4);

  // Compare to known value: 2310 hrs UT on 1998 August 10th.
  // J2000 is -508.53472 (precision: 5 decimal places).
  date.Set(1998LL, 8, 10, 23, 10, 0, 0, 0);
  EXPECT_NEAR(-508.53472, date.GetJ2000Date(), 1e-5);
}

///////////////////////////////////////////////////////////////////////////////
//  Date-Time string unit tests
TEST(DateTime, BCEDateWrittenProperly) {
  static const DateTimeTestInfo kTestInfo[] = {
      {-8000000001LL, 12, 31, 17, 0, 0, -7, 0, "8.00 Billion BCE", ""},
      {-4000000001LL, 12, 31, 17, 0, 0, -7, 0, "4.00 Billion BCE", ""},
      {-2000000001LL, 12, 31, 17, 0, 0, -7, 0, "2.00 Billion BCE", ""},
      {-300000001LL, 12, 31, 17, 0, 0, -7, 0, "300 Million BCE", ""},
      {-10000001, 12, 31, 17, 0, 1, -7, 0, "10.0 Million BCE", ""},
      {-1000001, 12, 31, 17, 0, 1, -7, 0, "1.00 Million BCE", ""},
      {-1, 12, 31, 17, 0, 0, -7, 0, "1 BCE", ""},
      {998, 12, 31, 17, 0, 0, -7, 0, " 998", ""}};
  static const int kNumTestInfo = ABSL_ARRAYSIZE(kTestInfo);

  for (int i = 0; i < kNumTestInfo; ++i) {
    const DateTimeTestInfo& test_info = kTestInfo[i];
    DateTime dateTime;
    std::string outputString;
    dateTime.Set(test_info.year,
                 test_info.month,
                 test_info.day,
                 test_info.hour,
                 test_info.minute,
                 test_info.second,
                 test_info.zone_hours,
                 test_info.zone_minutes);
    dateTime.ComputeDateString(DateTime::kRenderYearOnly, &outputString);
    EXPECT_EQ(outputString, test_info.expected_date_string);
  }
}

TEST(DateTime, CommonEraDateTimeWrittenProperly) {
  static const DateTimeTestInfo kTestInfo[] = {
      {2004, 7, 9, 14, 41, 2, -7, 0, "7/9/2004", " 2:41:02 pm"},
      {2004, 7, 8, 22, 41, 2, -7, 0, "7/8/2004", " 10:41:02 pm"},
      {2004, 7, 9, 6, 41, 2, -7, 0, "7/9/2004", " 6:41:02 am"}};
  static const int kNumTestInfo = ABSL_ARRAYSIZE(kTestInfo);

  for (int i = 0; i < kNumTestInfo; ++i) {
    const DateTimeTestInfo& test_info = kTestInfo[i];
    DateTime dateTime;
    std::string outputString;
    dateTime.Set(test_info.year,
                 test_info.month,
                 test_info.day,
                 test_info.hour,
                 test_info.minute,
                 test_info.second,
                 test_info.zone_hours,
                 test_info.zone_minutes);
    dateTime.ComputeDateString(DateTime::kRenderDayMonthYear, &outputString);
    EXPECT_EQ(outputString, test_info.expected_date_string);
    dateTime.ComputeTimeString(DateTime::kRenderHoursMinutesSeconds,
                               &outputString);
    EXPECT_EQ(outputString, test_info.expected_time_string);
  }
}

TEST(DateTime, Use24HourTime) {
  std::string time_string;
  TestableDateTime time;
  time.Set(2010, 9, 21, 15, 5, 25, 0, 0);

  EXPECT_FALSE(time.Use24HourTime());
  time.ComputeTimeString(DateTime::kRenderHoursMinutesSeconds, &time_string);
  EXPECT_EQ(time_string, " 3:05:25 pm");
  time.ComputeTimeString(DateTime::kRenderHoursMinutes, &time_string);
  EXPECT_EQ(time_string, " 3:05 pm");
  time.ComputeTimeString(DateTime::kRenderHoursOnly, &time_string);
  EXPECT_EQ(time_string, " 3 pm");

  time.SetUse24HourTime(true);
  EXPECT_TRUE(time.Use24HourTime());
  time.ComputeTimeString(DateTime::kRenderHoursMinutesSeconds, &time_string);
  EXPECT_EQ(time_string, " 15:05:25");
  time.ComputeTimeString(DateTime::kRenderHoursMinutes, &time_string);
  EXPECT_EQ(time_string, " 15:05");
  time.ComputeTimeString(DateTime::kRenderHoursOnly, &time_string);
  EXPECT_EQ(time_string, " 15");
}

TEST(DateTime, DurationWrittenProperly) {
  static const DateTimeTestInfo kTestInfo[] = {
      // Test top 3 fields with no following fields.
      {1, 7, 8, 0, 0, 0, 0, 0, "1y 7m 8d", ""},
      // With following fields.
      {1, 7, 8, 1, 0, 0, 0, 0, "1y 7m 8d", ""},

      // Test top 3 fields with one empty and no followers.
      {1, 7, 0, 0, 0, 0, 0, 0, "1y 7m", ""},
      // With a following field.
      {1, 7, 0, 1, 0, 0, 0, 0, "1y 7m", ""},

      // Test top 3 fields with two empty and no followers.
      {1, 0, 0, 0, 0, 0, 0, 0, "1y", ""},
      // With a following field.
      {1, 0, 0, 1, 0, 0, 0, 0, "1y", ""},

      // Test remaining field labels.
      {0, 0, 0, 6, 41, 2, 2, 0, "6h 41m 2s", ""},

      // Check field skipping in the middle.
      {1, 0, 1, 0, 0, 0, 0, 0, "1y 1d", ""},

      // Check fractional seconds argument..
      {0, 0, 0, 0, 41, 0, 10, 0, "41m 0.1s", ""},
      {0, 0, 0, 0, 41, 0, 22, 0, "41m 0.2s", ""},

      // Check 0 string.
      {0, 0, 0, 0, 0, 0, 0, 0, "0.0s", ""}};
  static const int kNumTestInfo = ABSL_ARRAYSIZE(kTestInfo);

  for (int i = 0; i < kNumTestInfo; ++i) {
    const DateTimeTestInfo& test_info = kTestInfo[i];
    DateTime dateTime;
    std::string outputString;
    dateTime.Set(test_info.year,
                 test_info.month,
                 test_info.day,
                 test_info.hour,
                 test_info.minute,
                 test_info.second,
                 0,
                 0);
    double fractional_seconds = test_info.zone_hours / 100.;
    outputString = dateTime.ComputeDurationString(fractional_seconds);
    EXPECT_EQ(outputString, test_info.expected_date_string);
  }
}

// ComputeDurationString tests all branches of GetDateTimeField except for
// nanoseconds because of its fracitonal_seconds argument which replaces
// them.
TEST(DateTime, NanosecondFieldAccessor) {
  DateTime d;
  d.SetNanosecond(200);
  EXPECT_EQ(200U, d.GetDateTimeField(DateTime::kNanosecond));
  d.SetNanosecond(333);
  EXPECT_EQ(333U, d.GetDateTimeField(DateTime::kNanosecond));
}

TEST(DateTime, InvalidFieldAccessor) {
  LogChecker log_checker;
  DateTime d;
  EXPECT_EQ(-1L, d.GetDateTimeField(DateTime::kNumFields));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Invalid DateTime field"));
}

TEST(DateTime, PartialDateTimeStringWrittenProperly) {
  struct DateRenderTestInfo {
    DateTime::DateStringEnum mode;
    std::string expected_string;
  };

  struct TimeRenderTestInfo {
    DateTime::TimeStringEnum mode;
    std::string expected_string;
  };

  const DateRenderTestInfo kTestDateInfo[] = {
      {DateTime::kRenderDayMonthYear, "7/9/2004"},
      {DateTime::kRenderMonthYear, "7/2004"},
      {DateTime::kRenderYearOnly, "2004"}};
  const int kNumTestDateInfo =
      sizeof(kTestDateInfo) / sizeof(DateRenderTestInfo);

  const TimeRenderTestInfo kTestTimeInfo[] = {
      {DateTime::kRenderHoursMinutesSeconds, " 2:41:02 pm"},
      {DateTime::kRenderHoursMinutes, " 2:41 pm"},
      {DateTime::kRenderHoursOnly, " 2 pm"}};
  const int kNumTestTimeInfo =
      sizeof(kTestDateInfo) / sizeof(TimeRenderTestInfo);

  DateTime dateTime;
  dateTime.Set(2004, 7, 9, 14, 41, 2, -7, 0);

  for (int i = 0; i < kNumTestDateInfo; ++i) {
    const DateRenderTestInfo& test_info = kTestDateInfo[i];
    std::string outputString;
    dateTime.ComputeDateString(test_info.mode, &outputString);
    EXPECT_EQ(outputString, test_info.expected_string);
  }
  for (int i = 0; i < kNumTestTimeInfo; ++i) {
    const TimeRenderTestInfo& test_info = kTestTimeInfo[i];
    std::string outputString;
    dateTime.ComputeTimeString(test_info.mode, &outputString);
    EXPECT_EQ(outputString, test_info.expected_string);
  }
}

TEST(DateTime, LerpTest) {
  const int64 kMillionSeconds = 1000000;
  DateTime date1 = DateTime::CreateFromPosixSeconds(0, 0, 0);
  DateTime date2 = DateTime::CreateFromPosixSeconds(kMillionSeconds, 0, 0);

  DateTime result;
  int64 resultSecs;

  // Test interpolation at 0.
  result.Lerp(date1, date2, 0.0);
  resultSecs = result.GetPosixSeconds<int64>();
  EXPECT_EQ(0, resultSecs);

  // Test interpolation at 1.
  result.Lerp(date1, date2, 1.0);
  resultSecs = result.GetPosixSeconds<int64>();
  EXPECT_EQ(1000000, resultSecs);

  // Test at 1/2.
  result.Lerp(date1, date2, 0.5);
  resultSecs = result.GetPosixSeconds<int64>();
  EXPECT_EQ(500000, resultSecs);

  // Test at 1/4 to check linearity.
  result.Lerp(date1, date2, 0.25);
  resultSecs = result.GetPosixSeconds<int64>();
  EXPECT_EQ(250000, resultSecs);

  const int64 kBillionSeconds = 1000 * kMillionSeconds;
  // Test two Datetimes at high second values. Check that we can lerp
  // with nanosecond precision at high values.
  date1 = DateTime::CreateFromPosixSeconds(kBillionSeconds, 0, 0);
  date2 = DateTime::CreateFromPosixSeconds(kBillionSeconds + 10, 0, 0);

  const uint32 kNsTolerance = 1;

  // Test interpolation at 0 with high-precision interpolation.
  result.Lerp(date1, date2, 0.0);
  resultSecs = result.GetPosixSeconds<int64>();
  EXPECT_EQ(kBillionSeconds, resultSecs);
  uint32 result_nanosecs = result.GetNanosecond();
  EXPECT_EQ(0, static_cast<int>(result_nanosecs));

  result.Lerp(date1, date2, 0.1 / kNanoSecondsPerSecond);
  resultSecs = result.GetPosixSeconds<int64>();
  result_nanosecs = result.GetNanosecond();
  EXPECT_EQ(kBillionSeconds, resultSecs);
  EXPECT_NEAR(result_nanosecs, 1, kNsTolerance);

  result.Lerp(date1, date2, 0.2 / kNanoSecondsPerSecond);
  resultSecs = result.GetPosixSeconds<int64>();
  result_nanosecs = result.GetNanosecond();
  EXPECT_EQ(kBillionSeconds, resultSecs);
  EXPECT_NEAR(result_nanosecs, 2, kNsTolerance);

  result.Lerp(date1, date2, 0.5 / kNanoSecondsPerSecond);
  resultSecs = result.GetPosixSeconds<int64>();
  result_nanosecs = result.GetNanosecond();
  EXPECT_EQ(kBillionSeconds, resultSecs);
  EXPECT_NEAR(result_nanosecs, 5, kNsTolerance);

  result.Lerp(date1, date2, 0.001);
  resultSecs = result.GetPosixSeconds<int64>();
  EXPECT_EQ(kBillionSeconds, resultSecs);
  result_nanosecs = result.GetNanosecond();
  EXPECT_NEAR(result_nanosecs, kNanoSecondsPerSecond / 100, kNsTolerance);

  result.Lerp(date1, date2, 0.101);
  resultSecs = result.GetPosixSeconds<int64>();
  EXPECT_EQ(kBillionSeconds + 1, resultSecs);
  result_nanosecs = result.GetNanosecond();
  EXPECT_NEAR(result_nanosecs, kNanoSecondsPerSecond / 100, kNsTolerance);

  result.Lerp(date1, date2, 0.201);
  resultSecs = result.GetPosixSeconds<int64>();
  EXPECT_EQ(kBillionSeconds + 2, resultSecs);
  result_nanosecs = result.GetNanosecond();
  EXPECT_NEAR(result_nanosecs, kNanoSecondsPerSecond / 100, kNsTolerance);

  result.Lerp(date1, date2, 0.301);
  resultSecs = result.GetPosixSeconds<int64>();
  EXPECT_EQ(kBillionSeconds + 3, resultSecs);
  result_nanosecs = result.GetNanosecond();
  EXPECT_NEAR(result_nanosecs, kNanoSecondsPerSecond / 100, kNsTolerance);

  result.Lerp(date1, date2, 0.404);
  resultSecs = result.GetPosixSeconds<int64>();
  EXPECT_EQ(kBillionSeconds + 4, resultSecs);
  result_nanosecs = result.GetNanosecond();
  EXPECT_NEAR(result_nanosecs, 4 * (kNanoSecondsPerSecond / 100), kNsTolerance);

  result.Lerp(date1, date2, 0.909);
  resultSecs = result.GetPosixSeconds<int64>();
  EXPECT_EQ(kBillionSeconds + 9, resultSecs);
  result_nanosecs = result.GetNanosecond();
  EXPECT_NEAR(result_nanosecs, 9 * (kNanoSecondsPerSecond / 100), kNsTolerance);

  result.Lerp(date1, date2, 1.0);
  resultSecs = result.GetPosixSeconds<int64>();
  EXPECT_EQ(kBillionSeconds + 10, resultSecs);
  result_nanosecs = result.GetNanosecond();
  EXPECT_NEAR(result_nanosecs, 0, kNsTolerance);
}

TEST(DateTime, GetInterpValue) {
  const int64 kMillionSeconds = 1000000;
  const int64 kTwoMillionSeconds = 2 * kMillionSeconds;

  DateTime date1 = DateTime::CreateFromPosixSeconds(kMillionSeconds, 0, 0);
  DateTime date2 = DateTime::CreateFromPosixSeconds(kTwoMillionSeconds, 0, 0);

  DateTime test_date;

  // Test a value at one-million.
  test_date = DateTime::CreateFromPosixSeconds(kMillionSeconds, 0, 0);
  double interp = DateTime::GetInterpValue(test_date, date1, date2);
  EXPECT_NEAR(0.0, interp, kDateTimeTolerance);
  interp = DateTime::GetInterpValue(test_date, date2, date1);
  EXPECT_NEAR(1.0, interp, kDateTimeTolerance);

  // Test a value at two-million.
  test_date = DateTime::CreateFromPosixSeconds(kTwoMillionSeconds, 0, 0);
  interp = DateTime::GetInterpValue(test_date, date1, date2);
  EXPECT_NEAR(1.0, interp, kDateTimeTolerance);
  interp = DateTime::GetInterpValue(test_date, date2, date1);
  EXPECT_NEAR(0.0, interp, kDateTimeTolerance);

  // Test a value at exactly half-way between 1 and 2 million.
  test_date = DateTime::CreateFromPosixSeconds(kMillionSeconds * 1.5, 0, 0);
  interp = DateTime::GetInterpValue(test_date, date1, date2);
  EXPECT_NEAR(0.5, interp, kDateTimeTolerance);
  interp = DateTime::GetInterpValue(test_date, date2, date1);
  EXPECT_NEAR(0.5, interp, kDateTimeTolerance);

  // Test a value 1/4 way between 1 and two million.
  test_date = DateTime::CreateFromPosixSeconds(kMillionSeconds * 1.25, 0, 0);
  interp = DateTime::GetInterpValue(test_date, date1, date2);
  EXPECT_NEAR(0.25, interp, kDateTimeTolerance);
  interp = DateTime::GetInterpValue(test_date, date2, date1);
  EXPECT_NEAR(0.75, interp, kDateTimeTolerance);

  // Test a value 3/4 way between 1 and two million.
  test_date = DateTime::CreateFromPosixSeconds(kMillionSeconds * 1.75, 0, 0);
  interp = DateTime::GetInterpValue(test_date, date1, date2);
  EXPECT_NEAR(0.75, interp, kDateTimeTolerance);
  interp = DateTime::GetInterpValue(test_date, date2, date1);
  EXPECT_NEAR(0.25, interp, kDateTimeTolerance);

  // Test a value outside of the range, towards an earlier time.
  test_date = DateTime::CreateFromPosixSeconds(0, 0, 0);
  interp = DateTime::GetInterpValue(test_date, date1, date2);
  EXPECT_NEAR(-1.0, interp, kDateTimeTolerance);
  interp = DateTime::GetInterpValue(test_date, date2, date1);
  EXPECT_NEAR(2.0, interp, kDateTimeTolerance);

  // Test a value outside of the range, towards a later time.
  test_date = DateTime::CreateFromPosixSeconds(kTwoMillionSeconds * 2, 0, 0);
  interp = DateTime::GetInterpValue(test_date, date1, date2);
  EXPECT_NEAR(3.0, interp, kDateTimeTolerance);
  interp = DateTime::GetInterpValue(test_date, date2, date1);
  EXPECT_NEAR(-2.0, interp, kDateTimeTolerance);

  // test a degenerate range of zero-length.
  interp = DateTime::GetInterpValue(test_date, date1, date1);
  EXPECT_NEAR(0.0, interp, kDateTimeTolerance);
  interp = DateTime::GetInterpValue(test_date, date2, date2);
  EXPECT_NEAR(0.0, interp, kDateTimeTolerance);

  // Test high-precision interpolation in the nanosecond regime, from
  // two values that are ten seconds apart.
  DateTime a, b;
  a.Set(2008LL, 4, 5, 23, 12, 30, kNanoSecondsPerSecond / 2, 0, 0);
  b.Set(2008LL, 4, 5, 23, 12, 40, kNanoSecondsPerSecond / 2, 0, 0);

  // test 1/2 second later. Should be 1/20th interpolation.
  test_date.Set(2008LL, 4, 5, 23, 12, 31, 0, 0, 0);
  interp = DateTime::GetInterpValue(test_date, a, b);
  EXPECT_NEAR(0.05, interp, kDateTimeTolerance);

  // test 1/2 way through the interpolation.
  test_date.Set(2008LL, 4, 5, 23, 12, 35, kNanoSecondsPerSecond / 2, 0, 0);
  interp = DateTime::GetInterpValue(test_date, a, b);
  EXPECT_NEAR(0.5, interp, kDateTimeTolerance);

  // test 1 millisecond from the end of the interpolation.
  const int kMilliSecondInNS = kNanoSecondsPerSecond / 1000;
  test_date.Set(2008LL,
                4,
                5,
                23,
                12,
                40,
                kNanoSecondsPerSecond / 2 - kMilliSecondInNS,
                0,
                0);
  interp = DateTime::GetInterpValue(test_date, a, b);

  const double kExpectedInterp = 1.0 - (1.0 / 10000.0);
  EXPECT_NEAR(kExpectedInterp, interp, kDateTimeTolerance);
}

TEST(DateTime, IsEqualByComponent) {
  DateTime dt0, dt1;

  dt0.Set(1997LL, 7, 16, 7, 30, 15, 1290, 2,  31);
  dt1.Set(1997LL, 7, 16, 7, 30, 15, 1290, 2,  31);
  EXPECT_TRUE(dt0 == dt1);                   // Absolute times are equal.
  EXPECT_TRUE(dt0.IsEqualByComponent(dt1));  // Components are equal.

  dt0.Set(1997LL, 7, 16, 7, 30, 15, 0,  0);
  dt1.Set(1997LL, 7, 16, 10, 30, 15, 3, 0);
  EXPECT_TRUE(dt0 == dt1);                    // Absolute times are equal.
  EXPECT_FALSE(dt0.IsEqualByComponent(dt1));  // Components not equal in hours.

  dt0.Set(1997LL, 7, 16, 7, 30, 15, 0,  0);
  dt1.Set(1997LL, 7, 16, 7, 45, 15, 0, 15);
  EXPECT_TRUE(dt0 == dt1);                    // Absolute times are equal.
  EXPECT_FALSE(dt0.IsEqualByComponent(dt1));  // Components not equal in mins.
}

TEST(DateTime, ToSecondsWithZeroMonth) {
  // Convert toSeconds and then back, and make sure things match.
  // And verify that month and day of 0 work ok.
  DateTime orig_time;
  orig_time.SetYear(2008);
  orig_time.SetMonth(0);
  orig_time.SetDay(0);

  int64 orig_seconds = orig_time.GetPosixSeconds<int64>();
  DateTime from_seconds_time =
      DateTime::CreateFromPosixSeconds(orig_seconds, 0, 0);
  EXPECT_EQ(from_seconds_time.GetYear(), 2008);
  EXPECT_EQ(from_seconds_time.GetMonth(), 1);
  EXPECT_EQ(from_seconds_time.GetDay(), 1);
}

// Test that we can read in all legal xml:dateTime strings.
TEST(DateTime, FromString) {
  LogChecker log_checker;
  // Try an invalid string and check for no mutation.
  {
    const std::string kInvalidDateString = "invalid string";
    DateTime d;
    d.SetYear(12345);
    EXPECT_FALSE(d.FromString(kInvalidDateString));
    EXPECT_TRUE(log_checker.HasMessage("WARNING", "Couldn't parse DateTime"));
    EXPECT_EQ(d.GetYear(), 12345);
  }

  // Try a string with only a year.
  {
    const std::string kYearString = "2009";
    DateTime d;
    EXPECT_TRUE(d.FromString(kYearString));
    EXPECT_EQ(2009, d.GetYear());
    EXPECT_EQ(1, d.GetMonth());
    EXPECT_EQ(1, d.GetDay());
    EXPECT_EQ(0, d.GetHour());
    EXPECT_EQ(0, d.GetMinute());
    EXPECT_EQ(0, d.GetSecond());
    EXPECT_EQ(0, static_cast<int>(d.GetNanosecond()));
    EXPECT_EQ(0, d.GetZoneHours());
    EXPECT_EQ(0, d.GetZoneMinutes());
  }

  // Try a string with only a negative year
  {
    const std::string kNegYearString = "-4000000";
    DateTime d;
    EXPECT_TRUE(d.FromString(kNegYearString));
    EXPECT_EQ(-4000000, d.GetYear());
    EXPECT_EQ(1, d.GetMonth());
    EXPECT_EQ(1, d.GetDay());
    EXPECT_EQ(0, d.GetHour());
    EXPECT_EQ(0, d.GetMinute());
    EXPECT_EQ(0, d.GetSecond());
    EXPECT_EQ(0, static_cast<int>(d.GetNanosecond()));
    EXPECT_EQ(0, d.GetZoneHours());
    EXPECT_EQ(0, d.GetZoneMinutes());
  }

  // Try a string with months.
  {
    const std::string kMonthString = "2009-05";
    DateTime d;
    EXPECT_TRUE(d.FromString(kMonthString));
    EXPECT_EQ(2009, d.GetYear());
    EXPECT_EQ(5, d.GetMonth());
    EXPECT_EQ(1, d.GetDay());
    EXPECT_EQ(0, d.GetHour());
    EXPECT_EQ(0, d.GetMinute());
    EXPECT_EQ(0, d.GetSecond());
    EXPECT_EQ(0, static_cast<int>(d.GetNanosecond()));
    EXPECT_EQ(0, d.GetZoneHours());
    EXPECT_EQ(0, d.GetZoneMinutes());
  }

  // Try a string with days.
  {
    const std::string kDayString = "2009-05-21";
    DateTime d;
    EXPECT_TRUE(d.FromString(kDayString));
    EXPECT_EQ(2009, d.GetYear());
    EXPECT_EQ(05, d.GetMonth());
    EXPECT_EQ(21, d.GetDay());
    EXPECT_EQ(0, d.GetHour());
    EXPECT_EQ(0, d.GetMinute());
    EXPECT_EQ(0, d.GetSecond());
    EXPECT_EQ(0, static_cast<int>(d.GetNanosecond()));
    EXPECT_EQ(0, d.GetZoneHours());
    EXPECT_EQ(0, d.GetZoneMinutes());
  }

  // Try a string with hours.
  {
    const std::string kHourString = "2009-05-21T06";
    DateTime d;
    EXPECT_TRUE(d.FromString(kHourString));
    EXPECT_EQ(2009, d.GetYear());
    EXPECT_EQ(05, d.GetMonth());
    EXPECT_EQ(21, d.GetDay());
    EXPECT_EQ(6, d.GetHour());
    EXPECT_EQ(0, d.GetMinute());
    EXPECT_EQ(0, d.GetSecond());
    EXPECT_EQ(0, static_cast<int>(d.GetNanosecond()));
    EXPECT_EQ(0, d.GetZoneHours());
    EXPECT_EQ(0, d.GetZoneMinutes());
  }

  // Try a string with minutes
  {
    const std::string kMinuteString = "2009-05-21T06:24";
    DateTime d;
    EXPECT_TRUE(d.FromString(kMinuteString));
    EXPECT_EQ(2009, d.GetYear());
    EXPECT_EQ(05, d.GetMonth());
    EXPECT_EQ(21, d.GetDay());
    EXPECT_EQ(6, d.GetHour());
    EXPECT_EQ(24, d.GetMinute());
    EXPECT_EQ(0, d.GetSecond());
    EXPECT_EQ(0, static_cast<int>(d.GetNanosecond()));
    EXPECT_EQ(0, d.GetZoneHours());
    EXPECT_EQ(0, d.GetZoneMinutes());
  }

  // Try a string with seconds
  {
    const std::string kSecondString = "2009-05-21T06:24:47";
    DateTime d;
    EXPECT_TRUE(d.FromString(kSecondString));
    EXPECT_EQ(2009, d.GetYear());
    EXPECT_EQ(05, d.GetMonth());
    EXPECT_EQ(21, d.GetDay());
    EXPECT_EQ(6, d.GetHour());
    EXPECT_EQ(24, d.GetMinute());
    EXPECT_EQ(47, d.GetSecond());
    EXPECT_EQ(0, static_cast<int>(d.GetNanosecond()));
    EXPECT_EQ(0, d.GetZoneHours());
    EXPECT_EQ(0, d.GetZoneMinutes());
  }

  // Try a string with fractional seconds
  {
    const std::string kNanoSecondString = "2009-05-21T06:24:47.123456789";
    DateTime d;
    EXPECT_TRUE(d.FromString(kNanoSecondString));
    EXPECT_EQ(2009, d.GetYear());
    EXPECT_EQ(05, d.GetMonth());
    EXPECT_EQ(21, d.GetDay());
    EXPECT_EQ(6, d.GetHour());
    EXPECT_EQ(24, d.GetMinute());
    EXPECT_EQ(47, d.GetSecond());
    EXPECT_EQ(123456789, static_cast<int>(d.GetNanosecond()));
    EXPECT_EQ(0, d.GetZoneHours());
    EXPECT_EQ(0, d.GetZoneMinutes());
  }

  // Try another string with fractional seconds, where the time isn't
  // specified all the way to nanosecond precision
  {
    const std::string kNanoSecondString = "2009-05-21T06:24:47.010";
    DateTime d;
    EXPECT_TRUE(d.FromString(kNanoSecondString));
    EXPECT_EQ(2009, d.GetYear());
    EXPECT_EQ(05, d.GetMonth());
    EXPECT_EQ(21, d.GetDay());
    EXPECT_EQ(6, d.GetHour());
    EXPECT_EQ(24, d.GetMinute());
    EXPECT_EQ(47, d.GetSecond());
    EXPECT_EQ(kNanoSecondsPerSecond / 100, static_cast<int>(d.GetNanosecond()));
    EXPECT_EQ(0, d.GetZoneHours());
    EXPECT_EQ(0, d.GetZoneMinutes());
  }

  // Try another string with fractional seconds, where the time isn't
  // specified all the way to nanosecond precision
  {
    const std::string kNanoSecondString = "2009-05-21T06:24:47.0006";
    DateTime d;
    EXPECT_TRUE(d.FromString(kNanoSecondString));
    EXPECT_EQ(2009, d.GetYear());
    EXPECT_EQ(05, d.GetMonth());
    EXPECT_EQ(21, d.GetDay());
    EXPECT_EQ(6, d.GetHour());
    EXPECT_EQ(24, d.GetMinute());
    EXPECT_EQ(47, d.GetSecond());
    EXPECT_EQ(kNanoSecondsPerSecond / 10000 * 6,
              static_cast<int>(d.GetNanosecond()));
    EXPECT_EQ(0, d.GetZoneHours());
    EXPECT_EQ(0, d.GetZoneMinutes());
  }

  // Try a Zulu string with nanoseconds.
  {
    const std::string kZuluString = "2009-05-21T06:24:47.123456789Z";
    DateTime d;
    EXPECT_TRUE(d.FromString(kZuluString));
    EXPECT_EQ(2009, d.GetYear());
    EXPECT_EQ(05, d.GetMonth());
    EXPECT_EQ(21, d.GetDay());
    EXPECT_EQ(6, d.GetHour());
    EXPECT_EQ(24, d.GetMinute());
    EXPECT_EQ(47, d.GetSecond());
    EXPECT_EQ(123456789, static_cast<int>(d.GetNanosecond()));
    EXPECT_EQ(0, d.GetZoneHours());
    EXPECT_EQ(0, d.GetZoneMinutes());
  }

  // Try a Zulu string without nanoseconds.
  {
    const std::string kZuluString = "2009-05-21T06:24:47Z";
    DateTime d;
    EXPECT_TRUE(d.FromString(kZuluString));
    EXPECT_EQ(2009, d.GetYear());
    EXPECT_EQ(05, d.GetMonth());
    EXPECT_EQ(21, d.GetDay());
    EXPECT_EQ(6, d.GetHour());
    EXPECT_EQ(24, d.GetMinute());
    EXPECT_EQ(47, d.GetSecond());
    EXPECT_EQ(0, static_cast<int>(d.GetNanosecond()));
    EXPECT_EQ(0, d.GetZoneHours());
    EXPECT_EQ(0, d.GetZoneMinutes());
  }

  // Try a Zulu string without seconds.
  {
    const std::string kZuluString = "2009-05-21T06:24Z";
    DateTime d;
    EXPECT_TRUE(d.FromString(kZuluString));
    EXPECT_EQ(2009, d.GetYear());
    EXPECT_EQ(05, d.GetMonth());
    EXPECT_EQ(21, d.GetDay());
    EXPECT_EQ(6, d.GetHour());
    EXPECT_EQ(24, d.GetMinute());
    EXPECT_EQ(0, d.GetSecond());
    EXPECT_EQ(0, static_cast<int>(d.GetNanosecond()));
    EXPECT_EQ(0, d.GetZoneHours());
    EXPECT_EQ(0, d.GetZoneMinutes());
  }

  // Try a Zulu string without minutes.
  {
    const std::string kZuluString = "2009-05-21T06Z";
    DateTime d;
    EXPECT_TRUE(d.FromString(kZuluString));
    EXPECT_EQ(2009, d.GetYear());
    EXPECT_EQ(05, d.GetMonth());
    EXPECT_EQ(21, d.GetDay());
    EXPECT_EQ(6, d.GetHour());
    EXPECT_EQ(0, d.GetMinute());
    EXPECT_EQ(0, d.GetSecond());
    EXPECT_EQ(0, static_cast<int>(d.GetNanosecond()));
    EXPECT_EQ(0, d.GetZoneHours());
    EXPECT_EQ(0, d.GetZoneMinutes());
  }

  // Try a Zulu string without time at all, just date.
  {
    const std::string kZuluString = "2009-05-21Z";
    DateTime d;
    EXPECT_TRUE(d.FromString(kZuluString));
    EXPECT_EQ(2009, d.GetYear());
    EXPECT_EQ(05, d.GetMonth());
    EXPECT_EQ(21, d.GetDay());
    EXPECT_EQ(0, d.GetHour());
    EXPECT_EQ(0, d.GetMinute());
    EXPECT_EQ(0, d.GetSecond());
    EXPECT_EQ(0, static_cast<int>(d.GetNanosecond()));
    EXPECT_EQ(0, d.GetZoneHours());
    EXPECT_EQ(0, d.GetZoneMinutes());
  }

  // Try a string with positive zone hours and minutes, with the time
  // specified to the sub-second level
  {
    const std::string kZoneString = "2009-05-21T06:24:47.0006+05:42";
    DateTime d;
    EXPECT_TRUE(d.FromString(kZoneString));
    EXPECT_EQ(2009, d.GetYear());
    EXPECT_EQ(05, d.GetMonth());
    EXPECT_EQ(21, d.GetDay());
    EXPECT_EQ(6, d.GetHour());
    EXPECT_EQ(24, d.GetMinute());
    EXPECT_EQ(47, d.GetSecond());
    EXPECT_EQ(kNanoSecondsPerSecond / 10000 * 6,
              static_cast<int>(d.GetNanosecond()));
    EXPECT_EQ(5, d.GetZoneHours());
    EXPECT_EQ(42, d.GetZoneMinutes());
  }

  // Try a string with negative zone hours and minutes, with the time
  // specified to the sub-second level
  {
    const std::string kZoneString = "2009-05-21T06:24:47.0006-07:24";
    DateTime d;
    EXPECT_TRUE(d.FromString(kZoneString));
    EXPECT_EQ(2009, d.GetYear());
    EXPECT_EQ(05, d.GetMonth());
    EXPECT_EQ(21, d.GetDay());
    EXPECT_EQ(6, d.GetHour());
    EXPECT_EQ(24, d.GetMinute());
    EXPECT_EQ(47, d.GetSecond());
    EXPECT_EQ(kNanoSecondsPerSecond / 10000 * 6,
              static_cast<int>(d.GetNanosecond()));
    EXPECT_EQ(-7, d.GetZoneHours());
    EXPECT_EQ(-24, d.GetZoneMinutes());
  }

  // Try a string with zone hours and minutes specified, without
  // fractional seconds.
  {
    const std::string kZoneString = "2009-05-21T06:24:47-07:24";
    DateTime d;
    EXPECT_TRUE(d.FromString(kZoneString));
    EXPECT_EQ(2009, d.GetYear());
    EXPECT_EQ(05, d.GetMonth());
    EXPECT_EQ(21, d.GetDay());
    EXPECT_EQ(6, d.GetHour());
    EXPECT_EQ(24, d.GetMinute());
    EXPECT_EQ(47, d.GetSecond());
    EXPECT_EQ(0, static_cast<int>(d.GetNanosecond()));
    EXPECT_EQ(-7, d.GetZoneHours());
    EXPECT_EQ(-24, d.GetZoneMinutes());
  }

  // Try a string with zone hours and minutes specified, without
  // seconds specified.
  {
    const std::string kZoneString = "2009-05-21T06:24-08:00";
    DateTime d;
    EXPECT_TRUE(d.FromString(kZoneString));
    EXPECT_EQ(2009, d.GetYear());
    EXPECT_EQ(05, d.GetMonth());
    EXPECT_EQ(21, d.GetDay());
    EXPECT_EQ(6, d.GetHour());
    EXPECT_EQ(24, d.GetMinute());
    EXPECT_EQ(0, d.GetSecond());
    EXPECT_EQ(0, static_cast<int>(d.GetNanosecond()));
    EXPECT_EQ(-8, d.GetZoneHours());
    EXPECT_EQ(0, d.GetZoneMinutes());
  }

  // Try a string with zone hours and minutes specified, without any time
  // specified.
  {
    const std::string kZoneString = "2009-05-21+05:45";
    DateTime d;
    EXPECT_TRUE(d.FromString(kZoneString));
    EXPECT_EQ(2009, d.GetYear());
    EXPECT_EQ(05, d.GetMonth());
    EXPECT_EQ(21, d.GetDay());
    EXPECT_EQ(0, d.GetHour());
    EXPECT_EQ(0, d.GetMinute());
    EXPECT_EQ(0, d.GetSecond());
    EXPECT_EQ(0, static_cast<int>(d.GetNanosecond()));
    EXPECT_EQ(5, d.GetZoneHours());
    EXPECT_EQ(45, d.GetZoneMinutes());
  }

  // Try another string with zone hours and minutes specified, without any time
  // specified.
  {
    const std::string kZoneString = "2009-05-21-07:16";
    DateTime d;
    EXPECT_TRUE(d.FromString(kZoneString));
    EXPECT_EQ(2009, d.GetYear());
    EXPECT_EQ(05, d.GetMonth());
    EXPECT_EQ(21, d.GetDay());
    EXPECT_EQ(0, d.GetHour());
    EXPECT_EQ(0, d.GetMinute());
    EXPECT_EQ(0, d.GetSecond());
    EXPECT_EQ(0, static_cast<int>(d.GetNanosecond()));
    EXPECT_EQ(-7, d.GetZoneHours());
    EXPECT_EQ(-16, d.GetZoneMinutes());
  }

  // Try a string with zone hours specified, without zone minutes
  // specified, and without any time specified.
  {
    const std::string kZoneString = "2009-05-21+07";
    DateTime d;
    EXPECT_TRUE(d.FromString(kZoneString));
    EXPECT_EQ(2009, d.GetYear());
    EXPECT_EQ(05, d.GetMonth());
    EXPECT_EQ(21, d.GetDay());
    EXPECT_EQ(0, d.GetHour());
    EXPECT_EQ(0, d.GetMinute());
    EXPECT_EQ(0, d.GetSecond());
    EXPECT_EQ(0, static_cast<int>(d.GetNanosecond()));
    EXPECT_EQ(7, d.GetZoneHours());
    EXPECT_EQ(0, d.GetZoneMinutes());
  }

  // Try another string with zone hours specified, without zone minutes
  // specified, and without any time specified.
  {
    const std::string kZoneString = "2009-05-21-06";
    DateTime d;
    EXPECT_TRUE(d.FromString(kZoneString));
    EXPECT_EQ(2009, d.GetYear());
    EXPECT_EQ(05, d.GetMonth());
    EXPECT_EQ(21, d.GetDay());
    EXPECT_EQ(0, d.GetHour());
    EXPECT_EQ(0, d.GetMinute());
    EXPECT_EQ(0, d.GetSecond());
    EXPECT_EQ(0, static_cast<int>(d.GetNanosecond()));
    EXPECT_EQ(-6, d.GetZoneHours());
    EXPECT_EQ(0, d.GetZoneMinutes());
  }
}

// Test behavior of operator+= and operator-= with double input
// (fractional second addition).
TEST(DateTime, AddingWithFractionalSeconds) {
  static const uint32 kOneNanoSecond = 1;
  DateTime d;
  d.SetYear(2009);
  d.SetMonth(12);
  d.SetDay(31);
  d.SetHour(23);
  d.SetMinute(59);
  d.SetSecond(59);
  d.SetNanosecond(kNanoSecondsPerSecond / 2);
  d += 0.25;

  // Expect that, within 1 nanosecond rounding error, we are now at
  // .75 seconds.
  EXPECT_EQ(59, d.GetMinute());
  EXPECT_EQ(59, d.GetSecond());
  EXPECT_NEAR(d.GetNanosecond(),
      3 * (kNanoSecondsPerSecond / 4), kOneNanoSecond);
  std::string dtstring = d.ToString();
  EXPECT_EQ(std::string("2009-12-31T23:59:59.75Z"), dtstring);

  // We should now be at 0.25 nanoseconds.
  d -= 0.5;
  EXPECT_EQ(59, d.GetSecond());
  EXPECT_EQ(59, d.GetMinute());
  EXPECT_NEAR(d.GetNanosecond(), kNanoSecondsPerSecond / 4, kOneNanoSecond);
  dtstring = d.ToString();
  EXPECT_EQ(std::string("2009-12-31T23:59:59.25Z"), dtstring);

  // Check that we roll over into the year 2010.
  d += 66.25;
  EXPECT_EQ(2010, d.GetYear());
  EXPECT_EQ(1, d.GetMonth());
  EXPECT_EQ(1, d.GetDay());
  EXPECT_EQ(0, d.GetHour());
  EXPECT_EQ(1, d.GetMinute());
  EXPECT_EQ(5, d.GetSecond());
  EXPECT_NEAR(d.GetNanosecond(), kNanoSecondsPerSecond / 2, kOneNanoSecond);
  dtstring = d.ToString();
  EXPECT_EQ(std::string("2010-01-01T00:01:05.5Z"), dtstring);

  // Check adding negative numbers.
  d += -5.25;
  EXPECT_EQ(2010, d.GetYear());
  EXPECT_EQ(1, d.GetMonth());
  EXPECT_EQ(1, d.GetDay());
  EXPECT_EQ(0, d.GetHour());
  EXPECT_EQ(1, d.GetMinute());
  EXPECT_EQ(0, d.GetSecond());
  EXPECT_NEAR(d.GetNanosecond(), kNanoSecondsPerSecond / 4, kOneNanoSecond);
  dtstring = d.ToString();
  EXPECT_EQ(std::string("2010-01-01T00:01:00.25Z"), dtstring);

  // Check subtracting negative numbers.
  d -= -3600.5;
  EXPECT_EQ(2010, d.GetYear());
  EXPECT_EQ(1, d.GetMonth());
  EXPECT_EQ(1, d.GetDay());
  EXPECT_EQ(1, d.GetHour());
  EXPECT_EQ(1, d.GetMinute());
  EXPECT_EQ(0, d.GetSecond());
  EXPECT_NEAR(d.GetNanosecond(),
      3 * (kNanoSecondsPerSecond / 4), kOneNanoSecond);
  dtstring = d.ToString();
  EXPECT_EQ(std::string("2010-01-01T01:01:00.75Z"), dtstring);
}

TEST(DateTime, GetDurationSecs) {
  DateTime begin;
  DateTime end;

  const double kLargeSeconds = 1e10;

  // Test two times that are very large but one nanosecond apart.
  begin = DateTime::CreateFromPosixSeconds(kLargeSeconds);
  end = DateTime::CreateFromPosixSeconds(kLargeSeconds);
  EXPECT_EQ(0U, end.GetNanosecond());
  end.SetNanosecond(1);
  static const double kTightTolerance = 1e-12;
  double duration = DateTime::GetDurationSecs(begin, end);
  EXPECT_NEAR(duration, 1.0 / kNanoSecondsPerSecond, kTightTolerance);
  duration = DateTime::GetDurationSecs(end, begin);
  EXPECT_NEAR(duration, -1.0 / kNanoSecondsPerSecond, kTightTolerance);

  // Test two times that are very large but two seconds and one
  // nanosecond apart.
  begin = DateTime::CreateFromPosixSeconds(kLargeSeconds);
  end = DateTime::CreateFromPosixSeconds(kLargeSeconds + 2.0);
  EXPECT_EQ(0U, end.GetNanosecond());
  end.SetNanosecond(1);
  duration = DateTime::GetDurationSecs(begin, end);
  EXPECT_NEAR(duration, 2.0 + 1.0 / kNanoSecondsPerSecond, kTightTolerance);
  duration = DateTime::GetDurationSecs(end, begin);
  EXPECT_NEAR(duration, -2.0 - 1.0 / kNanoSecondsPerSecond, kTightTolerance);

  // Test two times that are very large and very far apart.
  begin = DateTime::CreateFromPosixSeconds(kLargeSeconds);
  end = DateTime::CreateFromPosixSeconds(kLargeSeconds * 2.0);
  EXPECT_EQ(0U, end.GetNanosecond());
  end.SetNanosecond(1);
  duration = DateTime::GetDurationSecs(begin, end);
  EXPECT_NEAR(duration, 1.0 * kLargeSeconds, kTightTolerance);
  duration = DateTime::GetDurationSecs(end, begin);
  EXPECT_NEAR(duration, -1.0 * kLargeSeconds, kTightTolerance);
}

TEST(DateTime, ParseYMString) {
  DateTime datetime(-1, 0, 0, 0, 0, 0, 0);
  EXPECT_TRUE(DateTime::ParseYMString("2012-01", &datetime));
  EXPECT_EQ(datetime.GetYear(), 2012);
  EXPECT_EQ(datetime.GetMonth(), 1);
  EXPECT_TRUE(DateTime::ParseYMString("2011-12", &datetime));
  EXPECT_EQ(datetime.GetYear(), 2011);
  EXPECT_EQ(datetime.GetMonth(), 12);
  EXPECT_FALSE(DateTime::ParseYMString("2011-6", &datetime));
  EXPECT_FALSE(DateTime::ParseYMString("201112", &datetime));
}

TEST(DateTime, ToFromPosixSeconds) {
  DateTime d_time;
  d_time = DateTime::CreateFromPosixSeconds(0.0, 0, 0);
  EXPECT_EQ(DateTime(1970, 1, 1, 0, 0, 0, 0).GetPosixSeconds<int64>(),
            d_time.GetPosixSeconds<int64>());

  // Test for different POSIX time (30 seconds).
  d_time.Set(1970, 1, 1, 0, 0, 30, 0, 0, 0);
  EXPECT_EQ(30LL, d_time.GetPosixSeconds<int64>());

  // Test for different time zone.
  d_time.Set(1970, 1, 1, 0, 0, 30, 0, 1, 1);
  EXPECT_EQ(-3630LL, d_time.GetPosixSeconds<int64>());
}

TEST(DateTime, ComparisonOperators) {
  DateTime dt_initial(1970LL, 1, 1, 0, 0, 0, 100, 0, 0);
  DateTime dta(dt_initial);
  DateTime dtb(dt_initial);
  EXPECT_TRUE(dta == dtb);
  EXPECT_TRUE(dta >= dtb);
  EXPECT_TRUE(dta <= dtb);

  dta.SetYear(1971LL);
  COHelper(dta, dtb);

  dtb.SetMonth(2);
  dtb.SetDay(2);
  dtb.SetHour(2);
  dtb.SetMinute(2);
  dtb.SetSecond(2);
  dtb.SetNanosecond(200);
  COHelper(dta, dtb);

  dtb.SetZoneHours(2);
  dtb.SetZoneMinutes(2);
  COHelper(dta, dtb);

  // TimeZone comparison will be tested later.
  dtb.SetZoneHours(0);
  dtb.SetZoneMinutes(0);

  // Now year field is equal and all dtb fields are greater.
  dta.Set(dt_initial);
  COHelper(dtb, dta);

  dtb.SetMonth(1);
  COHelper(dtb, dta);

  dtb.SetDay(1);
  COHelper(dtb, dta);

  dtb.SetHour(0);
  COHelper(dtb, dta);

  dtb.SetMinute(0);
  COHelper(dtb, dta);

  dtb.SetSecond(0);
  COHelper(dtb, dta);

  // Setting nanosecond to 100 should equate them now, hence no helper.
  dtb.SetNanosecond(100);
  EXPECT_TRUE(dta == dtb);
  EXPECT_TRUE(dta >= dtb);
  EXPECT_TRUE(dta <= dtb);

  // Test time-zone difference comparison.
  dtb.SetZoneMinutes(10);
  COHelper(dta, dtb);
  dtb.SetZoneMinutes(-10);
  COHelper(dtb, dta);
  dtb.SetZoneMinutes(0);

  dtb.SetZoneHours(2);
  COHelper(dta, dtb);
  dtb.SetZoneHours(-2);
  COHelper(dtb, dta);
}

}  // namespace base
}  // namespace ion
