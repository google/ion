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

#include <limits>
#include <memory>
#include <stack>
#include <string>

#include "ion/profile/tracerecorder.h"

#include "ion/base/serialize.h"
#include "ion/base/stringutils.h"
#include "ion/port/threadutils.h"
#include "ion/profile/calltracemanager.h"
#include "ion/profile/timelineevent.h"
#include "ion/profile/timelineframe.h"
#include "ion/profile/timelinenode.h"
#include "ion/profile/timelinerange.h"
#include "ion/profile/timelinescope.h"
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
// TODO(user): Maybe pull this out into a setting?
size_t TraceRecorder::s_default_buffer_size_ =
#if ION_PRODUCTION
    1 * 1024 * 1024;
#else
    20 * 1024 * 1024;
#endif

bool TraceRecorder::s_reserve_buffer_ = false;

TraceRecorder::TraceRecorder(CallTraceManager* manager)
    : manager_(manager),
      trace_buffer_(s_default_buffer_size_ / sizeof(uint32),
                    GetAllocator(),
                    s_reserve_buffer_),
      scope_level_(0),
      thread_id_(ion::port::GetCurrentThreadId()),
      thread_name_("UnnamedThread"),
      frame_level_(0),
      current_frame_number_(0) {
  trace_buffer_.AddItem(kEmptyScopeMarker);
}

TraceRecorder::TraceRecorder(CallTraceManager* manager, size_t buffer_size)
    : manager_(manager),
      trace_buffer_(
          buffer_size / sizeof(uint32), GetAllocator(), s_reserve_buffer_),
      scope_level_(0),
      thread_id_(ion::port::GetCurrentThreadId()),
      thread_name_("UnnamedThread"),
      frame_level_(0),
      current_frame_number_(0) {
  trace_buffer_.AddItem(kEmptyScopeMarker);
}

void TraceRecorder::EnterScope(int event_id) {
  EnterScopeAtTime(manager_->GetTimeInUs(), event_id);
}

void TraceRecorder::AnnotateCurrentScope(const std::string& name,
                                         const std::string& value) {
  AnnotateCurrentScopeAtTime(manager_->GetTimeInUs(), name, value);
}

void TraceRecorder::LeaveScope() {
  LeaveScopeAtTime(manager_->GetTimeInUs());
}

void TraceRecorder::EnterScopeAtTime(uint32 timestamp, int event_id) {
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
  uint32 name_index = GetStringIndex(TrimEndWhitespace(name));
  uint32 value_index = GetStringIndex(TrimEndWhitespace(value));

  trace_buffer_.AddItem(CallTraceManager::kScopeAppendDataEvent);
  trace_buffer_.AddItem(timestamp);
  trace_buffer_.AddItem(name_index);
  trace_buffer_.AddItem(value_index);
}

void TraceRecorder::LeaveScopeAtTime(uint32 timestamp) {
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
    trace_buffer_.AddItem(CallTraceManager::kFrameEndEvent);
    trace_buffer_.AddItem(manager_->GetTimeInUs());
    trace_buffer_.AddItem(current_frame_number_);
  }
}

void TraceRecorder::EnterTimeRange(
    uint32 unique_id, const char* name, const char* value) {
  DCHECK(name);
  uint32 name_index = GetStringIndex(TrimEndWhitespace(name));

  uint32 value_index = -1;
  if (value) {
    value_index = GetStringIndex(TrimEndWhitespace(value));
  }

  trace_buffer_.AddItem(CallTraceManager::kTimeRangeStartEvent);
  trace_buffer_.AddItem(manager_->GetTimeInUs());
  trace_buffer_.AddItem(unique_id);
  trace_buffer_.AddItem(name_index);
  trace_buffer_.AddItem(value_index);
}

uint32 TraceRecorder::EnterTimeRange(const char* name, const char* value) {
  DCHECK(name);
  uint32 name_index = GetStringIndex(TrimEndWhitespace(name));

  uint32 value_index = -1;
  if (value) {
    value_index = GetStringIndex(TrimEndWhitespace(value));
  }

  trace_buffer_.AddItem(CallTraceManager::kTimeRangeStartEvent);
  trace_buffer_.AddItem(manager_->GetTimeInUs());
  // In this case, the index of the name inside the string_buffer_ serves as a
  // unique ID for the time range event. The call to AddItem sets the unique_id,
  // and the call refers to the string within string_buffer_.
  trace_buffer_.AddItem(name_index);
  trace_buffer_.AddItem(name_index);
  trace_buffer_.AddItem(value_index);

  return name_index;
}

void TraceRecorder::LeaveTimeRange(uint32 id) {
  trace_buffer_.AddItem(CallTraceManager::kTimeRangeEndEvent);
  trace_buffer_.AddItem(manager_->GetTimeInUs());
  trace_buffer_.AddItem(id);
}

void TraceRecorder::CreateTimeStamp(const char* name, const char* value) {
  CreateTimeStampAtTime(manager_->GetTimeInUs(), name, value);
}

