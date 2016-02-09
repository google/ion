/**
Copyright 2016 Google Inc. All Rights Reserved.

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

#include <chrono>  // NOLINT
#include <memory>
#include <vector>

#include "ion/analytics/benchmark.h"
#include "ion/base/lockguards.h"
#include "ion/base/setting.h"
#include "ion/base/stlalloc/allocunorderedmap.h"
#include "ion/base/stlalloc/allocvector.h"
#include "ion/base/threadlocalobject.h"
#include "ion/port/mutex.h"
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

  // Custom scope event mapping from name (as a pointer to a literal string) to
  // uint32 id. Note we explicitly cast the pointer to const void* since we do
  // want to hash the pointer instead of the string; std::hash<const char*>
  // would return error here.
  typedef base::AllocUnorderedMap<const void*, uint32> ScopeEventMap;

  // Reversed mapping from custom scope event id to name.
  typedef base::AllocUnorderedMap<uint32, const char*> ReverseScopeEventMap;

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

  // Queries the event id of a scope enter event, based on string_id.
  // Only raw string literals are allowed for the string_id argument.
  int GetScopeEnterEvent(const char* string_id);

  // Returns the name of the scope enter event with the given id.
  const char* GetScopeEnterEventName(uint32 event_id) const;

  // Gets the number of arguments for a particular event.
  static int GetNumArgsForEvent(uint32 event_id);

  // Gets the argument types for a particular event.
  static EventArgType GetArgType(uint32 event_id, int arg_index);

  // Returns the number of unique custom scope events recorded.
  size_t GetNumScopeEvents() const { return scope_event_map_.size(); }

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

  // Runs all registered metrics on the current timeline and returns a benchmark
  // object containing the collected statistics.
  analytics::Benchmark RunTimelineMetrics() const;

 private:
  // Array of TraceRecorder pointers that will be stored per thread.
  struct NamedTraceRecorderArray {
    NamedTraceRecorderArray() {
      for (int i = 0; i < kNumNamedTraceRecorders; ++i) {
        recorders[i] = NULL;
      }
    }

    // Pointers to named trace recorders, also present in the recorder_list_.
    TraceRecorder* recorders[kNumNamedTraceRecorders];
  };

  // Allocate a trace recorder and add it to recorder_list_.
  TraceRecorder* AllocateTraceRecorder();

  // Protect state with a mutex!
  port::Mutex mutex_;

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

  // Map of custom scope events (literal strings to uint32 ids).
  ScopeEventMap scope_event_map_;

  // Reverse map of custom scope events (uint32 ids to literal strings).
  ReverseScopeEventMap reverse_scope_event_map_;

  // The timeline metrics that have been registerd. These metrics will be run
  // when RunTimelineMetrics gets called.
  std::vector<std::unique_ptr<TimelineMetric>> timeline_metrics_;
};

// Class to automatically record scope start and end events using the given
// TraceRecorder.
class ScopedTracer {
 public:
  ScopedTracer(TraceRecorder* recorder, int id);
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
