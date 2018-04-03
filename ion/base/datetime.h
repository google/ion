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

// Copyright 2005 Google Inc. All Rights Reserved.
//
#ifndef ION_BASE_DATETIME_H_
#define ION_BASE_DATETIME_H_

#include <chrono>  // NOLINT
#include <cmath>
#include <sstream>
#include <string>
#include <type_traits>

#include "base/integral_types.h"
#include "ion/base/referent.h"
#include "ion/external/gtest/gunit_prod.h"  // For FRIEND_TEST().
#include "absl/memory/memory.h"

namespace ion {
namespace base {

// DateTime represents a particular date and time down to the nanosecond level
// along with timezone information. This class also provides functions for
// parsing string representations of date and time, and computing a number of
// output formats (std::chrono::system_clock::time_point, seconds since the
// POSIX epoch, and string representations).
// The calendar used for DateTime is the proleptic Gregorian calendar as
// defined in ISO 8601.  In particular, year 0 exists in this calendar, and is a
// leap year.
//
// See: https://en.wikipedia.org/wiki/Proleptic_Gregorian_calendar
class DateTime {
 public:
  // Enumeration of the time-value fields of DateTime (used for iteration
  // and numerical access).
  // Note: Localization code could rearrange these based on locale to
  // properly order the date fields.
  enum DateTimeField {
    kYear = 0,
    kMonth = 1,
    kDay = 2,
    kHour = 3,
    kMinute = 4,
    kSecond = 5,
    kNanosecond = 6,
    kNumFields
  };

  // A class to contain a beginning and ending DateTime.
  class Range {
   public:
    Range() {
      begin_ = absl::make_unique<DateTime>();
      end_ = absl::make_unique<DateTime>();
    }
    Range(const Range& rhs) {
      *begin_ = rhs.begin();
      *end_ = rhs.end();
    }
    ~Range() {}

    const DateTime& begin() const { return *begin_; }
    const DateTime& end() const { return *end_; }

    void SetBegin(const DateTime& begin) { *begin_ = begin; }
    void SetEnd(const DateTime& end) { *end_ = end; }

    void SetInterpolation(const Range& begin,
        const Range& end,
        float t) {
      *begin_ = DateTime::Interpolate(begin.begin(), end.begin(), t);
      *end_ = DateTime::Interpolate(begin.end(), end.end(), t);
    }

    static Range Interpolate(const Range& begin,
        const Range& end,
        float t) {
      Range interpolated;
      interpolated.SetInterpolation(begin, end, t);
      return interpolated;
    }

   private:
    std::unique_ptr<DateTime> begin_;
    std::unique_ptr<DateTime> end_;
  };

  DateTime();
  DateTime(const DateTime& rhs);

  // Note: |requested_zone_hours| and |requested_zone_minutes| describe the
  // resultant DateTime, not the input secs.
  DateTime(int64 year, uint8 month, uint8 day, uint8 hour, uint8 minute,
           uint8 second, uint32 nanosecond = 0, int8 zone_hours = 0,
           int8 zone_minutes = 0);

  explicit DateTime(std::chrono::system_clock::time_point time,
                    int8 zone_hours = 0, int8 zone_minutes = 0);

  // Create a DateTime as an offset (in seconds) from the POSIX epoch, i.e.,
  // 00:00:00 01 January 1970.
  // Note: |requested_zone_hours| and |requested_zone_minutes| describe the
  // resultant DateTime, not the input secs.
  template <typename Rep>
  static DateTime CreateFromPosixSeconds(Rep secs, int8 zone_hours = 0,
                                         int8 zone_minutes = 0) {
    DateTime date;
    date.Reset();
    if (std::is_integral<Rep>::value) {
      date.SetFromPosixSecondsOnly(static_cast<int64>(secs), zone_hours,
                                   zone_minutes);
    } else {
      // Increment |secs| by 0.5 ns, as we will be truncating the count of
      // nanoseconds into an integer subsequently.
      secs += static_cast<Rep>(0.5e-9);
      const Rep seconds = static_cast<Rep>(std::floor(secs));
      const Rep nanoseconds = static_cast<Rep>(1e9) * (secs - seconds);
      date.SetFromPosixSecondsOnly(static_cast<int64>(seconds), zone_hours,
                                   zone_minutes);
      date.nanosecond_ = static_cast<uint32>(nanoseconds);
    }
    return date;
  }

  virtual ~DateTime() {}

