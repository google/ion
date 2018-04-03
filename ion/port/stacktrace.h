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

#ifndef ION_PORT_STACKTRACE_H_
#define ION_PORT_STACKTRACE_H_

#include <string>
#include <vector>

namespace ion {
namespace port {

// StackTrace acquires a stack trace for the current thread (not suitable for
// calling in an interrupt handler) on construction and supports conversion of
// raw stack pointers to a string of symbolic function names. Currently supports
// Android, OSX, iOS, Linux, and Windows -- on Windows addresses only, no
// symbols. Unsupported platforms will produce an empty stack.
//
class ION_API StackTrace {
 public:
  StackTrace();
  ~StackTrace() {}
  // Returns the stack as a vector of addresses.
  const std::vector<void*>& GetAddresses() const { return addresses_; }
  // Returns the stack as a vector of symbol names.
  const std::vector<std::string>& GetSymbols() const;
  // Returns formatted string containing symbolic function names for elements of
  // the stack trace.
  const std::string GetSymbolString() const;

 private:
  // Obtain symbol information for addresses.
  void ObtainSymbols() const;

  std::vector<void*> addresses_;
  // Mutable to support lazy evaluation.
  mutable std::vector<std::string> symbols_;
  mutable std::vector<uintptr_t> offsets_;
  mutable std::vector<std::string> modules_;
};

}  // namespace port
}  // namespace ion

#endif  // ION_PORT_STACKTRACE_H_
