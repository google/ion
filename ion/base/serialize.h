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

#ifndef ION_BASE_SERIALIZE_H_
#define ION_BASE_SERIALIZE_H_

#include <chrono> // NOLINT
#include <sstream>
#include <string>
#include <utility>

#include "ion/base/stringutils.h"

namespace ion {
namespace base {

// This file defines two public functions: StringToValue() and ValueToString().
// These functions serialize data types to std::strings and from
// std::istringstreams, and also support most STL containers. Serializing custom
// types requires defining only the insertion and extraction operators (<< and
// >>)

//-----------------------------------------------------------------------------
//
// StringToValue
//
//-----------------------------------------------------------------------------

template <typename T>
inline bool StringToValue(std::istringstream& in, T* val) {  // NOLINT
  T value;
  in >> value;
  if (!in.fail()) {
    *val = value;
    return true;
  } else {
    return false;
  }
}

// Specialize for bools.
template <>
inline bool StringToValue(std::istringstream& in, bool* val) {  // NOLINT
  std::string value;
  in >> value;
  if (!in.fail() && (value == "true" || value == "false")) {
    *val = value == "true";
    return true;
  } else {
    return false;
  }
}

// Overload for reading double quoted strings.
template <typename T, typename U, typename V>
inline bool StringToValue(std::istringstream& in,  // NOLINT
                          std::basic_string<T, U, V>* val) {
  std::string v;
  bool done = false;
  bool escaping = false;
  if (GetExpectedChar<'"'>(in)) {
    char c = 0;
    do {
      // Use get() to be able to read whitespace.
      c = static_cast<char>(in.get());
      done = !escaping && c == '"';
      escaping = !escaping && c == '\\';
      if (!done && !escaping)
        v.push_back(c);
    } while (!in.fail() && !done);
  }
  if (!in.fail() && done) {
    *val = v;
    return true;
  } else {
    return false;
  }
}

// Overload for std::pair types.
template <typename T, typename U>
inline bool StringToValue(std::istringstream& in,  // NOLINT
                          std::pair<const T, U>* val) {
  T key;
  U value;
  if (StringToValue(in, &key) && GetExpectedChar<':'>(in) &&
      StringToValue(in, &value)) {
    // This is necessary since std::map uses const keys.
    *const_cast<T*>(&val->first) = key;
    val->second = value;
    return true;
  } else {
    return false;
  }
}

// Overload for std::chrono::duration types.
template <typename R, typename P>
inline bool StringToValue(std::istringstream& in,
                          std::chrono::duration<R, P>* val) {
  using std::chrono::duration;
  using std::chrono::duration_cast;
  using OutputDuration = duration<R, P>;

  // First read the count of ticks of type R.
  R rep;
  if (!StringToValue(in, &rep)) {
    return false;
  }

  // Now read the input period and convert |rep| to the output period P.
  std::string period_identifier;
  in >> period_identifier;
  if (period_identifier == "ns") {
    *val = duration_cast<OutputDuration>(duration<R, std::nano>(rep));
  } else if (period_identifier == "us") {
    *val = duration_cast<OutputDuration>(duration<R, std::micro>(rep));
  } else if (period_identifier == "ms") {
    *val = duration_cast<OutputDuration>(duration<R, std::milli>(rep));
  } else if (period_identifier == "s") {
    *val = duration_cast<OutputDuration>(duration<R>(rep));
  } else {
    return false;
  }
  return true;
}

// Constructs a STL container from a stream. If any errors occur then the
// stream's failure bit is set and val is not modified.
template <typename ContainerType>
inline bool StringToStlContainer(std::istringstream& in,  // NOLINT
                                 ContainerType* val) {
  if (GetExpectedChar<'{'>(in)) {
    ContainerType v;
    // Check for a space or closing brace.
    while (!in.fail()) {
      typename ContainerType::value_type value;
      if (StringToValue(in, &value)) {
        v.insert(v.end(), value);
      } else {
        in.setstate(std::ios_base::failbit);
        break;
      }
      // Check if this is the end of the container or the comma between
      // elements.
      if (GetExpectedChar<'}'>(in)) {
        *val = v;
        break;
      } else {
        in.clear();
        if (!GetExpectedChar<','>(in))
          break;
      }
    }
  }
  return !in.fail();
}

// Overload for std::unordered_map with a bool at the end.
template <typename T, typename U, typename V, typename W, typename X, bool B,
          template <class, class, class, class, class, bool>
          class ContainerType>
inline bool StringToValue(std::istringstream& in,  // NOLINT
                          ContainerType<T, U, V, W, X, B>* val) {
  return StringToStlContainer(in, val);
}

// Overload for std::unordered_map without a bool at the end.
template <typename T, typename U, typename V, typename W, typename X,
          template <class, class, class, class, class> class ContainerType>
inline bool StringToValue(std::istringstream& in,  // NOLINT
                ContainerType<T, U, V, W, X>* val) {
  return StringToStlContainer(in, val);
}

// Overload for std::unordered_set with a bool at the end.
template <typename T, typename U, typename V, typename W, bool B,
          template <class, class, class, class, bool> class ContainerType>
inline bool StringToValue(std::istringstream& in,  // NOLINT
                          ContainerType<T, U, V, W, B>* val) {
  return StringToStlContainer(in, val);
}

// Overload for std::map and std::unordered_set without a bool at the end.
template <typename T, typename U, typename V, typename W,
          template <class, class, class, class> class ContainerType>
inline bool StringToValue(std::istringstream& in,  // NOLINT
                          ContainerType<T, U, V, W>* val) {
  return StringToStlContainer(in, val);
}

// Overload for std::set.
template <typename T, typename U, typename V,
          template <class, class, class> class ContainerType>
inline bool StringToValue(std::istringstream& in,  // NOLINT
                          ContainerType<T, U, V>* val) {
  return StringToStlContainer(in, val);
}

// Overload for STL containers like deque, list, vector.
template <typename T, typename U, template <class, class> class ContainerType>
inline bool StringToValue(std::istringstream& in,  // NOLINT
                          ContainerType<T, U>* val) {
  return StringToStlContainer(in, val);
}

// Convenience function that converts a std::string to a T, constructing the
// istringstream automatically.
template <typename T>
inline bool StringToValue(const std::string& s, T* val) {
  std::istringstream in(s);
  return StringToValue(in, val);
}

//-----------------------------------------------------------------------------
//
// ValueToString
//
//-----------------------------------------------------------------------------

// Serializes a generic value to a string.
template <typename T>
inline std::string ValueToString(const T& val) {
  std::ostringstream out;
  out << val;
  return out.str();
}

// Specialize for bools.
template <>
inline std::string ValueToString(const bool& val) {
  std::ostringstream out;
  out << (val ? "true" : "false");
  return out.str();
}

// Specialize for printing significant digits of floating point numbers.
template <>
inline std::string ValueToString(const float& val) {
  std::ostringstream out;
  out.precision(6);
  out << val;
  return out.str();
}
template <>
inline std::string ValueToString(const double& val) {
  std::ostringstream out;
  out.precision(12);
  out << val;
  return out.str();
}

// Overload for writing double quoted strings.
template <typename T, typename U, typename V>
inline std::string ValueToString(const std::basic_string<T, U, V>& val) {
  std::ostringstream out;
  out << '"' << EscapeString(val) << '"';
  return out.str();
}

// Overload for writing double quoted C-strings.
template <>
inline std::string ValueToString(const char* const& val) {
  return ValueToString(std::string(val));
}

// Overload for std::pair types.
template <typename T, typename U>
inline std::string ValueToString(const std::pair<const T, U>& val) {
  std::ostringstream out;
  out << ValueToString(val.first) << " : " << ValueToString(val.second);
  return out.str();
}

// Overload for std::chrono::duration types.
template <typename R, typename P>
inline std::string ValueToString(const std::chrono::duration<R, P>& val) {
  using std::chrono::duration;
  using std::chrono::duration_cast;

  // Determine the smallest std::ratio for the given input period and value.
  R rep = val.count();
  R ratio = P::den / P::num;
  while (std::abs(rep) > 0 && rep % 1000 == 0) {
    rep /= 1000;
    ratio /= 1000;
  }

  // Populate a stream based on the value of |val| and |ratio|.
  std::ostringstream out;
  switch (ratio) {
    case 1000000000:
      out << duration_cast<duration<R, std::nano>>(val).count() << " ns";
      break;
    case 1000000:
      out << duration_cast<duration<R, std::micro>>(val).count() << " us";
      break;
    case 1000:
      out << duration_cast<duration<R, std::milli>>(val).count() << " ms";
      break;
    default:
      // |ratio| is either 1 or something we don't explicitly support, so we
      // default to seconds.
      out << duration_cast<duration<R>>(val).count() << " s";
      break;
  }

  // Extract a string from the previously populated stream.
  return out.str();
}

// Serializes an STL container to a string.
template <typename ContainerType>
inline std::string StlContainerToString(const ContainerType& c) {
  std::ostringstream out;
  out << "{ ";
  for (typename ContainerType::const_iterator it = c.begin(); it != c.end();
       ++it) {
    if (it != c.begin())
      out << ", ";
    out << ValueToString(*it);
  }
  out << " }";
  return out.str();
}

// Overload for std::unordered_map with a bool at the end.
template <typename T, typename U, typename V, typename W, typename X, bool B,
          template <class, class, class, class, class, bool>
          class ContainerType>
inline std::string ValueToString(
    const ContainerType<T, U, V, W, X, B>& val) {
  return StlContainerToString(val);
}

// Overload for std::unordered_map without a bool at the end.
template <typename T, typename U, typename V, typename W, typename X,
          template <class, class, class, class, class>
          class ContainerType>
inline std::string ValueToString(
    const ContainerType<T, U, V, W, X>& val) {
  return StlContainerToString(val);
}

// Overload for std::unordered_set with a bool at the end.
template <typename T, typename U, typename V, typename W, bool B,
          template <class, class, class, class, bool> class ContainerType>
inline std::string ValueToString(
    const ContainerType<T, U, V, W, B>& val) {
  return StlContainerToString(val);
}

// Overload for std::map and std::unordered_set without a bool at the end.
template <typename T, typename U, typename V, typename W,
          template <class, class, class, class> class ContainerType>
inline std::string ValueToString(
    const ContainerType<T, U, V, W>& val) {
  return StlContainerToString(val);
}

// Overload for std::set.
template <typename T, typename U, typename V,
          template <class, class, class> class ContainerType>
inline std::string ValueToString(const ContainerType<T, U, V>& val) {
  return StlContainerToString(val);
}

// Overload for non-associative STL containers (deque, list, vector).
template <typename T, typename U, template <class, class> class ContainerType>
inline std::string ValueToString(const ContainerType<T, U>& val) {
  return StlContainerToString(val);
}

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_SERIALIZE_H_
