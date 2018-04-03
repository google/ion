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

#ifndef ION_BASE_SCALARSEQUENCE_H_
#define ION_BASE_SCALARSEQUENCE_H_

#include <array>
#include <cstddef>

namespace ion {
namespace base {

// A variadic template containing a sequence of scalars.
template <typename T, T... values>
struct ScalarSequence {
  static constexpr std::size_t kCount = sizeof...(values);
  using ValueType = T;
  using ArrayType = std::array<ValueType, kCount>;

  static constexpr ArrayType ToArray() { return ArrayType({{values...}}); }
};

// Scalar sequence generator, generating an ScalarSequence starting at 0 'count'
// times, increasing at values of 'step'.
template <typename T, std::size_t count, T step = 1, std::size_t index = 0,
          T... values>
struct ScalarSequenceGenerator {
  using Sequence =
      typename ScalarSequenceGenerator<T, count, step, index + 1, values...,
                                       index * step>::Sequence;
};

// Terminator case for scalar sequence generation.
template <typename T, std::size_t count, T step, T... values>
struct ScalarSequenceGenerator<T, count, step, count, values...> {
  using Sequence = ScalarSequence<T, values...>;
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_SCALARSEQUENCE_H_
