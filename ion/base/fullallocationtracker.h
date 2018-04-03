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

#ifndef ION_BASE_FULLALLOCATIONTRACKER_H_
#define ION_BASE_FULLALLOCATIONTRACKER_H_

#include <iostream>  // NOLINT
#include <memory>

#include "ion/base/allocationtracker.h"

namespace ion {
namespace base {

// FullAllocationTracker is a derived AllocationTracker class that keeps track
// of all active allocations and also provides tracing facilities for
// debugging and error checking of leaked allocations.
class ION_API FullAllocationTracker : public AllocationTracker {
 public:
  FullAllocationTracker();

  // Sets an output stream to use for tracing allocations and deallocations. If
  // the stream is non-NULL, tracing is enabled, and a message is printed to
  // the stream for each allocation or deallocation. The default is a NULL
  // stream.
  void SetTracingStream(std::ostream* s) { tracing_ostream_ = s; }
  std::ostream* GetTracingStream() const { return tracing_ostream_; }

  // AllocationTracker interface implementations.
  void TrackAllocation(const Allocator& allocator,
                       size_t requested_size, const void* memory) override;
  void TrackDeallocation(const Allocator& allocator,
                         const void* memory) override;
  size_t GetAllocationCount() override;
  size_t GetDeallocationCount() override;
  size_t GetAllocatedBytesCount() override;
  size_t GetDeallocatedBytesCount() override;
  size_t GetActiveAllocationCount() override;
  size_t GetActiveAllocationBytesCount() override;

  void SetGpuTracker(const AllocationSizeTrackerPtr& gpu_tracker) override;
  AllocationSizeTrackerPtr GetGpuTracker() override;

 protected:
  // The destructor is protected because all instances should be managed
  // through SharedPtr. This logs an error for each allocation that is still
  // active.
  ~FullAllocationTracker() override;

 private:
  // Helper class that does most of the work, hiding the implementation.
  class Helper;
  std::unique_ptr<Helper> helper_;

  // Output stream for tracing. NULL when tracing is disabled.
  std::ostream* tracing_ostream_;
};

// Convenience typedef for shared pointer to a FullAllocationTracker.
typedef SharedPtr<FullAllocationTracker> FullAllocationTrackerPtr;

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_FULLALLOCATIONTRACKER_H_
