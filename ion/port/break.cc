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

#include "ion/port/break.h"

#if defined(ION_PLATFORM_LINUX) || defined(ION_PLATFORM_ANDROID) || \
    defined(ION_GOOGLE_INTERNAL)
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#elif defined(ION_PLATFORM_MAC) || defined(ION_PLATFORM_IOS)
#include <assert.h>
#include <signal.h>  // NOLINT
#include <stdbool.h>
#include <sys/sysctl.h>
#include <sys/types.h>  // NOLINT
#include <unistd.h>  // NOLINT
#elif defined(ION_PLATFORM_WINDOWS)
#include <intrin.h>
#include <windows.h>
#endif

#include <cstdlib>

namespace ion {
namespace port {

#if defined(ION_PLATFORM_LINUX) || defined(ION_PLATFORM_ANDROID) || \
    defined(ION_GOOGLE_INTERNAL)

bool IsDebuggerAttached() {
  // If current process is being ptrace()d, 'TracerPid' in /proc/self/status
  // will be non-zero. Limit stack usage.
  // Currently, TracerPid is at max offset 76
  // (depending on length of argv[0]) into /proc/self/status.
  char buf[100];
  int fd = open("/proc/self/status", O_RDONLY);
  if (fd == -1) {
    return false;  // Can't tell for sure.
  }
  const ssize_t len = read(fd, buf, sizeof(buf));
  bool rc = false;
  if (len > 0) {
    const char *const kTracerPid = "TracerPid:\t";
    buf[len - 1] = '\0';
    const char *p = strstr(buf, kTracerPid);
    if (p != nullptr) {
      rc = (strncmp(p + strlen(kTracerPid), "0\n", 2) != 0);
    }
  }
  close(fd);
  return rc;
}

#elif defined(ION_PLATFORM_MAC) || defined(ION_PLATFORM_IOS)

bool IsDebuggerAttached() {
  // Mostly copied from chromium/base/debug/debugger_posix.cc.
  //
  // Returns true if the current process is being debugged (either
  // running under the debugger or has a debugger attached post facto).
  // Taken from: http://developer.apple.com/mac/library/qa/qa2004/qa1361.html
  int junk;
  int mib[4];
  struct kinfo_proc info;
  size_t size;

  // Initialize the flags so that, if sysctl fails for some bizarre
  // reason, we get a predictable result.
  info.kp_proc.p_flag = 0;

  // Initialize mib, which tells sysctl the info we want, in this case
  // we're looking for information about a specific process ID.
  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_PID;
  mib[3] = getpid();

  // Call sysctl.
  size = sizeof(info);
  junk = sysctl(mib, static_cast<u_int>(sizeof(mib) / sizeof(*mib)), &info,
                &size, nullptr, 0);
  assert(junk == 0);

  // We're being debugged if the P_TRACED flag is set.
  return (info.kp_proc.p_flag & P_TRACED) != 0;
}

#elif defined(ION_PLATFORM_WINDOWS)

bool IsDebuggerAttached() {
  return ::IsDebuggerPresent() ? true : false;
}

#else

// Don't break on unsupported platforms.
bool IsDebuggerAttached() {
  return false;
}

#endif

void Break() {
  if (IsDebuggerAttached()) {
#if defined(ION_PLATFORM_LINUX) || defined(ION_PLATFORM_ANDROID) || \
    defined(ION_PLATFORM_MAC) || defined(ION_PLATFORM_IOS) || \
    defined(ION_GOOGLE_INTERNAL)
    raise(SIGINT);
#elif defined(ION_PLATFORM_WINDOWS)
    __debugbreak();
#else
    // No-op on unsupported platforms.
#endif
  }
}

void BreakOrAbort() {
#if defined(ION_PLATFORM_WINDOWS)
  __debugbreak();
#else
  if (IsDebuggerAttached()) {
    Break();
  } else {
    abort();
  }
#endif
}

}  // namespace port
}  // namespace ion
