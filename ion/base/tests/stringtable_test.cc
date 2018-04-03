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

#include "ion/base/stringtable.h"

#include <algorithm>
#include <atomic>
#include <functional>
#include <list>
#include <mutex>  // NOLINT(build/c++11)
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "ion/base/logging.h"
#include "ion/base/threadspawner.h"
#include "ion/port/barrier.h"
#include "ion/port/timer.h"
#include "testing/base/public/gunit.h"

namespace ion {
namespace base {
namespace {

std::vector<std::string> Sort(std::vector<std::string> strings) {
  std::sort(strings.begin(), strings.end());
  return strings;
}

std::vector<std::string> CreateRandomStrings(
    int seed, int count, const std::vector<std::string>& mixin_strings) {
  const int kStringLength = 32;

  std::mt19937 gen(seed);
  std::uniform_int_distribution<int> dist(' ', '~');
  std::vector<std::string> strings;
  for (int i = 0; i < count; ++i) {
    std::string string(kStringLength, '\0');
    for (int j = 0; j < kStringLength; ++j) {
      string[j] = static_cast<char>(dist(gen));
    }
    strings.emplace_back(std::move(string));
  }
  strings.insert(strings.end(), mixin_strings.begin(), mixin_strings.end());
  std::shuffle(strings.begin(), strings.end(), gen);
  return strings;
}

TEST(StringTableTest, TestConstruction) {
  // Test default construction.
  StringTablePtr s1(new StringTable());
  EXPECT_EQ(0U, s1->GetSize());
  EXPECT_EQ(Sort({}), Sort(s1->GetTable()));
  {
    const StringTable::StringIndex index = s1->FindIndex("test", 4);
    EXPECT_EQ(index, s1->FindIndex("test"));
    EXPECT_EQ(std::string("test"), s1->GetString(index));
  }
  EXPECT_EQ(1U, s1->GetSize());
  EXPECT_EQ(Sort({"test"}), Sort(s1->GetTable()));
}

// Test the use of the StringTable::View interface.
TEST(StringTableTest, TestView) {
  StringTablePtr s1(new StringTable());
  const StringTable::StringIndex test_id = s1->FindIndex("test", 4);
  EXPECT_EQ(1U, s1->GetSize());
  EXPECT_EQ(Sort({"test"}), Sort(s1->GetTable()));

  const base::SharedPtr<StringTable::View> v1 = s1->CreateView(2);
  EXPECT_EQ(test_id, v1->FindIndex("test"));
  const StringTable::StringIndex test2_id = s1->FindIndex("test2");
  EXPECT_EQ(test2_id, v1->FindIndex("test2"));

  const StringTable::StringIndex test3_id = v1->FindIndex("test3");
  EXPECT_EQ(test3_id, s1->FindIndex("test3"));

  EXPECT_EQ(Sort({"test", "test2", "test3"}), Sort(s1->GetTable()));

  EXPECT_EQ(std::string("test"), v1->GetString(test_id));
  EXPECT_EQ(std::string("test2"), v1->GetString(test2_id));
  EXPECT_EQ(std::string("test3"), v1->GetString(test3_id));
}

// Test the use of StringTable::View in a multithreaded context.
TEST(StringTableTest, TestViewMultithreaded) {
  const int kStringCount = 128;
  const int kThreadCount = 8;

  port::Barrier barrier(kThreadCount + 1);
  std::vector<std::string> common_strings =
      CreateRandomStrings(0, kStringCount / 2, {});

  // Each thread gets half unique strings and half common strings.
  std::vector<std::vector<std::string>> thread_strings;
  for (int i = 0; i < kThreadCount; ++i) {
    thread_strings.emplace_back(
        CreateRandomStrings(i, kStringCount / 2, common_strings));
  }

  std::vector<std::pair<std::string, StringTable::StringIndex>> all_strings;
  StringTablePtr table(new StringTable());

  // The thread function.
  const auto kThreadFunc = [&](std::vector<std::string> strings) -> bool {
    barrier.Wait();
    const StringTable::ViewPtr view = table->CreateView(16);

    // Insert all the strings for this thread through the View.
    for (const std::string& string : strings) {
      view->FindIndex(string.c_str());
    }
    barrier.Wait();
    barrier.Wait();

    // Verify the retrieval of all strings through the View.
    for (const auto& entry : all_strings) {
      EXPECT_EQ(entry.first, view->GetString(entry.second));
      EXPECT_EQ(entry.second, view->FindIndex(entry.first.c_str()));
    }
    return true;
  };

  // Run the threads.
  std::list<ThreadSpawner> threads;
  for (int i = 0; i < kThreadCount; ++i) {
    threads.emplace_back(std::string("thread_") + std::to_string(i),
                         std::bind(kThreadFunc, thread_strings[i]));
  }
  barrier.Wait();
  barrier.Wait();

  // Retrieve all the strings and store to |all_strings|, for verification on
  // each thread.
  std::vector<std::string> strings = table->GetTable();
  for (int i = 0; i < static_cast<int>(strings.size()); ++i) {
    all_strings.emplace_back(strings[i], i);
  }
  barrier.Wait();
}

// Benchmarks concurrent string lookups using:
// * std::unordered_map
// * StringTable
// * StringTable::View
template <typename T>
bool BenchmarkThread(T table, const std::vector<std::string>& strings,
                     int iteration_count, int seed, port::Barrier* barrier,
                     std::atomic<port::Timer::Clock::duration::rep>* time) {
  std::mt19937 gen(seed);
  std::uniform_int_distribution<size_t> rand(0, strings.size() - 1);
  barrier->Wait();
  port::Timer timer;
  for (int i = 0; i < iteration_count; ++i) {
    table->FindIndex(strings[rand(gen)].c_str());
  }
  *time += timer.Get().count();
  return true;
}

template <typename T>
void RunBenchmark(const std::vector<T>& tables,
                  const std::vector<std::vector<std::string>>& strings,
                  int iteration_count, const char* print_name) {
  std::list<ThreadSpawner> threads;
  port::Barrier barrier(static_cast<uint32>(tables.size()) + 1);
  std::atomic<port::Timer::Clock::duration::rep> time(0);
  for (int i = 0; i < static_cast<int>(tables.size()); ++i) {
    threads.emplace_back(std::string("thread_") + std::to_string(i),
                         std::bind(&BenchmarkThread<T>, tables[i], strings[i],
                                   iteration_count, i, &barrier, &time));
  }
  barrier.Wait();
  threads.clear();
  LOG(INFO)
      << " " << print_name << ": "
      << std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(
             port::Timer::Clock::duration(time))
                 .count() /
             (static_cast<int>(tables.size()) * iteration_count)
      << " ns/iteration";
}

class UnorderedMapWrapper {
 public:
  StringTable::StringIndex FindIndex(const char* string) {
    std::lock_guard<std::mutex> lock(mutex_);
    return map_[string];
  }

