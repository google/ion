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

#include "ion/gfxprofile/gpuprofiler.h"

#include "ion/profile/calltracemanager.h"
#include "ion/profile/tracerecorder.h"

namespace ion {
namespace gfxprofile {

GpuProfiler* GpuProfiler::Get() {
  ION_DECLARE_SAFE_STATIC_POINTER(GpuProfiler, profiler);
  return profiler;
}

GpuProfiler::GpuProfiler()
    : manager_(ion::profile::GetCallTraceManager()),
      enable_gpu_tracing_("enable_gpu_tracing", false),
      gl_timer_offset_ns_(0) {
}

GpuProfiler::GpuProfiler(ion::profile::CallTraceManager* manager)
    : manager_(manager),
      enable_gpu_tracing_("enable_gpu_tracing", false),
      gl_timer_offset_ns_(0) {
}

GpuProfiler::~GpuProfiler() {}

bool GpuProfiler::IsGpuProfilingSupported(
    const ion::gfx::GraphicsManagerPtr& gfx_mgr) const {
  if (!gfx_mgr->IsExtensionSupported("GL_EXT_disjoint_timer_query")) {
    return false;
  }

  GLint bits = 0;
  gfx_mgr->GetQueryiv(GL_TIMESTAMP_EXT, GL_QUERY_COUNTER_BITS_EXT, &bits);
  if (bits == 0) {
    return false;
  }

  return true;
}

void GpuProfiler::SetGraphicsManager(
    const ion::gfx::GraphicsManagerPtr& gfx_mgr) {
  if (!IsGpuProfilingSupported(gfx_mgr)) {
    return;
  }

  graphics_manager_ = gfx_mgr;

  SyncGlTimebase();
}

GLuint GpuProfiler::TryAllocateGlQueryId() {
  ion::gfx::GraphicsManager* gfx_mgr = GetGraphicsManagerOrNull();
  if (!gfx_mgr) {
    return 0;
  }

  GLuint query_id = 0;
  if (gl_timer_query_id_pool_.empty()) {
    DCHECK(gfx_mgr);
    gfx_mgr->GenQueries(1, &query_id);
  } else {
    query_id = gl_timer_query_id_pool_.top();
    gl_timer_query_id_pool_.pop();
  }
  return query_id;
}

void GpuProfiler::EnterGlScope(const char* name) {
  ion::gfx::GraphicsManager* gfx_mgr = GetGraphicsManagerOrNull();
  if (!gfx_mgr) {
    return;
  }

  GLuint query_id = TryAllocateGlQueryId();
  if (query_id != 0) {
    const uint32 id = manager_
                          ->GetNamedTraceRecorder(
                              ion::profile::CallTraceManager::kRecorderGpu)
                          ->GetScopeEvent(name);
    gfx_mgr->QueryCounter(query_id, GL_TIMESTAMP_EXT);
    pending_gpu_queries_.push_back(GpuTimerQuery(
        manager_->GetTimeInNs(), id, query_id,
        GpuTimerQuery::kQueryBeginScope));
  }
}

void GpuProfiler::LeaveGlScope() {
  ion::gfx::GraphicsManager* gfx_mgr = GetGraphicsManagerOrNull();
  if (!gfx_mgr) {
    return;
  }

  GLuint query_id = TryAllocateGlQueryId();
  if (query_id != 0) {
    gfx_mgr->QueryCounter(query_id, GL_TIMESTAMP_EXT);
    pending_gpu_queries_.push_back(
        GpuTimerQuery(manager_->GetTimeInNs(), 0, query_id,
                      GpuTimerQuery::kQueryEndScope));
  }
}

void GpuProfiler::SyncGlTimebase() {
  ion::gfx::GraphicsManager* gfx_mgr = GetGraphicsManagerOrNull();
  if (!gfx_mgr) {
    return;
  }

  // Clear disjoint error status.
  // This error status indicates that we need to ignore the result of the
  // timer query because of some kind of disjoint GPU event such as heat
  // throttling.
  GLint disjoint = 0;
  gfx_mgr->GetIntegerv(GL_GPU_DISJOINT_EXT, &disjoint);

  // Try to get the current GL timestamp. Since the GPU can supposedly fail to
  // produce a timestamp occasionally we try a few times before giving up.
  int attempts_remaining = 3;
  do {
    GLint64 gl_timestamp = 0;
    gfx_mgr->GetInteger64v(GL_TIMESTAMP_EXT, &gl_timestamp);

    // Now get the CPU timebase.
    int64 cpu_timebase_ns = static_cast<int64>(manager_->GetTimeInNs());

    disjoint = 0;
    gfx_mgr->GetIntegerv(GL_GPU_DISJOINT_EXT, &disjoint);
    if (!disjoint) {
      gl_timer_offset_ns_ = cpu_timebase_ns - gl_timestamp;
      break;
    }
    LOG(WARNING) << "Skipping disjoint GPU timestamp";
  } while (--attempts_remaining > 0);

  if (attempts_remaining == 0) {
    LOG(ERROR) << "Failed to sync GL timebase due to disjoint results";
    gl_timer_offset_ns_ = 0;
  }
}

void GpuProfiler::PollGlTimerQueries() {
  ion::gfx::GraphicsManager* gfx_mgr = GetGraphicsManagerOrNull();
  if (!gfx_mgr) {
    return;
  }

  ion::profile::TraceRecorder* recorder = manager_->GetNamedTraceRecorder(
     ion::profile::CallTraceManager::kRecorderGpu);

  GLuint begin_frame_id = TryAllocateGlQueryId();
  if (begin_frame_id != 0) {
    gfx_mgr->QueryCounter(begin_frame_id, GL_TIMESTAMP_EXT);
    pending_gpu_queries_.push_back(
       GpuTimerQuery(manager_->GetTimeInNs(), 0, begin_frame_id,
                     GpuTimerQuery::kQueryBeginFrame));
  }

  bool has_checked_disjoint = false;
  bool was_disjoint = false;
  for (;;) {
    if (pending_gpu_queries_.empty()) {
      // No queries pending.
      return;
    }

    GpuTimerQuery query = pending_gpu_queries_.front();

    GLint available = 0;
    gfx_mgr->GetQueryObjectiv(query.gl_query_id,
                              GL_QUERY_RESULT_AVAILABLE_EXT, &available);
    if (!available) {
      // No queries available.
      return;
    }

    // Found an available query, remove it from pending queue.
    pending_gpu_queries_.pop_front();
    gl_timer_query_id_pool_.push(query.gl_query_id);

    if (!has_checked_disjoint) {
      // Check if we need to ignore the result of the timer query because
      // of some kind of disjoint GPU event such as heat throttling.
      // If so, we ignore all events that are available during this loop.
      has_checked_disjoint = true;
      GLint disjoint_occurred = 0;
      gfx_mgr->GetIntegerv(GL_GPU_DISJOINT_EXT, &disjoint_occurred);
      was_disjoint = !!disjoint_occurred;
      if (was_disjoint) {
        LOG(WARNING) << "Skipping disjoint GPU events";
      }
    }

    if (was_disjoint) {
      continue;
    }

    GLint64 timestamp_ns = 0;
    gfx_mgr->GetQueryObjecti64v(query.gl_query_id,
                                GL_QUERY_RESULT_EXT, &timestamp_ns);

    uint64 adjusted_timestamp_ns = timestamp_ns + gl_timer_offset_ns_;

    if (query.query_type == GpuTimerQuery::kQueryBeginFrame ||
        query.query_type == GpuTimerQuery::kQueryBeginScope) {
      if (adjusted_timestamp_ns < query.cpu_timestamp_ns) {
        // GPU clock is behind, adjust our offset to correct it.
        gl_timer_offset_ns_ += query.cpu_timestamp_ns - adjusted_timestamp_ns;
        adjusted_timestamp_ns = query.cpu_timestamp_ns;
      }
    }

    uint32 adjusted_timestamp_us =
        static_cast<uint32>(adjusted_timestamp_ns / 1000ll);
    switch (query.query_type) {
    case GpuTimerQuery::kQueryBeginFrame:
      break;
    case GpuTimerQuery::kQueryBeginScope:
      recorder->EnterScopeAtTime(adjusted_timestamp_us, query.scope_event_id);
      break;
    case GpuTimerQuery::kQueryEndScope:
      recorder->LeaveScopeAtTime(adjusted_timestamp_us);
      break;
    }
  }
}

}  // namespace gfxprofile
}  // namespace ion
