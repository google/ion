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

#include <atomic>
#include <chrono>  // NOLINT
#include <fstream>  // NOLINT
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <string>

#include "ion/analytics/benchmark.h"
#include "ion/base/serialize.h"
#include "ion/base/stringutils.h"
#include "ion/base/threadspawner.h"
#include "ion/gfx/tests/fakeglcontext.h"
#include "ion/gfx/tests/fakegraphicsmanager.h"
#include "ion/gfxprofile/gpuprofiler.h"
#include "ion/port/atomic.h"
#include "ion/port/fileutils.h"
#include "ion/port/timer.h"
#include "ion/portgfx/glcontext.h"
#include "ion/profile/timeline.h"
#include "ion/profile/timelineevent.h"
#include "ion/profile/timelineframe.h"
#include "ion/profile/timelinemetric.h"
#include "ion/profile/timelinenode.h"
#include "ion/profile/timelinesearch.h"
#include "ion/profile/timelinethread.h"
#include "ion/profile/tracerecorder.h"
#include "ion/profile/vsyncprofiler.h"

#include "absl/memory/memory.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"
#include "third_party/jsoncpp/include/json/json.h"

namespace ion {
namespace profile {

using gfxprofile::GpuProfiler;
using gfxprofile::ScopedGlTracer;

typedef std::vector<std::string> StringVec;
typedef std::shared_ptr<StringVec> StringVecPtr;

// Stores information about a chunk.
struct ChunkHeader {
  uint32 id;
  uint32 type;
  uint32 length;
  uint32 start_time;
  uint32 end_time;
  uint32 part_count;
};

// Stores information about a part.
struct PartHeader {
  uint32 type;
  uint32 offset;
  uint32 length;
};

// Stores a part, which can be either a file header, string table, or trace
// event buffer.
struct Part {
  explicit Part(const PartHeader& header) : header(header) {
    EXPECT_EQ(0U, header.offset % 4);
  }

  virtual ~Part() {}

  // Utility function to print out a human-readable version of the trace.
  // This is purely for debugging use.
  virtual void AppendToStream(std::ostream* os_ptr) const {
    std::ostream& os = *os_ptr;
    os << "    Part type: 0x" << std::hex << header.type << std::dec
       << std::endl;
    os << "    Part offset in chunk: " << header.offset << std::endl;
    os << "    Part length: " << header.length << std::endl;
  }

  PartHeader header;
};

// Stores a specific event along with its arguments. This class also contains
// information about the types of each argument and their values.
struct Event {
  // Initialize an event with a name and argument info.
  Event(const std::string& name, const std::string& args_info)
      : name(name) {
    if (!args_info.empty()) {
      const std::vector<std::string> all_info =
          base::SplitString(args_info, ",");
      for (size_t i = 0; i < all_info.size(); ++i) {
        const std::string& arg_info = all_info[i];
        const std::vector<std::string> info = base::SplitString(arg_info, " ");
        CHECK_EQ(info.size(), 2U);
        arg_type.push_back(info[0]);
        arg_name.push_back(info[1]);
        arg_value.push_back(-1);
      }
    }
  }

  // Parse the binary argument data for this event, based on the pre-defined
  // argument types.
  uint32 ParseArgs(const std::vector<char>& data, int offset,
                   const StringVecPtr& string_table) {
    this->string_table = string_table;
    EXPECT_TRUE(string_table);
    uint32 local_offset = 0;
    for (size_t i = 0; i < arg_type.size(); ++i) {
      const std::string& type = arg_type[i];

      if (type == "ascii" || type == "utf8") {
        // Look up ascii/utf8 arguments from the string table, interpet as
        // a signed integer.
        int32 string_index =
            *reinterpret_cast<const int32*>(&data[offset + local_offset]);
        local_offset += 4;
        arg_value[i] = string_index;
        if (string_index >= 0) {
          EXPECT_GT(static_cast<int>(string_table->size()), arg_value[i]);
        }
      } else {
        // All other arguments, we just assume it's an unsigned integer.
        int32 value =
            *reinterpret_cast<const uint32*>(&data[offset + local_offset]);
        local_offset += 4;
        arg_value[i] = value;
      }
    }
    return local_offset;
  }

  // Return the value of a string argument as a std::string.
  std::string GetAsciiArg(const std::string& name) const {
    std::string result;
    for (size_t i = 0; i < arg_name.size(); ++i) {
      if (arg_name[i] == name && arg_value[i] >= 0 && (arg_type[i] == "ascii" ||
          arg_type[i] == "utf8" || arg_type[i] == "any")) {
        EXPECT_TRUE(string_table);
        EXPECT_GT(static_cast<int>(string_table->size()), arg_value[i]);
        result = string_table->at(arg_value[i]);
        break;
      }
    }
    return result;
  }

  // Return the value of a non-string argument as a uint32.
  uint32 GetGenericArg(const std::string& name) const {
    uint32 result = 0;
    for (size_t i = 0; i < arg_name.size(); ++i) {
      if (arg_name[i] == name) {
        result = arg_value[i];
        break;
      }
    }
    return result;
  }

  // The event name.
  std::string name;
  // Event specification: list of argument names and types.
  std::vector<std::string> arg_name;
  std::vector<std::string> arg_type;
  // This value could be one of many different types. Right now we distinguish
  // only between numbers and string table indices, and static_cast them to
  // int32s.
  std::vector<int32> arg_value;
  // String table associated with defining these events.
  StringVecPtr string_table;
  // Event time.
  uint32 time_value;
};

// Stores a part that contains the file header.
struct FileHeader : public Part {
  explicit FileHeader(const PartHeader& header) : Part(header) {
    EXPECT_EQ(0x10000U, header.type);
  }

  void AppendToStream(std::ostream* os_ptr) const override {
    Part::AppendToStream(os_ptr);
    std::ostream& os = *os_ptr;
    os << "<<<FILE HEADER BELOW>>>" << std::endl;
    os << file_header_string << std::endl;
  }

  std::string file_header_string;
};

// Stores a part that contains a string table.
struct StringTable : public Part {
  explicit StringTable(const PartHeader& header) : Part(header) {
    EXPECT_EQ(0x30000U, header.type);
  }

  void AppendToStream(std::ostream* os_ptr) const override {
    Part::AppendToStream(os_ptr);
    std::ostream& os = *os_ptr;

    for (size_t i = 0; i < table->size(); ++i) {
      os << "      String table [" << i << "]: " << table->at(i) << std::endl;
    }
  }

  StringVecPtr table;
};

// Stores a part that contains a buffer of trace events.
struct EventBuffer : public Part {
  explicit EventBuffer(const PartHeader& header) : Part(header) {
    EXPECT_EQ(0x20002U, header.type);
  }

  void AppendToStream(std::ostream* os_ptr) const override {
    Part::AppendToStream(os_ptr);
    std::ostream& os = *os_ptr;

    for (size_t i = 0; i < events.size(); ++i) {
      const Event& event = events[i];
      os << "[" << event.time_value << " us] ";
      os << event.name;
      if (!event.arg_type.empty()) {
        os << ":";
      }

      for (size_t i = 0; i < event.arg_type.size(); ++i) {
        const std::string& type = event.arg_type[i];
        os << " " << event.arg_name[i];

        if (type == "ascii" || type == "utf8") {
          // Print ascii/utf8 arguments from the string table.
          int32 string_index = event.arg_value[i];
          if (string_index >= 0) {
            os << " (" << event.string_table->at(string_index) << ")";
          } else {
            os << " (empty)";
          }
        } else {
          // Just print the value directly.
          os << " (" << event.arg_value[i] << ")";
        }
      }

      os << std::endl;
    }
  }

  std::vector<Event> events;
};

// Stores a chunk, which consists of a header and multiple parts.
struct Chunk {
  ChunkHeader chunk_info;
  std::vector<std::shared_ptr<Part>> parts;
};

// Utility function to print out a human-readable version of the trace.
std::ostream& operator<<(std::ostream& os, const Part& part) {
  part.AppendToStream(&os);
  return os;
}

// Utility function to print out a human-readable version of the trace.
std::ostream& operator<<(std::ostream& os, const Chunk& chunk) {
  os << "Chunk header" << std::endl;
  os << "  Chunk ID: " << chunk.chunk_info.id << std::endl;
  os << "  Chunk Type: 0x" << std::hex << chunk.chunk_info.type << std::dec
     << std::endl;
  os << "  Chunk Length: " << chunk.chunk_info.length << std::endl;
  os << "  Chunk Start Time: " << chunk.chunk_info.start_time << std::endl;
  os << "  Chunk End Time: " << chunk.chunk_info.end_time << std::endl;
  os << "  Chunk Part Count: " << chunk.chunk_info.part_count << std::endl;

  for (size_t i = 0; i < chunk.parts.size(); ++i) {
    if (!chunk.parts[i]) {
      continue;
    }
    os << *chunk.parts[i];
  }

  return os;
}

// Class to parse a binary WTF trace file. Used for debugging and testing
// development of WTF traces.
class TraceReader {
 public:
  explicit TraceReader(const std::string& data)
      : data_source_(data), read_offset_(0) {}