  // Accessors for all fields.
  int64 GetYear() const { return year_; }
  uint8 GetMonth() const { return month_; }
  uint8 GetDay() const { return day_; }
  uint8 GetHour() const { return hour_; }
  uint8 GetMinute() const { return minute_; }
  uint8 GetSecond() const { return second_; }
  uint32 GetNanosecond() const { return nanosecond_; }
  int8 GetZoneHours() const { return zone_hours_; }
  int8 GetZoneMinutes() const { return zone_minutes_; }

  // Mutators for individual fields.
  void SetYear(int64 year);
  void SetMonth(uint8 month);
  void SetDay(uint8 day);
  void SetHour(uint8 hour);
  void SetMinute(uint8 minute);
  void SetSecond(uint8 second);
  void SetNanosecond(uint32 nanosecond);
  void SetZoneHours(int8 zone_hours);
  void SetZoneMinutes(int8 zone_minutes);

  // Set the DateTime to default values (<year>/1/1T00:00:00.0Z00:00, where
  // <year> is set as |kUndefinedYear|.
  void Reset();

  void Set(int64 years,
           uint8 months,
           uint8 days,
           uint8 hours,
           uint8 minutes,
           uint8 seconds,
           int8 zone_hours,
           int8 zone_minutes);

  void Set(int64 years, uint8 months, uint8 days, uint8 hours, uint8 minutes,
           uint8 seconds, uint32 nanosecond, int8 zone_hours,
           int8 zone_minutes);

  void Set(const DateTime& other);

  // Converts this DateTime to UTC time (+0:00 time zone).
  void Normalize() { AdjustTimeZone(0, 0); }

  // Checks each component of the time object, including time zone.
  bool IsEqualByComponent(const DateTime& dtime) const;

  // This operator converts the date times to absolute and compares
  // the absolute times for equality (hence time zone comparison is omitted).
  bool operator==(const DateTime& dtime) const;

  bool operator>(const DateTime& dtime) const;
  bool operator<(const DateTime& dtime) const { return dtime > *this; }
  bool operator>=(const DateTime& dtime) const { return !(*this < dtime); }
  bool operator<=(const DateTime& dtime) const { return !(*this > dtime); }
  bool operator!=(const DateTime& dtime) const { return !(*this == dtime); }
  DateTime& operator=(const DateTime& rhs);

  // Return a std::chrono::system_clock::time_point.
  std::chrono::system_clock::time_point GetTimePoint() const;

  // Return the number of seconds offset from the POSIX epoch, as a given type
  // |Rep|.
  template <typename Rep = double>
  Rep GetPosixSeconds() const {
    const int64 posix_seconds = GetPosixSecondsOnly();
    return static_cast<Rep>(posix_seconds) +
           static_cast<Rep>(1e-9) * nanosecond_;
  }

  // Set the current time to the interpolation of the two given times
  // according to the interpolant, normalized on [0-1]. The result is in
  // whatever zone this DateTime was in before calling lerp.
  void Lerp(const DateTime& origin, const DateTime& target, double t);

  // Converts time to another time zone.
  void AdjustTimeZone(int new_hours, int new_mins);

  // Converts time to user-readable string.
  //
  // Example: "2009-12-31T23:59:59.75Z" See tests/datetime_test.cc for more
  // examples.
  std::string ToString() const;

  // Parses |str| into this DateTime object. The parsing code accepts
  // the union of xml:date, and xml:dateTime. See unit-test for
  // examples of valid strings that this regular expression parses.
  bool FromString(const std::string& str);

  // ComputeDateString() and ComputeTimeString() render the DateTime object to
  // a std::string.  DateStringEnum and TimeStringEnum specify how much of the
  // date or time to render.
  enum DateStringEnum {
    kRenderDayMonthYear = 1,
    kRenderMonthYear,
    kRenderYearOnly
  };
  enum TimeStringEnum {
    kRenderHoursMinutesSeconds = 1,
    kRenderHoursMinutes,
    kRenderHoursOnly
  };
  void ComputeDateString(const DateStringEnum output_date_format,
                         std::string* out_string) const;
  void ComputeTimeString(const TimeStringEnum output_time_format,
                         std::string* out_string) const;

  // Returns string for 'this', e.g., "2y3m18d", interpreted as a duration.
  // |fractional_seconds| supplied as arg since DateTime only supports integer
  // seconds.
  std::string ComputeDurationString(double fractional_seconds) const;

  // The Julian Day is the integer number of days that have elapsed since noon
  // on Monday, January 1, 4713 BC. The fractional component represents time
  // of day (starting at noon). Confusingly, the Julian Day bears little
  // relation to the Julian Calendar.
  //
  // Why is this important? Astronomers base most of their calculations on
  // Julian Days rather than UTC. This code was converted from equations
  // presented in "Astronomical Algorithms" by Jean Mesus.
  double GetJulianDate() const;

