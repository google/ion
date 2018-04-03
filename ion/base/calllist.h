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

#ifndef ION_BASE_CALLLIST_H_
#define ION_BASE_CALLLIST_H_

#include <memory>

#include "base/macros.h"
#include "ion/base/functioncall.h"
#include "ion/base/referent.h"
#include "ion/base/stlalloc/allocvector.h"

namespace ion {
namespace base {

// CallList contains a list of function calls to execute. Individual calls and
// in particular their arguments can be modified directly.
//
// Example usage:
//   Value v;
//   CallListPtr cl(new CallList());
//   // Member function int Value::SetInt(int i) {...}.
//   cl->Add(std::bind(&ValueStorage::SetInt, &v, std::placeholders::_1), 4);
//   // Free function void SetInt(int i) {...}.
//   cl->Add(SetInt, 3);
//   // Same free function, but using a bind (somewhat less efficient).
//   cl->Add(std::bind(SetInt, std::placeholders::_1), 4);
//   // Returns the value of the first argument of the 2nd call.
//   cl->GetCall<int(int)>(2)->GetArg<0>();
//   // Sets the first argument of the 0th call.
//   cl->GetCall<int(int)>(0)->SetArg<0>(1);
//   // Sets the first argument of the 1st call.
//   cl->GetCall<void(int)>(1)->SetArg<0>(2);
//   // Sets the first argument of the 2nd call.
//   cl->GetCall<void(int)>(2)->SetArg<0>(3);
//   cl->Execute();  // Execute the calls.
//   cl->Clear();  // Clears the set of calls.
class ION_API CallList : public Referent {
 public:
  CallList();

  // Adds a function call to the list of calls to execute, overloaded for
  // non-member (free) functions. This version of Add() takes a free function
  // directly, which avoids the overhead of a std::function, but only works for
  // free functions.
  template <typename ReturnType, class... Args>
  void Add(ReturnType(*func)(Args...), Args&&... args) {
    calls_.push_back(std::unique_ptr<FunctionCallBase>(
        new (GetAllocator()) FunctionCall<ReturnType(Args...)>(
            func, std::forward<Args>(args)...)));
  }

  // Adds a function call to the list of calls to execute, overloaded for
  // std::functions. This allows passing more general functions than the above,
  // at the cost of calling through a std::function.
  template <typename Func, class... Args>
  void Add(Func&& func, Args&&... args) {
    calls_.push_back(std::unique_ptr<FunctionCallBase>(
        new (GetAllocator()) FunctionCall<typename Func::result_type(Args...)>(
            std::forward<Func>(func), std::forward<Args>(args)...)));
  }

  // Executes the stored calls.
  void Execute();

  // Clears the set of calls.
  void Clear();

  // Returns the ith FunctionCall in this list. Note that this is
  // unsafe if the index is invalid or the template arguments for the
  // function signature are incorrect, because we disallow
  // dynamic_cast in ion. This must be called with the function
  // signature as the template argument, e.g., GetCall<void(int)>().
  template <typename Func>
  FunctionCall<Func>* GetCall(size_t i) {
    if (i >= calls_.size())
        return nullptr;
#if ION_NO_RTTI
    return static_cast<FunctionCall<Func>*>(calls_[i].get());
#else
    return dynamic_cast<FunctionCall<Func>*>(calls_[i].get());
#endif
  }

 private:
  // The destructor is private because this is derived from Referent.
  ~CallList() override;

  // The vector of function calls to make.
  AllocVector<std::unique_ptr<FunctionCallBase>> calls_;

  DISALLOW_COPY_AND_ASSIGN(CallList);
};

using CallListPtr = SharedPtr<CallList>;

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_CALLLIST_H_
