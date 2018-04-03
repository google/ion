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
#include <iostream>

#if ION_PRODUCTION || defined(__myriad2__)
#define ION_NO_STACKTRACE
#endif

#if defined(ION_NO_STACKTRACE)
// Do not include any headers.
#elif defined(ION_PLATFORM_ANDROID) || defined(ION_PLATFORM_LINUX) || \
    defined(ION_PLATFORM_MAC) ||                                      \
    (defined(ION_PLATFORM_IOS) && !defined(ION_ARCH_ARM32))
// Darwin 32-bit ARM does not support _Unwind_Backtrace, but 32-bit on iOS is
// quite deprecated.
#define ION_STACKTRACE_POSIX
#include <cxxabi.h>
#include <dlfcn.h>
#include <string.h>
#include <unwind.h>
#include <cstdlib>
#elif defined(ION_PLATFORM_WINDOWS)
#include <windows.h>  // NOLINT
#include <DbgHelp.h>
#include <WinBase.h>
#pragma comment(lib, "dbghelp")  // Add library in non-production build
#pragma comment(lib, "psapi")    // Add library in non-production build
#endif

#include <assert.h>  // For checking return values (port has no logging).

#include <iomanip>
#include <sstream>

namespace ion {
namespace port {

namespace {

#if defined(ION_NO_STACKTRACE)
// Do not define helper functions.
#elif defined(ION_STACKTRACE_POSIX)

struct BacktraceFrame {
  void** current;
  void** end;
};

// Callback used by Unwind to obtain the program counter address.
_Unwind_Reason_Code UnwindCallback(_Unwind_Context* context, void* arg) {
  BacktraceFrame* frame = reinterpret_cast<BacktraceFrame*>(arg);
  uintptr_t pc = _Unwind_GetIP(context);
  if (pc) {
    if (frame->current == frame->end) {
      return _URC_END_OF_STACK;
    } else {
      *frame->current++ = reinterpret_cast<void*>(pc);
    }
  }
  return _URC_NO_REASON;
}

// Writes the addresses of the current stack into |addresses| up to |size| deep.
// Returns the number of actual frames written.
int GetBacktrace(void** addresses, int size) {
  BacktraceFrame frame;
  frame.current = addresses;
  frame.end = addresses + size;

  _Unwind_Backtrace(UnwindCallback, &frame);
  int count = static_cast<int>(frame.current - addresses);
  return count;
}

#elif defined(ION_PLATFORM_WINDOWS)
// Writes the addresses of the current stack into |addresses| up to |size| deep.
// Returns the number of actual frames written.
int GetBacktrace(PVOID* addresses, int size) {
  CONTEXT context;
  RtlCaptureContext(&context);

  STACKFRAME64 stack_frame;
  memset(&stack_frame, 0, sizeof(stack_frame));
#if defined(ION_ARCH_X86_64)
  int arch = IMAGE_FILE_MACHINE_AMD64;
  stack_frame.AddrPC.Offset = context.Rip;
  stack_frame.AddrFrame.Offset = context.Rbp;
  stack_frame.AddrStack.Offset = context.Rsp;
#elif defined(ION_ARCH_X86)
  int arch = IMAGE_FILE_MACHINE_I386;
  stack_frame.AddrPC.Offset = context.Eip;
  stack_frame.AddrFrame.Offset = context.Ebp;
  stack_frame.AddrStack.Offset = context.Esp;
#else
#error "Unsupported Windows architecture"
#endif
  stack_frame.AddrPC.Mode = AddrModeFlat;
  stack_frame.AddrFrame.Mode = AddrModeFlat;
  stack_frame.AddrStack.Mode = AddrModeFlat;

  const HANDLE process = GetCurrentProcess();
  const HANDLE thread = GetCurrentThread();
  int frame;
  for (frame = 0; frame < size; ++frame) {
    BOOL ret = StackWalk64(arch, process, thread, &stack_frame, &context, NULL,
                           SymFunctionTableAccess64, SymGetModuleBase64, NULL);
    if (ret != TRUE) break;
    addresses[frame] = reinterpret_cast<PVOID>(stack_frame.AddrPC.Offset);
  }
  return frame;
}

#endif

}  // anonymous namespace

StackTrace::StackTrace() {
#if !defined(ION_NO_STACKTRACE)
  // Obtain the addresses of stack functions.
#if defined(ION_PLATFORM_WINDOWS)
  const int kMaxStackTraceDepth = 62;  // < 63 according to Windows API.
  // Symbol information needs to be initialized before taking a stack trace,
  // otherwise StackWalk64 will fail on x64.  Of course, this fact is completely
  // undocumented.
  static const BOOL kSymbolsInitialized = []() -> BOOL {
    std::atexit([]() { SymCleanup(GetCurrentProcess()); });
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    return SymInitialize(GetCurrentProcess(), nullptr, TRUE);
  }();
  assert(kSymbolsInitialized);
#elif defined(ION_STACKTRACE_POSIX)
  const int kMaxStackTraceDepth = 128;  // Arbitrary limit.
#endif
#if defined(ION_STACKTRACE_POSIX) || defined(ION_PLATFORM_WINDOWS)
  addresses_.resize(kMaxStackTraceDepth);
  const int depth = GetBacktrace(&addresses_[0], kMaxStackTraceDepth);
  assert(depth > 0);
  addresses_.resize(depth);
#endif
#endif  // !ION_NO_STACKTRACE
}

void StackTrace::ObtainSymbols() const {
#if !defined(ION_NO_STACKTRACE)
  symbols_.resize(addresses_.size());
  offsets_.resize(addresses_.size());
  modules_.resize(addresses_.size());

#if defined(ION_STACKTRACE_POSIX)
  for (size_t i = 0; i < addresses_.size(); ++i) {
    const char* symbol = "";

    Dl_info info;
    if (dladdr(addresses_[i], &info)) {
      if (info.dli_sname) symbol = info.dli_sname;
      if (info.dli_fname) modules_[i] = std::string(info.dli_fname);
      offsets_[i] = reinterpret_cast<uintptr_t>(addresses_[i]) -
                    reinterpret_cast<uintptr_t>(info.dli_saddr);
    }

    int status = 0;
    char* demangled =
        __cxxabiv1::__cxa_demangle(symbol, nullptr, nullptr, &status);
    if (!demangled) demangled = strdup(symbol);

    symbols_[i] = demangled;
    free(demangled);
  }

#elif defined(ION_PLATFORM_WINDOWS)
  // Obtain the symbols corresponding to the stack function addresses.
  constexpr int kSymbolSize =
      sizeof(SYMBOL_INFO) + (MAX_SYM_NAME * sizeof(TCHAR));
  char symbol_memory[kSymbolSize] = {0};
  SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(symbol_memory);
  symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
  symbol->MaxNameLen = MAX_SYM_NAME;

  const HANDLE process = GetCurrentProcess();
  for (size_t i = 0; i < addresses_.size(); ++i) {
    SymFromAddr(process, reinterpret_cast<DWORD64>(addresses_[i]),
                reinterpret_cast<PDWORD64>(&offsets_[i]), symbol);
    symbols_[i] = symbol->Name;
  }

#endif
#endif
}

#undef ION_STACKTRACE_POSIX

const std::vector<std::string>& StackTrace::GetSymbols() const {
  if (symbols_.empty()) ObtainSymbols();
  return symbols_;
}

const std::string StackTrace::GetSymbolString() const {
  const std::vector<void*>& addresses = GetAddresses();
  const std::vector<std::string>& symbols = GetSymbols();
  assert(addresses.size() == symbols.size());
  if (!modules_.empty()) assert(modules_.size() == addresses.size());
  if (!offsets_.empty()) assert(offsets_.size() == addresses.size());
  std::ostringstream output;
  // The first two frames are StackTrace itself.
  // NOTE: If we later add an alternative Windows implementation based on
  // RtlVirtualUnwind, the start index will need to become platform-specific,
  // since RtlVirtualUnwind does not return for the caller's frame and thus
  // GetBacktrace() is not present in the trace.
  for (size_t i = 2; i < addresses.size(); ++i) {
    // Output format is similar to the Android debuggerd format:
    // #XX pc <address> <module> (<proc>+<offset>)

    // Frame counter.
    output << "#" << std::setfill('0') << std::setw(2) << std::dec << (i - 2)
           << " ";
    // Address.
    output << "pc " << std::setfill('0') << std::setw(16) << std::hex
           << reinterpret_cast<uintptr_t>(addresses[i]) << " ";
    // Module name.
    if (!modules_.empty()) output << modules_[i] << " ";
    // Symbol (mangled or demangled).
    if (!symbols[i].empty()) {
      output << "(" << symbols[i];
      // Instruction byte offset.
      if (!offsets_.empty()) output << "+" << std::dec << offsets_[i] << ")";
    }
    output << std::endl;
  }
  return output.str();
}

}  // namespace port
}  // namespace ion
