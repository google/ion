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

#include "ion/port/stacktrace.h"

#if defined(ION_PLATFORM_LINUX) || defined(ION_PLATFORM_MAC) || \
    defined(ION_PLATFORM_IOS)
#  define ION_STACKTRACE_POSIX
#  include <execinfo.h>
#  include <cstdlib>
#elif defined(ION_PLATFORM_WINDOWS)
#  if !ION_PRODUCTION
#    include <windows.h>
#    include <WinBase.h>
#    include <DbgHelp.h>
#    include <iomanip>
#    pragma comment(lib, "dbghelp")  // Add library in non-production build
#    pragma comment(lib, "psapi")  // Add library in non-production build
#  endif
#endif

#include <assert.h>  // For checking return values (port has no logging).
#include <sstream>

namespace ion {
namespace port {

StackTrace::StackTrace() {
#if !ION_PRODUCTION
#if defined(ION_STACKTRACE_POSIX)
  // Obtain the addresses of stack functions.
  const int kMaxStackTraceDepth = 128;  // Arbitrary limit.
  addresses_.resize(kMaxStackTraceDepth);
  const int depth = backtrace(&addresses_[0], kMaxStackTraceDepth);
  assert(depth > 0);
  addresses_.resize(depth);

#elif defined(ION_PLATFORM_WINDOWS)
  // Obtain the addresses of stack functions.
  const int kMaxStackTraceDepth = 62;  // < 63 according to Windows API.
  addresses_.resize(kMaxStackTraceDepth);
  const int depth =
      CaptureStackBackTrace(0, kMaxStackTraceDepth, &addresses_[0], nullptr);
  assert(depth > 0);
  addresses_.resize(depth);

#else

  // Intentionally do nothing.

#endif
#endif
}

void StackTrace::ObtainSymbols() const {
#if !ION_PRODUCTION
#if defined(ION_STACKTRACE_POSIX)
  // Obtain the symbols corresponding to the stack function addresses.
  char** trace_cstrings =
      backtrace_symbols(&addresses_[0], static_cast<int>(addresses_.size()));
  for (size_t i = 0; i < addresses_.size(); ++i) {
    symbols_.push_back(std::string(trace_cstrings[i]));
  }
  free(trace_cstrings);

#elif defined(ION_PLATFORM_WINDOWS)
  // Obtain the symbols corresponding to the stack function addresses.
  const int kMaxNameLength = 255;
  const int kSymbolSize =
      sizeof(SYMBOL_INFO) + (kMaxNameLength * sizeof(TCHAR));
  char symbol_memory[kSymbolSize] = {0};
  SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(symbol_memory);
  symbol->MaxNameLen = kMaxNameLength;
  symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

  const HANDLE process = GetCurrentProcess();
  SymInitialize(process, nullptr, TRUE);
  for (size_t i = 0; i < addresses_.size(); ++i) {
    const DWORD64 address = reinterpret_cast<DWORD64>(addresses_[i]);
    const BOOL success = SymFromAddr(process, address, 0, symbol);
    std::ostringstream output;
    if (success == TRUE) {
      output << symbol->Name;
    } else {
      output << "??? [0x" << std::hex << address << "]"
             << "symbol not found; GetLastError() returns " << GetLastError();
    }
    symbols_.push_back(output.str());
  }
  SymCleanup(process);

#else

  // Intentionally do nothing.

#endif
#endif
}

#undef ION_STACKTRACE_POSIX

const std::vector<std::string>& StackTrace::GetSymbols() const {
  if (symbols_.empty())
    ObtainSymbols();
  return symbols_;
}

const std::string StackTrace::GetSymbolString() const {
  const std::vector<void*>& addresses = GetAddresses();
  const std::vector<std::string>& symbols = GetSymbols();
  std::ostringstream output;
  for (size_t i = 0; i < addresses.size(); ++i) {
    output << "[" << addresses[i] << "]: " << symbols[i] << std::endl;
  }
  return output.str();
}

}  // namespace port
}  // namespace ion