  // Convert "Standard" time to decimal time (or "French Revolutionary" time).
  // This represents time as a fraction of a day, and ignores year, month, and
  // day.
  // Interestingly, representing time as a decimal actually preceeded
  // representing length, volume, etc as decimal (the metric system).
  // However, it never caught on and lasted only two years (1793-1795). The
  // same law which repealed decimal time introduced the metric system.
  double GetTimeAsDecimal() const;

  // Returns a day decimal value relative to J2000, the epoch relative to
  // 2000 Jan 1.5 (12h on January 1) or JD 2451545.0.
  double GetJ2000Date() const {
    static const double kJulianDate2000 = 2451545.0;
    double julian_date = GetJulianDate();
    return julian_date - kJulianDate2000;
  }

  // Checks if the date is unset or marked undefined.
  bool IsUndefined() const { return year_ == kUndefinedYear; }

  // Changes this DateTime to mark it as undefined.
  void MakeUndefined() { year_ = kUndefinedYear; }

  void operator+=(int64 secs) {
    uint32 nanoseconds = nanosecond_;
    SetFromPosixSecondsOnly(GetPosixSecondsOnly() + secs, zone_hours_,
                            zone_minutes_);
    nanosecond_ = nanoseconds;
  }
  void operator-=(int64 secs) { *this += -secs; }

  // Regular int operators provided for compiler convenience so that
  // you don't have to static cast.
  void operator+=(int secs) { *this += static_cast<int64>(secs); }
  void operator+=(double secs);
  void operator-=(int secs) { *this += -secs; }
  void operator-=(double secs) { *this += -secs; }

  // Returns a linearly-interpolated DateTime between |begin| and |end| as
  // defined by parameter |t|. At t=0, this returns |begin| and at t=1
  // this returns |end|.
  static DateTime Interpolate(const DateTime& begin,
                              const DateTime& end,
                              double t);

  // Compute the duration from the first datetime to the second
  // datetime in seconds, down to nanosecond resolution.
  //
  // Use this method instead of subtracting GetPosixSeconds() values for
  // maximum precision.
  static double GetDurationSecs(const DateTime& begin, const DateTime& end);

  // Returns a double value representing the interpolation of |now|
  // with respect to |time_a| and |time_b|, where time_a is 0.0, and time_b is
  // 1.0.
  static double GetInterpValue(const DateTime& now,
                               const DateTime& time_a,
                               const DateTime& time_b);

  // Checks if given string is in YYYY-MM formatr. If |date| is properly
  // formatted, this sets |date_out| to the time date specified and returns
  // true, otherwise it leaves |date|_out untouched and returns false.
  static bool ParseYMString(const std::string& str, DateTime* date_out);

  // Returns a specific field value in the DateTime object as defined by
  // DateTimeField |field| (kYear, kMonth, etc.).
  int64 GetDateTimeField(DateTimeField field) const;

  // A convenience function to iterate through DateTime fields.
  int64 GetDateTimeField(uint8 field) const {
    DCHECK(field < static_cast<uint8>(kNumFields));
    return GetDateTimeField(static_cast<DateTimeField>(field));
  }

 protected:
  // Determines whether to render 24-hour time strings based on a value set by
  // translators.  Made virtual for testing purposes only.
  virtual bool Use24HourTime() const;

 private:
  // Set the current time to an offset (in seconds) from the POSIX epoch, i.e.,
  // 00:00:00 01 January 1970.  Note that the count of seconds is integral.
  void SetFromPosixSecondsOnly(int64 secs, int8 requested_zone_hours,
                               int8 requested_zone_minutes);

  // Get the current time as an offset (in seconds) from the POSIX epoch, i.e.,
  // 00:00:00 01 January 1970.  Note that the retrieved count of seconds is
  // integral.
  int64 GetPosixSecondsOnly() const;

  static const int64 kUndefinedYear = kint64max;

  // Years could be geologic (e.g. -4 billion).
  int64 year_;
  uint8 month_;
  uint8 day_;
  uint8 hour_;  // Always a 24-hour representation internally.
  uint8 minute_;
  uint8 second_;
  uint32 nanosecond_;
  int8 zone_hours_;
  int8 zone_minutes_;

  FRIEND_TEST(DateTime, Use24HourTime);
  FRIEND_TEST(DateTime, NewestOldestDateTime);
};

std::ostream& operator<<(std::ostream& os, const DateTime& dtime);
std::istream& operator>>(std::istream& in, DateTime& dtime);

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_DATETIME_H_
