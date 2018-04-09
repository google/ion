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

#include "ion/analytics/discrepancy.h"

#include <random>
#include <vector>

#include "third_party/googletest/googletest/include/gtest/gtest.h"

using std::vector;
using ion::analytics::AbsoluteTimestampDiscrepancy;
using ion::analytics::Discrepancy;
using ion::analytics::IntervalDiscrepancy;
using ion::analytics::SampleMapping;

// Lloyd relaxation in 1D.
// Keeps the position of the first and last sample.
template <class RandomIt>
void Relax(RandomIt first, RandomIt last, int iterations) {
  size_t num_samples = last - first;
  for (int iteration = 0; iteration < iterations; ++iteration) {
    vector<double> voronoi_boundaries;
    for (size_t i = 1; i < num_samples; ++i) {
      voronoi_boundaries.push_back((first[i] + first[i - 1]) * 0.5);
    }

    vector<double> relaxed_samples;
    for (size_t i = 1; i < num_samples - 1; ++i) {
      first[i] = (voronoi_boundaries[i - 1] + voronoi_boundaries[i]) * 0.5;
    }
  }
}

vector<double> CreateRandomSamples(size_t num_samples) {
  vector<double> samples = {0.0};
  std::default_random_engine engine;
  std::uniform_real_distribution<double> uniform_dist(0.0, 1.0);
  for (size_t i = 1; i < num_samples; ++i)
    samples.push_back(uniform_dist(engine));
  return samples;
}

TEST(SampleMapping, NormalizedFromTime) {
  SampleMapping sample_mapping(2.0, 5.0, 4);
  CHECK_EQ(0.125, sample_mapping.NormalizedFromTime(2.0));
  CHECK_EQ(0.375, sample_mapping.NormalizedFromTime(3.0));
  CHECK_EQ(0.625, sample_mapping.NormalizedFromTime(4.0));
  CHECK_EQ(0.875, sample_mapping.NormalizedFromTime(5.0));
}

TEST(SampleMapping, TimeFromNormalized) {
  SampleMapping sample_mapping(2.0, 5.0, 4);
  CHECK_EQ(2.0, sample_mapping.TimeFromNormalized(0.125));
  CHECK_EQ(3.0, sample_mapping.TimeFromNormalized(0.375));
  CHECK_EQ(4.0, sample_mapping.TimeFromNormalized(0.625));
  CHECK_EQ(5.0, sample_mapping.TimeFromNormalized(0.875));
}

TEST(SampleMapping, DurationFromLength) {
  SampleMapping sample_mapping(2.0, 5.0, 4);
  CHECK_EQ(1.0, sample_mapping.DurationFromLength(0.25));
  CHECK_EQ(3.0, sample_mapping.DurationFromLength(0.75));
}

TEST(NormalizeSamples, Edge) {
  vector<double> samples = {0.0, 1.0 / 3.0, 2.0 / 3.0, 1.0};
  vector<double> expected_normalized_samples = {1.0 / 8.0, 3.0 / 8.0, 5.0 / 8.0,
                                                7.0 / 8.0};
  SampleMapping sample_mapping(samples.front(), samples.back(), samples.size());
  vector<double> normalized_samples = NormalizeSamples(samples, sample_mapping);
  EXPECT_EQ(expected_normalized_samples, normalized_samples);
}

TEST(NormalizeSamples, Center) {
  vector<double> samples = {1.0 / 8.0, 3.0 / 8.0, 5.0 / 8.0, 7.0 / 8.0};
  vector<double> expected_normalized_samples = {1.0 / 8.0, 3.0 / 8.0, 5.0 / 8.0,
                                                7.0 / 8.0};
  SampleMapping sample_mapping(samples.front(), samples.back(), samples.size());
  vector<double> normalized_samples = NormalizeSamples(samples, sample_mapping);
  EXPECT_EQ(expected_normalized_samples, normalized_samples);
}

