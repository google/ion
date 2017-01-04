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

#include "ion/analytics/benchmarkutils.h"

#include <sstream>
#include <string>
#include <vector>

#include "ion/base/logchecker.h"
#include "ion/base/tests/multilinestringsequal.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

using ion::analytics::Benchmark;
using ion::base::LogChecker;

TEST(BenchmarkUtils, MergeBenchmarksBothEmpty) {
  LogChecker log_checker;
  Benchmark from, to;
  EXPECT_EQ(0U, MergeBenchmarks(from, &to));
  EXPECT_TRUE(to.GetConstants().empty());
  EXPECT_TRUE(to.GetSampledVariables().empty());
  EXPECT_TRUE(to.GetAccumulatedVariables().empty());
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // Also check NULL case for coverage.
  EXPECT_EQ(0U, MergeBenchmarks(from, nullptr));
}

TEST(BenchmarkUtils, MergeBenchmarksFromEmpty) {
  LogChecker log_checker;
  Benchmark from, to;

  to.AddConstant(Benchmark::Constant(
      Benchmark::Descriptor("Foo", "Group", "Desc", "Liters"), 127.2));
  to.AddSampledVariable(Benchmark::SampledVariable(
      Benchmark::Descriptor("Goo", "Group", "Desc", "Meters/s")));
  to.AddAccumulatedVariable(Benchmark::AccumulatedVariable(
      Benchmark::Descriptor("Foo", "Group", "Desc", "Liters/s"),
      12U, 100.0, 106.5, 102.5, 1.5));

  EXPECT_EQ(0U, MergeBenchmarks(from, &to));

  ASSERT_EQ(1U, to.GetConstants().size());
  {
    const Benchmark::Constant& c = to.GetConstants()[0];
    EXPECT_EQ("Foo", c.descriptor.id);
    EXPECT_EQ("Group", c.descriptor.group);
    EXPECT_EQ("Desc", c.descriptor.description);
    EXPECT_EQ("Liters", c.descriptor.units);
    EXPECT_EQ(127.2, c.value);
  }
  ASSERT_EQ(1U, to.GetSampledVariables().size());
  {
    const Benchmark::SampledVariable& v = to.GetSampledVariables()[0];
    EXPECT_EQ("Goo", v.descriptor.id);
    EXPECT_EQ("Group", v.descriptor.group);
    EXPECT_EQ("Desc", v.descriptor.description);
    EXPECT_EQ("Meters/s", v.descriptor.units);
  }
  ASSERT_EQ(1U, to.GetAccumulatedVariables().size());
  {
    const Benchmark::AccumulatedVariable& v = to.GetAccumulatedVariables()[0];
    EXPECT_EQ("Foo", v.descriptor.id);
    EXPECT_EQ("Group", v.descriptor.group);
    EXPECT_EQ("Desc", v.descriptor.description);
    EXPECT_EQ("Liters/s", v.descriptor.units);
    EXPECT_EQ(12U, v.samples);
    EXPECT_EQ(100.0, v.minimum);
    EXPECT_EQ(106.5, v.maximum);
    EXPECT_EQ(102.5, v.mean);
    EXPECT_EQ(1.5, v.standard_deviation);
  }

  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST(BenchmarkUtils, MergeBenchmarksToEmpty) {
  LogChecker log_checker;
  Benchmark from, to;

  from.AddConstant(Benchmark::Constant(
      Benchmark::Descriptor("Foo", "Group", "Desc", "Liters"), 127.2));
  from.AddSampledVariable(Benchmark::SampledVariable(
      Benchmark::Descriptor("Goo", "Group", "Desc", "Meters/s")));
  from.AddAccumulatedVariable(Benchmark::AccumulatedVariable(
      Benchmark::Descriptor("Foo", "Group", "Desc", "Liters/s"),
      12U, 100.0, 106.5, 102.5, 1.5));

  EXPECT_EQ(0U, MergeBenchmarks(from, &to));

  ASSERT_EQ(1U, to.GetConstants().size());
  {
    const Benchmark::Constant& c = to.GetConstants()[0];
    EXPECT_EQ("Foo", c.descriptor.id);
    EXPECT_EQ("Group", c.descriptor.group);
    EXPECT_EQ("Desc", c.descriptor.description);
    EXPECT_EQ("Liters", c.descriptor.units);
    EXPECT_EQ(127.2, c.value);
  }
  ASSERT_EQ(1U, to.GetSampledVariables().size());
  {
    const Benchmark::SampledVariable& v = to.GetSampledVariables()[0];
    EXPECT_EQ("Goo", v.descriptor.id);
    EXPECT_EQ("Group", v.descriptor.group);
    EXPECT_EQ("Desc", v.descriptor.description);
    EXPECT_EQ("Meters/s", v.descriptor.units);
  }
  ASSERT_EQ(1U, to.GetAccumulatedVariables().size());
  {
    const Benchmark::AccumulatedVariable& v = to.GetAccumulatedVariables()[0];
    EXPECT_EQ("Foo", v.descriptor.id);
    EXPECT_EQ("Group", v.descriptor.group);
    EXPECT_EQ("Desc", v.descriptor.description);
    EXPECT_EQ("Liters/s", v.descriptor.units);
    EXPECT_EQ(12U, v.samples);
    EXPECT_EQ(100.0, v.minimum);
    EXPECT_EQ(106.5, v.maximum);
    EXPECT_EQ(102.5, v.mean);
    EXPECT_EQ(1.5, v.standard_deviation);
  }

  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST(BenchmarkUtils, MergeBenchmarksNoConflicts) {
  LogChecker log_checker;
  Benchmark from, to;

  from.AddConstant(Benchmark::Constant(
      Benchmark::Descriptor("FromC0", "Group", "Desc", "Liters"), 127.2));
  from.AddConstant(Benchmark::Constant(
      Benchmark::Descriptor("FromC1", "Group", "Desc", "Liters"), 5.1));
  from.AddSampledVariable(Benchmark::SampledVariable(
      Benchmark::Descriptor("FromSV0", "Group", "Desc", "Meters/s")));
  from.AddSampledVariable(Benchmark::SampledVariable(
      Benchmark::Descriptor("FromSV1", "Group", "Desc", "Meters/s")));
  from.AddAccumulatedVariable(Benchmark::AccumulatedVariable(
      Benchmark::Descriptor("FromAV0", "Group", "Desc", "Liters/s"),
      12U, 100.0, 106.5, 102.5, 1.5));
  from.AddAccumulatedVariable(Benchmark::AccumulatedVariable(
      Benchmark::Descriptor("FromAV1", "Group", "Desc", "Liters/s"),
      14U, 100.0, 106.5, 102.5, 1.5));

  to.AddConstant(Benchmark::Constant(
      Benchmark::Descriptor("ToC0", "Group", "Desc", "Liters"), 127.2));
  to.AddConstant(Benchmark::Constant(
      Benchmark::Descriptor("ToC1", "Group", "Desc", "Liters"), 5.1));
  to.AddSampledVariable(Benchmark::SampledVariable(
      Benchmark::Descriptor("ToSV0", "Group", "Desc", "Meters/s")));
  to.AddSampledVariable(Benchmark::SampledVariable(
      Benchmark::Descriptor("ToSV1", "Group", "Desc", "Meters/s")));
  to.AddAccumulatedVariable(Benchmark::AccumulatedVariable(
      Benchmark::Descriptor("ToAV0", "Group", "Desc", "Liters/s"),
      12U, 100.0, 106.5, 102.5, 1.5));
  to.AddAccumulatedVariable(Benchmark::AccumulatedVariable(
      Benchmark::Descriptor("ToAV1", "Group", "Desc", "Liters/s"),
      14U, 100.0, 106.5, 102.5, 1.5));

  EXPECT_EQ(0U, MergeBenchmarks(from, &to));

  ASSERT_EQ(4U, to.GetConstants().size());
  EXPECT_EQ("ToC0", to.GetConstants()[0].descriptor.id);
  EXPECT_EQ("ToC1", to.GetConstants()[1].descriptor.id);
  EXPECT_EQ("FromC0", to.GetConstants()[2].descriptor.id);
  EXPECT_EQ("FromC1", to.GetConstants()[3].descriptor.id);

  ASSERT_EQ(4U, to.GetSampledVariables().size());
  EXPECT_EQ("ToSV0", to.GetSampledVariables()[0].descriptor.id);
  EXPECT_EQ("ToSV1", to.GetSampledVariables()[1].descriptor.id);
  EXPECT_EQ("FromSV0", to.GetSampledVariables()[2].descriptor.id);
  EXPECT_EQ("FromSV1", to.GetSampledVariables()[3].descriptor.id);

  ASSERT_EQ(4U, to.GetAccumulatedVariables().size());
  EXPECT_EQ("ToAV0", to.GetAccumulatedVariables()[0].descriptor.id);
  EXPECT_EQ("ToAV1", to.GetAccumulatedVariables()[1].descriptor.id);
  EXPECT_EQ("FromAV0", to.GetAccumulatedVariables()[2].descriptor.id);
  EXPECT_EQ("FromAV1", to.GetAccumulatedVariables()[3].descriptor.id);

  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST(BenchmarkUtils, MergeBenchmarksConflicts) {
  LogChecker log_checker;
  Benchmark from, to;

  from.AddConstant(Benchmark::Constant(
      Benchmark::Descriptor("FromC0", "Group", "Desc", "Liters"), 127.2));
  from.AddConstant(Benchmark::Constant(
      Benchmark::Descriptor("CommonC0", "Group", "Desc", "Liters"), 20.0));
  from.AddConstant(Benchmark::Constant(
      Benchmark::Descriptor("FromC1", "Group", "Desc", "Liters"), 5.1));
  from.AddConstant(Benchmark::Constant(
      Benchmark::Descriptor("CommonC1", "Group", "Desc", "Liters"), 21.0));
  from.AddSampledVariable(Benchmark::SampledVariable(
      Benchmark::Descriptor("CommonSV0", "Group", "Desc", "Meters/s")));
  from.AddSampledVariable(Benchmark::SampledVariable(
      Benchmark::Descriptor("FromSV0", "Group", "Desc", "Meters/s")));
  from.AddSampledVariable(Benchmark::SampledVariable(
      Benchmark::Descriptor("FromSV1", "Group", "Desc", "Meters/s")));
  from.AddSampledVariable(Benchmark::SampledVariable(
      Benchmark::Descriptor("CommonSV1", "Group", "Desc", "Meters/s")));
  from.AddAccumulatedVariable(Benchmark::AccumulatedVariable(
      Benchmark::Descriptor("CommonAV0", "Group", "Desc", "Liters/s"),
      100U, 100.0, 106.5, 102.5, 1.5));
  from.AddAccumulatedVariable(Benchmark::AccumulatedVariable(
      Benchmark::Descriptor("FromAV0", "Group", "Desc", "Liters/s"),
      612U, 100.0, 106.5, 102.5, 1.5));
  from.AddAccumulatedVariable(Benchmark::AccumulatedVariable(
      Benchmark::Descriptor("FromAV1", "Group", "Desc", "Liters/s"),
      614U, 100.0, 106.5, 102.5, 1.5));
  from.AddAccumulatedVariable(Benchmark::AccumulatedVariable(
      Benchmark::Descriptor("CommonAV1", "Group", "Desc", "Liters/s"),
      101U, 100.0, 106.5, 102.5, 1.5));

  to.AddConstant(Benchmark::Constant(
      Benchmark::Descriptor("CommonC1", "Group", "Desc", "Liters"), 50.0));
  to.AddConstant(Benchmark::Constant(
      Benchmark::Descriptor("CommonC0", "Group", "Desc", "Liters"), 51.0));
  to.AddConstant(Benchmark::Constant(
      Benchmark::Descriptor("ToC0", "Group", "Desc", "Liters"), 127.2));
  to.AddConstant(Benchmark::Constant(
      Benchmark::Descriptor("ToC1", "Group", "Desc", "Liters"), 5.1));
  to.AddSampledVariable(Benchmark::SampledVariable(
      Benchmark::Descriptor("ToSV0", "Group", "Desc", "Meters/s")));
  to.AddSampledVariable(Benchmark::SampledVariable(
      Benchmark::Descriptor("CommonSV0", "Group", "Desc", "Meters/s")));
  to.AddSampledVariable(Benchmark::SampledVariable(
      Benchmark::Descriptor("ToSV1", "Group", "Desc", "Meters/s")));
  to.AddSampledVariable(Benchmark::SampledVariable(
      Benchmark::Descriptor("CommonSV1", "Group", "Desc", "Meters/s")));
  to.AddAccumulatedVariable(Benchmark::AccumulatedVariable(
      Benchmark::Descriptor("ToAV0", "Group", "Desc", "Liters/s"),
      12U, 100.0, 106.5, 102.5, 1.5));
  to.AddAccumulatedVariable(Benchmark::AccumulatedVariable(
      Benchmark::Descriptor("CommonAV0", "Group", "Desc", "Liters/s"),
      1000U, 100.0, 106.5, 102.5, 1.5));
  to.AddAccumulatedVariable(Benchmark::AccumulatedVariable(
      Benchmark::Descriptor("ToAV1", "Group", "Desc", "Liters/s"),
      14U, 100.0, 106.5, 102.5, 1.5));
  to.AddAccumulatedVariable(Benchmark::AccumulatedVariable(
      Benchmark::Descriptor("CommonAV1", "Group", "Desc", "Liters/s"),
      1001U, 100.0, 106.5, 102.5, 1.5));

  // Should be 2 constant conflicts and 2 variable conflicts.
  EXPECT_EQ(6U, MergeBenchmarks(from, &to));

  ASSERT_EQ(6U, to.GetConstants().size());
  EXPECT_EQ("CommonC1", to.GetConstants()[0].descriptor.id);
  EXPECT_EQ("CommonC0", to.GetConstants()[1].descriptor.id);
  EXPECT_EQ("ToC0", to.GetConstants()[2].descriptor.id);
  EXPECT_EQ("ToC1", to.GetConstants()[3].descriptor.id);
  EXPECT_EQ("FromC0", to.GetConstants()[4].descriptor.id);
  EXPECT_EQ("FromC1", to.GetConstants()[5].descriptor.id);

  ASSERT_EQ(6U, to.GetSampledVariables().size());
  EXPECT_EQ("ToSV0", to.GetSampledVariables()[0].descriptor.id);
  EXPECT_EQ("CommonSV0", to.GetSampledVariables()[1].descriptor.id);
  EXPECT_EQ("ToSV1", to.GetSampledVariables()[2].descriptor.id);
  EXPECT_EQ("CommonSV1", to.GetSampledVariables()[3].descriptor.id);
  EXPECT_EQ("FromSV0", to.GetSampledVariables()[4].descriptor.id);
  EXPECT_EQ("FromSV1", to.GetSampledVariables()[5].descriptor.id);

  ASSERT_EQ(6U, to.GetAccumulatedVariables().size());
  EXPECT_EQ("ToAV0", to.GetAccumulatedVariables()[0].descriptor.id);
  EXPECT_EQ("CommonAV0", to.GetAccumulatedVariables()[1].descriptor.id);
  EXPECT_EQ("ToAV1", to.GetAccumulatedVariables()[2].descriptor.id);
  EXPECT_EQ("CommonAV1", to.GetAccumulatedVariables()[3].descriptor.id);
  EXPECT_EQ("FromAV0", to.GetAccumulatedVariables()[4].descriptor.id);
  EXPECT_EQ("FromAV1", to.GetAccumulatedVariables()[5].descriptor.id);

  // Verify that values were not overwritten.
  EXPECT_EQ(12U, to.GetAccumulatedVariables()[0].samples);
  EXPECT_EQ(1000U, to.GetAccumulatedVariables()[1].samples);
  EXPECT_EQ(14U, to.GetAccumulatedVariables()[2].samples);
  EXPECT_EQ(1001U, to.GetAccumulatedVariables()[3].samples);
  EXPECT_EQ(612U, to.GetAccumulatedVariables()[4].samples);
  EXPECT_EQ(614U, to.GetAccumulatedVariables()[5].samples);

#if !ION_PRODUCTION
  const std::vector<std::string> errors = log_checker.GetAllMessages();
  EXPECT_EQ(6U, errors.size());
  EXPECT_EQ(0U, errors[0].find("ERROR"));
  EXPECT_EQ(0U, errors[1].find("ERROR"));
  EXPECT_EQ(0U, errors[2].find("ERROR"));
  EXPECT_EQ(0U, errors[3].find("ERROR"));
  EXPECT_EQ(0U, errors[4].find("ERROR"));
  EXPECT_EQ(0U, errors[5].find("ERROR"));
  EXPECT_FALSE(errors[0].find("Conflicting Constant") == std::string::npos);
  EXPECT_FALSE(errors[1].find("Conflicting Constant") == std::string::npos);
  EXPECT_FALSE(
      errors[2].find("Conflicting SampledVariable") == std::string::npos);
  EXPECT_FALSE(
      errors[3].find("Conflicting SampledVariable") == std::string::npos);
  EXPECT_FALSE(
      errors[4].find("Conflicting AccumulatedVariable") == std::string::npos);
  EXPECT_FALSE(
      errors[5].find("Conflicting AccumulatedVariable") == std::string::npos);
#endif
}

TEST(BenchmarkUtils, OutputBenchmarkAsCsv) {
  // Set up some SampledVariable samples to test accumulation. Time values do
  // not matter.
  Benchmark::SampledVariable sv1(
      Benchmark::Descriptor("SVar1", "Group1", "SVDesc1", "Units1"));
  sv1.samples.push_back(Benchmark::Sample(10, 10.0));
  sv1.samples.push_back(Benchmark::Sample(20, 20.0));
  sv1.samples.push_back(Benchmark::Sample(30, 30.0));
  Benchmark::SampledVariable sv2(
      Benchmark::Descriptor("SVar2", "Group2", "SVDesc2", "Units2"));
  sv2.samples.push_back(Benchmark::Sample(40, 100.0));
  sv2.samples.push_back(Benchmark::Sample(50, 500.0));
  sv2.samples.push_back(Benchmark::Sample(60, 900.0));

  Benchmark b;
  b.AddConstant(Benchmark::Constant(
      Benchmark::Descriptor("Const1", "Group1", "CDesc1", "Units1"), 1.0));
  b.AddConstant(Benchmark::Constant(
      Benchmark::Descriptor("Const2", "Group2", "cDesc2", "Units2"), 2.0));
  b.AddSampledVariable(sv1);
  b.AddSampledVariable(sv2);
  b.AddAccumulatedVariable(Benchmark::AccumulatedVariable(
      Benchmark::Descriptor("AVar1", "Group1", "AVDesc1", "Units1"),
      4U, 99.0, 101.0, 100.0, 2.0));
  b.AddAccumulatedVariable(Benchmark::AccumulatedVariable(
      Benchmark::Descriptor("AVar2", "Group2", "AVDesc2", "Units2"),
      10U, 999.0, 1001.0, 1000.0, 2.0));
  // Special case: maximum == minimum.
  b.AddAccumulatedVariable(Benchmark::AccumulatedVariable(
      Benchmark::Descriptor("AVar3", "Group3", "AVDesc3", "Units3"),
      18U, 2000.0, 2000.0, 2000.0, 0.0));

  std::ostringstream s;
  OutputBenchmarkAsCsv(b, s);

  const std::string kExpected =
      "Entry ID, Description, Group, Average, Units, Minimum, Maximum,"
      " Standard Deviation, Relative Deviation %\n"
      "Const1,CDesc1,Group1,1,Units1,,,,\n"
      "Const2,cDesc2,Group2,2,Units2,,,,\n"
      "SVar1,SVDesc1,Group1,20,Units1,10,30,10,50\n"
      "SVar2,SVDesc2,Group2,500,Units2,100,900,400,80\n"
      "AVar1,AVDesc1,Group1,100,Units1,99,101,2,2\n"
      "AVar2,AVDesc2,Group2,1000,Units2,999,1001,2,0.2\n"
      "AVar3,AVDesc3,Group3,2000,Units3,,,,\n";
  EXPECT_TRUE(ion::base::testing::MultiLineStringsEqual(kExpected, s.str()));
}

TEST(BenchmarkUtils, OutputBenchmarkAsJson) {
  // Set up some SampledVariable samples to test accumulation. Time values do
  // not matter.
  Benchmark::SampledVariable sv1(
      Benchmark::Descriptor("SVar1", "Group1", "SVDesc1", "Units1"));
  sv1.samples.push_back(Benchmark::Sample(10, 10.0));
  sv1.samples.push_back(Benchmark::Sample(20, 20.0));
  sv1.samples.push_back(Benchmark::Sample(30, 30.0));
  Benchmark::SampledVariable sv2(
      Benchmark::Descriptor("SVar2", "Group2", "SVDesc2", "Units2"));
  sv2.samples.push_back(Benchmark::Sample(40, 100.0));
  sv2.samples.push_back(Benchmark::Sample(50, 500.0));
  sv2.samples.push_back(Benchmark::Sample(60, 900.0));

  Benchmark b;
  b.AddConstant(Benchmark::Constant(
      Benchmark::Descriptor("Const1", "Group1", "CDesc1", "Units1"), 1.0));
  b.AddConstant(Benchmark::Constant(
      Benchmark::Descriptor("Const2", "Group2", "cDesc2", "Units2"), 2.0));
  b.AddSampledVariable(sv1);
  b.AddSampledVariable(sv2);
  b.AddAccumulatedVariable(Benchmark::AccumulatedVariable(
      Benchmark::Descriptor("AVar1", "Group1", "AVDesc1", "Units1"),
      4U, 99.0, 101.0, 100.0, 2.0));
  b.AddAccumulatedVariable(Benchmark::AccumulatedVariable(
      Benchmark::Descriptor("AVar2", "Group2", "AVDesc2", "Units2"),
      10U, 999.0, 1001.0, 1000.0, 2.0));
  // Special case: maximum == minimum.
  b.AddAccumulatedVariable(Benchmark::AccumulatedVariable(
      Benchmark::Descriptor("AVar3", "Group3", "AVDesc3", "Units3"),
      18U, 2000.0, 2000.0, 2000.0, 0.0));
  b.AddAccumulatedVariable(Benchmark::AccumulatedVariable(
      Benchmark::Descriptor("AVar4", "Group4", "AVDesc4", "Units4"), 18U,
      4000.0, 4000.0, 4000.0, std::numeric_limits<double>::infinity()));
  std::ostringstream s;
  OutputBenchmarkAsJson(b, "  ", s);

  const std::string kExpected =
      "  {\n"
      "    \"constants\": [\n"
      "      {\n"
      "        \"id\": \"Const1\",\n"
      "        \"description\": \"CDesc1\",\n"
      "        \"group\": \"Group1\",\n"
      "        \"value\": 1,\n"
      "        \"units\": \"Units1\"\n"
      "      },\n"
      "      {\n"
      "        \"id\": \"Const2\",\n"
      "        \"description\": \"cDesc2\",\n"
      "        \"group\": \"Group2\",\n"
      "        \"value\": 2,\n"
      "        \"units\": \"Units2\"\n"
      "      }\n"
      "    ],\n"
      "    \"sampled_variables\": [\n"
      "      {\n"
      "        \"id\": \"SVar1\",\n"
      "        \"description\": \"SVDesc1\",\n"
      "        \"group\": \"Group1\",\n"
      "        \"mean\": 20,\n"
      "        \"units\": \"Units1\",\n"
      "        \"minimum\": 10,\n"
      "        \"maximum\": 30,\n"
      "        \"standard_deviation\": 10,\n"
      "        \"variation\": 50\n"
      "      },\n"
      "      {\n"
      "        \"id\": \"SVar2\",\n"
      "        \"description\": \"SVDesc2\",\n"
      "        \"group\": \"Group2\",\n"
      "        \"mean\": 500,\n"
      "        \"units\": \"Units2\",\n"
      "        \"minimum\": 100,\n"
      "        \"maximum\": 900,\n"
      "        \"standard_deviation\": 400,\n"
      "        \"variation\": 80\n"
      "      }\n"
      "    ],\n"
      "    \"accumulated_variables\": [\n"
      "      {\n"
      "        \"id\": \"AVar1\",\n"
      "        \"description\": \"AVDesc1\",\n"
      "        \"group\": \"Group1\",\n"
      "        \"mean\": 100,\n"
      "        \"units\": \"Units1\",\n"
      "        \"minimum\": 99,\n"
      "        \"maximum\": 101,\n"
      "        \"standard_deviation\": 2,\n"
      "        \"variation\": 2\n"
      "      },\n"
      "      {\n"
      "        \"id\": \"AVar2\",\n"
      "        \"description\": \"AVDesc2\",\n"
      "        \"group\": \"Group2\",\n"
      "        \"mean\": 1000,\n"
      "        \"units\": \"Units2\",\n"
      "        \"minimum\": 999,\n"
      "        \"maximum\": 1001,\n"
      "        \"standard_deviation\": 2,\n"
      "        \"variation\": 0.2\n"
      "      },\n"
      "      {\n"
      "        \"id\": \"AVar3\",\n"
      "        \"description\": \"AVDesc3\",\n"
      "        \"group\": \"Group3\",\n"
      "        \"mean\": 2000,\n"
      "        \"units\": \"Units3\"\n"
      "      },\n"
      "      {\n"
      "        \"id\": \"AVar4\",\n"
      "        \"description\": \"AVDesc4\",\n"
      "        \"group\": \"Group4\",\n"
      "        \"mean\": 4000,\n"
      "        \"units\": \"Units4\"\n"
      "      }\n"
      "    ]\n"
      "  }\n";
  EXPECT_TRUE(ion::base::testing::MultiLineStringsEqual(kExpected, s.str()));
}

TEST(BenchmarkUtils, OutputBenchmarkPretty) {
  Benchmark::SampledVariable sv1(
      Benchmark::Descriptor("SVar1", "Group1", "SVDesc1", "Units1"));
  sv1.samples.push_back(Benchmark::Sample(10, 10.0));
  sv1.samples.push_back(Benchmark::Sample(20, 20.0));
  sv1.samples.push_back(Benchmark::Sample(30, 30.0));

  Benchmark b;
  b.AddConstant(Benchmark::Constant(
      Benchmark::Descriptor("Const1", "Group1", "CDesc1", "Units1"), 1.0));
  b.AddConstant(Benchmark::Constant(
      Benchmark::Descriptor("Const2", "Group2", "CDesc2", "Units2"), 2.0));
  b.AddSampledVariable(sv1);
  b.AddAccumulatedVariable(Benchmark::AccumulatedVariable(
      Benchmark::Descriptor("AVar1", "Group1", "AVDesc1", "Units1"),
      4U, 99.0, 101.0, 100.0, 2.0));
  b.AddAccumulatedVariable(Benchmark::AccumulatedVariable(
      Benchmark::Descriptor("AVar2", "Group2", "AVDesc2", "Units2"),
      10U, 999.0, 1001.0, 1000.0, 2.0));
  // Special case: maximum == minimum.
  b.AddAccumulatedVariable(Benchmark::AccumulatedVariable(
      Benchmark::Descriptor("AVar3", "Group3", "AVDesc3", "Units3"),
      18U, 2000.0, 2000.0, 2000.0, 0.0));

  std::ostringstream s;
  OutputBenchmarkPretty("Sample Header", true, b, s);

  const std::string kExpected =
      "--------------------------------------------------------------------"
      "-----------\n"
      "Benchmark report for \"Sample Header\"\n"
      "\n"
      " [Constant] Const1: CDesc1 (Units1)\n"
      " [Constant] Const2: CDesc2 (Units2)\n"
      " [Variable]  SVar1: SVDesc1 (Units1)\n"
      " [Variable]  AVar1: AVDesc1 (Units1)\n"
      " [Variable]  AVar2: AVDesc2 (Units2)\n"
      " [Variable]  AVar3: AVDesc3 (Units3)\n"
      "--------------------------------------------------------------------"
      "-----------\n"
      "    ID        MEAN  UNITS     MINIMUM     MAXIMUM  REL STDDEV\n"
      "Const1           1 Units1\n"
      "Const2           2 Units2\n"
      " SVar1          20 Units1          10          30        50 %\n"
      " AVar1         100 Units1          99         101         2 %\n"
      " AVar2        1000 Units2         999        1001       0.2 %\n"
      " AVar3        2000 Units3        2000        2000         0 %\n"
      "--------------------------------------------------------------------"
      "-----------\n"
      "\n";
  EXPECT_TRUE(ion::base::testing::MultiLineStringsEqual(kExpected, s.str()));

  std::ostringstream s_no_descriptions;
  OutputBenchmarkPretty("Sample Header 2", false, b, s_no_descriptions);

  const std::string kExpectedNoDescriptions =
      "--------------------------------------------------------------------"
      "-----------\n"
      "Benchmark report for \"Sample Header 2\"\n"
      "\n"
      "    ID        MEAN  UNITS     MINIMUM     MAXIMUM  REL STDDEV\n"
      "Const1           1 Units1\n"
      "Const2           2 Units2\n"
      " SVar1          20 Units1          10          30        50 %\n"
      " AVar1         100 Units1          99         101         2 %\n"
      " AVar2        1000 Units2         999        1001       0.2 %\n"
      " AVar3        2000 Units3        2000        2000         0 %\n"
      "--------------------------------------------------------------------"
      "-----------\n"
      "\n";
  EXPECT_TRUE(ion::base::testing::MultiLineStringsEqual(
      kExpectedNoDescriptions, s_no_descriptions.str()));
}
