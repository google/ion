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

// ----------------------------------------------------------------------------
// Discrepancy
//   Measures how much a sequence of numbers deviates from a uniformly
//   distributed sequence.
//
// Discrepancy has traditionally been used to measure the quality of samples for
// Monte Carlo integration. It is also a good metric for measuring rendering
// performance, specifically the worst case performance.  When applied to a
// series of timestamps, discrepancy essentially measures the duration of the
// worst stretch of bad frames adjusted for good frames inbetween. In contrast
// to histogram based metrics, it takes the temporal order of the frames into
// account, i.e. it does coalesce consecutive bad frames. Given the timestamps
// series A and B, for example:
// A = +++++++++++++++++ +++++++
// B = +++++++ +++++++++++ +++++
// C = ++++++++++ + ++++++++++++
// The discrepancy of C will be roughly twice the discrepancy of B. Note that
// discrepancy is not meant to measure the average case; The discrepancy of B
// is approximately the same as the discrepancy of A. The metric is therefore
// most useful in combination with the average frame time (or frames/second).
//
// The implementation in this file is a C++ port of the python code in Chrome's
// telemetry framework.

#ifndef ION_ANALYTICS_DISCREPANCY_H_
#define ION_ANALYTICS_DISCREPANCY_H_

#include <algorithm>
#include <vector>

#include "ion/base/logging.h"

namespace ion {
namespace analytics {

// Helper class for transforming samples between the time domain and the
// (unitless) normalized domain used for discrepancy calculation.
//
// For N input values the first output value becomes 0.5/N and the last value
// becomes (N-0.5)/N. E.g. for four uniformly distributed input numbers, the
// output will be (* marks the location of an output sample):
// 0   1/8  1/4  3/8  1/2  5/8  3/4  7/8   1
// |    *    |    *    |    *    |    *    |
//
// Background: The discrepancy of the sequence i/(N-1); i=0, ..., N-1 is 2/N,
// twice the discrepancy of the sequence (i+1/2)/N; i=0, ..., N-1. In our case
// we don't want to distinguish between these two cases, as our original domain
// is not bounded (it is for Monte Carlo integration, where discrepancy was
// originally used).
class SampleMapping {
 public:
  SampleMapping(double time_begin, double time_end, size_t num_samples) {
    CHECK_LT(1, num_samples);
    CHECK_GT(time_end, time_begin);
    time_begin_ = time_begin;
    normalized_begin_ = 0.5 / static_cast<double>(num_samples);
    const double normalized_end = (static_cast<double>(num_samples) - 0.5) /
                                  static_cast<double>(num_samples);
    scale_ = (normalized_end - normalized_begin_) / (time_end - time_begin_);
    inv_scale_ = 1.0 / scale_;
  }

  // Maps a sample from time domain to normalized (unitless) domain.
  double NormalizedFromTime(double time_sample) const {
    return normalized_begin_ + scale_ * (time_sample - time_begin_);
  }

  // Maps a sample back from normalized (unitless) domain to time domain.
  double TimeFromNormalized(double normalized_sample) const {
    return time_begin_ + inv_scale_ * (normalized_sample - normalized_begin_);
  }

  // Maps a duration back from normalized (unitless) domain to time domain.
  double DurationFromLength(double length) const { return length * inv_scale_; }

