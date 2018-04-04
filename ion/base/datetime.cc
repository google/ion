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

// Copyright 2007 Google Inc. All Rights Reserved.
//
#include "ion/base/datetime.h"

#include <cstring>  // For memset.
#include <regex>  // NOLINT

#include "base/macros.h"
#include "ion/base/logging.h"
#include "ion/base/stringutils.h"

namespace ion {
namespace base {

namespace {

static const int kNanoSecondsPerSecond = 1000000000;

std::string MakeNanoSecondString(uint8 second, uint32 nanosecond) {
  // We shouldn't have more than 60 seconds.
  DCHECK_LT(second, 60);

  // Build a second string with a width of 2 (seconds should always be
  // "05" not "5").
  static const std::string kSecondReplString = "%02u";
  char seconds_buffer[3];
  snprintf(seconds_buffer, sizeof(seconds_buffer), kSecondReplString.c_str(),
      second);

  if (nanosecond == 0) {
    return std::string(seconds_buffer);
  } else {
    // Check that there are fewer than one-second worth of nanoseconds
    // given, and take the modulo just to be sure.
    DCHECK_LT(nanosecond, static_cast<uint32>(kNanoSecondsPerSecond));
    nanosecond = nanosecond % kNanoSecondsPerSecond;

    // Truncate nanoseconds (necessary for uint representation).
    while (nanosecond % 10 == 0)
      nanosecond /= 10;

    static const char kResultFormat[] = "%s.%u";
    char result_buffer[256];
    snprintf(result_buffer, sizeof(result_buffer), kResultFormat,
        seconds_buffer, nanosecond);
    return std::string(result_buffer);
  }
}
}  // anonymous namespace

static inline int64 Quotient(int64 a, int64 b) {
  return static_cast<int64>(
      floor(static_cast<double>(a) / static_cast<double>(b)));
}

static inline int64 Quotient(int64 a, int64 low, int64 high) {
  return Quotient(a - low, high - low);
}

static inline int64 Modulo(int64 a, int64 b) { return a - Quotient(a, b) * b; }

static inline int64 Modulo(int64 a, int64 low, int64 high) {
  return Modulo(a - low, high - low) + low;
}

static inline int MaximumDayInMonthFor(int64 year, int month) {
  int m = static_cast<int>(Modulo(month, 1, 13));
  int64 y = year + Quotient(month, 1, 13);
  switch (m) {
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12:
      return 31;
      break;
    case 4:
    case 6:
    case 9:
    case 11:
      return 30;
      break;
    case 2:
      if ((Modulo(y, 400) == 0 || Modulo(y, 100) != 0) && Modulo(y, 4) == 0)
        return 29;
      return 28;
    default:
      DCHECK(0);
      break;
  }
  return -1;
}

static inline bool IsLeapYear(int64 y) {
  return (Modulo(y, 4) == 0 && (Modulo(y, 400) == 0 || Modulo(y, 100) != 0));
}

// Returns number of days within year up to a given month.
static inline int CumulativeDays(int64 y, int m) {
  static int month_days[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304,
                             334, 365};

  // m can be zero in the datastream, this just means that we
  // only know the year, and not the month that the data was
  // captured.
  if (m < 1)
    m = 1;
  DCHECK_LT(m, 13);  // m should always be less than 13
  return month_days[m - 1] + (m > 2 && IsLeapYear(y));
}

// Returns signed offset number of days from the year of the POSIX epoch (1970)
// to a given year.
//
// Examples:
//    CumulativeEpochDaysToYear(1970)  -> 0.
//    CumulativeEpochDaysToYear(1974)  -> 3 * 365 + 366  (1972 is leap).
//    CumulativeEpochDaysToYear(-1) -> -365 (1971 is not leap).
static inline int64 CumulativeEpochDaysToYear(int64 year) {
  // A year is leap if:
  // - Divisible by 400, or
  // - Divisible by 4 but not by 100.
  //
  // See http://en.wikipedia.org/wiki/Leap_year
  //
  // Positive and negative cases have to be dealt differently because
  // year zero (which is leap) should only be included for positive years
  // but not negative.
  int64 days_since_year_0 = 0;
  if (year > 0) {
    int64 prev_years = year - 1;
    // Note +1 for leap year zero.
    // Perform multiplications in uint64 domain to avoid undefined overflow
    // behavior.
    uint64 num_leap_years = static_cast<uint64>(
        (prev_years / 4 - prev_years / 100) + prev_years / 400 + 1);
    days_since_year_0 = static_cast<int64>(num_leap_years * 366 +
                                           (year - num_leap_years) * 365);
  } else if (year < 0) {
    int64 pos_years = -year;
    // Perform multiplications in uint64 domain to avoid undefined overflow
    // behavior.
    uint64 num_leap_years = static_cast<uint64>(
        (pos_years / 4 - pos_years / 100) + pos_years / 400);
    days_since_year_0 = -static_cast<int64>(num_leap_years * 366 +
                                            (pos_years - num_leap_years) * 365);
  } else {
    days_since_year_0 = 0;
  }

  // Subtract the number of days from year 0 to 1970.
  return days_since_year_0 - 719528;
}

DateTime::DateTime() { Reset(); }

void DateTime::Reset() {
  Set(kUndefinedYear, 1, 1, 0, 0, 0, 0, 0, 0);
}

DateTime::DateTime(const DateTime& rhs) {
  Set(rhs);
}

DateTime::DateTime(int64 year, uint8 month, uint8 day, uint8 hour,
                   uint8 minute, uint8 second, uint32 nanosecond,
                   int8 zone_hours, int8 zone_minutes) {
  Reset();
  Set(year, month, day, hour, minute, second, nanosecond, zone_hours,
      zone_minutes);
}

DateTime::DateTime(std::chrono::system_clock::time_point time, int8 zone_hours,
                   int8 zone_minutes) {
  Reset();
  // Note: this assumes that the epoch of std::chrono::system_clock is the POSIX
  // epoch.
  const std::chrono::nanoseconds time_since_epoch_nanos =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          time.time_since_epoch());
  const std::chrono::seconds time_since_epoch_seconds =
      std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch());
  SetFromPosixSecondsOnly(time_since_epoch_seconds.count(), zone_hours,
                          zone_minutes);
  nanosecond_ =
      std::chrono::duration_cast<std::chrono::duration<uint32, std::nano>>(
          time_since_epoch_nanos - time_since_epoch_seconds)
          .count();
}

