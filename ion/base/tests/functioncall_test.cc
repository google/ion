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

#include "ion/base/functioncall.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace base {

namespace {

static int g_int = 0;
static double g_double = 0.0;
static int g_call_count = 0;

// Simple global setters.
static void SetInt(int i) {
  ++g_call_count;
  g_int = i;
}

static void SetDouble(double d) {
  ++g_call_count;
  g_double = d;
}

// Resets the global count and static values.
static void Reset() {
  g_call_count = 0;
  g_int = 0;
  g_double = 0.0;
}

// Simple class with accessors for a bool and int.
class ValueStorage {
 public:
  ValueStorage() : b_(false), i_(0), calls_(0) {}

  bool SetBool(bool b) {
    ++calls_;
    b_ = b;
    return b;
  }

  bool GetBool() const {
    return b_;
  }

  int SetInt(int i) {
    ++calls_;
    i_  = i;
    return i;
  }

  int GetInt() const {
    return i_;
  }

  int GetIntWithParam(int i) const {
    ++calls_;
    return i;
  }

  void SetIntAndBool(int i, bool b) {
    ++calls_;
    i_  = i;
    b_ = b;
  }

  int GetCallCount() const { return calls_; }

  static void NoOp() {
    ++g_call_count;
  }

 private:
  bool b_;
  int i_;
  mutable int calls_;
};

}  // anonymous namespace

TEST(FunctionCallTest, StaticFunctions) {
  Reset();

  FunctionCall<void(int)> int_func(SetInt, 1);
  EXPECT_EQ(0, g_int);
  int_func();
  EXPECT_EQ(1, g_int);
  EXPECT_EQ(1, g_call_count);

  // Either signature works since std::function can be implicitly constructed
  // from SetInt.
  FunctionCall<void(int)> int_func2(SetInt, 2);
  EXPECT_EQ(1, g_int);
  int_func2();
  EXPECT_EQ(2, g_int);
  EXPECT_EQ(2, g_call_count);

  FunctionCall<void(double)> double_func(
      std::bind(SetDouble, std::placeholders::_1), 3.14);
  double_func();
  EXPECT_EQ(3.14, g_double);
  EXPECT_EQ(3, g_call_count);
}

TEST(FunctionCallTest, MemberFunctions) {
  Reset();

  ValueStorage v;
  FunctionCall<int(int)> int_func(
      std::bind(&ValueStorage::SetInt, &v, std::placeholders::_1), 1);
  EXPECT_EQ(0, v.GetInt());
  int_func();
  EXPECT_EQ(1, v.GetInt());
  EXPECT_EQ(1, v.GetCallCount());

  FunctionCall<bool(bool)> bool_func(
      std::bind(&ValueStorage::SetBool, &v, std::placeholders::_1), true);
  EXPECT_FALSE(v.GetBool());
  bool_func();
  EXPECT_TRUE(v.GetBool());
  EXPECT_EQ(2, v.GetCallCount());

  FunctionCall<void(int, bool)> both_func(
      std::bind(&ValueStorage::SetIntAndBool, &v, std::placeholders::_1,
                std::placeholders::_2),
      5, false);
  both_func();
  EXPECT_FALSE(v.GetBool());
  EXPECT_EQ(5, v.GetInt());
  EXPECT_EQ(3, v.GetCallCount());

  // Const member function.
  FunctionCall<void()> const_func(ValueStorage::NoOp);
  EXPECT_EQ(0, g_call_count);
  const_func();
  EXPECT_EQ(1, g_call_count);
}

TEST(FunctionCallTest, ModifyArgs) {
  Reset();

  FunctionCall<void(int)> int_func(std::bind(SetInt, std::placeholders::_1), 1);
  EXPECT_EQ(0, g_int);
  int_func();
  EXPECT_EQ(1, g_int);
  EXPECT_EQ(1, g_call_count);

  EXPECT_EQ(1, int_func.GetArg<0>());
  int_func.SetArg<0>(2);
  EXPECT_EQ(2, int_func.GetArg<0>());
  int_func();
  EXPECT_EQ(2, g_int);
  EXPECT_EQ(2, g_call_count);

  ValueStorage v;
  FunctionCall<void(int, bool)> both_func(
      std::bind(&ValueStorage::SetIntAndBool, &v, std::placeholders::_1,
                std::placeholders::_2),
      5, true);
  EXPECT_EQ(5, both_func.GetArg<0>());
  EXPECT_TRUE(both_func.GetArg<1>());
  EXPECT_FALSE(v.GetBool());
  both_func();
  EXPECT_TRUE(v.GetBool());
  EXPECT_EQ(5, v.GetInt());
  both_func.SetArg<0>(3);
  both_func();
  EXPECT_EQ(3, v.GetInt());
  EXPECT_TRUE(v.GetBool());
  both_func.SetArg<1>(false);
  both_func();
  EXPECT_EQ(3, v.GetInt());
  EXPECT_FALSE(v.GetBool());
  EXPECT_EQ(3, v.GetCallCount());

  // Class static function.
  FunctionCall<int(int)> static_func(
      std::bind(&ValueStorage::GetIntWithParam, &v, std::placeholders::_1), 1);
  EXPECT_EQ(1, static_func.GetArg<0>());
  static_func.SetArg<0>(10);
  EXPECT_EQ(10, static_func.GetArg<0>());
  EXPECT_EQ(3, v.GetCallCount());
  static_func();
  EXPECT_EQ(4, v.GetCallCount());
}

}  // namespace base
}  // namespace ion
