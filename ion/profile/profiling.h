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

#ifndef ION_PROFILE_PROFILING_H_
#define ION_PROFILE_PROFILING_H_

// This file contains classes and macros related to run-time performance
// profiling.

#include "ion/base/staticsafedeclare.h"
#include "ion/profile/calltracemanager.h"
#include "ion/profile/tracerecorder.h"

namespace ion {
namespace profile {

// Get the global, static instance of CallTraceManager.
ION_API CallTraceManager* GetCallTraceManager();

}  // namespace profile
}  // namespace ion

// Some levels of indirection required to make C preprocessor recursively expand
// __LINE__ macro in presence of ## operator.
#define ION_PROFILING_PASTE1(x, y) x ## y
#define ION_PROFILING_PASTE2(x, y) ION_PROFILING_PASTE1(x, y)
#define ION_PROFILING_PASTE3(x) ION_PROFILING_PASTE2(x, __LINE__)

// This macro can be used at the top of a function scope to declare the
// function and create a DefaultScopedTracer instance to automatically mark
// the entry and exit points of the function.
#define ION_PROFILE_FUNCTION(func_name)                             \
  ::ion::profile::ScopedTracer ION_PROFILING_PASTE3(scope_tracer_)( \
      ::ion::profile::GetCallTraceManager()->GetTraceRecorder(), func_name)

// A version of ION_PROFILE_FUNCTION which allows attaching a single key/value
// pair to the scope. value must be in JSON format, e.g. "\"my_string\"" for a
// string value, "18" for the integer value 18,
// "{ \"name\": \"my_name\", \"count\": 17 }" for an object with two key value
// pairs.
#define ION_PROFILE_FUNCTION_ANNOTATED(func_name, key, value) \
    ION_PROFILE_FUNCTION(func_name);                          \
    ::ion::profile::GetCallTraceManager()->GetTraceRecorder() \
        ->AnnotateCurrentScope(key, value);

#define ION_PROFILE_FRAME                                                   \
    ION_DECLARE_SAFE_STATIC_POINTER_WITH_CONSTRUCTOR(                       \
        int, ION_PROFILING_PASTE3(frame_number_), new int(0));              \
    DCHECK_NE(0, ION_PROFILING_PASTE3(frame_number_));                      \
    ::ion::profile::ScopedFrameTracer ION_PROFILING_PASTE3(frame_tracer_)(  \
        ::ion::profile::GetCallTraceManager()->GetTraceRecorder(),          \
        ++(*ION_PROFILING_PASTE3(frame_number_)));

// Use this macro to annotate a name/value pair in the current scope (opened by,
// e.g., ION_PROFILE_FUNCTION().) |value| can be a string (char* or
// std::string), boolean, or numerical values.
#define ION_ANNOTATE(name, value) ::ion::profile::GetCallTraceManager()-> \
    GetTraceRecorder()->AnnotateCurrentScopeWithJsonSafeValue(name, value);

#endif  // ION_PROFILE_PROFILING_H_