void DateTime::SetYear(int64 year) {
  // No test is necessary since the year field can logically support any year
  // that fits in int64.
  year_ = year;
}

void DateTime::SetMonth(uint8 month) {
  // Zero is allowed for duration values.
  if (month > 12)
    LOG(ERROR) << "Invalid month " << month << " provided. Skipping set.";
  else
    month_ = month;
}

void DateTime::SetDay(uint8 day) {
  // Zero is allowed for duration values.
  if (day > MaximumDayInMonthFor(year_, month_))
    LOG(ERROR) << "Invalid day " << day << " provided for year/month "
        << year_ << "/" << month_ << ". Skipping set.";
  else
    day_ = day;
}

void DateTime::SetHour(uint8 hour) {
  if (hour > 23)
    LOG(ERROR) << "Invalid hour " << hour
        << " for 24-hour time representation. Skipping set.";
  else
    hour_ = hour;
}

void DateTime::SetMinute(uint8 minute) {
  if (minute > 59)
    LOG(ERROR) << "Invalid minute " << minute << " provided. Skipping set.";
  else
    minute_ = minute;
}

void DateTime::SetSecond(uint8 second) {
  if (second > 59)
    LOG(ERROR) << "Invalid second " << second << " provided. Skipping set.";
  else
    second_ = second;
}

void DateTime::SetNanosecond(uint32 nanosecond) {
  if (nanosecond >= kNanoSecondsPerSecond)
    LOG(ERROR) << "Invalid nanosecond " << nanosecond
        << " provided. Skipping set.";
  else
    nanosecond_ = nanosecond;
}

void DateTime::SetZoneHours(int8 zone_hours) {
  if (zone_hours < -12 || zone_hours > 14)
    LOG(ERROR) << "Invalid time zone hour " << zone_hours
        << " provided. Skipping set.";
  else
    zone_hours_ = zone_hours;
}

void DateTime::SetZoneMinutes(int8 zone_minutes) {
  if (zone_minutes < -59 || zone_minutes > 59)
    LOG(ERROR) << "Invalid time zone minute " << zone_minutes
               << " provided. Skipping set.";
  else
    zone_minutes_ = zone_minutes;
}


void DateTime::Set(int64 years,
                   uint8 months,
                   uint8 days,
                   uint8 hours,
                   uint8 minutes,
                   uint8 seconds,
                   int8 zone_hours,
                   int8 zone_minutes) {
  SetYear(years);
  SetMonth(months);
  SetDay(days);
  SetHour(hours);
  SetMinute(minutes);
  SetSecond(seconds);
  SetZoneHours(zone_hours);
  SetZoneMinutes(zone_minutes);
  SetNanosecond(0);
}

