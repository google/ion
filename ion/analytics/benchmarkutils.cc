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

#include <algorithm>
#include <iomanip>
#include <set>

#include "ion/base/logging.h"
#include "ion/math/utils.h"
#include "ion/port/override/base/port.h"

namespace ion {
namespace analytics {

//-----------------------------------------------------------------------------
//
// Helper types and functions.
//
//-----------------------------------------------------------------------------

namespace {

// Field widths and precisions for pretty output.
static const int kValueWidth = 12;
static const int kValuePrecision = 6;
static const double kTolerance(1e-6);

// This type is used to output a string with a given width with operator<<().
struct Str {
  Str(const std::string& str_in, int width_in) : str(str_in), width(width_in) {}
  const std::string& str;
  int width;
};
std::ostream& operator<<(std::ostream& out, const Str& s) {
  return out << std::setw(s.width) << s.str;
}

// This type is used to output a double with a given width and precision with
// operator<<().
struct Double {
  Double(double value_in, int width_in, int precision_in)
  : value(value_in),
    width(width_in),
    precision(precision_in) {}
  double value;
  int width;
  int precision;
};
std::ostream& operator<<(std::ostream& out, const Double& d) {
  return out << std::right << std::setw(d.width)
             << std::setprecision(d.precision) << d.value;
}

// Returns a set containing IDs of all items (Constant, SampledVariable, or
// AccumulatedVariable) from a vector in a Benchmark.
template <typename T>
static const std::set<std::string> GetBenchmarkIdSet(
    const std::vector<T>& items) {
  std::set<std::string> ids;
  const size_t count = items.size();
  for (size_t i = 0; i < count; ++i)
    ids.insert(items[i].descriptor.id);
  return ids;
}

// Specialized function to add an item when merging.
template <typename T> static void AddItem(const T& item, Benchmark* b) {
  DCHECK(false) << "Unspecialized AddItem() called";
}
template <> inline void AddItem(const Benchmark::Constant& item, Benchmark* b) {
  b->AddConstant(item);
}
template <> inline void AddItem(const Benchmark::SampledVariable& item,
                                Benchmark* b) {
  b->AddSampledVariable(item);
}
template <> inline void AddItem(const Benchmark::AccumulatedVariable& item,
                                Benchmark* b) {
  b->AddAccumulatedVariable(item);
}

// Merges constants or variables from one Benchmark to another. Returns the
// number of conflicts (items found in both from_items and the "to" Benchmark).
template <typename T>
static size_t MergeBenchmarkItems(const std::string& item_type,
                                  const std::vector<T>& from_items,
                                  const std::set<std::string>& ids,
                                  Benchmark* to) {
  size_t num_conflicts = 0;
  const size_t count = from_items.size();
  for (size_t i = 0; i < count; ++i) {
    const std::string& id = from_items[i].descriptor.id;
    if (ids.count(id)) {
      LOG(ERROR) << "Conflicting " << item_type << " \"" << id
                 << "\" found while merging benchmarks";
      ++num_conflicts;
    } else {
      AddItem(from_items[i], to);
    }
  }
  return num_conflicts;
}

// Outputs a Constant as CSV.
static void OutputConstantAsCsv(
    const Benchmark::Constant& c, std::ostream& out) {  // NOLINT
  out << c.descriptor.id << ","
      << c.descriptor.description << ","
      << c.descriptor.group << ","
      << c.value << ","
      << c.descriptor.units << ","
      << ",,,"  // No minimum, maximum, or standard deviation.
      << std::endl;
}

// Outputs an AccumulatedVariable as CSV.
static void OutputAccumulatedVariableAsCsv(
    const Benchmark::AccumulatedVariable& v, std::ostream& out) {  // NOLINT
  out << v.descriptor.id << ","
      << v.descriptor.description << ","
      << v.descriptor.group << ","
      << v.mean << ","
      << v.descriptor.units << ",";

  // Output min/max only if they differ.
  if (v.minimum != v.maximum) {
    out << v.minimum << "," << v.maximum << ",";
  } else {
    out << ",,";
  }

  // Output standard deviation only if it is not zero.
  if (v.standard_deviation != 0.0) {
    out << v.standard_deviation << ","
        << (100.0 * v.standard_deviation / v.mean);
  } else {
    out << ",";
  }
  out << std::endl;
}

// Outputs a Descriptor key for pretty format.
static void OutputKey(const std::string& type,
                      const Benchmark::Descriptor& descriptor,
                      int id_width,
                      std::ostream& out) {  // NOLINT
  out << " [" << type << "] " << std::right << Str(descriptor.id, id_width)
      << ": " << descriptor.description;
  if (!descriptor.units.empty())
    out << " (" << descriptor.units << ")";
  out << "\n";
}

// Outputs a Constant in pretty format.
static void OutputConstantPretty(
    const Benchmark::Constant& c,
    int id_width,
    int units_width,
    std::ostream& out) {  // NOLINT
  out << Str(c.descriptor.id, id_width)
      << Double(c.value, kValueWidth, kValuePrecision)
      << Str(c.descriptor.units, units_width)
      << "\n";
}

// Outputs an AccumulatedVariable in pretty format.
static void OutputAccumulatedVariablePretty(
    const Benchmark::AccumulatedVariable& v,
    int id_width,
    int units_width,
    std::ostream& out) {  // NOLINT
  const double rel_stddev =
      v.mean == 0.0 ? 0.0 : 100.0 * v.standard_deviation / v.mean;
  out << Str(v.descriptor.id, id_width)
      << Double(v.mean, kValueWidth, kValuePrecision)
      << Str(v.descriptor.units, units_width)
      << Double(v.minimum, kValueWidth, kValuePrecision)
      << Double(v.maximum, kValueWidth, kValuePrecision)
      << Double(rel_stddev, kValueWidth - 2, 4) << " %"
      << "\n";
}

}  // anonymous namespace

//-----------------------------------------------------------------------------
//
// Public functions.
//
//-----------------------------------------------------------------------------

size_t MergeBenchmarks(const Benchmark& from, Benchmark* to) {
  if (!to)
    return 0;

  // Store Constant and Variable IDs in sets to detect conflicts.
  const std::set<std::string> constant_ids =
      GetBenchmarkIdSet(to->GetConstants());
  const std::set<std::string> sampled_variable_ids =
      GetBenchmarkIdSet(to->GetSampledVariables());
  const std::set<std::string> accumulated_variable_ids =
      GetBenchmarkIdSet(to->GetAccumulatedVariables());

  // Merge the items. The code is structured this way to guarantee that
  // constants are processed before variables.
  size_t num_conflicts =
      MergeBenchmarkItems("Constant", from.GetConstants(), constant_ids, to);
  num_conflicts += MergeBenchmarkItems(
      "SampledVariable", from.GetSampledVariables(), sampled_variable_ids, to);
  num_conflicts += MergeBenchmarkItems(
      "AccumulatedVariable", from.GetAccumulatedVariables(),
      accumulated_variable_ids, to);
  return num_conflicts;
}

void OutputBenchmarkAsCsv(const Benchmark& benchmark,
                          std::ostream& out) {  // NOLINT
  // Header.
  out << "Entry ID, Description, Group, Average, Units, Minimum, Maximum, "
      << "Standard Deviation, Relative Deviation %\n";

  // Constants.
  const std::vector<Benchmark::Constant>& constants = benchmark.GetConstants();
  const size_t num_constants = constants.size();
  for (size_t i = 0; i < num_constants; ++i)
    OutputConstantAsCsv(constants[i], out);

  // SampledVariables are converted to AccumulatedVariables.
  const std::vector<Benchmark::SampledVariable>& sampled_variables =
      benchmark.GetSampledVariables();
  const size_t num_sampled_variables = sampled_variables.size();
  for (size_t i = 0; i < num_sampled_variables; ++i)
    OutputAccumulatedVariableAsCsv(
        Benchmark::AccumulateSampledVariable(sampled_variables[i]), out);

  // AccumulatedVariables.
  const std::vector<Benchmark::AccumulatedVariable>& accumulated_variables =
      benchmark.GetAccumulatedVariables();
  const size_t num_accumulated_variables = accumulated_variables.size();
  for (size_t i = 0; i < num_accumulated_variables; ++i)
    OutputAccumulatedVariableAsCsv(accumulated_variables[i], out);
}

void OutputConstantAsJson(const Benchmark::Constant& c,
                          const std::string& indent,
                          std::ostream& out) {  // NOLINT
  out << indent << "{" << std::endl;
  out << indent << "  \"id\": \"" << c.descriptor.id << "\"," << std::endl
      << indent << "  \"description\": \"" << c.descriptor.description << "\","
      << std::endl
      << indent << "  \"group\": \"" << c.descriptor.group << "\"," << std::endl
      << indent << "  \"value\": " << c.value << "," << std::endl
      << indent << "  \"units\": \"" << c.descriptor.units << "\"" << std::endl;
  out << indent << "}";
}

void OutputAccumulatedVariableAsJson(const Benchmark::AccumulatedVariable& v,
                                     const std::string& indent,
                                     std::ostream& out) {  // NOLINT
  out << indent << "{" << std::endl;
  out << indent << "  \"id\": \"" << v.descriptor.id << "\"," << std::endl
      << indent << "  \"description\": \"" << v.descriptor.description << "\","
      << std::endl
      << indent << "  \"group\": \"" << v.descriptor.group << "\"," << std::endl
      << indent << "  \"mean\": " << v.mean << "," << std::endl
      << indent << "  \"units\": \"" << v.descriptor.units << "\"";

  // Output min/max only if they differ.
  if (v.minimum != v.maximum) {
    out << "," << std::endl;
    out << indent << "  \"minimum\": " << v.minimum << "," << std::endl
        << indent << "  \"maximum\": " << v.maximum;
  }

  // Output standard deviation and variation only if they are not zero,
  // infinite, or NaN.
  if (!std::isnan(v.standard_deviation) && !std::isinf(v.standard_deviation) &&
      math::Abs(v.standard_deviation) > kTolerance &&
      math::Abs(v.mean) > kTolerance) {
    out << "," << std::endl;
    out << indent << "  \"standard_deviation\": " << v.standard_deviation << ","
        << std::endl << indent
        << "  \"variation\": " << (100.0 * v.standard_deviation / v.mean);
  }
  out << std::endl << indent << "}";
}

void OutputBenchmarkAsJson(const Benchmark& benchmark,
                           const std::string& indent_in,
                           std::ostream& out) {  // NOLINT
  const std::string indent = indent_in + std::string("    ");
  out << indent_in << "{" << std::endl;

  // Constants.
  const std::vector<Benchmark::Constant>& constants = benchmark.GetConstants();
  const size_t num_constants = constants.size();
  if (num_constants) {
    out << indent_in << "  \"constants\": [" << std::endl;
    for (size_t i = 0; i < num_constants; ++i) {
      OutputConstantAsJson(constants[i], indent, out);
      if (i < num_constants - 1)
        out << "," << std::endl;
    }
    out << std::endl << indent_in << "  ]";
  }

  // SampledVariables are converted to AccumulatedVariables.
  const std::vector<Benchmark::SampledVariable>& sampled_variables =
      benchmark.GetSampledVariables();
  const size_t num_sampled_variables = sampled_variables.size();
  if (num_sampled_variables) {
    if (num_constants)
      out << "," << std::endl;
    out << indent_in << "  \"sampled_variables\": [" << std::endl;
    for (size_t i = 0; i < num_sampled_variables; ++i) {
      OutputAccumulatedVariableAsJson(
          Benchmark::AccumulateSampledVariable(sampled_variables[i]), indent,
          out);
      if (i < num_sampled_variables - 1)
        out << "," << std::endl;
    }
    out << std::endl << indent_in << "  ]";
  }

  // AccumulatedVariables.
  const std::vector<Benchmark::AccumulatedVariable>& accumulated_variables =
      benchmark.GetAccumulatedVariables();
  const size_t num_accumulated_variables = accumulated_variables.size();
  if (num_accumulated_variables) {
    if (num_constants || num_sampled_variables)
      out << "," << std::endl;
    out << indent_in << "  \"accumulated_variables\": [" << std::endl;
    for (size_t i = 0; i < num_accumulated_variables; ++i) {
      OutputAccumulatedVariableAsJson(accumulated_variables[i], indent, out);
      if (i < num_accumulated_variables - 1)
        out << "," << std::endl;
    }
    out << std::endl << indent_in << "  ]";
  }

  out << std::endl << indent_in << "}" << std::endl;
}

// Outputs benchmark results in pretty format.
void OutputBenchmarkPretty(const std::string& id_string,
                           bool print_descriptions,
                           const Benchmark& benchmark,
                           std::ostream& out) {  // NOLINT
  static const char kSeparator[] = "----------------------------------------"
                                   "---------------------------------------\n";

  // Header.
  out << kSeparator << "Benchmark report for \"" << id_string << "\"\n\n";

  const std::vector<Benchmark::Constant>& constants = benchmark.GetConstants();
  const std::vector<Benchmark::SampledVariable>& sampled_variables =
      benchmark.GetSampledVariables();
  const std::vector<Benchmark::AccumulatedVariable>& accumulated_variables =
      benchmark.GetAccumulatedVariables();
  const size_t num_constants = constants.size();
  const size_t num_sampled_variables = sampled_variables.size();
  const size_t num_accumulated_variables = accumulated_variables.size();

  // Compute ID width.
  static const int kMinIdWidth = 2;  // width of "ID"
  size_t id_width_size_t = kMinIdWidth;
  for (size_t i = 0; i < num_constants; ++i)
    id_width_size_t = std::max(id_width_size_t,
                               constants[i].descriptor.id.length());
  for (size_t i = 0; i < num_sampled_variables; ++i)
    id_width_size_t = std::max(id_width_size_t,
                               sampled_variables[i].descriptor.id.length());
  for (size_t i = 0; i < num_accumulated_variables; ++i)
    id_width_size_t = std::max(id_width_size_t,
                               accumulated_variables[i].descriptor.id.length());
  const int id_width = static_cast<int>(id_width_size_t);

  // Compute units width.
  static const int kMinUnitsWidth = 5;  // width of "UNITS"
  size_t units_width_size_t = kMinUnitsWidth;
  for (size_t i = 0; i < num_constants; ++i)
    units_width_size_t = std::max(units_width_size_t,
                                 constants[i].descriptor.units.length());
  for (size_t i = 0; i < num_sampled_variables; ++i)
    units_width_size_t = std::max(
        units_width_size_t, sampled_variables[i].descriptor.units.length());
  for (size_t i = 0; i < num_accumulated_variables; ++i)
    units_width_size_t = std::max(
        units_width_size_t, accumulated_variables[i].descriptor.units.length());
  ++units_width_size_t;  // Add one preceding space.
  const int units_width = static_cast<int>(units_width_size_t);

  // Item keys.
  if (print_descriptions) {
    for (size_t i = 0; i < num_constants; ++i)
      OutputKey("Constant", constants[i].descriptor, id_width, out);
    for (size_t i = 0; i < num_sampled_variables; ++i)
      OutputKey("Variable", sampled_variables[i].descriptor, id_width, out);
    for (size_t i = 0; i < num_accumulated_variables; ++i)
      OutputKey("Variable", accumulated_variables[i].descriptor, id_width, out);

    out << kSeparator;
  }

  // Header for values.
  out << Str("ID", id_width)
      << Str("MEAN", kValueWidth)
      << Str("UNITS", units_width)
      << Str("MINIMUM", kValueWidth)
      << Str("MAXIMUM", kValueWidth)
      << Str("REL STDDEV", kValueWidth)
      << "\n";

  // Constant values.
  for (size_t i = 0; i < num_constants; ++i)
    OutputConstantPretty(constants[i], id_width, units_width, out);

  // SampledVariables are converted to AccumulatedVariables.
  for (size_t i = 0; i < num_sampled_variables; ++i) {
    OutputAccumulatedVariablePretty(Benchmark::AccumulateSampledVariable(
        sampled_variables[i]), id_width, units_width, out);
  }

  // AccumulatedVariables.
  for (size_t i = 0; i < num_accumulated_variables; ++i)
    OutputAccumulatedVariablePretty(
        accumulated_variables[i], id_width, units_width, out);

  out << kSeparator << std::endl;
}

}  // namespace analytics
}  // namespace ion
