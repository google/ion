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

#include "ion/profile/calltracemanager.h"

#include <fstream>  // NOLINT

#include "ion/analytics/benchmark.h"
#include "ion/analytics/benchmarkutils.h"
#include "ion/base/bufferbuilder.h"
#include "ion/base/serialize.h"
#include "ion/port/threadutils.h"
#include "ion/profile/timelinenode.h"
#include "ion/profile/timelinethread.h"
#include "ion/profile/tracerecorder.h"
#include "third_party/jsoncpp/include/json/json.h"

namespace ion {
namespace profile {

namespace {

// WTF trace chunk header. Contains information about the parts contained
// within the chunk.
struct ChunkInfo {
  uint32 id;
  uint32 type;
  uint32 length;
  uint32 start_time;
  uint32 end_time;
  uint32 part_count;
};

// WTF trace part header. Contains information about the type of the part,
// and the length of the data within the part.
struct PartInfo {
  uint32 type;
  uint32 offset;
  uint32 length;
};

// Round up to nearest integer divisible by four.
static uint32 UpToNearestFour(uint32 n) {
  if (n % 4 == 0) {
    return n;
  }
  return n + (4 - (n % 4));
}

// Represents an abstract "part" in WTF trace format. A part could either be a
// file header, a string table, or a buffer of trace events. Usually the first
// "chunk" of a WTF trace file contains the file header part, and the second
// "chunk" contains a string table along with a list of trace event definitions,
// and the third chunk a string table along with the list of actual events.
// For more information, consult the WTF trace format documentation:
// https://github.com/google/tracing-framework/blob/master/docs/wtf-trace.md
class Part {
 public:
  virtual ~Part() {}

  virtual uint32 GetRawSizeInBytes() const = 0;

  virtual void AppendToBuffer(base::BufferBuilder* output) const = 0;

  // All part data within a chunk is aligned to 4b boundaries. This function
  // helps compute the size of the part so that it is padded to be a multiple
  // of 4 bytes.
  uint32 GetAlignedSizeInBytes() const {
    return UpToNearestFour(GetRawSizeInBytes());
  }
};

// The main data structure containing trace data in WTF format. A WTF trace file
// divides the data up into chunks, and each chunk is specified as follows:
// 4b  chunk id
// 4b  chunk type
// 4b  chunk length (including header)
// 4b  chunk starting time/value
// 4b  chunk ending time/value
// 4b  part count
// list of length part count, with the following info:
//   4b  part type
//   4b  part offset in chunk (from header end)
//   4b  part length
// list chunk data, if any
// For more information, consult the WTF trace format documentation:
// https://github.com/google/tracing-framework/blob/master/docs/wtf-trace.md
struct Chunk {
  void AddPart(uint32 type, Part* part) {
    DCHECK(part);

    PartInfo info;
    info.type = type;
    info.length = part->GetRawSizeInBytes();
    info.offset = 0;
    for (size_t i = 0; i < parts.size(); i++) {
      info.offset += parts[i]->GetAlignedSizeInBytes();
    }
    part_headers.push_back(info);
    parts.push_back(part);
  }

  void AppendToBuffer(uint32 id, uint32 type, base::BufferBuilder* output) {
    DCHECK(parts.size() == part_headers.size());

    ChunkInfo info;
    info.part_count = static_cast<uint32>(parts.size());
    info.id = id;
    info.type = type;
    info.length = static_cast<uint32>(sizeof(ChunkInfo)) +
        static_cast<uint32>(sizeof(PartInfo)) * info.part_count;
    for (size_t i = 0; i < parts.size(); i++) {
      info.length += parts[i]->GetAlignedSizeInBytes();
    }
    info.start_time = -1;
    info.end_time = -1;

    output->Append(info);
    for (size_t i = 0; i < part_headers.size(); i++) {
      output->Append(part_headers[i]);
    }
    for (size_t i = 0; i < parts.size(); i++) {
      parts[i]->AppendToBuffer(output);
    }
  }

  std::vector<PartInfo> part_headers;
  std::vector<Part*> parts;
};

class StringTable : public Part {
 public:
  StringTable()
      : include_null_(true) {}

  explicit StringTable(bool include_null)
      : include_null_(include_null) {}