void DateTime::Set(int64 years,
                   uint8 months,
                   uint8 days,
                   uint8 hours,
                   uint8 minutes,
                   uint8 seconds,
                   uint32 nanosecond,
                   int8 zone_hours,
                   int8 zone_minutes) {
  Set(years, months, days, hours, minutes, seconds, zone_hours, zone_minutes);
  SetNanosecond(nanosecond);
}

void DateTime::Set(const DateTime& other) {
  Set(other.year_, other.month_, other.day_, other.hour_, other.minute_,
      other.second_, other.nanosecond_, other.zone_hours_, other.zone_minutes_);
}

std::string DateTime::ToString() const {
  char buf[256] = { 0 };
  int64 year = year_;
  if (hour_ == 0 && minute_ == 0 && second_ == 0 && nanosecond_ == 0 &&
      zone_hours_ == 0 && zone_minutes_ == 0) {
    if (month_ == 1 && day_ == 1)
      snprintf(buf, sizeof(buf), "%04" GG_LL_FORMAT "d", year);
    else if (day_ == 1)
      snprintf(buf, sizeof(buf), "%04" GG_LL_FORMAT "d-%02d", year, month_);
    else
      snprintf(buf,
               sizeof(buf),
               "%04" GG_LL_FORMAT "d-%02d-%02d",
               year,
               month_,
               day_);
  } else {
    std::string second_string = MakeNanoSecondString(second_, nanosecond_);
    if (zone_hours_ == 0 && zone_minutes_ == 0) {
      snprintf(buf,
               256,
               "%04" GG_LL_FORMAT "d-%02d-%02dT%02d:%02d:%sZ",
               year,
               month_,
               day_,
               hour_,
               minute_,
               second_string.c_str());
    } else {
      char sign;
      if (zone_hours_ == 0)
        sign = zone_minutes_ >= 0 ? '+' : '-';
      else
        sign = zone_hours_ >= 0 ? '+' : '-';
      snprintf(buf,
               256,
               "%04" GG_LL_FORMAT "d-%02d-%02dT%02d:%02d:%s%c%02d:%02d",
               year,
               month_,
               day_,
               hour_,
               minute_,
               second_string.c_str(),
               sign,
               abs(zone_hours_),
               abs(zone_minutes_));
    }
  }
  return std::string(buf);
}

std::ostream& operator<<(std::ostream& os, const DateTime& dtime) {
  os << dtime.ToString();
  return os;
}

