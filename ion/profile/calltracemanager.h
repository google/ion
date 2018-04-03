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

#ifndef ION_PROFILE_CALLTRACEMANAGER_H_
#define ION_PROFILE_CALLTRACEMANAGER_H_

#include <chrono>  // NOLINT(build/c++11)
#include <memory>
#include <mutex>  // NOLINT(build/c++11)
#include <vector>

#include "ion/analytics/benchmark.h"
#include "ion/base/setting.h"
#include "ion/base/stringtable.h"
#include "ion/base/threadlocalobject.h"
#include "ion/port/timer.h"
#include "ion/profile/timeline.h"
#include "ion/profile/timelinemetric.h"

namespace ion {
namespace profile {

class TraceRecorder;

// Manages call trace recording for visualization in Web Tracing Framework
// (WTF) format. This class maintains a separate recording buffer for each
// thread.
class CallTraceManager : public base::Allocatable {
 public:
  // Defines argument types for WTF events.
  enum EventArgType {
    kArgNone,
    kArgNumeric,
    kArgString,
  };

  // Defines the built-in WTF events that we use.
  enum BuiltinEventType {
    // The event for defining new WTF events, both custom and built-in events.
    kDefineEvent = 1,

    // Events for managing zones.
    kCreateZoneEvent = 3,
    kDeleteZoneEvent = 4,
    kSetZoneEvent = 5,

    // Leaving the current scope.
    kScopeLeaveEvent = 8,

    // Attaching data to the current scope.
    kScopeAppendDataEvent = 9,

    // Mark events.
    kMarkEvent = 10,

    // TimeStamp events.
    kTimeStampEvent = 11,

    // Time range start and end events.
    kTimeRangeStartEvent = 12,
    kTimeRangeEndEvent = 13,

    // The frame start and end events.
    kFrameStartEvent = 14,
    kFrameEndEvent = 15,

    // The starting offset for scope event ids, purposefully set high
    // to make room for built-in WTF events.
    kCustomScopeEvent = 100,
  };

  // Defines name trace recorders.
  enum NamedTraceRecorderType {
    kRecorderGpu = 0,
    kRecorderVSync,
    kNumNamedTraceRecorders,
  };

  // List of TraceRecorders, one created for each thread of execution.
  typedef base::AllocVector<TraceRecorder*> TraceList;

  CallTraceManager();

  // Construct using the specified trace capacity in bytes.
  explicit CallTraceManager(size_t buffer_size);

  ~CallTraceManager() override;

  // Gets the TraceRecorder instance specific to the current thread.
  TraceRecorder* GetTraceRecorder();

  // Gets the TraceRecorder instance specific to the current thread of the
  // given name. These are used for non-CPU-thread tracing such as for GPU
  // events.
  TraceRecorder* GetNamedTraceRecorder(NamedTraceRecorderType name);

  // Gets the list of all trace recorders for all threads.
  const TraceList& GetAllTraceRecorders() const { return recorder_list_; }

  // Returns the StringTable used for mapping strings to string IDs.
  const base::StringTablePtr& GetStringTable() const { return string_table_; }

  // Returns the StringTable used for mapping scope event names to IDs.
  const base::StringTablePtr& GetScopeEventTable() const {
    return scope_events_;
  }

  // Gets the number of arguments for a particular event.
  static int GetNumArgsForEvent(uint32 event_id);

  // Gets the argument types for a particular event.
  static EventArgType GetArgType(uint32 event_id, int arg_index);

  // Returns a snapshot of traces to a string in binary .wtf-trace format.
  // https://github.com/google/tracing-framework/blob/master/docs/wtf-trace.md
  std::string SnapshotCallTraces() const;

  // Returns the time in microseconds, relative to the timebase. The timebase
  // is the time when this CallTraceManager instance was created, expressed
  // in microseconds since the epoch.
  virtual uint32 GetTimeInUs() const {
    return static_cast<uint32>(
        std::chrono::duration_cast<std::chrono::microseconds>(timer_.Get())
            .count());
  }

  // Returns the time in nanoseconds, relative to the timebase. The timebase
  // is the time when this CallTraceManager instance was created, expressed
  // in nanoseconds since the epoch.
  uint64 GetTimeInNs() const {
    return static_cast<uint64>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(timer_.Get())
            .count());
  }

  // Writes the current WTF trace to a file, which usually ends in the
  // extension ".wtf-trace".
  void WriteFile(const std::string& filename) const;

  // Convert the current WTF trace into a timeline.
  Timeline BuildTimeline() const;

  // Registers a timeline metric.
  void RegisterTimelineMetric(std::unique_ptr<TimelineMetric> metric) {
    timeline_metrics_.push_back(std::move(metric));
  }

  // Remove all registered timeline metrics.
  void RemoveAllTimelineMetrics() { timeline_metrics_.clear(); }

  // Runs all registered metrics on the current timeline and returns a benchmark
  // object containing the collected statistics.
  analytics::Benchmark RunTimelineMetrics() const;

 private:
  // Array of TraceRecorder pointers that will be stored per thread.
  struct NamedTraceRecorderArray {
    NamedTraceRecorderArray() {
      for (int i = 0; i < kNumNamedTraceRecorders; ++i) {
        recorders[i] = nullptr;
      }
    }

    // Pointers to named trace recorders, also present in the recorder_list_.
    TraceRecorder* recorders[kNumNamedTraceRecorders];
  };

  // Allocate a trace recorder and add it to recorder_list_.
  TraceRecorder* AllocateTraceRecorder();

  // Protect state with a mutex!
  std::mutex mutex_;

  // Thread local pointer to a TraceRecorder for recording call traces.
  base::ThreadLocalObject<TraceRecorder*> trace_recorder_;

  // Pointers to named trace recorders, also present in the recorder_list_.
  base::ThreadLocalObject<NamedTraceRecorderArray> named_trace_recorders_;

  // List of TraceRecoders for all threads.
  TraceList recorder_list_;

  // Sets the trace recorder capacity (maximum number of bytes per trace
  // recorder). If zero, creates recorders with a predefined default capacity.
  size_t buffer_size_;

  // Provides accurate timing.
  port::Timer timer_;

  // A StringTable for mapping string instances to unique IDs.
  const base::StringTablePtr string_table_;

  // A StringTable for mapping scope events to unique IDs.
  const base::StringTablePtr scope_events_;

  // The timeline metrics that have been registered. These metrics will be run
  // when RunTimelineMetrics gets called.
  std::vector<std::unique_ptr<TimelineMetric>> timeline_metrics_;
};

// Class to automatically record scope start and end events using the given
// TraceRecorder.
class ScopedTracer {
 public:
  ScopedTracer(TraceRecorder* recorder, const char* name);
  ~ScopedTracer();

 private:
  TraceRecorder* recorder_;
};

// Class to automatically record frame start and end events using the given
// TraceRecorder.
class ScopedFrameTracer {
 public:
  ScopedFrameTracer(TraceRecorder* recorder, uint32 id);
  ~ScopedFrameTracer();

 private:
  TraceRecorder* recorder_;
};

}  // namespace profile
}  // namespace ion

#endif  // ION_PROFILE_CALLTRACEMANAGER_H_
