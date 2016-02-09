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

#ifndef ION_PROFILE_TRACERECORDER_H_
#define ION_PROFILE_TRACERECORDER_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/integral_types.h"
#include "ion/base/allocatable.h"
#include "ion/base/circularbuffer.h"
#include "ion/base/stlalloc/allocvector.h"
#include "ion/port/threadutils.h"
#include "ion/profile/timelineevent.h"
#include "ion/profile/timelinenode.h"
#include "third_party/jsoncpp/include/json/json.h"

namespace ion {
namespace profile {

class CallTraceManager;

// Class for recording frame events. This class tracks events based on
// pointers to raw string literals. It hashes the pointer value of the
// literal to a unique frame event id to keep track of events.
class TraceRecorder : public ion::base::Allocatable {
 public:
  struct TraceHeader {
    TraceHeader(uint32 id, uint32 time_micros)
        : id(id), time_micros(time_micros) {}
    uint32 id;  // Event id
    uint32 time_micros;  // in microseconds since the timebase
  };

  typedef base::CircularBuffer<uint32> TraceBuffer;
  typedef std::unordered_map<uint32, std::string> IndexToStringMap;

  explicit TraceRecorder(CallTraceManager* manager);

  // Explicitly specify the capacity of this recorder in bytes.
  TraceRecorder(CallTraceManager* manager, size_t buffer_size);

  // Manipulate the default buffer size in bytes used for future instantiations.
  static size_t GetDefaultBufferSize() { return s_default_buffer_size_; }
  static void SetDefaultBufferSize(size_t s) { s_default_buffer_size_ = s; }

  // Maniupulate whether or not to reserve the full buffer size immediately for
  // future instantiations.
  static bool GetReserveBuffer() { return s_reserve_buffer_; }
  static void SetReserveBuffer(bool reserve) { s_reserve_buffer_ = reserve; }

  // Queries and records the event corresponding to the provided event_id.
  void EnterScope(int event_id);

  // Attaches data to the current scope, which will be visible on mouse-over.
  // The string |value| must be in JSON format, e.g. "\"my_string\"" for a
  // string value, "18" for the integer value 18,
  // "{ \"name\": \"my_name\", \"count\": 17 }" for an object with two key
  // value pairs.
  // Note that |value| must not be a string representation of NaN or infinity,
  // because these values are not supported by JSON.
  void AnnotateCurrentScope(const std::string& name, const std::string& value);

  // Leaves the current (most recent) scope. Scope events must be strictly
  // nested.
  void LeaveScope();

  // Same as EnterScope, but with specified timestamp.
  void EnterScopeAtTime(uint32 timestamp, int event_id);

  // Same as AnnotateCurrentScope, but with specified timestamp.
  void AnnotateCurrentScopeAtTime(uint32 timestamp,
                                  const std::string& name,
                                  const std::string& value);

  // Same as LeaveScope, but with specified timestamp.
  void LeaveScopeAtTime(uint32 timestamp);

  // Records a frame enter event with a specified frame index.
  void EnterFrame(uint32 frame_number);

  // Records a frame exit event for the current frame index.
  void LeaveFrame();

  // Records the start of a time range event. The event is uniquely specified
  // by some 32-bit integer id, with a given name and optional string value in
  // JSON format. The name must be given, but the value is optional and can be
  // NULL.
  void EnterTimeRange(uint32 unique_id, const char* name, const char* value);

  // Records the start of a time range event. This method will automatically
  // assign and return a unique uint32 "id" used to identify the time range
  // event. Note the name must be given; however the parameter string |value|
  // (JSON format) is an optional value and can be NULL.
  uint32 EnterTimeRange(const char* name, const char* value);

  // Records the end of a time range event for the specified unique id.
  void LeaveTimeRange(uint32 id);

  // Records a timeStamp event.
  void CreateTimeStamp(const char* name, const char* value);

  // Same as CreateTimeStamp, but with specified timestamp.
  void CreateTimeStampAtTime(
      uint32 timestamp, const char* name, const char* value);

  // Returns the total number of recorded trace events.
  // Note: this is SLOW, it goes through a linear scan of the trace buffer.
  size_t GetNumTraces() const;

  // Returns the ID of the thread that this recorder is tracing.
  const ion::port::ThreadId& GetThreadId() const { return thread_id_; }

  // Sets a name for the thread that this recorder is tracing.
  void SetThreadName(const std::string& name) { thread_name_ = name; }

  // Returns the name for the thread that this recorder is tracing. The name is
  // "UnnamedThread" unless it has been set by a call to SetThreadName.
  std::string GetThreadName() const { return thread_name_; }

  // Dumps the strings one by one to a string table.
  void DumpStrings(std::vector<std::string>* table) const;

  // Appends a binary dump of the trace to the output string.
  void DumpTrace(std::string* output, uint32 string_index_offset) const;

  // Adds all events in the trace as a sub-tree under the passed in root node.
  void AddTraceToTimelineNode(TimelineNode* root) const;

  // Returns the frame number of the current frame scope, or 0 (and a warning
  // message) if TraceRecorder is not in a frame scope.
  uint32 GetCurrentFrameNumber() const;

  // Returns if the TraceRecorder is currently in a frame scope.
  bool IsInFrameScope() const { return frame_level_ > 0; }

 private:
  // Default size in bytes of future buffer instantiations.
  static size_t s_default_buffer_size_;
  // If true, reserve the entire buffer at the time of instantiation. Defaults
  // to false.
  static bool s_reserve_buffer_;

  // Returns an index to use for this string, and records it in the
  // string_buffer_ if necessary.
  uint32 GetStringIndex(const std::string& str);

  // Returns the string stored at argument location |arg_index| for the trace
  // event at position |index| in the trace buffer. |inverse_string_buffer| maps
  // from a string index to the actual string.
  std::string GetStringArg(size_t index, int arg_index,
                           const IndexToStringMap& inverse_string_buffer) const;

  // Returns a new timeline event for the trace event stored at position |index|
  // in the trace buffer. |inverse_string_buffer| is a map from a string index
  // to the actual string.
  std::unique_ptr<TimelineEvent> GetTimelineEvent(
      size_t index, const IndexToStringMap& inverse_string_buffer) const;

  // Reference to the parent CallTraceManager, used to query time.
  CallTraceManager* manager_;

  // Circular buffer of traces.
  TraceBuffer trace_buffer_;

  // Maps strings to their indexes.
  // TODO(user): The capacity on this isn't limited, it may be a good idea to
  // limit this.
  // TODO(user): The unlimited capacity can be a real issue when annotating
  // different string to every frame (e.g., annotating frame time in floating
  // point number). This increases memory usage without bound, and eventually
  // affects the actual frame time due to excessive look-up cost in
  // |string_buffer_|.
  std::unordered_map<std::string, uint32> string_buffer_;

  // Keep track of the scope level for inserting empty scope markers.
  int scope_level_;

  // The ID of the thread that this recorder is tracing.
  ion::port::ThreadId thread_id_;

  // The name for the thread that this recorder is tracing.
  std::string thread_name_;

  // These keep track of whether the TraceRecorder is currently in a frame, and
  // the frame number.
  int frame_level_;
  uint32 current_frame_number_;
};

}  // namespace profile
}  // namespace ion

#endif  // ION_PROFILE_TRACERECORDER_H_
