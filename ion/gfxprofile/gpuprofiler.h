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

#ifndef ION_GFXPROFILE_GPUPROFILER_H_
#define ION_GFXPROFILE_GPUPROFILER_H_

// This file contains classes and macros related to run-time performance
// profiling of GPU processing.

#include <deque>
#include <stack>
#include <vector>

#include "base/integral_types.h"
#include "ion/gfx/graphicsmanager.h"
#include "ion/profile/calltracemanager.h"
#include "ion/profile/profiling.h"
#include "ion/profile/tracerecorder.h"

namespace ion {
namespace profile {
class CallTraceTest;
}  // namespace profile
namespace gfxprofile {

// Singleton class that augments CallTraceManager with GPU tracing support.
// See calltracemanager_test.cc for tests.
//
// While enabled, GL commands will be submitted each frame to query timestamps
// of GPU workloads that have been traced using the ION_PROFILE_GPU macro
// defined below.
//
// Basic workflow:
//  - have the app framework call SetGraphicsManager and
//    then call PollGlTimerQueries at the start of each frame.
//  - place ION_PROFILE_GPU("MyGlWorkload") at the start of code scopes where
//    GL draw commands are performed that you want to trace.
//  - enable the enable_gpu_tracing_ setting via Ion Remote.
//  - connect to the WTF tracing output via Ion Remote and view traces.
class GpuProfiler {
 public:
  // Gets the GpuProfiler singleton instance.
  static GpuProfiler* Get();

  GpuProfiler();
  ~GpuProfiler();

  bool IsGpuProfilingSupported(const ion::gfx::GraphicsManagerPtr& gfx_mgr)
      const;

  // Sets the GraphicsManager that is required for performing GPU tracing via
  // OpenGL.
  void SetGraphicsManager(const gfx::GraphicsManagerPtr& gfx_mgr);

  // Enables runtime GPU tracing. While enabled, GL commands will be submitted
  // each frame to query timestamps of GPU workloads that have been traced using
  // the ION_PROFILE_GPU macro defined below. Note that this has no effect if
  // GPU tracing is not supported or if SetGraphicsManager was not called.
  void SetEnableGpuTracing(bool enabled) {
    enable_gpu_tracing_ = enabled;
  }

  // Gets the GraphicsManager if GPU tracing is enabled. NULL is returned if
  // GPU tracing is not enabled.
  gfx::GraphicsManager* GetGraphicsManagerOrNull() const {
    if (enable_gpu_tracing_) {
      return graphics_manager_.Get();
    } else {
      return nullptr;
    }
  }

  // Polls (non-blocking) for completed GL timer query data and adds events into
  // the trace buffer. Must call once close to the start of each frame.
  void PollGlTimerQueries();

  // Records the beginning of a scoped GL trace event.
  void EnterGlScope(const char* name);

  // Records the end of a scoped GL trace event.
  void LeaveGlScope();

 private:
  // Data to queue the pending GPU timer queries that need to be polled
  // for completion.
  struct GpuTimerQuery {
    enum QueryType {
      kQueryBeginFrame,
      kQueryBeginScope,
      kQueryEndScope,
    };

    // scope_id is only required for kQueryBeginScope query types.
    GpuTimerQuery(uint64 timestamp_ns,
                  int scope_id,
                  GLuint query_id,
                  QueryType type)
        : cpu_timestamp_ns(timestamp_ns),
          scope_event_id(scope_id),
          gl_query_id(query_id),
          query_type(type) {}

    uint64 cpu_timestamp_ns;
    int scope_event_id;
    GLuint gl_query_id;
    QueryType query_type;
  };

  // For testing purposes, constructs a GpuProfiler instance with a custom
  // CallTraceManager.
  explicit GpuProfiler(ion::profile::CallTraceManager* manager);

  // Synchronises the GL timebase with the CallTraceManager timebase.
  void SyncGlTimebase();

  // Returns a GL timer query ID if possible. Otherwise returns 0.
  GLuint TryAllocateGlQueryId();

  // Reference to the parent CallTraceManager, used to query time.
  ion::profile::CallTraceManager* manager_;

  // Setting for enabling GPU tracing.
  base::Setting<bool> enable_gpu_tracing_;

  // Nanosecond offset to the GL timebase to compute the CallTraceManager time.
  int64 gl_timer_offset_ns_;

  // Optional pointer to graphics manager for tracing GPU events.
  gfx::GraphicsManagerPtr graphics_manager_;

  // For GPU event TraceRecords, this tracks the pending queries that will
  // be asynchronously polled (in order) and then added to the TraceRecorder
  // buffer with the GPU timestamps.
  std::deque<GpuTimerQuery> pending_gpu_queries_;

  // Available ids for use with GLTimerQuery as needed. This will generally
  // reach a steady state after a few frames. Always push and pop from the back
  // to avoid shifting the vector.
  std::stack<GLuint, std::vector<GLuint> > gl_timer_query_id_pool_;

  friend class ion::profile::CallTraceTest;
};

// Traces the GPU start and end times of the GL commands submitted in the
// same scope. Typically used via the ION_PROFILE_GPU macro.
class ScopedGlTracer {
 public:
  ScopedGlTracer(GpuProfiler* profiler, const char* name)
      : profiler_(profiler) {
    profiler_->EnterGlScope(name);
  }

  ~ScopedGlTracer() {
    profiler_->LeaveGlScope();
  }

 private:
  GpuProfiler* profiler_;
};

}  // namespace gfxprofile
}  // namespace ion

#if ION_PRODUCTION
#define ION_PROFILE_GPU(group_name)
#else
// This macro can be used in any GL operation scope to trace the resulting
// GPU work.
#define ION_PROFILE_GPU(group_name)                                    \
  ION_PROFILE_FUNCTION(group_name);                                    \
  ::ion::gfxprofile::ScopedGlTracer ION_PROFILING_PASTE3(group_name_)( \
      ::ion::gfxprofile::GpuProfiler::Get(), group_name)
#endif  // ION_PRODUCTION

#endif  // ION_GFXPROFILE_GPUPROFILER_H_