bool DateTime::FromString(const std::string& str) {
  DateTime result;
  // Set all values to defaults, so that optional values are correctly
  // set if not specified.
  result.Reset();

  // Note that this parsing code accepts the union of xml:date, and
  // xml:dateTime. See unit-test for examples of valid strings that
  // this regular expression parses.

  // Comments inlined about the regular expression syntax.
  static const std::regex re(
      "((?:-)?\\d+)"  // year (mandatory). Optional "-" sign.
      // Cap 1 is the year digits and the optional minus sign.
      "(?:-(\\d{2})"        // "-" month (optional). Cap 2 is the month digits.
      "(?:-(\\d{2})"        // "-" day (optional). Cap 3 is the day digits.
      "(?:T"                // Time Delimiter (optional).
      "(\\d{2})"            // Hour (Mandatory after "T"). Cap 4 is hour digits.
      "(?::(\\d{2})"        // ":" minutes (optional). Cap 5 is minute digits.
      "(?::(\\d{2})"        // ":" seconds (optional). Cap 6 is second digits.
      "(?:\\.(\\d+))?"      // Decimal seconds. Cap 7 is sec. digits after ".".
      ")?)?)?"              // Closing optional time string.
      "(?:(?:Z)|(?:([+-])"  // Opening of TimeZone string "Z" "+" or "-".
      // Cap 8 is "+" or "-"
      "(\\d{2})"        // Zone Hours. Capture 9 is zone-hours digits.
      "(?::(\\d{2}))?"  // ":" + Zone Minutes. Cap10 is the minute digits.
      ")?"              // Closing TimeZone group.
      ")?)?)?");        // Closing all optional tags.

  std::smatch date_regex_match;
  if (!std::regex_match(str, date_regex_match, re)) {
    LOG(WARNING) << "Couldn't parse DateTime\n";
    return false;
  }
  DCHECK_GE(date_regex_match.size(), 11);
  {
    int64 year;
    std::stringstream ss(date_regex_match.str(1));
    ss >> year;
    result.SetYear(year);
  }

  // Months.
  if (date_regex_match.str(2).empty()) {
    this->Set(result);
    return true;
  } else {
    std::stringstream ss(date_regex_match.str(2));
    uint16 ushort_val;
    ss >> ushort_val;
    result.SetMonth(static_cast<uint8>(ushort_val));
  }

  // Days.
  if (date_regex_match.str(3).empty()) {
    this->Set(result);
    return true;
  } else {
    std::stringstream ss(date_regex_match.str(3));
    uint16 ushort_val = 0;
    ss >> ushort_val;
    result.SetDay(static_cast<uint8>(ushort_val));
  }

  // Process zone-hours and zone-minutes here in case time isn't
  // specified. Zone Hours and Zone Minutes are still legal at this
  // point in XML date.
  if (!date_regex_match.str(8).empty()) {  // "+" or "-".
    // Zone Hours.
    std::stringstream ss(date_regex_match.str(9));
    int16 short_val = 0;
    ss >> short_val;
    int8 zone_hours = static_cast<int8>(short_val);

    // Zone Minutes.
    int8 zone_minutes = 0;
    if (!date_regex_match.str(10).empty()) {
      std::stringstream ss(date_regex_match.str(10));
      int16 short_val = 0;
      ss >> short_val;
      zone_minutes = static_cast<int8>(short_val);
    }

    // Negate zone hours and minutes if necessary.
    if (date_regex_match.str(8) == "-") {
      zone_hours = static_cast<int8>(-zone_hours);
      zone_minutes = static_cast<int8>(-zone_minutes);
      DCHECK_LE(zone_hours, 0);
      DCHECK_LE(zone_minutes, 0);
    }

    result.SetZoneHours(zone_hours);
    result.SetZoneMinutes(zone_minutes);
  }

  // Hours.
  if (date_regex_match.str(4).empty()) {
    this->Set(result);
    return true;
  } else {
    std::stringstream ss(date_regex_match.str(4));
    uint16 ushort_val = 0;
    ss >> ushort_val;
    result.SetHour(static_cast<uint8>(ushort_val));
  }

  // Minutes.
  if (date_regex_match.str(5).empty()) {
    this->Set(result);
    return true;
  } else {
    std::stringstream ss(date_regex_match.str(5));
    uint16 ushort_val = 0;
    ss >> ushort_val;
    result.SetMinute(static_cast<uint8>(ushort_val));
  }

  // Seconds.
  if (date_regex_match.str(6).empty()) {
    this->Set(result);
    return true;
  } else {
    std::stringstream ss(date_regex_match.str(6));
    uint16 ushort_val = 0;
    ss >> ushort_val;
    result.SetSecond(static_cast<uint8>(ushort_val));
  }

  // Fractional Seconds.
  if (date_regex_match.str(7).empty()) {
    this->Set(result);
    return true;
  } else {
    int32 nanosecond;
    size_t num_nanosecond_letters = date_regex_match.str(7).length();
    static const size_t kNumNanoSecondLetters = 9;

    std::stringstream ss(date_regex_match.str(7));
    ss >> nanosecond;
    for (size_t i = num_nanosecond_letters; i < kNumNanoSecondLetters; ++i) {
      nanosecond *= 10;
    }
    result.SetNanosecond(nanosecond);
  }

  DCHECK_LE(result.GetNanosecond(), static_cast<uint32>(kNanoSecondsPerSecond));
  this->Set(result);
  return true;
}

std::istream& operator>>(std::istream& in, DateTime& dtime) {
  // Save the current position in the steam in case we need to restore it.
  std::streampos pos = in.tellg();
  std::string s;
  in >> s;
  if (!dtime.FromString(s)) {
    // Restore the stream.
    in.seekg(pos);
    in.setstate(std::ios_base::failbit);
  }
  return in;
}

