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

#include "ion/port/memory.h"

#if defined(ION_PLATFORM_LINUX) || defined(ION_PLATFORM_ANDROID) || \
    defined(ION_GOOGLE_INTERNAL)
#include <fstream>  // NOLINT(readability/streams)
#include <sstream>
#endif

#if defined(ION_PLATFORM_MAC) || defined(ION_PLATFORM_IOS)
#include <mach/mach.h>
#include <sys/errno.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#endif

#if defined(ION_PLATFORM_WINDOWS)
#include <windows.h>
// psapi must be included after windows.h.
#include <psapi.h>  // NOLINT(build/include_alpha)
#pragma comment(lib, "psapi")
#endif

#include <assert.h>  // For checking return values since port has no logging.

namespace {

#if defined(ION_PLATFORM_LINUX) || defined(ION_PLATFORM_ANDROID) || \
    defined(ION_GOOGLE_INTERNAL)
// Return value for a particular key from a procfs file.
static uint64 GetProcFSValue(const std::string& filename,
                             const std::string& key) {
  const std::string colon_key = key + std::string(":");
  std::ifstream procfs_file(filename);
  assert(procfs_file.is_open());
  std::string line;
  uint64 value = 0U;
  while (std::getline(procfs_file, line)) {
    std::stringstream line_stream(line);
    std::string line_key, line_units;
    uint64 line_value;
    if (line_stream >> line_key && line_key == colon_key &&
        line_stream >> line_value &&
        line_stream >> line_units && line_units == "kB") {
      value = line_value;
      break;
    }
  }
  assert(value);
  static const int kKilobyte = 1024;
  value *= kKilobyte;
  return value;
}
#endif

}  // namespace

namespace ion {
namespace port {

uint64 GetProcessResidentMemorySize() {
#if defined(ION_PLATFORM_LINUX) || defined(ION_PLATFORM_ANDROID) || \
    defined(ION_GOOGLE_INTERNAL)
  return GetProcFSValue("/proc/self/status", "VmRSS");
#elif defined(ION_PLATFORM_MAC) || defined(ION_PLATFORM_IOS)
  struct task_basic_info info;
  mach_msg_type_number_t info_count = TASK_BASIC_INFO_COUNT;
  const int error_code = task_info(mach_task_self(), TASK_BASIC_INFO,
                                   (task_info_t)&info, &info_count);
  (void)error_code;  // Silence unused variable warning.
  assert(error_code == KERN_SUCCESS);
  return info.resident_size;
#elif defined(ION_PLATFORM_WINDOWS)
  PROCESS_MEMORY_COUNTERS pmc;
  GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
  return pmc.WorkingSetSize;
#else
  return 0U;
#endif
}

uint64 GetSystemMemorySize() {
#if defined(ION_PLATFORM_LINUX) || defined(ION_PLATFORM_ANDROID) || \
    defined(ION_GOOGLE_INTERNAL)
  return GetProcFSValue("/proc/meminfo", "MemTotal");
#elif defined(ION_PLATFORM_MAC) || defined(ION_PLATFORM_IOS)
  int mib[2];
  mib[0] = CTL_HW;
  mib[1] = HW_MEMSIZE;
  int64 system_memory_size;
  size_t size = sizeof(system_memory_size);
  const int error_code = sysctl(mib, 2, &system_memory_size, &size, nullptr, 0);
  (void)error_code;  // Silence unused variable warning.
  assert(error_code != ENOMEM);
  return system_memory_size;
#elif defined(ION_PLATFORM_WINDOWS)
  MEMORYSTATUSEX memory_info;
  memory_info.dwLength = sizeof(MEMORYSTATUSEX);
  GlobalMemoryStatusEx(&memory_info);
  return memory_info.ullTotalPhys;
#else
  return 0U;
#endif
}

}  // namespace port
}  // namespace ion
