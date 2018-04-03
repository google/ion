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

#include "ion/base/calllist.h"

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

  static void IncrementInt() {
    ++g_call_count;
  }

 private:
  bool b_;
  int i_;
  mutable int calls_;
};

}  // anonymous namespace

TEST(CallListTest, StaticFunctions) {
  Reset();

  CallListPtr cl(new CallList());
  cl->Add(SetInt, 1);
  EXPECT_EQ(0, g_int);
  cl->Execute();
  EXPECT_EQ(1, g_int);
  EXPECT_EQ(1, g_call_count);

  cl->Add(std::bind(SetInt, std::placeholders::_1), 2);
  cl->Add(std::bind(SetInt, std::placeholders::_1), 1);
  cl->Execute();
  EXPECT_EQ(1, g_int);
  // Three calls happen since the original call to SetInt still happens.
  EXPECT_EQ(4, g_call_count);

  cl->Clear();
  cl->Add(std::bind(SetInt, std::placeholders::_1), 2);
  cl->Execute();
  EXPECT_EQ(2, g_int);
  EXPECT_EQ(5, g_call_count);

  cl->Clear();
  cl->Add(std::bind(SetDouble, std::placeholders::_1), 3.14);
  cl->Add(std::bind(SetInt, std::placeholders::_1), 21);
  cl->Execute();
  EXPECT_EQ(21, g_int);
  EXPECT_EQ(3.14, g_double);
  EXPECT_EQ(7, g_call_count);

  // Check that class statics work.
  cl->Clear();
  cl->Add(ValueStorage::NoOp);
  cl->Execute();
  EXPECT_EQ(8, g_call_count);
}

TEST(CallListTest, MemberFunctions) {
  Reset();

  ValueStorage v;
  CallListPtr cl(new CallList());
  cl->Add(std::bind(&ValueStorage::SetInt, &v, std::placeholders::_1), 1);
  EXPECT_EQ(0, v.GetInt());
  EXPECT_EQ(0, v.GetCallCount());
  cl->Execute();
  EXPECT_EQ(1, v.GetInt());
  EXPECT_EQ(1, v.GetCallCount());

  cl->Add(std::bind(&ValueStorage::SetInt, &v, std::placeholders::_1), 2);
  cl->Add(std::bind(&ValueStorage::SetInt, &v, std::placeholders::_1), 1);
  cl->Execute();
  EXPECT_EQ(1, v.GetInt());
  EXPECT_EQ(4, v.GetCallCount());

  cl->Clear();
  cl->Add(std::bind(&ValueStorage::SetInt, &v, std::placeholders::_1), 2);
  cl->Execute();
  EXPECT_EQ(2, v.GetInt());
  EXPECT_EQ(5, v.GetCallCount());

  cl->Clear();
  cl->Add(std::bind(&ValueStorage::SetBool, &v, std::placeholders::_1), true);
  cl->Add(std::bind(&ValueStorage::SetInt, &v, std::placeholders::_1), 21);
  // Make sure const functions work.
  cl->Add(std::bind(&ValueStorage::GetInt, &v));
  EXPECT_FALSE(v.GetBool());
  cl->Execute();
  EXPECT_EQ(21, v.GetInt());
  EXPECT_TRUE(v.GetBool());
  EXPECT_EQ(7, v.GetCallCount());
}

TEST(CallListTest, MixedFunctions) {
  Reset();

  ValueStorage v;
  CallListPtr cl(new CallList());
  cl->Add(std::bind(&ValueStorage::SetInt, &v, std::placeholders::_1), 4);
  cl->Add(std::bind(SetInt, std::placeholders::_1), 3);
  EXPECT_EQ(0, g_int);
  EXPECT_EQ(0, v.GetInt());
  cl->Execute();
  EXPECT_EQ(3, g_int);
  EXPECT_EQ(4, v.GetInt());
  EXPECT_EQ(1, g_call_count);
  EXPECT_EQ(1, v.GetCallCount());

  cl->Add(std::bind(&ValueStorage::SetInt, &v, std::placeholders::_1), 31);
  cl->Add(std::bind(SetInt, std::placeholders::_1), 13);
  cl->Execute();
  EXPECT_EQ(31, v.GetInt());
  EXPECT_EQ(13, g_int);
  EXPECT_EQ(3, g_call_count);
  EXPECT_EQ(3, v.GetCallCount());

  cl->Clear();
  cl->Add(std::bind(&ValueStorage::SetInt, &v, std::placeholders::_1), 2);
  cl->Add(std::bind(SetInt, std::placeholders::_1), 2);
  cl->Execute();
  EXPECT_EQ(2, v.GetInt());
  EXPECT_EQ(2, g_int);
  EXPECT_EQ(4, g_call_count);
  EXPECT_EQ(4, v.GetCallCount());

  cl->Clear();
  cl->Add(std::bind(&ValueStorage::SetBool, &v, std::placeholders::_1), true);
  cl->Add(std::bind(SetDouble, std::placeholders::_1), 1.23);
  EXPECT_FALSE(v.GetBool());
  cl->Execute();
  EXPECT_EQ(1.23, g_double);
  EXPECT_TRUE(v.GetBool());
  EXPECT_EQ(5, g_call_count);
  EXPECT_EQ(5, v.GetCallCount());
}