 private:
  std::unordered_map<std::string, StringTable::StringIndex> map_;
  std::mutex mutex_;
};

TEST(StringTableTest, DISABLED_BenchmarkMultithreaded) {
  const int kThreadCount = 8;
  const int kIterationCount = 1024 * 1024;
  const int kStringCount = 4096;

  std::vector<std::string> common_strings =
      CreateRandomStrings(0, kStringCount / 2, {});

  // Each thread gets half unique strings and half common strings.
  std::vector<std::vector<std::string>> thread_strings;
  for (int i = 0; i < kThreadCount; ++i) {
    thread_strings.emplace_back(
        CreateRandomStrings(i + 1, kStringCount / 2, common_strings));
  }

  LOG(INFO) << "kThreadCount=" << kThreadCount
            << ", kIterationCount=" << kIterationCount
            << ", kStringCount=" << kStringCount;

  // Test using unordered_map.
  {
    UnorderedMapWrapper table;
    std::vector<UnorderedMapWrapper*> tables(kThreadCount, &table);
    RunBenchmark(tables, thread_strings, kIterationCount, "unordered_map");
  }

  // Test using StringTable.
  {
    StringTablePtr table(new StringTable());
    std::vector<StringTablePtr> tables(kThreadCount, table);
    RunBenchmark(tables, thread_strings, kIterationCount, "StringTable");
  }

  // Test using StringTable::View.
  {
    StringTablePtr table(new StringTable());
    std::vector<StringTable::ViewPtr> tables;
    for (int i = 0; i < kThreadCount; ++i) {
      tables.push_back(table->CreateView(256));
    }
    RunBenchmark(tables, thread_strings, kIterationCount, "StringTable::View");
  }
}

}  // namespace
}  // namespace base
}  // namespace ion
