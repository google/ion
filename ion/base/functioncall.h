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

#ifndef ION_BASE_FUNCTIONCALL_H_
#define ION_BASE_FUNCTIONCALL_H_

#include <tuple>

#include "base/macros.h"
#include "ion/base/allocatable.h"
#include "ion/base/scalarsequence.h"

namespace ion {
namespace base {

// FunctionCallBase does nothing but define a virtual operator() to allow
// containers of arbitrary FunctionCall<>s.
class FunctionCallBase : public Allocatable {
 public:
  virtual void operator()() const = 0;
  ~FunctionCallBase() override {}
};

// FunctionCall wraps an arbitrary call to a function, including its arguments.
// The arguments can be queried and modified after the instance is created,
// making this a very flexible method for storing and calling functions. Note
// that after creation, the target (function to call) is fixed, even though the
// arguments can be modified.
//
// Example usage:
//   // Free function bool IntFunc(int i) {...}.
//   FunctionCall<bool(int)> int_func(IntFunc, 1);
//   int_func();  // Invokes IntFunc(1).
//   int_func.SetArg<0>(10);  // Changes the value of the 0th argument to 10.
//   int_func();  // Invokes IntFunc(10).
//   // Member function void ClassType::DoubleFunc(double d) {...}.
//   FunctionCall<void(double)> double_func(
//       std::bind(&ClassType::DoubleFunc, &instance, std::placeholders::_1),
//       3.14);
//   double_func();
//
// This declares the template as taking a single argument so that it can be
// specialized for different function signatures which require multiple template
// arguments.
template <typename T>
class FunctionCall;

// Specialize for an arbitrary function signature.
template <typename ReturnType, typename... Types>
class FunctionCall<ReturnType(Types...)> : public FunctionCallBase {
 public:
  // Constructor for a free function (general non-member function).
  explicit FunctionCall(ReturnType (*func)(Types...),
                        Types&&... args)  // NOLINT
      : free_func_(func),
        args_(std::make_tuple(std::forward<Types>(args)...)) {}

  // Constructor for a std::function (returned by std::bind()).
  explicit FunctionCall(const std::function<ReturnType(Types...)>& func,
                        Types&&... args)  // NOLINT
      : free_func_(nullptr),
        std_func_(func),
        args_(std::make_tuple(std::forward<Types>(args)...)) {}

  ~FunctionCall() override {}

  // Calls the wrapped function with the stored arguments.
  void operator()() const override {
    // This expands out into a sequence for all
    if (free_func_)
      this->ExpandArgsAndCall(
          free_func_,
          typename ScalarSequenceGenerator<size_t,
                                           sizeof...(Types)>::Sequence());
    else
      this->ExpandArgsAndCall(
          std_func_,
          typename ScalarSequenceGenerator<size_t,
                                           sizeof...(Types)>::Sequence());
  }

  // Returns a const reference to the Ith argument of the function call.
  template <size_t I>
  const typename std::tuple_element<I, std::tuple<Types...> >::type& GetArg()
      const {
    return std::get<I>(args_);
  }

  // Sets the Ith argument of the stored arguments to the passed value.
  template <size_t I>
  void SetArg(const typename std::tuple_element<I, std::tuple<Types...> >::type&
                  value) {
    std::get<I>(args_) = value;
  }

 private:
  // Expands the arguments out of the tuple and passes them to the stored
  // function call, executing the function. The unpack notation instantiates a
  // std::get() call for each index of the sequence, thus unpacking the
  // arguments from args_.
  template <typename Func, size_t... Indices>
  void ExpandArgsAndCall(const Func& func,
                         ScalarSequence<size_t, Indices...> seq) const {
    func(std::get<Indices>(args_)...);
  }

  // The function to call, either a free function or std::function.
  // 
  ReturnType (*free_func_)(Types...);
  std::function<ReturnType(Types...)> std_func_;

  // The arguments to the function.
  std::tuple<Types...> args_;
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_FUNCTIONCALL_H_