void DateTime::ComputeDateString(const DateStringEnum output_date_format,
                                 std::string* out_string) const {
  static const char kDayMonthYearFormat[] = "%d/%d/%s";
  static const char kMonthYearFormat[] = "%d/%s";
  static const int64 kOneBillion = 1000000000LL;
  static const int64 k100Million = 100000000LL;
  static const int64 kTenMillion = 10000000LL;
  static const int64 kOneMillion = 1000000LL;

  // Compute the year string.
  char year_buffer[256];
  if (year_ < 0) {
    static const char kSingleBCEFormat[] = "%d BCE";
    static const char kMillionBCEFormat[] = "%.2f Million BCE";
    static const char k10MillionBCEFormat[] = "%.1f Million BCE";
    static const char k100MillionBCEFormat[] = "%d Million BCE";
    static const char kBillionBCEFormat[] = "%.2f Billion BCE";

    if (year_ <= -kOneBillion)
      snprintf(year_buffer, sizeof(year_buffer), kBillionBCEFormat,
          static_cast<double>(-year_) / kOneBillion);
    else if (year_ <= -k100Million)
      // 123 Million.
      snprintf(year_buffer, sizeof(year_buffer), k100MillionBCEFormat,
          static_cast<int>(-year_ / kOneMillion));
    else if (year_ <= -kTenMillion)
      snprintf(year_buffer, sizeof(year_buffer), k10MillionBCEFormat,
          static_cast<double>(-year_) / kOneMillion);
    else if (year_ <= -kOneMillion)
      snprintf(year_buffer, sizeof(year_buffer), kMillionBCEFormat,
          static_cast<double>(-year_) / kOneMillion);
    else
      snprintf(year_buffer, sizeof(year_buffer), kSingleBCEFormat,
          static_cast<int>(-year_));
  } else {
    snprintf(year_buffer, sizeof(year_buffer), "%4d", static_cast<int>(year_));
  }
  DCHECK_NE(std::string(""), std::string(year_buffer));

  // Render the output string.
  switch (output_date_format) {
    case kRenderDayMonthYear: {
      DCHECK_GE(month_, 1);
      DCHECK_LE(month_, 12);
      char out_buffer[256];
      snprintf(out_buffer, sizeof(out_buffer), kDayMonthYearFormat,
          month_, day_, year_buffer);
      *out_string = std::string(out_buffer);
      break;
    }
    case kRenderMonthYear: {
      DCHECK_GE(month_, 1);
      DCHECK_LE(month_, 12);
      char out_buffer[256];
      snprintf(out_buffer, sizeof(out_buffer), kMonthYearFormat,
          month_, year_buffer);
      *out_string = std::string(out_buffer);
      break;
    }
    case kRenderYearOnly: {
      *out_string = std::string(year_buffer);
      break;
    }
    default:
      DCHECK(false) << "Invalid DateStringEnum passed to "
                       "DateTime::ComputeDateString()";
  }
}

void DateTime::ComputeTimeString(const TimeStringEnum output_time_format,
                                 std::string* out_string) const {
  // Compute 12-hour time and am/pm state if necessary.
  int hour_value;
  bool pm_flag = false;
  if (Use24HourTime()) {
    hour_value = hour_;
  } else {
    if (hour_ > 12) {
      pm_flag = true;
      hour_value = hour_ - 12;
    } else {
      hour_value = hour_ == 0 ? 12 : hour_;
      pm_flag = hour_ == 12 ? true : false;
    }
  }

  static const char kHMSFormat[] = " %d:%02d:%02d%s";
  static const char kHMFormat[] = " %d:%02d%s";
  static const char kHFormat[] = " %d%s";

  // Render the output string.
  char output_buffer[256];
  switch (output_time_format) {
    case kRenderHoursMinutesSeconds:
      snprintf(output_buffer, sizeof(output_buffer), kHMSFormat,
          hour_value,
          minute_,
          second_,
          Use24HourTime() ? "" : (pm_flag ? " pm" : " am"));
      break;
    case kRenderHoursMinutes:
      snprintf(output_buffer, sizeof(output_buffer), kHMFormat,
          hour_value,
          minute_,
          Use24HourTime() ? "" : (pm_flag ? " pm" : " am"));
      break;
    case kRenderHoursOnly:
      snprintf(output_buffer, sizeof(output_buffer), kHFormat,
          hour_value,
          Use24HourTime() ? "" : (pm_flag ? " pm" : " am"));
      break;
    default:
      DCHECK(false) << "Invalid TimeStringEnum passed to "
                       "DateTime::ComputeTimeString()";
  }
  *out_string = std::string(output_buffer);
}

bool DateTime::Use24HourTime() const {
  // Set this string to 24 (just two numerals) to use 24-hour time format,
  // otherwise, this will use am/pm format.
  static const std::string kFormatToUse("using am/pm time format");
  return kFormatToUse == "24";
}

