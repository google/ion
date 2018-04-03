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

#include "ion/profile/vsyncprofiler.h"

#include <string>

#include "ion/base/logging.h"
#include "ion/base/serialize.h"
#include "ion/base/staticsafedeclare.h"
#include "ion/profile/profiling.h"

namespace ion {
namespace profile {

VSyncProfiler* VSyncProfiler::Get() {
  ION_DECLARE_SAFE_STATIC_POINTER(VSyncProfiler, profiler);
  return profiler;
}

VSyncProfiler::VSyncProfiler()
    : VSyncProfiler(ion::profile::GetCallTraceManager()) {
}

VSyncProfiler::VSyncProfiler(CallTraceManager* manager)
    : vsync_trace_recorder_(manager->GetNamedTraceRecorder(
        ion::profile::CallTraceManager::kRecorderVSync)),
      last_vsync_timestamp_(0) {
  DCHECK(vsync_trace_recorder_);
}

void VSyncProfiler::RecordVSyncEvent(uint32 timestamp, uint32 vsync_number) {
  // Ignore the timestamp if it is before the previous timestamp.
  if (timestamp < last_vsync_timestamp_) {
    LOG_ONCE(WARNING) << "The timestamp needs to increase monotonically. "
                      << "Last: " << last_vsync_timestamp_ << ", "
                      << "current: " << timestamp;
    return;
  }
  const std::string event_name = "VSync" + base::ValueToString(vsync_number);
  vsync_trace_recorder_->CreateTimeStampAtTime(
      timestamp, event_name.c_str(), nullptr);
  last_vsync_timestamp_ = timestamp;
}

}  // namespace profile
}  // namespace ion