  void AddString(const std::string& str) {
    const std::vector<std::string> split = base::SplitString(str, "\n");
    for (size_t i = 0; i < split.size(); ++i) {
      table_.push_back(split[i]);
    }
  }

  void AddStrings(std::vector<std::string> strings) {
    table_.reserve(table_.size() + strings.size());
    for (std::string& string : strings) {
      table_.emplace_back(std::move(string));
    }
  }

  uint32 GetRawSizeInBytes() const override {
    size_t raw_size_in_bytes = 0;
    for (size_t i = 0; i < table_.size(); ++i) {
      const size_t string_length = table_[i].length();
      if (include_null_) {
        raw_size_in_bytes += string_length + 1;
      } else {
        raw_size_in_bytes += string_length;
      }
    }
    return static_cast<uint32>(raw_size_in_bytes);
  }

  uint32 GetTableSize() const { return static_cast<uint32>(table_.size()); }

  void AppendToBuffer(base::BufferBuilder* output) const override {
    for (size_t i = 0; i < table_.size(); ++i) {
      output->AppendArray(table_[i].data(), table_[i].size());
      if (include_null_) {
        // Append final NUL character
        char ch = 0;
        output->Append(ch);
      }
    }

    // Pad the section with NUL characters so that the start of the next
    // section is word aligned (aligned with 4-byte addresses).
    uint32 extra = GetAlignedSizeInBytes() - GetRawSizeInBytes();
    for (uint32 i = 0; i < extra; ++i) {
      char ch = 0;
      output->Append(ch);
    }
  }

 private:
  std::vector<std::string> table_;
  const bool include_null_;
};

struct EventBuffer : public Part {
 public:
  uint32 GetRawSizeInBytes() const override {
    return static_cast<uint32>(buffer.Size());
  }

  void AppendToBuffer(base::BufferBuilder* output) const override {
    output->Append(buffer);
  }

  base::BufferBuilder buffer;
};

}  // namespace

ScopedTracer::ScopedTracer(TraceRecorder* recorder, const char* name)
    : recorder_(recorder) {
  DCHECK(recorder_ && name);
  recorder_->EnterScope(recorder_->GetScopeEvent(name));
}

ScopedTracer::~ScopedTracer() {
  DCHECK(recorder_);
  recorder_->LeaveScope();
}

ScopedFrameTracer::ScopedFrameTracer(TraceRecorder* recorder, uint32 id)
    : recorder_(recorder) {
  DCHECK(recorder_);
  recorder_->EnterFrame(id);
}

ScopedFrameTracer::~ScopedFrameTracer() {
  DCHECK(recorder_);
  recorder_->LeaveFrame();
}

CallTraceManager::CallTraceManager() : CallTraceManager(0) {}

CallTraceManager::CallTraceManager(size_t buffer_size)
    : trace_recorder_(GetAllocator()),
      named_trace_recorders_(GetAllocator()),
      recorder_list_(GetAllocator()),
      buffer_size_(buffer_size),
      string_table_(new base::StringTable()),
      scope_events_(new base::StringTable()) {}

CallTraceManager::~CallTraceManager() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!timeline_metrics_.empty()) {
    ion::analytics::Benchmark benchmark = RunTimelineMetrics();
    ion::analytics::OutputBenchmarkPretty("Timeline Metrics", false, benchmark,
                                          std::cout);
  }

  for (size_t i = 0; i < recorder_list_.size(); ++i) {
    delete recorder_list_[i];
    recorder_list_[i] = nullptr;
  }
}

TraceRecorder* CallTraceManager::GetTraceRecorder() {
  void* ptr = port::GetThreadLocalStorage(trace_recorder_.GetKey());
  if (ptr) {
    return *static_cast<TraceRecorder**>(ptr);
  } else {
    TraceRecorder** recorder = trace_recorder_.Get();
    *recorder = AllocateTraceRecorder();
    return *recorder;
  }
}

TraceRecorder* CallTraceManager::GetNamedTraceRecorder(
    NamedTraceRecorderType name) {
  DCHECK(name < kNumNamedTraceRecorders);
  NamedTraceRecorderArray* recorders = named_trace_recorders_.Get();
  TraceRecorder* recorder = recorders->recorders[name];
  if (recorder) {
    return recorder;
  }
  recorder = AllocateTraceRecorder();
  switch (name) {
    case kRecorderGpu:
      recorder->SetThreadName("GPU");
      break;
    case kRecorderVSync:
      recorder->SetThreadName("VSync");
      break;
    default:
      LOG(WARNING) << "Unknown name(" << name << ") for named TraceRecorder.";
  }
  recorders->recorders[name] = recorder;
  return recorder;
}