std::string DateTime::ComputeDurationString(double fractional_seconds) const {
  static const char kFieldSuffix[] = "ymdhms";

  // Find largest non-zero field.
  // Note: This function doesn't support the nanosecond field to match its
  // previous implementation.
  uint8 field_index = 0;
  while (field_index < static_cast<uint8>(kNanosecond)
      && GetDateTimeField(field_index) == 0) {
    ++field_index;
  }

  // Check if all (cared-about) fields are 0.
  if (field_index == static_cast<uint8>(kNanosecond))
    return std::string("0.0s");

  // Output at most 3 contiguous fields, skipping 0-value fields.
  char output_buffer[256];
  int output_chars = 0;
  memset(output_buffer, static_cast<int>('\0'), 256 * sizeof(char));
  uint8 output_field_index;
  for (output_field_index = 0
      ; output_field_index < 3 && field_index != static_cast<uint8>(kSecond)
      ; ++output_field_index, ++field_index) {
    int64 value = GetDateTimeField(field_index);
    if (value == 0)
      continue;

    output_chars += snprintf(&output_buffer[0] + output_chars,
        256 - output_chars, "%llu%c ", value, kFieldSuffix[field_index]);
  }

  // Handle seconds differently since fractional seconds may need to be printed.
  if (field_index == static_cast<uint8>(kSecond) && output_field_index < 3) {
    snprintf(&output_buffer[0] + output_chars, 256 - output_chars,
        output_field_index < 2 ? "%.1f%c " : "%.0f%c",
        static_cast<double>(second_) + fractional_seconds,
        kFieldSuffix[field_index]);
  }

  return ion::base::TrimStartAndEndWhitespace(std::string(output_buffer));
}

bool DateTime::operator>(const DateTime& dtime) const {
  DateTime norm_this(*this);
  norm_this.Normalize();
  DateTime norm_other(dtime);
  norm_other.Normalize();
  if (norm_this.year_ > norm_other.year_)
    return true;
  else if (norm_this.year_ < norm_other.year_)
    return false;
  else if (norm_this.month_ > norm_other.month_)
    return true;
  else if (norm_this.month_ < norm_other.month_)
    return false;
  else if (norm_this.day_ > norm_other.day_)
    return true;
  else if (norm_this.day_ < norm_other.day_)
    return false;
  else if (norm_this.hour_ > norm_other.hour_)
    return true;
  else if (norm_this.hour_ < norm_other.hour_)
    return false;
  else if (norm_this.minute_ > norm_other.minute_)
    return true;
  else if (norm_this.minute_ < norm_other.minute_)
    return false;
  else if (norm_this.second_ > norm_other.second_)
    return true;
  else if (norm_this.second_ < norm_other.second_)
    return false;
  else if (norm_this.nanosecond_ > norm_other.nanosecond_)
    return true;
  else if (norm_this.nanosecond_ < norm_other.nanosecond_)
    return false;

  return false;
}

bool DateTime::operator==(const DateTime& dtime) const {
  DateTime norm_this(*this);
  DateTime norm_other(dtime);
  norm_this.Normalize();
  norm_other.Normalize();
  return (norm_this.year_ == norm_other.year_ &&
          norm_this.month_ == norm_other.month_ &&
          norm_this.day_ == norm_other.day_ &&
          norm_this.hour_ == norm_other.hour_ &&
          norm_this.minute_ == norm_other.minute_ &&
          norm_this.second_ == norm_other.second_ &&
          norm_this.nanosecond_ == norm_other.nanosecond_ &&
          norm_this.zone_hours_ == norm_other.zone_hours_ &&
          norm_this.zone_minutes_ == norm_other.zone_minutes_);
}

bool DateTime::IsEqualByComponent(const DateTime& dtime) const {
  return (
      year_ == dtime.year_ && month_ == dtime.month_ && day_ == dtime.day_ &&
      hour_ == dtime.hour_ && minute_ == dtime.minute_ &&
      second_ == dtime.second_ && nanosecond_ == dtime.nanosecond_ &&
      zone_hours_ == dtime.zone_hours_ && zone_minutes_ == dtime.zone_minutes_);
}

std::chrono::system_clock::time_point DateTime::GetTimePoint() const {
  // Note: this assumes that the epoch of std::chrono::system_clock is the POSIX
  // epoch.
  int64 posix_seconds = GetPosixSecondsOnly();
  return std::chrono::system_clock::time_point(
      std::chrono::duration_cast<std::chrono::system_clock::duration>(
          std::chrono::seconds(posix_seconds) +
          std::chrono::nanoseconds(nanosecond_)));
}

double DateTime::GetJulianDate() const {
  int month = month_;
  int year = static_cast<int>(year_);
  // If January or Feburary, consider this to be the 13th or
  // 14th month of the preceding year.
  if (month_ == 1 || month_ == 2) {
    year -= 1;
    month += 12;
  }

  // Convert the hours to a fractional day.
  double time = GetTimeAsDecimal();

  // NOTE: This block of code is magic, as presented on page 61 of the
  // book. I am confident it works only because I've unit-tested it.
  double a = floor(year / 100.0);
  double b = 2.0 - a + floor(a / 4.0);
  double c = floor(365.25 * (year + 4716.0));
  double d = floor(30.6001 * (month + 1.0));
  double day = c + d + day_ + b - 1524.5;

  return day + time;
}