  // The main function for performing the binary trace file parsing.
  void Parse() {
    chunks_.clear();

    // We should at least have the WTF header.
    ASSERT_LE(12, static_cast<int>(data_source_.length()));

    // Read the WTF header (12 bytes).
    uint32 wtf_header[3];
    Read(wtf_header, sizeof(wtf_header));
    EXPECT_EQ(0xdeadbeef, wtf_header[0]);
    EXPECT_EQ(0xe8214400, wtf_header[1]);
    EXPECT_EQ(10U, wtf_header[2]);

    // Initialize table of events.
    std::map<int, Event> event_table;
    event_table.insert(std::make_pair(1, Event(
        "wtf.event#define", "uint16 wireId, uint16 eventClass, "
        "uint32 flags, ascii name, ascii args")));

    // Read each chunk one by one.
    ChunkHeader chunk_header;
    while (ReadPossible()) {
      Read(&chunk_header, sizeof(chunk_header));
      size_t data_offset = sizeof(chunk_header);

      // Check if chunk sizes are a multiple of four.
      EXPECT_EQ(0U, chunk_header.length % 4);

      uint32 num_parts = chunk_header.part_count;
      std::vector<PartHeader> part_headers;
      for (uint32 part_i = 0; part_i < num_parts; ++part_i) {
        PartHeader part_header;
        Read(&part_header, sizeof(part_header));

        // Check if part offsets are a multiple of four.
        EXPECT_EQ(0U, part_header.offset % 4);

        if (part_i > 0) {
          uint32 prev_length = part_headers.back().length;
          uint32 cur_offset = part_header.offset;

          // Check that offset is at least length of previous part.
          EXPECT_LE(prev_length, cur_offset);

          // Check that offset is at most 3 more than length of previous part.
          EXPECT_GT(4U, cur_offset - prev_length);
        }

        part_headers.push_back(part_header);
        data_offset += sizeof(part_header);
      }

      // Keep track of this chunk's string table.
      StringVecPtr chunk_string_table;

      // Read data for each part.
      std::vector<std::shared_ptr<Part>> parts;
      size_t part_data_offset = 0;
      for (uint32 part_i = 0; part_i < num_parts; ++part_i) {
        const PartHeader& part_header = part_headers[part_i];

        ReadExtra(part_header.offset, &part_data_offset);

        if (part_header.length == 0) {
          continue;
        }

        std::vector<char> data(part_header.length);
        Read(&data[0], part_header.length);
        part_data_offset += part_header.length;

        if (part_header.type == 0x30000) {
          // Parse string table.
          StringVecPtr string_table(new StringVec());
          string current;
          for (size_t char_i = 0; char_i < data.size(); ++char_i) {
            char c = data[char_i];
            if (c == '\0') {
              string_table->push_back(current);
              current.clear();
            } else {
              current += c;
            }
          }
          chunk_string_table = string_table;

          std::shared_ptr<StringTable> st(new StringTable(part_header));
          st->table = string_table;
          parts.push_back(st);
        } else if (part_header.type == 0x10000) {
          // Read file header.
          std::string file_header(&data[0], data.size());
          std::shared_ptr<FileHeader> fh(new FileHeader(part_header));
          fh->file_header_string = file_header;
          parts.push_back(fh);

          // Test if the file header is a valid JSON string and has the minimum
          // number of components.
          Json::Reader reader;
          Json::Value json;
          EXPECT_TRUE(reader.parse(file_header, json, false));
          EXPECT_TRUE(json.isMember("type"));
          EXPECT_TRUE(json.isMember("flags"));
          EXPECT_TRUE(json.isMember("timebase"));
          EXPECT_TRUE(json.isMember("contextInfo"));
          EXPECT_TRUE(json["type"].isString());
          EXPECT_TRUE(json["flags"].isArray());
          EXPECT_TRUE(json["timebase"].isDouble());
          EXPECT_TRUE(json["contextInfo"].isObject());
          EXPECT_EQ(json["type"].asString(), "file_header");
          EXPECT_GE(json["flags"].size(), 1U);
          EXPECT_TRUE(json["flags"][0U].isString());
          EXPECT_EQ(json["flags"][0U].asString(), "has_high_resolution_times");
        } else if (part_header.type == 0x20002) {
          // Read event buffer.
          std::shared_ptr<EventBuffer> eb(new EventBuffer(part_header));
          uint32 event_byte_offset = 0;
          uint32 wire_id;
          uint32 time_value;
          int scope_nesting_count = 0;

          do {
            wire_id = *reinterpret_cast<uint32*>(&data[event_byte_offset + 0]);
            time_value =
                *reinterpret_cast<uint32*>(&data[event_byte_offset + 4]);
            event_byte_offset += 8;

            auto it = event_table.find(wire_id);
            ASSERT_NE(it, event_table.end());

            Event new_event(it->second);
            event_byte_offset += new_event.ParseArgs(
                data, event_byte_offset, chunk_string_table);
            new_event.time_value = time_value;

            eb->events.push_back(new_event);

            // We know event type 1: we use it to define other events
            if (wire_id == 1) {
              EXPECT_TRUE(chunk_string_table);

              event_table.insert(std::make_pair(
                  new_event.GetGenericArg("wireId"),
                  Event(new_event.GetAsciiArg("name"),
                        new_event.GetAsciiArg("args"))));
            } else {
              const std::string& nm = new_event.name;
              const std::string wtf_prefix("wtf.");

              // Check that the event list is properly nested.
              if (wire_id >= CallTraceManager::kCustomScopeEvent &&
                  nm.compare(0, wtf_prefix.length(), wtf_prefix) != 0) {
                scope_nesting_count++;
                EXPECT_LE(0, scope_nesting_count);
              } else if (nm == "wtf.scope#leave") {
                scope_nesting_count--;
                EXPECT_LE(0, scope_nesting_count);
              }
            }
          } while (event_byte_offset < data.size());

          // Check that the scope nesting has been terminated.
          EXPECT_EQ(0, scope_nesting_count);

          parts.push_back(eb);
        }
      }

      data_offset += part_data_offset;

      ReadExtra(chunk_header.length, &data_offset);

      // Store the chunk.
      Chunk new_chunk;
      new_chunk.chunk_info = chunk_header;
      new_chunk.parts.swap(parts);
      chunks_.push_back(new_chunk);
    }

    // In the current file format there should be exactly three chunks, one for
    // the file header, one for defining the events, and lastly for the event
    // buffer.
    ASSERT_EQ(3U, chunks_.size());
    ASSERT_EQ(0x20002U, chunks_.back().parts.back()->header.type);
  }

  // Return the main event buffer that lists all of the zones and traces for
  // each zone.
  const std::vector<Event>& GetMainEventBuffer() {
    if (chunks_.empty()) {
      Parse();
    }
    return static_cast<const EventBuffer&>(*chunks_.back().parts.back()).events;
  }

  const std::vector<Chunk>& GetChunks() const { return chunks_; }

 private:
  // Read "length" number of bytes from the data source.
  void Read(void* output, size_t length) {
    memcpy(output, &data_source_[read_offset_], length);
    read_offset_ += length;
  }

  bool ReadPossible() {
    return read_offset_ < data_source_.length();
  }

  void ReadExtra(size_t expected_offset, size_t* actual_offset) {
    ASSERT_TRUE(actual_offset != nullptr);
    ASSERT_LE(*actual_offset, expected_offset);
    if (*actual_offset != expected_offset) {
      // Read additional bytes until we're at the correct offset.
      size_t extra = expected_offset - *actual_offset;

      std::vector<char> extra_data(extra);
      Read(&extra_data[0], extra);
      *actual_offset += extra;
    }
    ASSERT_EQ(expected_offset, *actual_offset);
  }

  // The input binary WTF trace data.
  std::string data_source_;
  // Stores the current reading offset into the data to help parsing.
  size_t read_offset_;
  // Parsed data stored as chunks.
  std::vector<Chunk> chunks_;
};

// Utility function to print out a human-readable version of the trace.
std::ostream& operator<<(std::ostream& os, const TraceReader& trace) {
  for (size_t i = 0; i < trace.GetChunks().size(); ++i) {
    os << trace.GetChunks()[i];
  }
  return os;
}

class CallTraceManagerWithMockTimer : public CallTraceManager {
 public:
  CallTraceManagerWithMockTimer() : time_in_us_(0U) {}
  uint32 GetTimeInUs() const override { return time_in_us_.load(); }
  void AdvanceTimer(const uint32 microseconds) { time_in_us_ += microseconds; }

 private:
  std::atomic<uint32> time_in_us_;
};

class CallTraceTest : public testing::Test {
 protected:
  void SetUp() override {
    // Code here will be called before *each* test.
    call_trace_manager_ = absl::make_unique<CallTraceManagerWithMockTimer>();
    gpu_profiler_.reset(new ion::profile::GpuProfiler(
        call_trace_manager_.get()));
  }

  // Code in TearDown will be called after *each* test.
  //
  // No need to implement TearDown() since call_trace_manager_
  // will be released by ~Test() after each test runs.
  // virtual void TearDown() {}

  TraceRecorder* GetTraceRecorder() {
    return call_trace_manager_->GetTraceRecorder();
  }

  // Returns true if GPU tracing is supported.
  bool AllowGpuTracing() {
    // Enable mock GPU tracing.
    gl_context_ = gfx::testing::FakeGlContext::Create(1, 1);
    portgfx::GlContext::MakeCurrent(gl_context_);
    fake_gm_.Reset(new gfx::testing::FakeGraphicsManager());
    gpu_profiler_->SetGraphicsManager(fake_gm_);
    return gpu_profiler_->IsGpuProfilingSupported(fake_gm_);
  }

  void EnableGpuTracing() {
    gpu_profiler_->SetEnableGpuTracing(true);
  }

  GpuProfiler* GetGpuProfiler() {
    return gpu_profiler_.get();
  }

  TraceRecorder* GetGpuTraceRecorder() {
    return call_trace_manager_->GetNamedTraceRecorder(
        CallTraceManager::kRecorderGpu);
  }

  const base::AllocVector<TraceRecorder*>& GetAllTraceRecorders() const {
    return call_trace_manager_->GetAllTraceRecorders();
  }

  void PollGlTimerQueries() {
    return gpu_profiler_->PollGlTimerQueries();
  }

  // Return the number of unique custom scope events recorded.
  size_t GetNumScopeEvents() const {
    return call_trace_manager_->GetScopeEventTable()->GetSize();
  }

  std::unique_ptr<CallTraceManagerWithMockTimer> call_trace_manager_;
  std::unique_ptr<GpuProfiler> gpu_profiler_;
  ion::portgfx::GlContextPtr gl_context_;
  ion::gfx::GraphicsManagerPtr fake_gm_;

 public:
  bool SetToSeventeen(int* arg) {
    ScopedTracer scope(GetTraceRecorder(), "SetToSeventeen");
    *arg = 17;
    return true;
  }

  struct ThreadStruct {
    explicit ThreadStruct(int count) : end_semaphore(0), count(count) {}
    port::Semaphore end_semaphore;
    int count;
  };

  bool ThreadFunction(ThreadStruct* threadStruct) {
    for (int i = 0; i < threadStruct->count; ++i) {
      ScopedTracer scope(GetTraceRecorder(), "For loop scope");
    }
    threadStruct->end_semaphore.Post();
    return true;
  }

  bool TimeRangeFunction(ThreadStruct* threadStruct) {
    for (int i = 0; i < threadStruct->count; ++i) {
      std::string name = "Thread for loop " + base::ValueToString(i);
      call_trace_manager_->AdvanceTimer(2000U);
      GetTraceRecorder()->EnterTimeRange(i, name.c_str(), nullptr);
      call_trace_manager_->AdvanceTimer(6000U);
      GetTraceRecorder()->LeaveTimeRange(i);
    }
    threadStruct->end_semaphore.Post();
    return true;
  }