TraceRecorder* CallTraceManager::AllocateTraceRecorder() {
  TraceRecorder* recorder = nullptr;
  if (buffer_size_ == 0) {
    recorder = new (GetAllocator()) TraceRecorder(this);
  } else {
    recorder = new (GetAllocator()) TraceRecorder(this, buffer_size_);
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    recorder_list_.push_back(recorder);
  }
  return recorder;
}

int CallTraceManager::GetNumArgsForEvent(uint32 event_id) {
  // These are the number of arguments for each built-in trace event.
  // The built-in WTF trace events that we support are documented in
  // the CallTraceManager::SnapshotCallTraces() function.
  static const int kBuiltinEventArgNum[] = {
      0, 5, 0, 4, 1, 1, 1, 0, 0, 2, 2, 2, 3, 1, 1, 1, 1, 1};
  if (event_id >= kCustomScopeEvent) {
    event_id = 0;
  }
  return kBuiltinEventArgNum[event_id];
}

CallTraceManager::EventArgType CallTraceManager::GetArgType(
    uint32 event_id, int arg_index) {
  CHECK(event_id < kCustomScopeEvent);

  // Provides the offset into the kBuiltinEventArgTypes array below to
  // look up the argument types of each built-in WTF trace event.
  static const int kOffsetTable[] = {
      0, 1, 6, 7, 11, 12, 13, 14, 15, 16, 18, 20, 22, 25, 26, 27, 28, 29};

  // Stores the types of each argument for every built-in WTF trace event.
  // The built-in WTF trace events that we support are documented in
  // the CallTraceManager::SnapshotCallTraces() function.
  static const EventArgType kBuiltinEventArgTypes[] = {
      kArgNone,  // The zeroth event is reserved and has no arguments.
      kArgNumeric, kArgNumeric, kArgNumeric, kArgString, kArgString,
      kArgNone,
      kArgNumeric, kArgString, kArgString, kArgString,
      kArgNumeric,
      kArgNumeric,
      kArgString,
      kArgNone,
      kArgNone,
      kArgString, kArgString,
      kArgString, kArgString,
      kArgString, kArgString,
      kArgNumeric, kArgString, kArgString,
      kArgNumeric,
      kArgNumeric,
      kArgNumeric,
      kArgString,
      kArgNumeric};

  return kBuiltinEventArgTypes[kOffsetTable[event_id] + arg_index];
}