double DateTime::GetTimeAsDecimal() const {
  double val = nanosecond_;
  val = val * 1e-9 + second_;
  val = val / 60.0 + minute_;
  val = val / 60.0 + hour_;
  val = val / 24.0;
  return val;
}

void DateTime::AdjustTimeZone(int new_hours, int new_mins) {
  if (new_hours != zone_hours_ || new_mins != zone_minutes_) {
    uint32 nanoseconds = nanosecond_;
    // Convert this time to seconds and back, ignoring the nanosecond
    // component, and then add it back in after correcting the timezone.
    SetFromPosixSecondsOnly(GetPosixSecondsOnly(), static_cast<int8>(new_hours),
                            static_cast<int8>(new_mins));
    nanosecond_ = nanoseconds;  // Preserve old nanoseconds.
  }
}

DateTime& DateTime::operator=(const DateTime& rhs) {
  if (this != &rhs) {
    Set(rhs);
  }
  return *this;
}

DateTime DateTime::Interpolate(const DateTime& begin,
                               const DateTime& end,
                               double t) {
  if (t == 1.0)
    return end;
  double interp_offset = GetDurationSecs(begin, end) * t;
  DateTime ret_val = begin;
  ret_val += interp_offset;
  return ret_val;
}

double DateTime::GetDurationSecs(const DateTime& begin, const DateTime& end) {
  // Get the second interpretation of these DateTimes without rounding
  // the Nanoseconds into the calculation.
  int64 begin_secs = begin.GetPosixSecondsOnly();
  int64 end_secs = end.GetPosixSecondsOnly();

  // Increase precision by doing calculation in the difference between
  // the two times. This will give us enough precision to factor
  // nanoseconds into the calculation when the times are close.
  double duration = static_cast<double>(end_secs - begin_secs);

  // Add the nanoseconds into the offset calculation.
  duration += (static_cast<int32>(end.nanosecond_) -
               static_cast<int32>(begin.nanosecond_)) *
              1e-9;
  return duration;
}

double DateTime::GetInterpValue(const DateTime& now,
                                const DateTime& time_a,
                                const DateTime& time_b) {
  // Convert to seconds, ignoring the nanosecond component.
  int64 a_secs = time_a.GetPosixSecondsOnly();
  int64 b_secs = time_b.GetPosixSecondsOnly();
  int64 now_secs = now.GetPosixSecondsOnly();

  double time_range = static_cast<double>(b_secs - a_secs);
  // Add the nanoseconds in.
  time_range += (static_cast<int32>(time_b.nanosecond_) -
                 static_cast<int32>(time_a.nanosecond_)) *
                1e-9;

  if (time_range == 0.0)
    return 0.0;

  double time_from_a = static_cast<double>(now_secs - a_secs);
  time_from_a += (static_cast<int32>(now.nanosecond_) -
                  static_cast<int32>(time_a.nanosecond_)) *
                 1e-9;
  return time_from_a / time_range;
}

void DateTime::operator+=(double secs) {
  int64 total_seconds = GetPosixSecondsOnly();
  int32 total_nanoseconds = nanosecond_;
  // Overflow of uint32 may be a problem in this algorithm if
  // nanosecond_ is ever in a bad-state and is more than
  // kNanoSecondsPerSecond.
  DCHECK_LT(total_nanoseconds, kNanoSecondsPerSecond);

  double seconds = 0.0;
  double nanoseconds = kNanoSecondsPerSecond * std::modf(secs, &seconds);
  total_seconds += static_cast<int64>(seconds);
  total_nanoseconds += static_cast<int32>(nanoseconds);
  if (total_nanoseconds >= kNanoSecondsPerSecond) {
    total_seconds += 1;
    total_nanoseconds -= kNanoSecondsPerSecond;
  } else if (total_nanoseconds < 0) {
    total_seconds -= 1;
    total_nanoseconds += kNanoSecondsPerSecond;
  }

  SetFromPosixSecondsOnly(total_seconds, zone_hours_, zone_minutes_);
  nanosecond_ = total_nanoseconds;
}

bool DateTime::ParseYMString(const std::string& str, DateTime* date_out) {
  if (str.size() == 7) {
    if (str[4] != '-') return false;
    // Check month.
    if (!isdigit(str[5])) return false;
    if (!isdigit(str[6])) return false;
    // Check year.
    for (int i = 0; i < 4; ++i) {
      if (!isdigit(str[i])) return false;
    }
    if (date_out)
      date_out->Set(
          ion::base::StringToInt32(str.substr(0, 4)),
          static_cast<uint8>(ion::base::StringToInt32(str.substr(5, 2))), 0, 0,
          0, 0, 0, 0);
    return true;
  }
  return false;
}