TEST(Discrepancy, Random) {
  const int num_samples = 50;
  const int num_tests = 100;
  const int lloyd_iterations = 10;

  for (int i = 0; i < num_tests; ++i) {
    vector<double> samples = CreateRandomSamples(num_samples);
    SampleMapping sample_mapping(samples.front(), samples.back(),
                                 samples.size());
    NormalizeSamples(samples.begin(), samples.end(), sample_mapping);
    double discrepancy = Discrepancy(samples).discrepancy;
    Relax(samples.begin(), samples.end(), lloyd_iterations);
    double relaxed_discrepancy = Discrepancy(samples).discrepancy;
    EXPECT_LE(relaxed_discrepancy, discrepancy);
  }
}

TEST(Discrepancy, Analytic) {
  vector<double> samples;
  double discrepancy;
  IntervalDiscrepancy interval_discrepancy;

  samples = vector<double>{};
  discrepancy = Discrepancy(samples).discrepancy;
  EXPECT_EQ(0.0, discrepancy);

  samples = vector<double>{0.5};
  discrepancy = Discrepancy(samples).discrepancy;
  EXPECT_EQ(0.5, discrepancy);

  samples = vector<double>{0.0, 1.0};
  interval_discrepancy = Discrepancy(samples);
  EXPECT_EQ(1.0, interval_discrepancy.discrepancy);
  EXPECT_EQ(0.0, interval_discrepancy.begin);
  EXPECT_EQ(1.0, interval_discrepancy.end);
  EXPECT_EQ(0U, interval_discrepancy.num_samples);

  samples = vector<double>{0.5, 0.5, 0.5};
  discrepancy = Discrepancy(samples).discrepancy;
  EXPECT_EQ(0.5, discrepancy);

  samples = vector<double>{1.0 / 8.0, 3.0 / 8.0, 5.0 / 8.0, 7.0 / 8.0};
  discrepancy = Discrepancy(samples).discrepancy;
  EXPECT_EQ(0.25, discrepancy);

  samples = vector<double>{1.0 / 8.0, 5.0 / 8.0, 5.0 / 8.0, 7.0 / 8.0};
  discrepancy = Discrepancy(samples).discrepancy;
  EXPECT_EQ(0.5, discrepancy);

  samples =
      vector<double>{1.0 / 8.0, 3.0 / 8.0, 5.0 / 8.0, 5.0 / 8.0, 7.0 / 8.0};
  interval_discrepancy = Discrepancy(samples);
  EXPECT_EQ(0.3, interval_discrepancy.discrepancy);
  EXPECT_EQ(0.125, interval_discrepancy.begin);
  EXPECT_EQ(0.625, interval_discrepancy.end);
  EXPECT_EQ(1U, interval_discrepancy.num_samples);

  samples = vector<double>{0.0, 1.0 / 3.0, 2.0 / 3.0, 1.0};
  discrepancy = Discrepancy(samples).discrepancy;
  EXPECT_EQ(0.5, discrepancy);

  SampleMapping sample_mapping(samples.front(), samples.back(), samples.size());
  vector<double> normalized_samples =
      NormalizeSamples(samples.begin(), samples.end(), sample_mapping);
  discrepancy = Discrepancy(normalized_samples).discrepancy;
  EXPECT_EQ(0.25, discrepancy);
}

TEST(AbsoluteTimestampDiscrepancy, Comparison) {
  double discrepancy;

  discrepancy = AbsoluteTimestampDiscrepancy(vector<double>()).discrepancy;
  EXPECT_EQ(0.0, discrepancy);

  discrepancy = AbsoluteTimestampDiscrepancy(vector<double>{4}).discrepancy;
  EXPECT_EQ(0.5, discrepancy);

  double d_a = AbsoluteTimestampDiscrepancy(vector<double>{0, 1, 2, 3, 5, 6})
                   .discrepancy;
  double d_b = AbsoluteTimestampDiscrepancy(vector<double>{0, 1, 2, 3, 5, 7})
                   .discrepancy;
  double d_c =
      AbsoluteTimestampDiscrepancy(vector<double>{0, 2, 3, 4}).discrepancy;
  double d_d =
      AbsoluteTimestampDiscrepancy(vector<double>{0, 2, 3, 4, 5}).discrepancy;

  EXPECT_LT(d_a, d_b);
  EXPECT_DOUBLE_EQ(d_c, d_d);
}
