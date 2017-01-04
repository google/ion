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

#ifndef ION_PORT_BREAK_H_
#define ION_PORT_BREAK_H_

namespace ion {
namespace port {

// If a debugger is attached, this function interrupts the program execution and
// causes any attached debugger to break. It does so by raising SIGINT on Posix
// platforms and __debugbreak on Windows.
//
// Currently this is supported for Linux, Mac, iOS, Android and Windows, but
// adding support for more operating systems is encouraged.
ION_API void Break();

// Calls Break() if running in a debugger, abort() otherwise.  The abort occurs
// on any platform.
ION_API void BreakOrAbort();

// Returns whether a debugger is attached to this process.
ION_API bool IsDebuggerAttached();

}  // namespace port
}  // namespace ion

#endif  // ION_PORT_BREAK_H_