std::string CallTraceManager::SnapshotCallTraces() const {
  base::BufferBuilder output;

  static const uint32 magic_number = 0xdeadbeef;
  static const uint32 wtf_version = 0xe8214400;
  static const uint32 format_version = 10;

  output.Append(magic_number);
  output.Append(wtf_version);
  output.Append(format_version);

  // Create the file header
  Json::Value flags(Json::arrayValue);
  flags.append("has_high_resolution_times");

  // 
  // publicly available icon image.
  Json::Value icon(Json::objectValue);
  icon["uri"] = "https://maps.gstatic.com/favicon3.ico";

  // 
  // type, and value information.
  Json::Value agent(Json::objectValue);
  agent["device"] = "Ion";
  agent["platform"] = "SomePlatform";
  agent["platformVersion"] = "";
  agent["type"] = "";
  agent["value"] = "";

  Json::Value context(Json::objectValue);
  context["args"] = Json::Value(Json::arrayValue);
  context["contextType"] = "script";
  context["icon"] = icon;
  context["taskId"] = "";
  context["title"] = "Ion";
  // 
  context["userAgent"] = agent;

  Json::Value json;
  json["type"] = "file_header";
  json["flags"] = flags;
  // 
  json["timebase"] = 1412611454780.701;
  json["contextInfo"] = context;

  Json::FastWriter json_writer;
  std::string json_string = json_writer.write(json);
  StringTable file_header_table(false);
  file_header_table.AddString(json_string);

  Chunk file_header;
  file_header.AddPart(0x10000, &file_header_table);
  file_header.AppendToBuffer(2, 0x1, &output);

  StringTable def_table;
  def_table.AddString(
      "wtf.event#define\n"
      "uint16 wireId, uint16 eventClass, uint32 flags, ascii name, ascii args\n"
      "wtf.trace#discontinuity\n"
      "wtf.zone#create\n"
      "uint16 zoneId, ascii name, ascii type, ascii location\n"
      "wtf.zone#delete\n"
      "uint16 zoneId\n"
      "wtf.zone#set\n"
      "uint16 zoneId\n"
      "wtf.scope#enter\n"
      "ascii name\n"
      "wtf.scope#enterTracing\n"
      "wtf.scope#leave\n"
      "wtf.scope#appendData\n"
      "ascii name, any value\n"
      "wtf.trace#mark\n"
      "ascii name, any value\n"
      "wtf.trace#timeStamp\n"
      "ascii name, any value\n"
      "wtf.timeRange#begin\n"
      "uint32 id, ascii name, any value\n"
      "wtf.timeRange#end\n"
      "uint32 id\n"
      "wtf.timing#frameStart\n"
      "uint32 number\n"
      "wtf.timing#frameEnd\n"
      "uint32 number\n"
      "wtf.scope#appendData_url_utf8\n"
      "utf8 url\n"
      "wtf.scope#appendData_readyState_int32\n"
      "int32 readyState");

  // This offset is used to index into strings defining custom scope events.
  const uint32 event_string_offset = def_table.GetTableSize();
  std::vector<std::string> scope_event_names = scope_events_->GetTable();
  const size_t scope_events_count = scope_event_names.size();
  def_table.AddStrings(std::move(scope_event_names));

  // Note: these are the built-in WTF events that are being defined below.
  // wireId (1)    wtf.event#define (uint16 wireId, uint16 eventClass,
  //                                 uint32 flags, ascii name, ascii args)
  // wireId (2)    wtf.trace#discontinuity ()
  // wireId (3)    wtf.zone#create (uint16 zoneId, ascii name, ascii type,
  //                                ascii location)
  // wireId (4)    wtf.zone#delete (uint16 zoneId)
  // wireId (5)    wtf.zone#set (uint16 zoneId)
  // wireId (6)    wtf.scope#enter (ascii name)
  // wireId (7)    wtf.scope#enterTracing ()
  // wireId (8)    wtf.scope#leave ()
  // wireId (9)    wtf.scope#appendData (ascii name, any value)
  // wireId (10)   wtf.trace#mark (ascii name, any value)
  // wireId (11)   wtf.trace#timeStamp (ascii name, any value)
  // wireId (12)   wtf.timeRange#begin (uint32 id, ascii name, any value)
  // wireId (13)   wtf.timeRange#end (uint32 id)
  // wireId (14)   wtf.timing#frameStart (uint32 number)
  // wireId (15)   wtf.timing#frameEnd (uint32 number)
  // wireId (16)   wtf.scope#appendData_url_utf8 (utf8 url)
  // wireId (17)   wtf.scope#appendData_readyState_int32 (int32 readyState)

  EventBuffer def_events;
  {
    base::BufferBuilder& event_buffer = def_events.buffer;
    uint32 builtin[] = {
        1, 0, 1, 0, 40, 0, 1,
        1, 0, 2, 0, 32, 2, 0xffffffff,
        1, 0, 3, 0, 40, 3, 4,
        1, 0, 4, 0, 40, 5, 6,
        1, 0, 5, 0, 40, 7, 8,
        1, 0, 6, 1, 32, 9, 10,
        1, 0, 7, 1, 44, 11, 0xffffffff,
        1, 0, 8, 0, 40, 12, 0xffffffff,
        1, 0, 9, 0, 56, 13, 14,
        1, 0, 10, 0, 40, 15, 16,
        1, 0, 11, 0, 32, 17, 18,
        1, 0, 12, 0, 40, 19, 20,
        1, 0, 13, 0, 40, 21, 22,
        1, 0, 14, 0, 8, 23, 24,
        1, 0, 15, 0, 8, 25, 26,
        1, 0, 16, 0, 24, 27, 28,
        1, 0, 17, 0, 24, 29, 30
    };
    event_buffer.AppendArray(builtin, sizeof(builtin) / sizeof(*builtin));

    // Define each scope event
    {
      uint32 temp;
      for (uint32 i = 0; i < scope_events_count; ++i) {
        temp = kDefineEvent;
        event_buffer.Append(temp);  // wtf.event#define
        temp = 0;
        event_buffer.Append(temp);  // timestamp
        temp = i + kCustomScopeEvent;
        event_buffer.Append(temp);  // wireId
        temp = 1;
        event_buffer.Append(temp);  // eventClass (scope)
        temp = 0;
        event_buffer.Append(temp);  // flags (unused)
        temp = event_string_offset + i;
        event_buffer.Append(temp);  // name
        temp = -1;
        event_buffer.Append(temp);  // args (none)
      }
    }
  }

  Chunk events_defined;
  events_defined.AddPart(0x30000, &def_table);
  events_defined.AddPart(0x20002, &def_events);
  events_defined.AppendToBuffer(3, 0x2, &output);

  const int num_trace_threads = static_cast<int>(recorder_list_.size());
  StringTable table;
  table.AddStrings(string_table_->GetTable());
  const uint32 zone_type_string = table.GetTableSize();
  table.AddString("script");
  const uint32 zone_location_string = table.GetTableSize();
  table.AddString("Some_Location");

  EventBuffer events;
  {
    base::BufferBuilder& event_buffer = events.buffer;
    for (int chunk_i = 0; chunk_i < num_trace_threads; ++chunk_i) {
      const int zone_id = chunk_i + 1;
      const uint32 zone_name_string = table.GetTableSize();
      table.AddString(std::string("Thread_") + base::ValueToString(zone_id));

      uint32 temp;

      // Create a new zone
      temp = kCreateZoneEvent;
      event_buffer.Append(temp);
      temp = 0;
      event_buffer.Append(temp);  // timestamp
      temp = zone_id;
      event_buffer.Append(temp);  // Zone id
      temp = zone_name_string;
      event_buffer.Append(temp);  // Zone name
      temp = zone_type_string;
      event_buffer.Append(temp);  // Zone type
      temp = zone_location_string;
      event_buffer.Append(temp);  // Zone location
    }

    for (int chunk_i = 0; chunk_i < num_trace_threads; ++chunk_i) {
      TraceRecorder* rec = recorder_list_[chunk_i];
      const int zone_id = chunk_i + 1;
      uint32 temp;

      // Set the zone id
      temp = kSetZoneEvent;
      event_buffer.Append(temp);
      temp = 0;
      event_buffer.Append(temp);  // timestamp
      temp = zone_id;
      event_buffer.Append(temp);  // Zone id

      // Define each trace event
      rec->DumpTrace(&event_buffer);
    }
  }

  Chunk trace;
  trace.AddPart(0x30000, &table);
  trace.AddPart(0x20002, &events);
  trace.AppendToBuffer(1, 0x2, &output);

  return output.Build();
}

void CallTraceManager::WriteFile(const std::string& filename) const {
  if (!filename.empty()) {
    LOG(INFO) << "Writing current WTF traces to: " << filename;
    std::ofstream filestream(
        filename.c_str(), std::ios::out | std::ios::binary);
    if (filestream.good()) {
      filestream << SnapshotCallTraces();
      filestream.close();
    } else {
      LOG(WARNING) << "Failed to open " << filename
                   << " for writing WTF traces.";
    }
  }
}

Timeline CallTraceManager::BuildTimeline() const {
  std::unique_ptr<TimelineNode> root(new TimelineNode("root"));
  for (const auto recorder : recorder_list_) {
    std::unique_ptr<TimelineThread> thread(
        new TimelineThread(recorder->GetThreadName(), recorder->GetThreadId()));
    recorder->AddTraceToTimelineNode(thread.get());
    root->AddChild(std::move(thread));
  }

  return Timeline(std::move(root));
}

analytics::Benchmark CallTraceManager::RunTimelineMetrics() const {
  analytics::Benchmark benchmark;
  Timeline timeline = BuildTimeline();
  for (const auto& metric : timeline_metrics_)
    metric->Run(timeline, &benchmark);
  return benchmark;
}

}  // namespace profile
}  // namespace ion
