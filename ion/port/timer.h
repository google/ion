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

#ifndef ION_PORT_TIMER_H_
#define ION_PORT_TIMER_H_

#include <chrono>  // NOLINT
#include <type_traits>

#include "base/integral_types.h"

namespace ion {
namespace port {

class Timer {
 public:
#if defined(ION_PLATFORM_WINDOWS)
  // The Microsoft implementation of steady_clock is not actually steady in
  // Visual Studio 2013.  This is patterned after the implementation in VS 2015.
  struct steady_clock {
    typedef int64 rep;
    typedef std::nano period;
    typedef std::chrono::duration<rep, period> duration;
    typedef std::chrono::time_point<steady_clock> time_point;
    static const bool is_steady = true;

    static time_point now();
  };

#else
  // Use the high_resolution_clock if it is steady, otherwise use the
  // steady_clock.
  typedef std::conditional<std::chrono::high_resolution_clock::is_steady,
                           std::chrono::high_resolution_clock,
                           std::chrono::steady_clock>::type steady_clock;
#endif

  typedef steady_clock Clock;

  Timer() { Reset(); }

  // Resets the timer.
  void Reset();

  // Returns the elapsed time since construction or the last Reset().
  Clock::duration Get() const;

  // Returns the elapsed time since construction or the last Reset() in seconds.
  // Convenience wrapper for Get().
  double GetInS() const;

  // Returns the elapsed time since construction or the last Reset() in
  // milliseconds.  Convenience wrapper for Get().
  double GetInMs() const;

  // Sleeps for the passed number of seconds.
  static void SleepNSeconds(unsigned int seconds);

  // Sleeps for n milliseconds.
  static void SleepNMilliseconds(unsigned int milliseconds);

 private:
  steady_clock::time_point start_;
};

}  // namespace port
}  // namespace ion

#endif  // ION_PORT_TIMER_H_