 private:
  // Timestamp of the first sample.
  double time_begin_;
  // Normalized (unitless) coordiate of the first sample.
  double normalized_begin_;
  // Scale factor going from time domain to normalized domain.
  double scale_;
  // Scale factor going from normalized domain back to time domain.
  double inv_scale_;
};

// Sorts a sequence of numbers and normalizes it to the range [0, 1] using
// the given |sample_mapping|.
template <class InputIterator>
std::vector<double> NormalizeSamples(InputIterator first, InputIterator last,
                                     const SampleMapping& sample_mapping) {
  size_t num_samples = last - first;
  std::vector<double> result(num_samples);
  std::copy(first, last, result.begin());
  std::sort(result.begin(), result.end());
  for (auto& iter : result) iter = sample_mapping.NormalizedFromTime(iter);
  return result;
}

// Normalize all samples in a container
template <class ContainerType>
std::vector<double> NormalizeSamples(const ContainerType& samples,
                                     const SampleMapping& sample_mapping) {
  return NormalizeSamples(samples.begin(), samples.end(), sample_mapping);
}

// Result of a discrepancy computation, including the value measured and the
// bounds of the interval where that value was measured. Used to diagnose edge
// cases where the value alone provides insufficient information.
struct IntervalDiscrepancy {
  // Default constructor, initializing all values to zero.
  IntervalDiscrepancy()
      : discrepancy(0.0), begin(0.0), end(0.0), num_samples(0) {}
  // Constructor.
  IntervalDiscrepancy(double discrepancy, double begin, double end,
                      size_t num_samples)
      : discrepancy(discrepancy),
        begin(begin),
        end(end),
        num_samples(num_samples) {}
  // The discrepancy of the samples in the interval.
  double discrepancy;
  // The beginning of the interval where the value was measured.
  double begin;
  // The end of the interval where the value was measured.
  double end;
  // The number of samples in the interval from begin to end. The interval might
  // be open or closed. Discrepancy checks both cases and reports the worst
  // case.
  size_t num_samples;
};

// Computes the discrepancy of a sequence of numbers in the range [0,1].
//
// The numbers must be sorted. We define the discrepancy of an empty sequence to
// be zero. This implementation only considers sampling densities lower than the
// average for the discrepancy. The original mathematical definition also
// considers higher densities.
//
// http://en.wikipedia.org/wiki/Equidistributed_sequence
// http://en.wikipedia.org/wiki/Low-discrepancy_sequence
// http://mathworld.wolfram.com/Discrepancy.html
template <class InputIterator>
IntervalDiscrepancy Discrepancy(InputIterator first, InputIterator last) {
  IntervalDiscrepancy largest_interval_discrepancy;
  const size_t num_samples = last - first;
  if (num_samples == 0) return largest_interval_discrepancy;

  const double inv_sample_count = 1.0 / static_cast<double>(num_samples);
  std::vector<double> locations;
  // For each location, stores the number of samples less than that location.
  std::vector<size_t> count_less;
  // For each location, stores the number of samples less than or equal to that
  // location.
  std::vector<size_t> count_less_equal;

  // Populate locations with sample positions. Append 0 and 1 if necessary.
  if (*first > 0.0) {
    locations.push_back(0.0);
    count_less.push_back(0);
    count_less_equal.push_back(0);
  }
  size_t i = 0;
  for (auto iter = first; iter != last; ++iter) {
    locations.push_back(*iter);
    count_less.push_back(i);
    count_less_equal.push_back(i + 1);
    ++i;
  }
  if (*(last - 1) < 1.0) {
    locations.push_back(1.0);
    count_less.push_back(num_samples);
    count_less_equal.push_back(num_samples);
  }

  // The following algorithm is modification of Kadane's algorithm,
  // see https://en.wikipedia.org/wiki/Maximum_subarray_problem.

  // The maximum of (length(k, i-1) - count_open(k, i-1)/N) for any k < i-1.
  // Note that this is not the global maximum. The interval where this
  // discrepancy was found is referred to as the current interval.
  double interval_discrepancy = 0.0;
  // The current interval is the open interval from locations[interval_begin] to
  // locations[interval_end].
  size_t interval_begin = 0;
  size_t interval_end = 0;
  for (size_t i = 1; i < locations.size(); ++i) {
    // The distance between the previous location and the current location.
    const double length = locations[i] - locations[i - 1];

    // Number of samples that are added, if we extend the current interval.
    const size_t count_open_increment = count_less[i] - count_less[i - 1];
    // The discrepancy, if we extend the current interval.
    const double extended_interval_discrepancy =
        interval_discrepancy +
        (length - static_cast<double>(count_open_increment) * inv_sample_count);

    // Number of samples in a new open interval from locations[i-1] to
    // locations[i].
    const size_t new_count_open = count_less[i] - count_less_equal[i - 1];
    // The discrepancy in this new open interval.
    const double new_interval_discrepancy =
        length - static_cast<double>(new_count_open) * inv_sample_count;

    // Use the interval with the larger discrepancy.
    if (extended_interval_discrepancy >= new_interval_discrepancy) {
      // Extend the current interval.
      interval_discrepancy = extended_interval_discrepancy;
      interval_end = i;
    } else {
      // Start a new interval.
      interval_discrepancy = new_interval_discrepancy;
      interval_begin = i - 1;
      interval_end = i;
    }

    // Update the global maximum, if necessary.
    if (interval_discrepancy > largest_interval_discrepancy.discrepancy) {
      largest_interval_discrepancy = IntervalDiscrepancy(
          interval_discrepancy, locations[interval_begin],
          locations[interval_end],
          count_less[interval_end] - count_less_equal[interval_begin]);
    }
  }

  return largest_interval_discrepancy;
}

// Compute analytic discrepancy of entire container.
template <class ContainerType>
IntervalDiscrepancy Discrepancy(const ContainerType& samples) {
  return Discrepancy(samples.begin(), samples.end());
}

// A discrepancy-based metric for measuring the irregularity of timestamps
//
// TimestampsDiscrepancy quantifies the largest area of irregularity observed
// in
// a series of timestamps.
//
// Absolute discrepancy is scaled to have the same unit as the input sequence
// (raw discrepancy is unitless in the range [0,1]).  This means that the
// value
// doesn't change if additional 'good' frames are added to the sequence, which
// is useful when benchmark runs of different durations are compared. E.g. the
// absolute discrepancies of {0, 2, 3, 4} and {0, 2, 3, 4, 5} are identical.
template <class RandomIt>
IntervalDiscrepancy AbsoluteTimestampDiscrepancy(RandomIt first,
                                                 RandomIt last) {
  const size_t num_samples = last - first;
  if (num_samples == 0) return IntervalDiscrepancy();
  if (num_samples == 1) return IntervalDiscrepancy(0.5, *first, *first, 0);

  SampleMapping sample_mapping(*std::min_element(first, last),
                               *std::max_element(first, last), num_samples);
  std::vector<double> normalized_timestamps =
      NormalizeSamples(first, last, sample_mapping);
  IntervalDiscrepancy largest_interval_discrepancy =
      Discrepancy(normalized_timestamps.begin(), normalized_timestamps.end());

  // Map results back from normalized to time domain.
  largest_interval_discrepancy.discrepancy = sample_mapping.DurationFromLength(
      largest_interval_discrepancy.discrepancy);
  largest_interval_discrepancy.begin =
      sample_mapping.TimeFromNormalized(largest_interval_discrepancy.begin);
  largest_interval_discrepancy.end =
      sample_mapping.TimeFromNormalized(largest_interval_discrepancy.end);

  return largest_interval_discrepancy;
}

// Compute absolute discrepancy of entire container analytically.
template <class ContainerType>
IntervalDiscrepancy AbsoluteTimestampDiscrepancy(
    const ContainerType& timestamps) {
  return AbsoluteTimestampDiscrepancy(timestamps.begin(), timestamps.end());
}

}  // namespace analytics
}  // namespace ion

#endif  // ION_ANALYTICS_DISCREPANCY_H_