void TraceRecorder::CreateTimeStampAtTime(
    uint32 timestamp, const char* name, const char* value) {
  DCHECK(name);
  uint32 name_index = GetStringIndex(TrimEndWhitespace(name));

  uint32 value_index = -1;
  if (value) {
    value_index = GetStringIndex(TrimEndWhitespace(value));
  }

  trace_buffer_.AddItem(CallTraceManager::kTimeStampEvent);
  trace_buffer_.AddItem(timestamp);
  trace_buffer_.AddItem(name_index);
  trace_buffer_.AddItem(value_index);
}

size_t TraceRecorder::GetNumTraces() const {
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

uint32 TraceRecorder::GetStringIndex(const std::string& str) {
  // Attempt to insert this string with a new index. If the insert fails,
  // return the preexisting index for this string.
  uint32 index = static_cast<uint32>(string_buffer_.size());
  return string_buffer_.insert(
      std::make_pair(str, index)).first->second;
}

void TraceRecorder::DumpStrings(std::vector<std::string>* table) const {
  size_t base = table->size();
  table->resize(base + string_buffer_.size());
  for (auto it = string_buffer_.begin(); it != string_buffer_.end(); ++it) {
    DCHECK_LT(base + it->second, table->size());
    (*table)[base + it->second] = it->first;
  }
}

void TraceRecorder::DumpTrace(
    std::string* output, uint32 string_index_offset) const {
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
      base::AppendBytes(output, wire_id);
      base::AppendBytes(output, trace_buffer_.GetItem(index + 1));

      // See if we need to write any arguments.
      const int num_args = CallTraceManager::GetNumArgsForEvent(wire_id);
      for (int i = 0; i < num_args; ++i) {
        const CallTraceManager::EventArgType arg_type =
            CallTraceManager::GetArgType(wire_id, i);
        DCHECK(arg_type != CallTraceManager::kArgNone);
        uint32 item = trace_buffer_.GetItem(index + 2 + i);
        if (arg_type == CallTraceManager::kArgString &&
            item != 0xffffffff) {
          base::AppendBytes(output, string_index_offset + item);
        } else {
          base::AppendBytes(output, item);
        }
      }
      index += num_args + 2;
    }
  }
}

std::string TraceRecorder::GetStringArg(
    size_t index, int arg_index,
    const IndexToStringMap& inverse_string_buffer) const {
  const uint32 wire_id = trace_buffer_.GetItem(index);
  CHECK_LT(arg_index, CallTraceManager::GetNumArgsForEvent(wire_id));
  CHECK_EQ(CallTraceManager::kArgString,
           CallTraceManager::GetArgType(wire_id, arg_index));
  const uint32 string_index = trace_buffer_.GetItem(index + 2 + arg_index);
  // Return an empty string if this arg is not present. This happens, e.g., for
  // the optional value parameter on time ranges.
  if (string_index == 0xffffffff) return std::string();
  return inverse_string_buffer.at(string_index);
}

std::unique_ptr<TimelineEvent> TraceRecorder::GetTimelineEvent(
    size_t index, const IndexToStringMap& inverse_string_buffer) const {
  const uint32 wire_id = trace_buffer_.GetItem(index);
  const uint32 timestamp = trace_buffer_.GetItem(index + 1);
  Json::Reader json_reader;

  std::string event_name;
  Json::Value args(Json::objectValue);

  if (wire_id == CallTraceManager::kTimeRangeStartEvent) {
    event_name = GetStringArg(index, 1, inverse_string_buffer);
    json_reader.parse(GetStringArg(index, 2, inverse_string_buffer), args);
    return std::unique_ptr<TimelineEvent>(
        new TimelineRange(event_name, timestamp, 0, args));
  } else if (wire_id == CallTraceManager::kFrameStartEvent) {
    const uint32 frame_number = trace_buffer_.GetItem(index + 2);
    event_name = std::string("Frame_") + base::ValueToString(frame_number);
    return std::unique_ptr<TimelineEvent>(
        new TimelineFrame(event_name, timestamp, 0, args, frame_number));
  } else if (wire_id >= CallTraceManager::kCustomScopeEvent) {
    event_name = manager_->GetScopeEnterEventName(wire_id);
    return std::unique_ptr<TimelineEvent>(
        new TimelineScope(event_name, timestamp, 0, args));
  } else {
    CHECK(false) << "Event type not supported by timeline exporter!";
    return std::unique_ptr<TimelineEvent>(
        new TimelineEvent(event_name, timestamp, 0, args));
  }
}

void TraceRecorder::AddTraceToTimelineNode(TimelineNode* root) const {
  CHECK(root);
  TimelineNode* parent_candidate = root;
  std::stack<TimelineEvent*> open_events;
  uint32 previous_begin = 0;
  bool first_event = true;
  size_t index = 0;
  Json::Reader json_reader;

  IndexToStringMap inverse_string_buffer;
  for (auto it = string_buffer_.begin(); it != string_buffer_.end(); ++it) {
    inverse_string_buffer[it->second] = it->first;
  }

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
      std::unique_ptr<TimelineEvent> timeline_event =
          GetTimelineEvent(index, inverse_string_buffer);
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
      const std::string arg_name =
          GetStringArg(index, 0, inverse_string_buffer);
      const std::string arg_value =
          GetStringArg(index, 1, inverse_string_buffer);
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
