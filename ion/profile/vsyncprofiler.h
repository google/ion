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

#ifndef ION_PROFILE_VSYNCPROFILER_H_
#define ION_PROFILE_VSYNCPROFILER_H_

// This file contains a singleton class and macros related to VSync profiling.

#include "ion/profile/calltracemanager.h"
#include "ion/profile/tracerecorder.h"

namespace ion {
namespace profile {

// Singleton class that augments CallTraceManager with VSync tracing support.
//
// VSync events are often asynchronously recorded (i.e., there is no callback
// when VSync happens so they can be recorded based on the wall clock time,) and
// therefore this class expects the caller to provide a VSync timestamp, either
// in the past or in the future. The VSync events are recorded as WTF timeStamp
// events in a named TraceRecorder "kRecorderVSync".
//
// It is the caller's responsibility to make sure the timestamps provided to
// this class are monotonically increasing. Invalid timestamps will be ignored.
class VSyncProfiler {
 public:
  // Gets the VSyncProfiler singleton instance.
  static VSyncProfiler* Get();

  VSyncProfiler();
  // For internal use and testing purposes only. User code should only
  // call the default constructor.
  explicit VSyncProfiler(ion::profile::CallTraceManager* manager);

  ~VSyncProfiler() {}

  // Records a VSync event at given |timestamp|.
  void RecordVSyncEvent(uint32 timestamp, uint32 vsync_number);

 private:
  // Pointer to the named TraceRecorder (CallTraceManager::kRecorderVSync).
  ion::profile::TraceRecorder* vsync_trace_recorder_;

  // The last recorded VSync time.
  uint32 last_vsync_timestamp_;
};

}  // namespace profile
}  // namespace ion

#if ION_PRODUCTION
#define ION_PROFILE_VSYNC(timestamp, vsync_number)
#else
#define ION_PROFILE_VSYNC(timestamp, vsync_number)           \
    ::ion::profile::VSyncProfiler::Get()->RecordVSyncEvent(  \
        timestamp, vsync_number);
#endif  // ION_PRODUCTION

#endif  // ION_PROFILE_VSYNCPROFILER_H_