  bool TimeStampFunction(ThreadStruct* threadStruct) {
    uint32 base_timestamp = 0U;
    for (int i = 0; i < threadStruct->count; ++i) {
      std::string name = "Thread timeStamp " + base::ValueToString(i);
      GetTraceRecorder()->CreateTimeStampAtTime(
          base_timestamp + static_cast<uint32>(i) * 2000U, name.c_str(),
          nullptr);
    }
    threadStruct->end_semaphore.Post();
    return true;
  }
};

class FakeTimelineMetric : public TimelineMetric {
 public:
  FakeTimelineMetric(const std::string& name, double value)
      : name_(name), value_(value) {}
  void Run(const Timeline& timeline,
           analytics::Benchmark* benchmark) const override {
    benchmark->AddConstant(analytics::Benchmark::Constant(
        analytics::Benchmark::Descriptor(name_, "FakeTimelineMetric",
                                         "A fake metric", ""),
        value_));
  }

 private:
  std::string name_;
  double value_;
};

class RunCountTimelineMetric : public TimelineMetric {
 public:
  explicit RunCountTimelineMetric(std::atomic<int>* run_count)
      : run_count_(run_count) {}
  void Run(const Timeline& timeline,
           analytics::Benchmark* benchmark) const override {
    ++(*run_count_);
  }

