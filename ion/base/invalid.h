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

#ifndef ION_BASE_INVALID_H_
#define ION_BASE_INVALID_H_

#include <cstring>  // For size_t.

namespace ion {
namespace base {

// kInvalidIndex is a size_t value that is very unlikely to be a valid index.
// It can be used to indicate an invalid return value from a function returning
// an index.
extern ION_API const size_t kInvalidIndex;

// InvalidReference() returns a const reference to an invalid instance of type
// T. It can be used to indicate an invalid return value from a function
// returning a const reference to a T.
template <typename T> static const T& InvalidReference() {
  return *reinterpret_cast<const T*>(&kInvalidIndex);
}

// IsInvalidReference() returns true if a passed const reference of type T
// has an address of InvalidReference<T>(). A return value of false does not
// guarantee that a reference is valid, just that it is not an InvalidReference.
template <typename T> bool IsInvalidReference(const T& value) {
  return &value == &InvalidReference<T>();
}

// InvalidEnumValue() returns an invalid enum value, assuming that -1 is not a
// valid value. This can be used for initializing enum variables or for testing.
template <typename EnumType> EnumType InvalidEnumValue() {
  // Some compilers choke when casting a const invalid value to an enum.
  int bad_value = -1;
  return static_cast<EnumType>(bad_value);
}

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_INVALID_H_
