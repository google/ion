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
#include <Windows.h>  // NOLINT
#else
#include <unistd.h>
#endif

#include <atomic>

namespace ion {
namespace port {

#if defined(ION_PLATFORM_WINDOWS)
// static
Timer::steady_clock::time_point Timer::steady_clock::now() {
  // Timing code often relies on precise timestamps, so we are interested in
  // keeping the initialization of |s_freq| as lightweight as possible.  In a
  // C++11-compliant environment, this could be done (albeit with slightly more
  // overhead) using a static const initializer, as C++11 ensures that such
  // initializers are run thread-safe and only once.
  // Unfortunately, vc_12_0 is not entirely C++11-compliant in this regard.
  static std::atomic<int64> s_freq;
  // We use relaxed memory order as we are not concerned with ordering here;
  // just that loads and stores of the entire variable are atomic.
  int64 freq = s_freq.load(std::memory_order_relaxed);
  if (freq == 0) {
    LARGE_INTEGER li_freq;
    QueryPerformanceFrequency(&li_freq);
    freq = li_freq.QuadPart;
    // QueryPerformanceFrequency() returns a fixed value and can overwrite
    // |s_freq| safely, even if two threads are data racing here.
    s_freq.store(freq, std::memory_order_relaxed);
  }

  LARGE_INTEGER pc;
  QueryPerformanceCounter(&pc);
  const int64 counter = pc.QuadPart;
  const int64 sec = (counter / freq) * period::den / period::num;
  const int64 nano = (counter % freq) * period::den / freq / period::num;
  return time_point(duration(sec + nano));
}

// static
void Timer::SleepNSeconds(unsigned int seconds) { Sleep(seconds * 1000); }

// static
void Timer::SleepNMilliseconds(unsigned int milliseconds) {
  Sleep(milliseconds);
}

#else
// static
void Timer::SleepNSeconds(unsigned int seconds) { sleep(seconds); }

// static
void Timer::SleepNMilliseconds(unsigned int milliseconds) {
  const int millis = static_cast<int>(milliseconds);
  const struct timespec sleeptime = {millis / 1000, (millis % 1000) * 1000000};
  struct timespec remaining = {0, 0};
  nanosleep(&sleeptime, &remaining);
}

#endif

void Timer::Reset() { start_ = Clock::now(); }

Timer::Clock::duration Timer::Get() const { return Clock::now() - start_; }

double Timer::GetInS() const {
  return std::chrono::duration_cast<std::chrono::duration<double>>(Get())
      .count();
}

double Timer::GetInMs() const {
  return std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(
             Get())
      .count();
}

}  // namespace port
}  // namespace ion