void DateTime::Lerp(const DateTime& origin, const DateTime& target, double t) {
  *this = Interpolate(origin, target, t);
}

int64 DateTime::GetDateTimeField(DateTimeField field) const {
  switch (field) {
    case kYear:
      return year_;
    case kMonth:
      return month_;
    case kDay:
      return day_;
    case kHour:
      return hour_;
    case kMinute:
      return minute_;
    case kSecond:
      return second_;
    case kNanosecond:
      return nanosecond_;
    default: {
      LOG(ERROR) << "Invalid DateTime field provided to GetDateTimeField().";
      return -1;
    }
  }
}

void DateTime::SetFromPosixSecondsOnly(int64 secs, int8 requested_zone_hours,
                                       int8 requested_zone_minutes) {
  // Initialize to time zero in requested zone.
  Set(0, 1, 1, 0, 0, 0, requested_zone_hours, requested_zone_minutes);

  // Adjust secs to be an offset of requested zone instead of UTC+0.
  int64 zh = static_cast<int64>(requested_zone_hours);
  int64 zm = static_cast<int64>(requested_zone_minutes);
  secs += ((zh * 60) + zm) * 60;

  // Get total number of days.
  int64 days = secs / (24 * 60 * 60);

  // Set seconds.
  secs -= days * (24 * 60 * 60);
  second_ = static_cast<uint8>(Modulo(secs, 60));
  // Set minutes.
  int64 mins = Quotient(secs, 60);
  minute_ = static_cast<uint8>(Modulo(mins, 60));
  // Set hours.
  int64 hours = Quotient(mins, 60);
  hour_ = static_cast<uint8>(Modulo(hours, 24));
  // Update total number of days.
  days += Quotient(hours, 24);

  // Find out closest year by iteratively guessing and checking.
  // This algorithm is O(log n) and converges very fast (i.e. about 2
  // iterations for years near 2000, and about 4 iterations for years
  // near -4 billion).
  int64 days_left = days;
  int64 guess_years = 1970;  // Guessed number of years.
  static const double days_to_years = 1. / 365.;
  // years_left represents approximate number of years between guess_years
  // and the desired year.
  int64 years_left;
  while ((years_left = static_cast<int64>(static_cast<double>(days_left) *
                                          days_to_years)) != 0) {
    // Adjust guess_years.
    guess_years += years_left;
    // Find out how many days guess_years represents.
    days_left = days - CumulativeEpochDaysToYear(guess_years);
  }
  // At this point, Jan 1 of guess_years should be within one year from the
  // desired date.
  DCHECK_GE(days_left, -366);
  DCHECK_LE(days_left, 366);
  days = days_left + 1;  // Days are one-indexed.
  year_ = guess_years;

  // Increment/decrement date by a month until we arrive at the right date.
  while (days < 1) {
    // Drop by a month.
    days += MaximumDayInMonthFor(year_, month_ - 1);
    int t = month_ - 1;
    month_ = static_cast<uint8>(Modulo(t, 1, 13));
    year_ += Quotient(t, 1, 13);
  }
  int max_day_in_month;
  while (max_day_in_month = MaximumDayInMonthFor(year_, month_),
         days > max_day_in_month) {
    // Bump by a month.
    days -= max_day_in_month;
    int t = month_ + 1;
    month_ = static_cast<uint8>(Modulo(t, 1, 13));
    year_ += Quotient(t, 1, 13);
  }
  day_ = static_cast<uint8>(days);
}

int64 DateTime::GetPosixSecondsOnly() const {
  const int64 days_to_year = CumulativeEpochDaysToYear(year_);
  const int64 days_to_month_within_year = CumulativeDays(year_, month_);
  // A value of day_ == 0 is valid, but don't subtract 1 in this case.
  const int64 days_within_month = (day_ == 0) ? 0 : day_ - 1;
  const int64 days_big =
      days_to_year + days_to_month_within_year + days_within_month;
  const int64 hours_big = static_cast<int64>(hour_) - zone_hours_;
  const int64 mins_big = static_cast<int64>(minute_) - zone_minutes_;

  int64 secs_big = second_;

  // Perform arithmetic in uint64 domain to avoid undefined overflow behavior
  return static_cast<int64>(
      ((((static_cast<uint64>(days_big) * 24) + hours_big) * 60) + mins_big) *
          60 +
      secs_big);
}

}  // namespace base
}  // namespace ion
