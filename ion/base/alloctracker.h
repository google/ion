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

#ifndef ION_BASE_ALLOCTRACKER_H_
#define ION_BASE_ALLOCTRACKER_H_

// This file enables more thorough testing of allocation management by defining
// the global new and delete operators to call functions in the singleton
// AllocTracker class.  This file needs to be force-included for all of Ion
// (and anything that uses it) for proper testing.

// This allows code to know whether this file was force-included or not.
#define ION_ALLOC_TRACKER_DEFINED 1

// Because this file is force-included, it may be included in C files. Any
// allocations from C files cannot be tracked.
#if defined(__cplusplus)

#include <memory>
#include <new>

#include "base/integral_types.h"

namespace ion {
namespace base {

// AllocTracker is a singleton class used to track and report all allocations
// made with the global new operator.
//
// NOTE: This will likely not work properly with multi-threaded applications.
class ION_API AllocTracker {
 public:
  // This enum specifies the type of allocation being performed.
  enum AllocType {
    kNonArrayAlloc,  // Allocations using new.
    kArrayAlloc,     // Allocations using new[].
    kInternalAlloc,  // Allocations for internal AllocTracker use.
  };
  static const int kNumAllocTypes = kInternalAlloc + 1;

  // This struct stores allocation and byte counts for a single type.
  struct TypeCounts {
    TypeCounts() : allocs(0), bytes(0) {}
    uint64 allocs;
    uint64 bytes;
  };

  // This struct stores allocation and byte counts for all types.
  struct Counts {
    TypeCounts counts[kNumAllocTypes];
  };

  // Returns the singleton instance.
  static AllocTracker* GetMutableInstance();
  static const AllocTracker& GetInstance();

  AllocTracker();

  // Sets the allocation tracking baseline to the current counts. This can be
  // used to ignore allocations made before tracking is possible.
  void SetBaseline();

  // Returns the baseline Counts. These will be 0 unless SetBaseline() was
  // called.
  const Counts& GetBaselineCounts() const { return baseline_counts_; }

  // Returns the Counts representing all allocations.
  const Counts& GetAllCounts() const { return all_counts_; }

  // Returns the Counts representing open allocations (new was called, but
  // delete was not).
  const Counts& GetOpenCounts() const { return open_counts_; }

  // These are used by global new and delete to track allocations.
  void* New(std::size_t size, AllocType type);
  void Delete(void* ptr, AllocType type);

 private:
  // Internal nested class that does most of the work.
  class Helper;

  // The destructor is private because this is a singleton.
  virtual ~AllocTracker();

  Counts baseline_counts_;   // Allocations when baseline is set.
  Counts all_counts_;        // All allocations.
  Counts open_counts_;       // Allocations for which delete was not called.

  std::unique_ptr<Helper> helper_;
};

}  // namespace base
}  // namespace ion

// Define the global new and delete operators to call tracking functions. The
// placement new and delete functions do not need to be defined, as they do
// nothing.
void* operator new(std::size_t size);
void* operator new[](std::size_t size);
void operator delete(void* ptr);
void operator delete[](void* ptr);

#endif  // __cplusplus

#endif  // ION_BASE_ALLOCTRACKER_H_
