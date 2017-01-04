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

#include <iomanip>
#include <iostream>  // NOLINT
#include <memory>

#include "ion/base/staticsafedeclare.h"
#include "ion/gfx/node.h"
#include "ion/gfx/statetable.h"

#if !defined(ION_ALLOC_TRACKER_DEFINED)
# error The allocations demo requires the --track-allocations build option.
#endif

namespace {

using ion::base::AllocTracker;

// Class that uses static construction/destruction to report allocation results.
class Reporter {
 public:
  Reporter() {}
  ~Reporter() { Report(std::cout); }

 private:
  static void Report(std::ostream& out);  // NOLINT
  static void ReportCounts(const std::string& row_title,
                           AllocTracker::AllocType type,
                           int column_width, std::ostream& out);  // NOLINT
};

void Reporter::Report(std::ostream& out) {  // NOLINT
  // This reports all counts in a table, with AllocType as the rows.
  static const int kCountWidth = 10;
  static const char kSeparator[] = "---------------------------------------"
                                   "--------------------------------------\n";

  // Table header.
  out << kSeparator
      << std::setw(56) << std::right << "Allocation Tracker Report\n"
      << std::setw(34) << std::right << "Allocations"
      << std::setw(28) << std::right << "Bytes\n"
      << kSeparator;

  ReportCounts("NonArray", AllocTracker::kNonArrayAlloc, kCountWidth, out);
  ReportCounts("Array", AllocTracker::kArrayAlloc, kCountWidth, out);
  ReportCounts("Internal", AllocTracker::kInternalAlloc, kCountWidth, out);
  out << kSeparator;
}

void Reporter::ReportCounts(const std::string& row_title,
                            AllocTracker::AllocType type,
                            int column_width, std::ostream& out) {  // NOLINT
  const AllocTracker& tracker = AllocTracker::GetInstance();

  const AllocTracker::TypeCounts& base =
      tracker.GetBaselineCounts().counts[type];
  const AllocTracker::TypeCounts& all = tracker.GetAllCounts().counts[type];
  const AllocTracker::TypeCounts& open = tracker.GetOpenCounts().counts[type];

  out << std::setw(column_width) << row_title;

  // Allocations.
  out << std::setw(column_width) << open.allocs
      << std::setw(column_width) << all.allocs
      << std::setw(column_width) << base.allocs;

  // Bytes.
  if (type == AllocTracker::kInternalAlloc) {
    out << std::setw(column_width) << "-"
        << std::setw(column_width) << "-"
        << std::setw(column_width) << "-";
  } else {
    out << std::setw(column_width) << open.bytes
        << std::setw(column_width) << all.bytes
        << std::setw(column_width) << base.bytes;
  }

  out << "\n";
}

}  // anonymous namespace

int main() {
  ION_DECLARE_SAFE_STATIC_POINTER(Reporter, reporter);

  // The allocation of the Reporter instance cannot be cleaned up before
  // reporting.  Similarly, any static allocations made before this will also
  // slip through. Set the baseline for allocations to whatever is currently
  // active.
  ion::base::AllocTracker::GetMutableInstance()->SetBaseline();

  // Just do a few simple allocations for now. These should all be cleaned up
  // properly.
  ion::gfx::StateTablePtr st(new ion::gfx::StateTable);
  ion::gfx::NodePtr node(new ion::gfx::Node);
  std::unique_ptr<int> pi(new int);
  std::unique_ptr<int[]> pia(new int[20]);

  return 0;
}
