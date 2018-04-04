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

#include <limits>
#include <memory>
#include <stack>
#include <string>

#include "ion/profile/tracerecorder.h"

#include "ion/base/serialize.h"
#include "ion/port/threadutils.h"
#include "ion/profile/calltracemanager.h"
#include "ion/profile/timelineevent.h"
#include "ion/profile/timelineframe.h"
#include "ion/profile/timelinenode.h"
#include "ion/profile/timelinerange.h"
#include "ion/profile/timelinescope.h"
#include "absl/memory/memory.h"
#include "third_party/jsoncpp/include/json/json.h"

namespace ion {
namespace profile {

using base::TrimEndWhitespace;
using base::ValueToString;

namespace {

// Special marker to denote that the scope event nesting level is zero at this
// point.
static const uint32 kEmptyScopeMarker = 0xfeeb1e57;

}  // namespace

// Use 20 MB in non-prod builds and 1 MB in prod builds for a tracing buffer.
// 
size_t TraceRecorder::s_default_buffer_size_ =
#if ION_PRODUCTION
    1 * 1024 * 1024;
#else
    20 * 1024 * 1024;
#endif

bool TraceRecorder::s_reserve_buffer_ = false;

TraceRecorder::TraceRecorder(CallTraceManager* manager)
    : TraceRecorder(manager, s_default_buffer_size_) {}

TraceRecorder::TraceRecorder(CallTraceManager* manager, size_t buffer_size)
    : manager_(manager),
      string_table_view_(manager_->GetStringTable()->CreateView(256)),
      scope_events_view_(manager_->GetScopeEventTable()->CreateView(256)),
      trace_buffer_(buffer_size / sizeof(uint32), GetAllocator(),
                    s_reserve_buffer_),
      scope_level_(0),
      thread_id_(std::this_thread::get_id()),
      thread_name_("UnnamedThread"),
      frame_level_(0),
      current_frame_number_(0) {
  Clear();
}

void TraceRecorder::Clear() {
  base::SpinLockGuard guard(&mutex_);
  // It is only safe to clear TraceRecorder when both the scope and frame are at
  // the top level.
  CHECK_EQ(0, scope_level_);
  CHECK_EQ(0, frame_level_);

  trace_buffer_.Clear();
  trace_buffer_.AddItem(kEmptyScopeMarker);
}

uint32 TraceRecorder::GetScopeEvent(const char* name) {
  return scope_events_view_->FindIndex(name) +
         CallTraceManager::kCustomScopeEvent;
}

void TraceRecorder::EnterScope(uint32 event_id) {
  EnterScopeAtTime(manager_->GetTimeInUs(), event_id);
}

void TraceRecorder::AnnotateCurrentScope(const std::string& name,
                                         const std::string& value) {
  AnnotateCurrentScopeAtTime(manager_->GetTimeInUs(), name, value);
}

void TraceRecorder::LeaveScope() {
  LeaveScopeAtTime(manager_->GetTimeInUs());
}

void TraceRecorder::EnterScopeAtTime(uint32 timestamp, uint32 event_id) {
  base::SpinLockGuard guard(&mutex_);
  trace_buffer_.AddItem(event_id);
  trace_buffer_.AddItem(timestamp);
  ++scope_level_;
}

void TraceRecorder::AnnotateCurrentScopeAtTime(uint32 timestamp,
                                               const std::string& name,
                                               const std::string& value) {
  DCHECK(!name.empty());
  DCHECK(!value.empty());
  // JSON does not support Nan and infinity.
  DCHECK_NE(ValueToString(std::numeric_limits<double>::quiet_NaN()), value);
  DCHECK_NE(ValueToString(-std::numeric_limits<double>::quiet_NaN()), value);
  DCHECK_NE(ValueToString(std::numeric_limits<double>::infinity()), value);
  DCHECK_NE(ValueToString(-std::numeric_limits<double>::infinity()), value);
  base::SpinLockGuard guard(&mutex_);
  uint32 name_index =
      string_table_view_->FindIndex(TrimEndWhitespace(name).c_str());
  uint32 value_index =
      string_table_view_->FindIndex(TrimEndWhitespace(value).c_str());

  trace_buffer_.AddItem(CallTraceManager::kScopeAppendDataEvent);
  trace_buffer_.AddItem(timestamp);
  trace_buffer_.AddItem(name_index);
  trace_buffer_.AddItem(value_index);
}

void TraceRecorder::LeaveScopeAtTime(uint32 timestamp) {
  base::SpinLockGuard guard(&mutex_);
  trace_buffer_.AddItem(CallTraceManager::kScopeLeaveEvent);
  trace_buffer_.AddItem(timestamp);
  DCHECK_GT(scope_level_, 0);
  --scope_level_;
  if (scope_level_ == 0) {
    trace_buffer_.AddItem(kEmptyScopeMarker);
  }
}

void TraceRecorder::EnterFrame(uint32 frame_number) {
  if (frame_level_ == 0) {
    base::SpinLockGuard guard(&mutex_);
    // Only record the frame for the outer-most EnterFrame() call.
    current_frame_number_ = frame_number;
    trace_buffer_.AddItem(CallTraceManager::kFrameStartEvent);
    trace_buffer_.AddItem(manager_->GetTimeInUs());
    trace_buffer_.AddItem(frame_number);
  }
  ++frame_level_;
}

void TraceRecorder::LeaveFrame() {
  if (!IsInFrameScope()) {
    LOG_ONCE(WARNING)
        << "LeaveFrame() should not be called outside of a frame.";
    return;
  }
  --frame_level_;
  // Only record the frame for the outer-most LeaveFrame() call.
  if (frame_level_ == 0) {
    base::SpinLockGuard guard(&mutex_);
    trace_buffer_.AddItem(CallTraceManager::kFrameEndEvent);
    trace_buffer_.AddItem(manager_->GetTimeInUs());
    trace_buffer_.AddItem(current_frame_number_);
  }
}

void TraceRecorder::EnterTimeRange(
    uint32 unique_id, const char* name, const char* value) {
  base::SpinLockGuard guard(&mutex_);
  DCHECK(name);
  uint32 name_index =
      string_table_view_->FindIndex(TrimEndWhitespace(name).c_str());

  uint32 value_index = -1;
  if (value) {
    value_index =
        string_table_view_->FindIndex(TrimEndWhitespace(value).c_str());
  }

  EnterTimeRange(unique_id, name_index, value_index);
}

uint32 TraceRecorder::EnterTimeRange(const char* name, const char* value) {
  base::SpinLockGuard guard(&mutex_);
  DCHECK(name);
  uint32 name_index =
      string_table_view_->FindIndex(TrimEndWhitespace(name).c_str());

  uint32 value_index = -1;
  if (value) {
    value_index =
        string_table_view_->FindIndex(TrimEndWhitespace(value).c_str());
  }

  // In this case, the index of the name inside the string_buffer_ serves as a
  // unique ID for the time range event.
  EnterTimeRange(name_index, name_index, value_index);
  return name_index;
}

void TraceRecorder::EnterTimeRange(
    uint32 unique_id, uint32 name_index, uint32 value_index) {
  DCHECK(mutex_.IsLocked());
  const auto result = open_time_range_events_.insert(unique_id);
  if (!result.second) {
    // Ignore the enter time range event if it is already opened.
    LOG(WARNING) << "Time range event with ID " << unique_id
                 << " is already opened.";
    return;
  }
  trace_buffer_.AddItem(CallTraceManager::kTimeRangeStartEvent);
  trace_buffer_.AddItem(manager_->GetTimeInUs());
  trace_buffer_.AddItem(unique_id);
  trace_buffer_.AddItem(name_index);
  trace_buffer_.AddItem(value_index);
}

void TraceRecorder::LeaveTimeRange(uint32 id) {
  base::SpinLockGuard guard(&mutex_);
  const size_t num_removed = open_time_range_events_.erase(id);
  if (num_removed == 0) {
    // Ignore the leave time range event if it is not opened.
    LOG(WARNING) << "Time range event with ID " << id << " is not opened.";
    return;
  }
  trace_buffer_.AddItem(CallTraceManager::kTimeRangeEndEvent);
  trace_buffer_.AddItem(manager_->GetTimeInUs());
  trace_buffer_.AddItem(id);
}

void TraceRecorder::CreateTimeStamp(const char* name, const char* value) {
  CreateTimeStampAtTime(manager_->GetTimeInUs(), name, value);
}

void TraceRecorder::CreateTimeStampAtTime(
    uint32 timestamp, const char* name, const char* value) {
  base::SpinLockGuard guard(&mutex_);
  DCHECK(name);
  uint32 name_index =
      string_table_view_->FindIndex(TrimEndWhitespace(name).c_str());

  uint32 value_index = -1;
  if (value) {
    value_index =
        string_table_view_->FindIndex(TrimEndWhitespace(value).c_str());
  }

  trace_buffer_.AddItem(CallTraceManager::kTimeStampEvent);
  trace_buffer_.AddItem(timestamp);
  trace_buffer_.AddItem(name_index);
  trace_buffer_.AddItem(value_index);
}

size_t TraceRecorder::GetNumTraces() const {
  base::SpinLockGuard guard(&mutex_);
  size_t index = 0;
  // Advance until the first empty scope marker.
  while (index < trace_buffer_.GetSize() &&
      trace_buffer_.GetItem(index) != kEmptyScopeMarker) {
    index++;
  }

  size_t length = 0;
  while (index < trace_buffer_.GetSize()) {
    // Write the next record.
    uint32 wire_id = trace_buffer_.GetItem(index);
    if (wire_id == kEmptyScopeMarker) {
      index++;
    } else {
      index += 2 + CallTraceManager::GetNumArgsForEvent(wire_id);
      length++;
    }
  }
  return length;
}

void TraceRecorder::DumpTrace(base::BufferBuilder* output) const {
  base::SpinLockGuard guard(&mutex_);
  size_t index = 0;

  // Advance until the first empty scope marker.
  while (index < trace_buffer_.GetSize() &&
      trace_buffer_.GetItem(index) != kEmptyScopeMarker) {
    index++;
  }

  // Write the next record.
  while (index < trace_buffer_.GetSize()) {
    uint32 wire_id = trace_buffer_.GetItem(index);
    if (wire_id == kEmptyScopeMarker) {
      index++;
    } else {
      // Output wire id and timestamp.
      output->Append(wire_id);
      output->Append(trace_buffer_.GetItem(index + 1));

      // See if we need to write any arguments.
      const int num_args = CallTraceManager::GetNumArgsForEvent(wire_id);
      for (int i = 0; i < num_args; ++i) {
        const CallTraceManager::EventArgType arg_type =
            CallTraceManager::GetArgType(wire_id, i);
        DCHECK(arg_type != CallTraceManager::kArgNone);
        uint32 item = trace_buffer_.GetItem(index + 2 + i);
        if (arg_type == CallTraceManager::kArgString &&
            item != 0xffffffff) {
          output->Append(item);
        } else {
          output->Append(item);
        }
      }
      index += num_args + 2;
    }
  }

  // If there are open time range events, close them.
  for (const uint32_t& id : open_time_range_events_) {
    output->Append(CallTraceManager::kTimeRangeEndEvent);
    output->Append(manager_->GetTimeInUs());
    output->Append(id);
  }

  // If the current scope_level_ is not zero, append additional leave scope
  // events.
  for (int level = scope_level_; level > 0; --level) {
    output->Append(CallTraceManager::kScopeLeaveEvent);
    output->Append(manager_->GetTimeInUs());
  }

  // If the current frame_level_ is not zero, append additional leave frame
  // events.
  for (int level = frame_level_; level > 0; --level) {
    output->Append(CallTraceManager::kFrameEndEvent);
    output->Append(manager_->GetTimeInUs());
    output->Append(current_frame_number_);
  }
}

std::string TraceRecorder::GetStringArg(size_t index, int arg_index) const {
  DCHECK(mutex_.IsLocked());
  const uint32 wire_id = trace_buffer_.GetItem(index);
  CHECK_LT(arg_index, CallTraceManager::GetNumArgsForEvent(wire_id));
  CHECK_EQ(CallTraceManager::kArgString,
           CallTraceManager::GetArgType(wire_id, arg_index));
  const uint32 string_index = trace_buffer_.GetItem(index + 2 + arg_index);
  // Return an empty string if this arg is not present. This happens, e.g., for
  // the optional value parameter on time ranges.
  if (string_index == base::StringTable::kInvalidIndex) return std::string();
  return string_table_view_->GetString(string_index);
}

std::unique_ptr<TimelineEvent> TraceRecorder::GetTimelineEvent(
    size_t index) const {
  DCHECK(mutex_.IsLocked());
  const uint32 wire_id = trace_buffer_.GetItem(index);
  const uint32 timestamp = trace_buffer_.GetItem(index + 1);
  Json::Reader json_reader;

  std::string event_name;
  Json::Value args(Json::objectValue);

  if (wire_id == CallTraceManager::kTimeRangeStartEvent) {
    event_name = GetStringArg(index, 1);
    json_reader.parse(GetStringArg(index, 2), args);
    return std::unique_ptr<TimelineEvent>(
        new TimelineRange(event_name, timestamp, 0, args));
  } else if (wire_id == CallTraceManager::kFrameStartEvent) {
    const uint32 frame_number = trace_buffer_.GetItem(index + 2);
    event_name = std::string("Frame_") + base::ValueToString(frame_number);
    return std::unique_ptr<TimelineEvent>(
        new TimelineFrame(event_name, timestamp, 0, args, frame_number));
  } else if (wire_id >= CallTraceManager::kCustomScopeEvent) {
    event_name = scope_events_view_->GetString(
        wire_id - CallTraceManager::kCustomScopeEvent);
    return std::unique_ptr<TimelineEvent>(
        new TimelineScope(event_name, timestamp, 0, args));
  } else {
    LOG(FATAL) << "Event type not supported by timeline exporter!";
    return absl::make_unique<TimelineEvent>(event_name, timestamp, 0, args);
  }
}

void TraceRecorder::AddTraceToTimelineNode(TimelineNode* root) const {
  CHECK(root);
  base::SpinLockGuard guard(&mutex_);
  TimelineNode* parent_candidate = root;
  std::stack<TimelineEvent*> open_events;
  uint32 previous_begin = 0;
  bool first_event = true;
  size_t index = 0;
  Json::Reader json_reader;

  // Advance until the first empty scope marker.
  while (index < trace_buffer_.GetSize() &&
         trace_buffer_.GetItem(index) != kEmptyScopeMarker) {
    ++index;
  }

  // Iterate over all events.
  while (index < trace_buffer_.GetSize()) {
    const uint32 wire_id = trace_buffer_.GetItem(index);
    if (wire_id == kEmptyScopeMarker) {
      ++index;
      continue;
    }

    const uint32 timestamp = trace_buffer_.GetItem(index + 1);
    CHECK(first_event || timestamp >= previous_begin)
        << "Timestamps not monotonically increasing!\n";
    first_event = false;
    previous_begin = timestamp;

    // Find the actual parent for this event. The event can begin after the
    // parent candidate ends. In this case, walk up the tree until we either hit
    // the root node, or an open duration event, or a scoped event that ends
    // after the current event begins.
    TimelineNode* parent = parent_candidate;
    while (parent->GetParent() &&
           (open_events.empty() || parent != open_events.top()) &&
           parent->GetEnd() < timestamp) {
      parent = parent->GetParent();
    }

    DCHECK(parent);

    if (wire_id == CallTraceManager::kTimeRangeStartEvent ||
        wire_id == CallTraceManager::kFrameStartEvent ||
        wire_id >= CallTraceManager::kCustomScopeEvent) {
      std::unique_ptr<TimelineEvent> timeline_event = GetTimelineEvent(index);
      open_events.push(timeline_event.get());
      // If we open a new duration event, it will become the parent of
      // subsequent events until it is closed or superceded by one of its
      // children.
      parent_candidate = timeline_event.get();
      parent->AddChild(std::move(timeline_event));
    } else if (wire_id == CallTraceManager::kTimeRangeEndEvent ||
               wire_id == CallTraceManager::kFrameEndEvent ||
               wire_id == CallTraceManager::kScopeLeaveEvent) {
      DCHECK_LT(0U, open_events.size());
      open_events.top()->UpdateDuration(timestamp);
      open_events.pop();
      // This event ends, it can't be the parent of any other event. Use its
      // parent as the new candidate.
      parent_candidate = parent->GetParent();
    } else if (wire_id == CallTraceManager::kScopeAppendDataEvent) {
      const std::string arg_name = GetStringArg(index, 0);
      const std::string arg_value = GetStringArg(index, 1);
      open_events.top()->GetArgs()[arg_name] = Json::objectValue;
      json_reader.parse(arg_value, open_events.top()->GetArgs()[arg_name]);
    }

    const int num_args = CallTraceManager::GetNumArgsForEvent(wire_id);
    index += num_args + 2;
  }
}

uint32 TraceRecorder::GetCurrentFrameNumber() const {
  if (!IsInFrameScope()) {
    LOG_ONCE(WARNING) << "GetCurrentFrameNumber() should not be called outside "
                         "of a frame.";
    return 0;
  }
  return current_frame_number_;
}

}  // namespace profile
}  // namespace ion