TEST(CallListTest, GetCall) {
  Reset();

  ValueStorage v;
  CallListPtr cl(new CallList());
  cl->Add(std::bind(&ValueStorage::SetInt, &v, std::placeholders::_1), 4);
  cl->Add(SetInt, 3);
  cl->Add(std::bind(SetInt, std::placeholders::_1), 4);
  cl->Add(std::bind(&ValueStorage::SetIntAndBool, &v, std::placeholders::_1,
                    std::placeholders::_2),
          5, true);

#if !ION_NO_RTTI
  // Check that an improper set of arguments returns nullptr.
  EXPECT_TRUE(cl->GetCall<void(float)>(0) == nullptr);
  EXPECT_TRUE(cl->GetCall<int(int)>(0) != nullptr);

  EXPECT_TRUE(cl->GetCall<int(int)>(1) == nullptr);
  // Check that both free and member functions have the same signature.
  EXPECT_TRUE(cl->GetCall<void(int)>(1) != nullptr);
  EXPECT_TRUE(cl->GetCall<void(int)>(2) != nullptr);

  EXPECT_TRUE(cl->GetCall<void(double)>(3) == nullptr);
  EXPECT_TRUE(cl->GetCall<void(int)>(3) == nullptr);
  EXPECT_TRUE(cl->GetCall<void(int, bool)>(3) != nullptr);
#endif

  // Check argument values.
  EXPECT_EQ(4, cl->GetCall<int(int)>(0)->GetArg<0>());
  EXPECT_EQ(3, cl->GetCall<void(int)>(1)->GetArg<0>());
  EXPECT_EQ(4, cl->GetCall<void(int)>(2)->GetArg<0>());
  cl->GetCall<int(int)>(0)->SetArg<0>(1);
  cl->GetCall<void(int)>(1)->SetArg<0>(2);
  cl->GetCall<void(int)>(2)->SetArg<0>(3);
  EXPECT_EQ(1, cl->GetCall<int(int)>(0)->GetArg<0>());
  EXPECT_EQ(2, cl->GetCall<void(int)>(1)->GetArg<0>());
  EXPECT_EQ(3, cl->GetCall<void(int)>(2)->GetArg<0>());

  // Multiple arguments.
  EXPECT_EQ(5, cl->GetCall<void(int, bool)>(3)->GetArg<0>());
  EXPECT_TRUE(cl->GetCall<void(int, bool)>(3)->GetArg<1>());
  cl->GetCall<void(int, bool)>(3)->SetArg<0>(10);
  cl->GetCall<void(int, bool)>(3)->SetArg<1>(false);
  EXPECT_EQ(10, cl->GetCall<void(int, bool)>(3)->GetArg<0>());
  EXPECT_FALSE(cl->GetCall<void(int, bool)>(3)->GetArg<1>());
  cl->Execute();

  EXPECT_EQ(3, g_int);
  EXPECT_EQ(10, v.GetInt());
  EXPECT_FALSE(v.GetBool());
  EXPECT_EQ(2, v.GetCallCount());
  EXPECT_EQ(2, g_call_count);

  // Check that const member functions with parameters work, but do not require
  // a const qualifier, just the basic signature
  cl->Clear();
  cl->Add(std::bind(&ValueStorage::GetIntWithParam, &v, std::placeholders::_1),
          1);
  EXPECT_TRUE(cl->GetCall<int(int)>(0) != nullptr);
  EXPECT_EQ(1, cl->GetCall<int(int)>(0)->GetArg<0>());
  cl->GetCall<int(int)>(0)->SetArg<0>(10);
  EXPECT_EQ(10, cl->GetCall<int(int)>(0)->GetArg<0>());
  cl->Execute();
  EXPECT_EQ(3, v.GetCallCount());
}

}  // namespace base
}  // namespace ion