 private:
  std::atomic<int>* const run_count_;
};

static bool EventLoop(CallTraceManagerWithMockTimer* call_trace_manager,
                      const std::string& thread_name,
                      const std::vector<const char*>& event_names) {
  call_trace_manager->GetTraceRecorder()->SetThreadName(thread_name);
  for (const char* event_name : event_names) {
    ScopedTracer scope(call_trace_manager->GetTraceRecorder(), event_name);
    call_trace_manager->AdvanceTimer(10U);
  }
  return true;
}

static void CheckTimelineThread(
    const Timeline& timeline, std::thread::id thread_id,
    const std::string& thread_name,
    const std::vector<const char*>& expected_event_names) {
  TimelineSearch search(timeline, thread_id);
  const auto iter = search.begin();
  EXPECT_NE(search.end(), iter);
  const TimelineNode* thread = *iter;
  EXPECT_EQ(TimelineNode::Type::kThread, thread->GetType());
  EXPECT_EQ(thread_name, thread->GetName());
  EXPECT_EQ(expected_event_names.size(), thread->GetChildren().size());

  for (size_t i = 0; i < expected_event_names.size(); ++i) {
    const TimelineNode* child = thread->GetChild(i);
    EXPECT_EQ(TimelineNode::Type::kScope, child->GetType());
    EXPECT_EQ(expected_event_names[i], child->GetName());
  }
}

TEST_F(CallTraceTest, DefaultBufferSize) {
  size_t initial_default = TraceRecorder::GetDefaultBufferSize();
  EXPECT_NE(555U, initial_default);
  TraceRecorder::SetDefaultBufferSize(555);
  EXPECT_EQ(555U, TraceRecorder::GetDefaultBufferSize());
  TraceRecorder::SetDefaultBufferSize(initial_default);
  EXPECT_EQ(initial_default, TraceRecorder::GetDefaultBufferSize());
}

TEST_F(CallTraceTest, ReserveBuffer) {
  bool initial_reserve = TraceRecorder::GetReserveBuffer();
  EXPECT_EQ(false, initial_reserve);
  TraceRecorder::SetReserveBuffer(true);
  EXPECT_EQ(true, TraceRecorder::GetReserveBuffer());
  TraceRecorder::SetReserveBuffer(false);
  EXPECT_EQ(false, TraceRecorder::GetReserveBuffer());
}

TEST_F(CallTraceTest, BasicRecord) {
  {
    // This is the first scope.
    ScopedTracer scope(GetTraceRecorder(), "First scope");
  }
  {
    // This is the second scope.
    ScopedTracer scope(GetTraceRecorder(), "Second scope");
  }
  EXPECT_EQ(2U, GetNumScopeEvents());
  EXPECT_EQ(4U, GetTraceRecorder()->GetNumTraces());
}

TEST_F(CallTraceTest, BasicGpuRecordEnabled) {
  EXPECT_TRUE(AllowGpuTracing());
  EnableGpuTracing();
  {
    // This is the first scope.
    ScopedGlTracer scope(GetGpuProfiler(), "First scope");
  }
  {
    // This is the second scope.
    ScopedGlTracer scope(GetGpuProfiler(), "Second scope");
  }
  EXPECT_EQ(2U, GetNumScopeEvents());
  EXPECT_EQ(0U, GetTraceRecorder()->GetNumTraces());
  EXPECT_EQ(0U, GetGpuTraceRecorder()->GetNumTraces());
  PollGlTimerQueries();
  EXPECT_EQ(2U, GetNumScopeEvents());
  EXPECT_EQ(0U, GetTraceRecorder()->GetNumTraces());
  EXPECT_EQ(4U, GetGpuTraceRecorder()->GetNumTraces());
}

TEST_F(CallTraceTest, BasicGpuRecordDisallowed) {
  EnableGpuTracing();
  { ScopedGlTracer scope(GetGpuProfiler(), "test"); }
  PollGlTimerQueries();
  EXPECT_EQ(0U, GetGpuTraceRecorder()->GetNumTraces());
}

TEST_F(CallTraceTest, BasicGpuRecordDisabled) {
  EXPECT_TRUE(AllowGpuTracing());
  { ScopedGlTracer scope(GetGpuProfiler(), "test"); }
  PollGlTimerQueries();
  EXPECT_EQ(0U, GetGpuTraceRecorder()->GetNumTraces());
}

#if !defined(ION_PLATFORM_ASMJS)  // ASMJS does not support threads.

// Test if a single thread records in its own event buffer.
TEST_F(CallTraceTest, ThreadRecord) {
  int x = 3;
  {
    ScopedTracer scope(GetTraceRecorder(), "ThreadRecord Test Outer");

    // Wait for the thread to finish.
    port::ThreadStdFunc func = std::bind(&CallTraceTest::SetToSeventeen,
                                         this, &x);
    base::ThreadSpawner spawner("thread_test_set_to_17", func);
  }

  EXPECT_EQ(x, 17);
  EXPECT_EQ(2, static_cast<int>(GetAllTraceRecorders().size()));
  EXPECT_EQ(2U, GetNumScopeEvents());
}

// Test that multiple threads record in their own event buffers.
TEST_F(CallTraceTest, MultipleThreads) {
  ThreadStruct thread_struct_1(10);
  ThreadStruct thread_struct_2(20);

  // Let threads run and complete.
  {
    port::ThreadStdFunc func1 = std::bind(
        &CallTraceTest::ThreadFunction, this, &thread_struct_1);
    port::ThreadStdFunc func2 = std::bind(
        &CallTraceTest::ThreadFunction, this, &thread_struct_2);
    base::ThreadSpawner thread1("Thread 1", func1);
    base::ThreadSpawner thread2("Thread 2", func2);
  }
  thread_struct_2.end_semaphore.Wait();
  thread_struct_1.end_semaphore.Wait();

  // We know thread is done, inspect recorded event counts.
  EXPECT_EQ(2, static_cast<int>(GetAllTraceRecorders().size()));
  EXPECT_EQ(1U, GetNumScopeEvents());

  const base::AllocVector<TraceRecorder*>& recorders = GetAllTraceRecorders();
  EXPECT_NE(recorders[0]->GetNumTraces(), recorders[1]->GetNumTraces());
  EXPECT_TRUE(
      (2U * thread_struct_1.count == recorders[0]->GetNumTraces() &&
      2U * thread_struct_2.count == recorders[1]->GetNumTraces()) ||
      (2U * thread_struct_1.count == recorders[1]->GetNumTraces() &&
      2U * thread_struct_2.count == recorders[0]->GetNumTraces()));
}

#endif  // !defined(ION_PLATFORM_ASMJS)

TEST_F(CallTraceTest, BasicTiming) {
  {
    // This is the first scope.
    ScopedTracer scope(GetTraceRecorder(), "First scope");
    call_trace_manager_->AdvanceTimer(8000U);
  }
  EXPECT_EQ(2U, GetTraceRecorder()->GetNumTraces());

  std::string output = call_trace_manager_->SnapshotCallTraces();
  TraceReader reader(output);
  const std::vector<Event>& eb = reader.GetMainEventBuffer();

  // Note: the first two events are zone create and set events.
  const Event& one = eb[2];
  const Event& two = eb[3];
  EXPECT_EQ("First scope", one.name);
  EXPECT_EQ("wtf.scope#leave", two.name);
  EXPECT_EQ(8000U, two.time_value - one.time_value);
}

TEST_F(CallTraceTest, BasicOutput) {
  const uint32 kNumIterations = 10;
  for (uint32 i = 0; i < kNumIterations; ++i) {
    call_trace_manager_->AdvanceTimer(2000U);
    ScopedTracer scope(GetTraceRecorder(), "First scope");
    call_trace_manager_->AdvanceTimer(6000U);
  }
  // Add an opened scope event. TraceRecorder::DumpTrace should close it in the
  // dumped trace when CallTraceManager::SnapshotCallTraces is called.
  GetTraceRecorder()->EnterScope(
      GetTraceRecorder()->GetScopeEvent("First scope"));
  // One enter and one exit event for each iteration, plus the additional enter
  // event at the end.
  EXPECT_EQ(kNumIterations*2+1, GetTraceRecorder()->GetNumTraces());
  // One scope.
  EXPECT_EQ(1U, GetNumScopeEvents());

  std::string output = call_trace_manager_->SnapshotCallTraces();
  TraceReader reader(output);

  // Query the events in the parser.
  const std::vector<Event>& eb = reader.GetMainEventBuffer();
  // We should have the following events in the buffer:
  // - 2 events to define the zone.
  // - 2 events per iteration for the scope.
  // - 2 events for the additional opened scope at the end. (One for entering
  //   the scope, and the second added by TracerRecorder::DumpTrace to close
  //   it.)
  const uint32 event_buffer_size = static_cast<uint32>(eb.size());
  EXPECT_EQ(2 + kNumIterations * 2 + 2, event_buffer_size);

  EXPECT_EQ("wtf.zone#create", eb[0].name);
  EXPECT_EQ(1U, eb[0].GetGenericArg("zoneId"));

  EXPECT_EQ("wtf.zone#set", eb[1].name);
  EXPECT_EQ(1U, eb[1].GetGenericArg("zoneId"));

  for (uint32 i = 0; i < kNumIterations; ++i) {
    const Event& one = eb[2 + 2 * i + 0];
    const Event& two = eb[2 + 2 * i + 1];
    EXPECT_EQ("First scope", one.name);
    EXPECT_EQ("wtf.scope#leave", two.name);
    EXPECT_EQ(6000U, two.time_value - one.time_value);

    if (i > 0) {
      const Event& prev = eb[2 + 2 * i - 1];
      EXPECT_EQ(2000U, one.time_value - prev.time_value);
    }
  }

  EXPECT_EQ("First scope", eb[event_buffer_size-2].name);
  EXPECT_EQ("wtf.scope#leave", eb[event_buffer_size-1].name);
}

TEST_F(CallTraceTest, AnnotatedOutput) {
  const uint32 kNumIterations = 10;
  for (uint32 i = 0; i < kNumIterations; ++i) {
    std::ostringstream count;
    count << i;
    ScopedTracer scope(GetTraceRecorder(), "Loop scope");
    GetTraceRecorder()->AnnotateCurrentScope("Iter", count.str().c_str());
  }
  // Enter, leave, and appendData events per iteration.
  EXPECT_EQ(kNumIterations*3, GetTraceRecorder()->GetNumTraces());
  EXPECT_EQ(1U, GetNumScopeEvents());

  std::string output = call_trace_manager_->SnapshotCallTraces();
  TraceReader reader(output);

  // Query the events in the parser.
  const std::vector<Event>& eb = reader.GetMainEventBuffer();

  bool in_scope = false;
  uint32 itercount = 0;
  for (uint32 i = 0; i < kNumIterations; ++i) {
    const Event& e = eb[i];
    if (e.name == "Loop scope") {
      in_scope = true;
    } else if (e.name == "wtf.scope#appendData") {
      EXPECT_TRUE(in_scope);
      std::ostringstream count;
      count << itercount;
      EXPECT_EQ(e.GetAsciiArg("name"), "Iter");
      EXPECT_EQ(e.GetAsciiArg("value"), count.str());
    } else if (e.name == "wtf.scope#leave") {
      ++itercount;
      in_scope = false;
    }
  }
}

TEST_F(CallTraceTest, JsonSafeAnnotations) {
  const std::vector<std::string> expected_json_strings{
    "123",
    "-123",
    "123",
    "true",
    "false",
    "\"normal_string\"",
    "\"\\\"\\\\\\b\\f\\n\\r\\t\"",
    "1e+9999",
    "-1e+9999",
    "null",
#if defined(JSON_HAS_INT64)
    "-733007751850",
    "733007751850",
#endif
  };

  {
    ScopedTracer scope(GetTraceRecorder(), "Scope");
    GetTraceRecorder()->AnnotateCurrentScope("Value", "123");
    GetTraceRecorder()->AnnotateCurrentScopeWithJsonSafeValue("Value", -123);
    GetTraceRecorder()->AnnotateCurrentScopeWithJsonSafeValue("Value", 123U);
    GetTraceRecorder()->AnnotateCurrentScopeWithJsonSafeValue("Value", true);
    GetTraceRecorder()->AnnotateCurrentScopeWithJsonSafeValue("Value", false);
    GetTraceRecorder()->AnnotateCurrentScopeWithJsonSafeValue(
        "Value", "normal_string");
    GetTraceRecorder()->AnnotateCurrentScopeWithJsonSafeValue(
        "Value", "\"\\\b\f\n\r\t");
    GetTraceRecorder()->AnnotateCurrentScopeWithJsonSafeValue(
        "Value", std::numeric_limits<double>::infinity());
    GetTraceRecorder()->AnnotateCurrentScopeWithJsonSafeValue(
        "Value", -std::numeric_limits<double>::infinity());
    GetTraceRecorder()->AnnotateCurrentScopeWithJsonSafeValue(
        "Value", std::numeric_limits<double>::quiet_NaN());
#if defined(JSON_HAS_INT64)
    GetTraceRecorder()->AnnotateCurrentScopeWithJsonSafeValue(
        "Value", -733007751850LL);
    GetTraceRecorder()->AnnotateCurrentScopeWithJsonSafeValue(
        "Value", 733007751850ULL);
#endif
  }
  // Enter, leave, and appendData events per iteration.
  EXPECT_EQ(2U + expected_json_strings.size(),
            GetTraceRecorder()->GetNumTraces());
  EXPECT_EQ(1U, GetNumScopeEvents());

  std::string output = call_trace_manager_->SnapshotCallTraces();
  TraceReader reader(output);

  // Query the events in the parser.
  const std::vector<Event>& eb = reader.GetMainEventBuffer();

  EXPECT_EQ("wtf.zone#create", eb[0].name);
  EXPECT_EQ("wtf.zone#set", eb[1].name);
  EXPECT_EQ("Scope", eb[2].name);
  // Test if all annotations match expected JSON strings.
  for (uint32 i = 0; i < expected_json_strings.size(); ++i) {
    const Event& e = eb[i + 3];
    EXPECT_EQ("wtf.scope#appendData", e.name);
    EXPECT_EQ("Value", e.GetAsciiArg("name"));
    EXPECT_EQ(expected_json_strings[i], e.GetAsciiArg("value"));
  }
  EXPECT_EQ("wtf.scope#leave", eb[expected_json_strings.size()+3].name);
}

TEST_F(CallTraceTest, BasicOutputWithFrames) {
  const int kNumIterations = 10;
  for (int i = 0; i < kNumIterations; ++i) {
    {
      ScopedFrameTracer frame(GetTraceRecorder(), i);
      {
        // Test recursive frame scope. This should do nothing (i.e., produce no
        // events to the result trace.)
        ScopedFrameTracer frame(GetTraceRecorder(), 999);

        call_trace_manager_->AdvanceTimer(2000U);
        ScopedTracer scope(GetTraceRecorder(), "First scope");
        call_trace_manager_->AdvanceTimer(6000U);
      }
      // Check if it is inside of frame scope, and the frame number is correct.
      EXPECT_EQ(true, GetTraceRecorder()->IsInFrameScope());
      EXPECT_EQ(static_cast<uint32>(i),
                GetTraceRecorder()->GetCurrentFrameNumber());
    }
    // Check if it is outside of frame scope.
    EXPECT_EQ(false, GetTraceRecorder()->IsInFrameScope());
  }
  // Add an opened frame event. TraceRecorder::DumpTrace should close it in the
  // dumped trace when CallTraceManager::SnapshotCallTraces is called.
  GetTraceRecorder()->EnterFrame(kNumIterations);
  EXPECT_EQ(41U, GetTraceRecorder()->GetNumTraces());
  EXPECT_EQ(1U, GetNumScopeEvents());

  std::string output = call_trace_manager_->SnapshotCallTraces();
  TraceReader reader(output);

  // Query the events in the parser.
  const std::vector<Event>& eb = reader.GetMainEventBuffer();
  // We should have the following events in the buffer:
  // - 2 events to define the zone.
  // - 4 events per iteration for the frame and scope.
  // - 2 events for the additional opened frame at the end. (One for entering
  //   the frame, and the second added by TracerRecorder::DumpTrace to close
  //   it.)
  const int event_buffer_size = static_cast<int>(eb.size());
  EXPECT_EQ(2 + kNumIterations * 4 + 2, event_buffer_size);

  EXPECT_EQ("wtf.zone#create", eb[0].name);
  EXPECT_EQ(1U, eb[0].GetGenericArg("zoneId"));

  EXPECT_EQ("wtf.zone#set", eb[1].name);
  EXPECT_EQ(1U, eb[1].GetGenericArg("zoneId"));

  for (int i = 0; i < kNumIterations; ++i) {
    const Event& one = eb[2 + 4 * i + 0];
    const Event& two = eb[2 + 4 * i + 1];
    const Event& three = eb[2 + 4 * i + 2];
    const Event& four = eb[2 + 4 * i + 3];
    EXPECT_EQ("wtf.timing#frameStart", one.name);
    EXPECT_EQ("First scope", two.name);
    EXPECT_EQ("wtf.scope#leave", three.name);
    EXPECT_EQ("wtf.timing#frameEnd", four.name);
    EXPECT_EQ(6000U, three.time_value - two.time_value);

    if (i > 0) {
      const Event& prev = eb[2 + 4 * i - 2];
      EXPECT_EQ(2000U, two.time_value - prev.time_value);
    }
  }

  EXPECT_EQ("wtf.timing#frameStart", eb[event_buffer_size-2].name);
  EXPECT_EQ("wtf.timing#frameEnd", eb[event_buffer_size-1].name);
}

// Test if more LeaveFrame is called than EnterFrame. In such case, the
// excessive LeaveFrame should do nothing.
TEST_F(CallTraceTest, UnbalancedFrames) {
  TraceRecorder* trace_recorder = GetTraceRecorder();
  trace_recorder->EnterFrame(10U);
  EXPECT_EQ(true, trace_recorder->IsInFrameScope());
  EXPECT_EQ(10U, trace_recorder->GetCurrentFrameNumber());

  trace_recorder->EnterFrame(11U);
  EXPECT_EQ(true, trace_recorder->IsInFrameScope());
  // Recursive call does not change frame number.
  EXPECT_EQ(10U, trace_recorder->GetCurrentFrameNumber());

  trace_recorder->LeaveFrame();
  EXPECT_EQ(true, trace_recorder->IsInFrameScope());
  EXPECT_EQ(10U, trace_recorder->GetCurrentFrameNumber());

  trace_recorder->LeaveFrame();
  EXPECT_EQ(false, trace_recorder->IsInFrameScope());

  // Excessive call to LeaveFrame has no effect.
  trace_recorder->LeaveFrame();
  EXPECT_EQ(false, trace_recorder->IsInFrameScope());

  trace_recorder->EnterFrame(11U);
  EXPECT_EQ(true, trace_recorder->IsInFrameScope());
  EXPECT_EQ(11U, trace_recorder->GetCurrentFrameNumber());

  trace_recorder->LeaveFrame();
  EXPECT_EQ(false, trace_recorder->IsInFrameScope());
}

#if !defined(ION_PLATFORM_ASMJS)  // ASMJS does not support threads.

TEST_F(CallTraceTest, BasicOutputWithThreads) {
  const int kNumIterations = 10;
  const int kThreadIterations = 7;
  for (int i = 0; i < kNumIterations; ++i) {
    call_trace_manager_->AdvanceTimer(2000U);
    ScopedTracer scope(GetTraceRecorder(), "First scope");
    call_trace_manager_->AdvanceTimer(6000U);
  }
  EXPECT_EQ(20U, GetTraceRecorder()->GetNumTraces());
  EXPECT_EQ(1U, GetNumScopeEvents());

  ThreadStruct thread_struct_1(kThreadIterations);
  ThreadStruct thread_struct_2(kThreadIterations);

  // Let threads run and complete.
  {
    port::ThreadStdFunc func1 = std::bind(
        &CallTraceTest::ThreadFunction, this, &thread_struct_1);
    port::ThreadStdFunc func2 = std::bind(
        &CallTraceTest::ThreadFunction, this, &thread_struct_2);
    base::ThreadSpawner thread1("Thread 1", func1);
    base::ThreadSpawner thread2("Thread 2", func2);
  }
  thread_struct_2.end_semaphore.Wait();
  thread_struct_1.end_semaphore.Wait();

  std::string output = call_trace_manager_->SnapshotCallTraces();
  TraceReader reader(output);

  // Query the events in the parser.
  const std::vector<Event>& eb = reader.GetMainEventBuffer();
  EXPECT_EQ((kNumIterations + kThreadIterations + kThreadIterations) * 2 +
            2 * 3, static_cast<int>(eb.size()));

  // Check if we got three zones, and if the zone ids are different and the
  // zone names are different.
  EXPECT_EQ("wtf.zone#create", eb[0].name);
  EXPECT_EQ("wtf.zone#create", eb[1].name);
  EXPECT_EQ("wtf.zone#create", eb[2].name);
  EXPECT_EQ(1U, eb[0].GetGenericArg("zoneId"));
  EXPECT_EQ(2U, eb[1].GetGenericArg("zoneId"));
  EXPECT_EQ(3U, eb[2].GetGenericArg("zoneId"));

  EXPECT_NE(eb[0].GetAsciiArg("name"), eb[1].GetAsciiArg("name"));
  EXPECT_NE(eb[0].GetAsciiArg("name"), eb[2].GetAsciiArg("name"));
  EXPECT_NE(eb[1].GetAsciiArg("name"), eb[2].GetAsciiArg("name"));

  size_t event_i = 3;
  EXPECT_EQ("wtf.zone#set", eb[event_i].name);
  EXPECT_EQ(1U, eb[event_i].GetGenericArg("zoneId"));
  event_i++;

  for (int i = 0; i < kNumIterations; ++i) {
    const Event& one = eb[event_i];
    const Event& two = eb[event_i + 1];
    EXPECT_EQ("First scope", one.name);
    EXPECT_EQ("wtf.scope#leave", two.name);
    EXPECT_EQ(6000U, two.time_value - one.time_value);

    if (i > 0) {
      const Event& prev = eb[event_i - 1];
      EXPECT_EQ(2000U, one.time_value - prev.time_value);
    }
    event_i += 2;
  }

  EXPECT_EQ("wtf.zone#set", eb[event_i].name);
  EXPECT_EQ(2U, eb[event_i].GetGenericArg("zoneId"));
  event_i++;

  for (int i = 0; i < kThreadIterations; ++i) {
    const Event& one = eb[event_i];
    const Event& two = eb[event_i + 1];
    EXPECT_EQ("For loop scope", one.name);
    EXPECT_EQ("wtf.scope#leave", two.name);
    EXPECT_LE(one.time_value, two.time_value);

    if (i > 0) {
      const Event& prev = eb[event_i - 1];
      EXPECT_LE(prev.time_value, one.time_value);
    }
    event_i += 2;
  }

  EXPECT_EQ("wtf.zone#set", eb[event_i].name);
  EXPECT_EQ(3U, eb[event_i].GetGenericArg("zoneId"));
  event_i++;

  for (int i = 0; i < kThreadIterations; ++i) {
    const Event& one = eb[event_i];
    const Event& two = eb[event_i + 1];
    EXPECT_EQ("For loop scope", one.name);
    EXPECT_EQ("wtf.scope#leave", two.name);
    EXPECT_LE(one.time_value, two.time_value);

    if (i > 0) {
      const Event& prev = eb[event_i - 1];
      EXPECT_LE(prev.time_value, one.time_value);
    }
    event_i += 2;
  }

  EXPECT_EQ(event_i, eb.size());
}

#endif  // !defined(ION_PLATFORM_ASMJS)

TEST_F(CallTraceTest, BasicTimeRanges) {
  const int kNumIterations = 10;

  for (int i = 0; i < kNumIterations; ++i) {
    std::string name =
        std::string("For loop range ") + base::ValueToString(i);
    call_trace_manager_->AdvanceTimer(2000U);
    GetTraceRecorder()->EnterTimeRange(i, name.c_str(), nullptr);
    call_trace_manager_->AdvanceTimer(6000U);
    GetTraceRecorder()->LeaveTimeRange(i);
  }

  Json::FastWriter json_writer;
  for (int i = 0; i < kNumIterations; ++i) {
    Json::Value json;
    json["index"] = i;
    call_trace_manager_->AdvanceTimer(2000U);
    GetTraceRecorder()->EnterTimeRange(
        kNumIterations + i, "For loop range", json_writer.write(json).c_str());
    call_trace_manager_->AdvanceTimer(6000U);
    GetTraceRecorder()->LeaveTimeRange(kNumIterations + i);
  }

  // Add an opened time range event. TraceRecorder::DumpTrace should close it in
  // the dumped trace when CallTraceManager::SnapshotCallTraces is called.
  GetTraceRecorder()->EnterTimeRange(
      kNumIterations * 2, "Dangling event", nullptr);

  EXPECT_EQ(0U, GetNumScopeEvents());
  EXPECT_EQ(4U * kNumIterations + 1U, GetTraceRecorder()->GetNumTraces());

  std::string output = call_trace_manager_->SnapshotCallTraces();
  TraceReader reader(output);

  const std::vector<Event>& eb = reader.GetMainEventBuffer();
  // We should have the following events in the buffer:
  // - 2 events to define the zone.
  // - 2 events per iteration for the time range events, and 2 * kNumIterations
  //   iterations are run.
  // - 2 events for the additional opened time range event at the end. (One for
  //   opening the event, and the second added by TracerRecorder::DumpTrace to
  //   close it.)
  EXPECT_EQ(2 + kNumIterations * 4 + 2, static_cast<int>(eb.size()));

  EXPECT_EQ("wtf.zone#create", eb[0].name);
  EXPECT_EQ(1U, eb[0].GetGenericArg("zoneId"));

  EXPECT_EQ("wtf.zone#set", eb[1].name);
  EXPECT_EQ(1U, eb[1].GetGenericArg("zoneId"));

  int event_i = 2;
  for (int i = 0; i < kNumIterations; ++i) {
    std::string name = "For loop range " + base::ValueToString(i);

    const Event& one = eb[event_i];
    const Event& two = eb[event_i + 1];
    EXPECT_EQ("wtf.timeRange#begin", one.name);
    EXPECT_EQ(i, static_cast<int>(one.GetGenericArg("id")));
    EXPECT_EQ(name, one.GetAsciiArg("name"));
    EXPECT_EQ("", one.GetAsciiArg("value"));
    EXPECT_EQ("wtf.timeRange#end", two.name);
    EXPECT_EQ(6000U, two.time_value - one.time_value);

    if (i > 0) {
      const Event& prev = eb[event_i - 1];
      EXPECT_EQ(2000U, one.time_value - prev.time_value);
    }
    event_i += 2;
  }

  for (int i = 0; i < kNumIterations; ++i) {
    std::string name = "For loop range";
    std::string value = "{\"index\":" + base::ValueToString(i) + "}";

    const Event& one = eb[event_i];
    const Event& two = eb[event_i + 1];
    EXPECT_EQ("wtf.timeRange#begin", one.name);
    EXPECT_EQ(kNumIterations + i, static_cast<int>(one.GetGenericArg("id")));
    EXPECT_EQ(name, one.GetAsciiArg("name"));
    EXPECT_EQ(value, one.GetAsciiArg("value"));
    EXPECT_EQ("wtf.timeRange#end", two.name);
    EXPECT_EQ(6000U, two.time_value - one.time_value);

    if (i > 0) {
      const Event& prev = eb[event_i - 1];
      EXPECT_EQ(2000U, one.time_value - prev.time_value);
    }
    event_i += 2;
  }

  EXPECT_EQ("wtf.timeRange#begin", eb[event_i].name);
  EXPECT_EQ("wtf.timeRange#end", eb[event_i+1].name);
}

#if !defined(ION_PLATFORM_ASMJS)  // ASMJS does not support threads.

TEST_F(CallTraceTest, ThreadedTimeRanges) {
  const int kNumIterations = 10;

  ThreadStruct thread_struct_1(kNumIterations);
  ThreadStruct thread_struct_2(kNumIterations);

  // Let threads run and complete.
  {
    port::ThreadStdFunc func1 = std::bind(
        &CallTraceTest::TimeRangeFunction, this, &thread_struct_1);
    port::ThreadStdFunc func2 = std::bind(
        &CallTraceTest::TimeRangeFunction, this, &thread_struct_2);
    base::ThreadSpawner thread1("Thread 1", func1);
    base::ThreadSpawner thread2("Thread 2", func2);
  }
  thread_struct_2.end_semaphore.Wait();
  thread_struct_1.end_semaphore.Wait();

  std::string output = call_trace_manager_->SnapshotCallTraces();
  TraceReader reader(output);

  const std::vector<Event>& eb = reader.GetMainEventBuffer();
  EXPECT_EQ(kNumIterations * 4 + 4, static_cast<int>(eb.size()));

  EXPECT_EQ("wtf.zone#create", eb[0].name);
  EXPECT_EQ(1U, eb[0].GetGenericArg("zoneId"));
  EXPECT_EQ("wtf.zone#create", eb[1].name);
  EXPECT_EQ(2U, eb[1].GetGenericArg("zoneId"));

  int event_i = 2;
  for (uint32 thread_i = 0; thread_i < 2; ++thread_i) {
    EXPECT_EQ("wtf.zone#set", eb[event_i].name);
    EXPECT_EQ(thread_i + 1, eb[event_i].GetGenericArg("zoneId"));
    event_i++;

    for (int i = 0; i < kNumIterations; ++i) {
      std::string name = "Thread for loop " + base::ValueToString(i);

      const Event& one = eb[event_i];
      const Event& two = eb[event_i + 1];
      EXPECT_EQ("wtf.timeRange#begin", one.name);
      EXPECT_EQ(i, static_cast<int>(one.GetGenericArg("id")));
      EXPECT_EQ(name, one.GetAsciiArg("name"));
      EXPECT_EQ("", one.GetAsciiArg("value"));
      EXPECT_EQ("wtf.timeRange#end", two.name);
      EXPECT_LE(5000U, two.time_value - one.time_value);

      if (i > 0) {
        const Event& prev = eb[event_i - 1];
        EXPECT_LE(1000U, one.time_value - prev.time_value);
      }
      event_i += 2;
    }
  }
}

#endif  // !defined(ION_PLATFORM_ASMJS)

#if !defined(ION_PLATFORM_IOS)  // This test crashes on ios-x86 for some reason.
// 
TEST_F(CallTraceTest, BasicTimeRangesByName) {
  const int kNumIterations = 10;

  for (int i = 0; i < kNumIterations; ++i) {
    std::string name =
        std::string("For loop range ") + base::ValueToString(i);
    call_trace_manager_->AdvanceTimer(2000U);
    uint32 id = GetTraceRecorder()->EnterTimeRange(name.c_str(), nullptr);
    call_trace_manager_->AdvanceTimer(6000U);
    GetTraceRecorder()->LeaveTimeRange(id);
  }

  EXPECT_EQ(0U, GetNumScopeEvents());
  EXPECT_EQ(2U * kNumIterations, GetTraceRecorder()->GetNumTraces());

  std::string output = call_trace_manager_->SnapshotCallTraces();
  TraceReader reader(output);

  const std::vector<Event>& eb = reader.GetMainEventBuffer();
  EXPECT_EQ(kNumIterations * 2 + 2, static_cast<int>(eb.size()));

  EXPECT_EQ("wtf.zone#create", eb[0].name);
  EXPECT_EQ(1U, eb[0].GetGenericArg("zoneId"));

  EXPECT_EQ("wtf.zone#set", eb[1].name);
  EXPECT_EQ(1U, eb[1].GetGenericArg("zoneId"));

  int event_i = 2;
  for (int i = 0; i < kNumIterations; ++i) {
    std::string name = "For loop range " + base::ValueToString(i);

    const Event& one = eb[event_i];
    const Event& two = eb[event_i + 1];
    EXPECT_EQ("wtf.timeRange#begin", one.name);
    EXPECT_EQ(i, static_cast<int>(one.GetGenericArg("id")));
    EXPECT_EQ(name, one.GetAsciiArg("name"));
    EXPECT_EQ("", one.GetAsciiArg("value"));
    EXPECT_EQ("wtf.timeRange#end", two.name);
    EXPECT_EQ(6000U, two.time_value - one.time_value);

    if (i > 0) {
      const Event& prev = eb[event_i - 1];
      EXPECT_EQ(2000U, one.time_value - prev.time_value);
    }
    event_i += 2;
  }
}

#endif  // !defined(ION_PLATFORM_IOS)

TEST_F(CallTraceTest, BasicTimeStamps) {
  const int kNumIterations = 10;

  // Test timeStamps without JSON.
  for (int i = 0; i < kNumIterations; ++i) {
    std::string name =
        std::string("TimeStamp ") + base::ValueToString(i);
    GetTraceRecorder()->CreateTimeStamp(name.c_str(), nullptr);
    call_trace_manager_->AdvanceTimer(2000U);
  }

  // Test timeStamps with JSON.
  Json::FastWriter json_writer;
  for (int i = 0; i < kNumIterations; ++i) {
    Json::Value json;
    json["index"] = kNumIterations + i;
    GetTraceRecorder()->CreateTimeStamp(
        "TimeStamp", json_writer.write(json).c_str());
    call_trace_manager_->AdvanceTimer(2000U);
  }

  // Test timeStamps with specified timestamps.
  uint32 base_timestamp = call_trace_manager_->GetTimeInUs();
  for (int i = 0; i < kNumIterations; ++i) {
    std::string name =
        std::string("TimeStamp ") +
        base::ValueToString(2 * kNumIterations + i);
    GetTraceRecorder()->CreateTimeStampAtTime(
        base_timestamp + static_cast<uint32>(i) * 2000U, name.c_str(), nullptr);
  }

  // Zero because TraceRecorder::GetScopeEvent() is not called.
  EXPECT_EQ(0U, GetNumScopeEvents());
  EXPECT_EQ(3U * kNumIterations, GetTraceRecorder()->GetNumTraces());

  std::string output = call_trace_manager_->SnapshotCallTraces();
  TraceReader reader(output);

  const std::vector<Event>& eb = reader.GetMainEventBuffer();
  // Plus two because zone create and set events.
  EXPECT_EQ(kNumIterations * 3 + 2, static_cast<int>(eb.size()));

  EXPECT_EQ("wtf.zone#create", eb[0].name);
  EXPECT_EQ(1U, eb[0].GetGenericArg("zoneId"));

  EXPECT_EQ("wtf.zone#set", eb[1].name);
  EXPECT_EQ(1U, eb[1].GetGenericArg("zoneId"));

  int event_i = 2;
  uint32 expect_timestamp = 0U;
  for (int i = 0; i < kNumIterations; ++i) {
    std::string name = "TimeStamp " + base::ValueToString(i);

    const Event& event = eb[event_i];
    EXPECT_EQ("wtf.trace#timeStamp", event.name);
    EXPECT_EQ(name, event.GetAsciiArg("name"));
    EXPECT_EQ("", event.GetAsciiArg("value"));
    EXPECT_EQ(expect_timestamp, event.time_value);

    ++event_i;
    expect_timestamp += 2000U;
  }

  for (int i = 0; i < kNumIterations; ++i) {
    std::string name = "TimeStamp";
    std::string value = "{\"index\":" +
        base::ValueToString(kNumIterations + i) + "}";

    const Event& event = eb[event_i];
    EXPECT_EQ("wtf.trace#timeStamp", event.name);
    EXPECT_EQ(name, event.GetAsciiArg("name"));
    EXPECT_EQ(value, event.GetAsciiArg("value"));
    EXPECT_EQ(expect_timestamp, event.time_value);

    ++event_i;
    expect_timestamp += 2000U;
  }

  for (int i = 0; i < kNumIterations; ++i) {
    std::string name =
        "TimeStamp " + base::ValueToString(2 * kNumIterations + i);

    const Event& event = eb[event_i];
    EXPECT_EQ("wtf.trace#timeStamp", event.name);
    EXPECT_EQ(name, event.GetAsciiArg("name"));
    EXPECT_EQ("", event.GetAsciiArg("value"));
    EXPECT_EQ(expect_timestamp, event.time_value);

    ++event_i;
    expect_timestamp += 2000U;
  }
}

#if !defined(ION_PLATFORM_ASMJS)  // ASMJS does not support threads.

TEST_F(CallTraceTest, ThreadedTimeStamps) {
  const int kNumIterations = 10;

  ThreadStruct thread_struct_1(kNumIterations);
  ThreadStruct thread_struct_2(kNumIterations);

  // Let threads run and complete.
  {
    port::ThreadStdFunc func1 = std::bind(
        &CallTraceTest::TimeStampFunction, this, &thread_struct_1);
    port::ThreadStdFunc func2 = std::bind(
        &CallTraceTest::TimeStampFunction, this, &thread_struct_2);
    base::ThreadSpawner thread1("Thread 1", func1);
    base::ThreadSpawner thread2("Thread 2", func2);
  }
  thread_struct_2.end_semaphore.Wait();
  thread_struct_1.end_semaphore.Wait();

  std::string output = call_trace_manager_->SnapshotCallTraces();
  TraceReader reader(output);

  const std::vector<Event>& eb = reader.GetMainEventBuffer();
  // We have 2 threads, kNumIterations in each thread, and one zone create and
  // one zone set event for each thread.
  EXPECT_EQ(2 * (kNumIterations + 2), static_cast<int>(eb.size()));

  EXPECT_EQ("wtf.zone#create", eb[0].name);
  EXPECT_EQ(1U, eb[0].GetGenericArg("zoneId"));
  EXPECT_EQ("wtf.zone#create", eb[1].name);
  EXPECT_EQ(2U, eb[1].GetGenericArg("zoneId"));

  int event_i = 2;
  for (uint32 thread_i = 0; thread_i < 2; ++thread_i) {
    EXPECT_EQ("wtf.zone#set", eb[event_i].name);
    EXPECT_EQ(thread_i + 1, eb[event_i].GetGenericArg("zoneId"));
    event_i++;

    for (int i = 0; i < kNumIterations; ++i) {
      std::string name = "Thread timeStamp " + base::ValueToString(i);

      const Event& event = eb[event_i];
      EXPECT_EQ("wtf.trace#timeStamp", event.name);
      EXPECT_EQ(name, event.GetAsciiArg("name"));
      EXPECT_EQ("", event.GetAsciiArg("value"));
      EXPECT_EQ(static_cast<uint32>(i) * 2000U, event.time_value);

      ++event_i;
    }
  }
}

#endif  // !defined(ION_PLATFORM_ASMJS)

TEST(CallTraceTesting, ClearTraceRecorder) {
  // Each outermost scope enter/leave event pair takes 20 bytes (with the
  // kEmptyScopeMarker), so 80 + 4 bytes is enough for 4 pairs + the initial
  // kEmptyScopeMarker.
  CallTraceManager manager(20 * 4 + 4);

  // Add some events.
  for (int i = 0; i < 3; ++i) {
    ScopedTracer scope(manager.GetTraceRecorder(), "First scope");
  }
  { ScopedTracer scope(manager.GetTraceRecorder(), "Second scope"); }

  EXPECT_EQ(2U, manager.GetScopeEventTable()->GetSize());
  EXPECT_EQ(8U, manager.GetTraceRecorder()->GetNumTraces());

  // Clear the event buffer.  This does not clear the reference map of
  // previously seen scope events.
  manager.GetTraceRecorder()->Clear();

  EXPECT_EQ(2U, manager.GetScopeEventTable()->GetSize());
  EXPECT_EQ(0U, manager.GetTraceRecorder()->GetNumTraces());

  // Add more events.
  for (int i = 0; i < 3; ++i) {
    ScopedTracer scope(manager.GetTraceRecorder(), "First scope");
  }
  { ScopedTracer scope(manager.GetTraceRecorder(), "Second scope"); }

  EXPECT_EQ(2U, manager.GetScopeEventTable()->GetSize());
  EXPECT_EQ(8U, manager.GetTraceRecorder()->GetNumTraces());
}

TEST(CallTraceTesting, RingBufferNotFilled) {
  // Each outermost scope enter/leave event pair takes 20 bytes (with the
  // kEmptyScopeMarker), so 60 + 4 bytes is enough for 3 pairs + the initial
  // kEmptyScopeMarker.
  CallTraceManager manager(20 * 4 + 4);

  for (int i = 0; i < 3; ++i) {
    ScopedTracer scope(manager.GetTraceRecorder(), "First scope");
  }

  EXPECT_EQ(1U, manager.GetScopeEventTable()->GetSize());
  EXPECT_EQ(6U, manager.GetTraceRecorder()->GetNumTraces());
}

TEST(CallTraceTesting, RingBufferFilled) {
  // Each outermost scope enter/leave event pair takes 20 bytes (with the
  // kEmptyScopeMarker), so 100 + 4 bytes is enough for 5 pairs (== 10
  // enter/leave events) + the initial kEmptyScopeMarker.
  CallTraceManager manager(20 * 5 + 4);
  TraceRecorder *tr = manager.GetTraceRecorder();

  for (int i = 0; i < 7; ++i) {
    ScopedTracer scope(tr, "First scope");
  }

  EXPECT_EQ(1U, manager.GetScopeEventTable()->GetSize());
  EXPECT_EQ(10U, tr->GetNumTraces());

  std::string output = manager.SnapshotCallTraces();
  TraceReader reader(output);
  reader.Parse();
}

TEST(CallTraceTesting, RingBufferFilledNested) {
  // Each outermost scope enter/leave event pair takes 20 bytes (with the
  // kEmptyScopeMarker), so 60 + 4 bytes is enough for 3 pairs + the initial
  // kEmptyScopeMarker.
  CallTraceManager manager(20 * 3 + 4);
  TraceRecorder *tr = manager.GetTraceRecorder();

  {
    ScopedTracer scope(tr, "Scope_A");
    {
      ScopedTracer scope(tr, "Scope_B");
      { ScopedTracer scope(tr, "Scope_C"); }
    }
  }
  { ScopedTracer scope(tr, "Scope_A"); }
  { ScopedTracer scope(tr, "Scope_A"); }
  { ScopedTracer scope(tr, "Scope_A"); }

  EXPECT_EQ(3U, manager.GetScopeEventTable()->GetSize());
  EXPECT_EQ(6U, tr->GetNumTraces());

  std::string output = manager.SnapshotCallTraces();
  TraceReader reader(output);
  reader.Parse();
}

#if !defined(ION_PLATFORM_NACL) && !defined(ION_PLATFORM_IOS)
// 
TEST_F(CallTraceTest, WriteFile) {
  const int kNumIterations = 10U;

  for (int i = 0; i < kNumIterations; ++i) {
    std::string name =
        std::string("For loop range ") + ion::base::ValueToString(i);
    call_trace_manager_->AdvanceTimer(2000U);
    GetTraceRecorder()->EnterTimeRange(i, name.c_str(), nullptr);
    call_trace_manager_->AdvanceTimer(6000U);
    GetTraceRecorder()->LeaveTimeRange(i);
  }

  EXPECT_EQ(0U, GetNumScopeEvents());
  EXPECT_EQ(2 * kNumIterations,
            static_cast<int>(GetTraceRecorder()->GetNumTraces()));

  // Exercise actual file output.
  const std::string output_file = port::GetTemporaryFilename();
  call_trace_manager_->WriteFile(output_file);

  // Read actual file output.
  std::ifstream in(output_file, std::ios::in | std::ios::binary);
  ASSERT_TRUE(in);
  std::string output;
  in.seekg(0, std::ios::end);
  output.resize(static_cast<size_t>(in.tellg()));
  in.seekg(0, std::ios::beg);
  in.read(&output[0], output.size());
  in.close();

  port::RemoveFile(output_file);

  // Verify actual file output.
  TraceReader reader(output);

  const std::vector<Event>& eb = reader.GetMainEventBuffer();
  EXPECT_EQ(kNumIterations * 2U + 2U, eb.size());

  EXPECT_EQ("wtf.zone#create", eb[0].name);
  EXPECT_EQ(1U, eb[0].GetGenericArg("zoneId"));

  EXPECT_EQ("wtf.zone#set", eb[1].name);
  EXPECT_EQ(1U, eb[1].GetGenericArg("zoneId"));

  size_t event_i = 2U;
  for (int i = 0; i < kNumIterations; ++i) {
    std::string name = "For loop range " + ion::base::ValueToString(i);

    const Event& one = eb[event_i];
    const Event& two = eb[event_i + 1];
    EXPECT_EQ("wtf.timeRange#begin", one.name);
    EXPECT_EQ(i, static_cast<int>(one.GetGenericArg("id")));
    EXPECT_EQ(name, one.GetAsciiArg("name"));
    EXPECT_EQ("", one.GetAsciiArg("value"));
    EXPECT_EQ("wtf.timeRange#end", two.name);
    EXPECT_EQ(6000U, two.time_value - one.time_value);

    if (i > 0) {
      const Event& prev = eb[event_i - 1];
      EXPECT_EQ(2000U, one.time_value - prev.time_value);
    }
    event_i += 2U;
  }
}

#endif  // !defined(ION_PLATFORM_NACL) && !defined(ION_PLATFORM_IOS)

TEST_F(CallTraceTest, RunTimelineMetrics) {
  call_trace_manager_->RegisterTimelineMetric(
      std::unique_ptr<TimelineMetric>(new FakeTimelineMetric("metric_a", 1.0)));
  call_trace_manager_->RegisterTimelineMetric(
      std::unique_ptr<TimelineMetric>(new FakeTimelineMetric("metric_b", 2.0)));
  analytics::Benchmark benchmark = call_trace_manager_->RunTimelineMetrics();
  const std::vector<analytics::Benchmark::Constant>& constants =
      benchmark.GetConstants();
  EXPECT_EQ(2U, constants.size());
  EXPECT_EQ("metric_a", constants[0].descriptor.id);
  EXPECT_EQ(1.0, constants[0].value);
  EXPECT_EQ("metric_b", constants[1].descriptor.id);
  EXPECT_EQ(2.0, constants[1].value);
}

TEST_F(CallTraceTest, RemoveAllTimelineMeetrics) {
  // Add two TimelineMetric instances that count in |counter_0|, and run them.
  std::atomic<int> counter_0(0);
  std::atomic<int> counter_1(0);
  call_trace_manager_->RegisterTimelineMetric(
      std::unique_ptr<TimelineMetric>(new RunCountTimelineMetric(&counter_0)));
  call_trace_manager_->RegisterTimelineMetric(
      std::unique_ptr<TimelineMetric>(new RunCountTimelineMetric(&counter_0)));

  call_trace_manager_->RunTimelineMetrics();
  EXPECT_EQ(2, counter_0.load());
  EXPECT_EQ(0, counter_1.load());

  // Remove all TimelineMetric instances from |call_trace_manager_|.  Running
  // the metrics now should not result in a change to the counters.
  call_trace_manager_->RemoveAllTimelineMetrics();
  call_trace_manager_->RunTimelineMetrics();
  EXPECT_EQ(2, counter_0.load());
  EXPECT_EQ(0, counter_1.load());

  // Add two more TimelineMetrics to the |call_trace_manager_| and run them.
  // These two instances now count in |counter_1|.
  call_trace_manager_->RegisterTimelineMetric(
      std::unique_ptr<TimelineMetric>(new RunCountTimelineMetric(&counter_1)));
  call_trace_manager_->RegisterTimelineMetric(
      std::unique_ptr<TimelineMetric>(new RunCountTimelineMetric(&counter_1)));
  call_trace_manager_->RunTimelineMetrics();
  EXPECT_EQ(2, counter_0.load());
  EXPECT_EQ(2, counter_1.load());

  // Clear the TimelineMetrics instances again to avoid a crash (since they
  // hold pointers to |counter_1|.
  call_trace_manager_->RemoveAllTimelineMetrics();
}

TEST_F(CallTraceTest, TimelineEmpty) {
  Timeline timeline = call_trace_manager_->BuildTimeline();
  auto event = timeline.begin();
  EXPECT_EQ(event, timeline.end());
}

TEST_F(CallTraceTest, TimelineMixedEvents) {
  // Create nested scoped events:
  // 0         1         2         3
  // 01234567890123456789012345678
  // [            R0             ]
  //  [ Frame#0X [S4] [ Frame#1 ]
  //   [S1] [S2]      [S5]  [S6]
  // The 'X' denotes the event S3, which starts and ends at the same time as
  // Frame#0 ends.
  // All events will be children of the TimelineThread "thread_0".
  static const int num_events = 10;
  GetTraceRecorder()->SetThreadName("MainThread");
  static const char* names[num_events] = {
      "MainThread", "R0", "Frame_0", "S1", "S2",
      "S3",         "S4", "Frame_1", "S5", "S6"};
  static const uint32 begins[num_events] = {0U,  0U,  1U,  2U,  7U,
                                            10U, 12U, 17U, 17U, 23U};
  static const uint32 durations[num_events] = {
      std::numeric_limits<uint32>::max(), 28U, 9U, 3U, 3U, 0U, 3U, 10U, 3U, 3U};

  {
    uint32 time_range_id = GetTraceRecorder()->EnterTimeRange(
        "R0", "{ \"arg_0\": \"A\", \"arg_1\": 17 }");
    call_trace_manager_->AdvanceTimer(1U);
    {
      ScopedFrameTracer frame(GetTraceRecorder(), 0U);
      call_trace_manager_->AdvanceTimer(1U);
      {
        ScopedTracer scope(GetTraceRecorder(), "S1");
        call_trace_manager_->AdvanceTimer(3U);
      }
      call_trace_manager_->AdvanceTimer(2U);
      {
        ScopedTracer scope(GetTraceRecorder(), "S2");
        call_trace_manager_->AdvanceTimer(3U);
      }
    }
    // Annotation with the same timestamp, but logged after the end of Frame#0
    // and before S3. Expected to be associated with R0.
    GetTraceRecorder()->AnnotateCurrentScope("annotation_A", "\"A\"");
    { ScopedTracer scope(GetTraceRecorder(), "S3"); }
    call_trace_manager_->AdvanceTimer(2U);
    {
      ScopedTracer scope(GetTraceRecorder(), "S4");
      // Annotation with same timestamp, but logged after the beginning of S4.
      // Expected to be associated with S4.
      GetTraceRecorder()->AnnotateCurrentScope("annotation_B", "\"B\"");
      call_trace_manager_->AdvanceTimer(3U);
      // Annotation with same timestamp, but logged before the end of S4.
      // Expected to be associated with S4.
      GetTraceRecorder()->AnnotateCurrentScope("annotation_C", "\"C\"");
    }
    call_trace_manager_->AdvanceTimer(1U);
    GetTraceRecorder()->AnnotateCurrentScope("annotation_D", "18");
    call_trace_manager_->AdvanceTimer(1U);
    {
      ScopedFrameTracer frame(GetTraceRecorder(), 1U);
      {
        ScopedTracer scope(GetTraceRecorder(), "S5");
        call_trace_manager_->AdvanceTimer(3U);
      }
      call_trace_manager_->AdvanceTimer(3U);
      {
        ScopedTracer scope(GetTraceRecorder(), "S6");
        call_trace_manager_->AdvanceTimer(3U);
      }
      call_trace_manager_->AdvanceTimer(1U);
    }
    call_trace_manager_->AdvanceTimer(1U);
    GetTraceRecorder()->LeaveTimeRange(time_range_id);
  }

  Timeline timeline = call_trace_manager_->BuildTimeline();

  // Test tree structure.
  const TimelineNode* root = timeline.GetRoot();
  EXPECT_EQ(TimelineNode::Type::kNode, root->GetType());
  EXPECT_EQ("root", root->GetName());
  EXPECT_EQ(1U, root->GetChildren().size());
  const TimelineThread* thread =
      static_cast<const TimelineThread*>(root->GetChild(0));
  EXPECT_EQ(TimelineNode::Type::kThread, thread->GetType());
  EXPECT_EQ(1U, thread->GetChildren().size());
  const TimelineEvent* r0 =
      static_cast<const TimelineEvent*>(thread->GetChild(0));
  EXPECT_EQ(TimelineNode::Type::kRange, r0->GetType());
  EXPECT_EQ("R0", r0->GetName());
  EXPECT_EQ(4U, r0->GetChildren().size());
  Json::Value meta_data_r0 = Json::objectValue;
  meta_data_r0["arg_0"] = "A";
  meta_data_r0["arg_1"] = 17;
  meta_data_r0["annotation_A"] = "A";
  meta_data_r0["annotation_D"] = 18;
  EXPECT_EQ(meta_data_r0.toStyledString(), r0->GetArgs().toStyledString());
  const TimelineFrame* f0 = static_cast<const TimelineFrame*>(r0->GetChild(0));
  EXPECT_EQ(TimelineNode::Type::kFrame, f0->GetType());
  EXPECT_EQ("Frame_0", f0->GetName());
  EXPECT_EQ(0U, f0->GetFrameNumber());
  EXPECT_EQ(2U, f0->GetChildren().size());
  const TimelineNode* s1 = f0->GetChild(0);
  EXPECT_EQ(TimelineNode::Type::kScope, s1->GetType());
  EXPECT_EQ("S1", s1->GetName());
  EXPECT_EQ(0U, s1->GetChildren().size());
  const TimelineNode* s2 = f0->GetChild(1);
  EXPECT_EQ(TimelineNode::Type::kScope, s2->GetType());
  EXPECT_EQ("S2", s2->GetName());
  EXPECT_EQ(0U, s2->GetChildren().size());
  const TimelineNode* s3 = r0->GetChild(1);
  EXPECT_EQ(TimelineNode::Type::kScope, s3->GetType());
  EXPECT_EQ("S3", s3->GetName());
  EXPECT_EQ(0U, s3->GetChildren().size());
  const TimelineEvent* s4 = static_cast<const TimelineEvent*>(r0->GetChild(2));
  EXPECT_EQ(TimelineNode::Type::kScope, s4->GetType());
  EXPECT_EQ("S4", s4->GetName());
  EXPECT_EQ(0U, s4->GetChildren().size());
  Json::Value annotation_s4 = Json::objectValue;
  annotation_s4["annotation_B"] = "B";
  annotation_s4["annotation_C"] = "C";
  EXPECT_EQ(annotation_s4.toStyledString(), s4->GetArgs().toStyledString());
  const TimelineFrame* f1 = static_cast<const TimelineFrame*>(r0->GetChild(3));
  EXPECT_EQ(TimelineNode::Type::kFrame, f1->GetType());
  EXPECT_EQ("Frame_1", f1->GetName());
  EXPECT_EQ(1U, f1->GetFrameNumber());
  EXPECT_EQ(2U, f1->GetChildren().size());
  const TimelineNode* s5 = f1->GetChild(0);
  EXPECT_EQ(TimelineNode::Type::kScope, s5->GetType());
  EXPECT_EQ("S5", s5->GetName());
  EXPECT_EQ(0U, s5->GetChildren().size());
  const TimelineNode* s6 = f1->GetChild(1);
  EXPECT_EQ(TimelineNode::Type::kScope, s6->GetType());
  EXPECT_EQ("S6", s6->GetName());
  EXPECT_EQ(0U, s6->GetChildren().size());

  // Test iteration order.
  int index = 0;
  for (auto node : timeline) {
    EXPECT_EQ(names[index], node->GetName());
    EXPECT_EQ(begins[index], node->GetBegin());
    EXPECT_EQ(durations[index], node->GetDuration());
    ++index;
  }
}

#if !defined(ION_PLATFORM_ASMJS)
TEST_F(CallTraceTest, TimelineMultiThreaded) {
  std::vector<const char*> event_names_0 = {"A", "B", "C"};
  std::vector<const char*> event_names_1 = {"D", "E", "F"};
  std::vector<const char*> event_names_2 = {"G", "H", "I"};
  std::thread t0(EventLoop, this->call_trace_manager_.get(), "thread_0",
                 event_names_0);
  std::thread t1(EventLoop, this->call_trace_manager_.get(), "thread_1",
                 event_names_1);
  std::thread t2(EventLoop, this->call_trace_manager_.get(), "thread_2",
                 event_names_2);
  std::thread::id thread_id_0 = t0.get_id();
  std::thread::id thread_id_1 = t1.get_id();
  std::thread::id thread_id_2 = t2.get_id();
  t0.join();
  t1.join();
  t2.join();

  Timeline timeline = call_trace_manager_->BuildTimeline();

  const TimelineNode* root = timeline.GetRoot();
  EXPECT_EQ(TimelineNode::Type::kNode, root->GetType());
  EXPECT_EQ("root", root->GetName());
  EXPECT_EQ(3U, root->GetChildren().size());

  CheckTimelineThread(timeline, thread_id_0, "thread_0", event_names_0);
  CheckTimelineThread(timeline, thread_id_1, "thread_1", event_names_1);
  CheckTimelineThread(timeline, thread_id_2, "thread_2", event_names_2);
}
#endif

TEST_F(CallTraceTest, VSyncProfilerTest) {
  VSyncProfiler vsync_profiler(call_trace_manager_.get());

  // Record |kNumVSyncEvents| VSync events 10ms apart.
  static const uint32 kNumVSyncEvents = 10;
  uint32 current_timestamp = 0U;
  for (uint32 i = 0; i < kNumVSyncEvents; ++i) {
    vsync_profiler.RecordVSyncEvent(current_timestamp, i);
    current_timestamp += 10000U;
  }

  // Verify the result trace.
  std::string output = call_trace_manager_->SnapshotCallTraces();
  TraceReader reader(output);

  // Query the events in the parser.
  const std::vector<Event>& eb = reader.GetMainEventBuffer();
  // Plus two for zone create and set events.
  EXPECT_EQ(kNumVSyncEvents + 2, eb.size());

  EXPECT_EQ("wtf.zone#create", eb[0].name);
  EXPECT_EQ(1U, eb[0].GetGenericArg("zoneId"));

  EXPECT_EQ("wtf.zone#set", eb[1].name);
  EXPECT_EQ(1U, eb[1].GetGenericArg("zoneId"));

  for (uint32 i = 0; i < kNumVSyncEvents; ++i) {
    const Event& event = eb[i + 2];
    const std::string name = "VSync" + base::ValueToString(i);
    EXPECT_EQ("wtf.trace#timeStamp", event.name);
    EXPECT_EQ(name.c_str(), event.GetAsciiArg("name"));
    EXPECT_EQ(i * 10000U, event.time_value);
  }
}

// This test benchmarks the TraceRecorder and CallTraceManager trace recording
// and snapshot capabilities.  It is listed as a DISABLED_ test so it does not
// run by default; this is because it is designed to take about a minute to run,
// to gather timing data.
TEST_F(CallTraceTest, DISABLED_SnapshotBenchmark) {
  // The number of EnterFrame()/LeaveFrame() "frames" per run of the
  // TraceRecorder.
  const int kNumFramesPerTrace = 1024;
  // The number of times the TraceRecorder is run per benchmark run.
  const int kNumTraceIterations = 100;
  // The number of times the CallTraecManager snapshots the TraceRecorder, per
  // benchmark run.
  const int kNumSnapshotIterations = 1000;

  int frames = 0;
  int scopes = 0;
  int annotations = 0;

  port::Timer timer;
  for (int iteration = 0; iteration < kNumTraceIterations; ++iteration) {
    GetTraceRecorder()->Clear();
    for (int i = 0; i < kNumFramesPerTrace; ++i) {
      GetTraceRecorder()->EnterFrame(i);
      ++frames;
      {
        ScopedTracer scope(GetTraceRecorder(), "S0:0");
        ++scopes;
        GetTraceRecorder()->AnnotateCurrentScope("i", std::to_string(i));
        ++annotations;
      }
      {
        ScopedTracer scope(GetTraceRecorder(), "S1:0");
        ++scopes;
        for (int j = 0; j < 4; ++j) {
          ScopedTracer scope(GetTraceRecorder(), "S1:0:0");
          ++scopes;
          for (int k = 0; k < 3; ++k) {
            ScopedTracer scope(GetTraceRecorder(), "S1:0:0:0");
            ++scopes;
            for (int l = 0; l < 2; ++l) {
              ScopedTracer scope(GetTraceRecorder(), "S1:0:0:0:0");
              ++scopes;
              GetTraceRecorder()->AnnotateCurrentScope("i", std::to_string(i));
              GetTraceRecorder()->AnnotateCurrentScope("j", std::to_string(j));
              GetTraceRecorder()->AnnotateCurrentScope("k", std::to_string(k));
              GetTraceRecorder()->AnnotateCurrentScope("l", std::to_string(l));
              annotations += 4;
            }
          }
        }
        for (int j = 0; j < 2; ++j) {
          ScopedTracer scope(GetTraceRecorder(), "S1:1");
          ++scopes;
          GetTraceRecorder()->AnnotateCurrentScope("i", std::to_string(i));
          GetTraceRecorder()->AnnotateCurrentScope("j", std::to_string(j));
          annotations += 2;
        }
      }
      {
        ScopedTracer scope(GetTraceRecorder(), "S2:0");
        ++scopes;
      }
      GetTraceRecorder()->LeaveFrame();
    }
  }
  const port::Timer::Clock::duration build_duration = timer.Get();

  LOG(INFO) << "frames=" << frames << ", scopes=" << scopes
            << ", annotations=" << annotations;

  timer.Reset();
  for (int iteration = 0; iteration < kNumSnapshotIterations; ++iteration) {
    call_trace_manager_->SnapshotCallTraces();
  }
  const port::Timer::Clock::duration snapshot_duration = timer.Get();

  const int total_events = frames + scopes + annotations;
  const int per_frame_events =
      total_events * kNumSnapshotIterations / kNumTraceIterations;
  LOG(INFO) << "build_duration="
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   build_duration)
                   .count()
            << " ms, snapshot_duration="
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   snapshot_duration)
                   .count()
            << " ms";
  LOG(INFO)
      << "build_event="
      << std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(
             build_duration)
                 .count() /
             total_events
      << " ns, snapshot_event="
      << std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(
             snapshot_duration)
                 .count() /
             per_frame_events
      << " ns";
}

}  // namespace profile
}  // namespace ion
