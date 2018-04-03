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

#include "ion/base/staticsafedeclare.h"

#include "ion/base/logging.h"
#include "ion/base/threadspawner.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace {

static int s_num_deletes = 0;

struct MyStructDeletedFirst {
  MyStructDeletedFirst() : a(0) {}
  explicit MyStructDeletedFirst(int a_in) : a(a_in) {}
  ~MyStructDeletedFirst() {
    // Since this will be deleted first by StaticDeleter, s_num_deletes should
    // be 0.
    EXPECT_EQ(0, s_num_deletes);
    s_num_deletes++;
  }
  int a;
};

struct MyStructDeletedSecond {
  MyStructDeletedSecond() : a(10) {}
  ~MyStructDeletedSecond() {
    // Since this will be deleted second by StaticDeleter, s_num_deletes should
    // be 1.
    EXPECT_EQ(1, s_num_deletes);
    s_num_deletes++;
  }
  int a;
};

// Two types that can depend on each other.
struct Base {
  void Call() { DCHECK(this); }
};

struct A : Base {
  explicit A(Base* b_in) : b(b_in) {}
  ~A() {
    if (b)
      b->Call();
  }
  Base* b;
};

struct B : Base {
  explicit B(Base* a_in) : a(a_in) {}
  ~B() {
    if (a)
      a->Call();
  }
  Base* a;
};

struct StaticInDestructor {
  StaticInDestructor() {}
  ~StaticInDestructor() {
    ION_DECLARE_SAFE_STATIC_POINTER_WITH_CONSTRUCTOR(int, test_int, new int(5));
    DCHECK(this);
  }
};

struct LogInDestructor {
  LogInDestructor() {}
  ~LogInDestructor() {
    LOG(INFO) << "Log in destructor";
    LOG_ONCE(INFO) << "Single log in destructor";
  }
};

// A numeric type that increments its value each time it is default-construted.
// Used to verify that ION_DECLARE_SAFE_STATIC_ARRAY is creating valid objects.
static int default_int_val = 0;
struct IntVal {
  IntVal() : val(default_int_val++) {}
  int val;
  char padding[6];  // Make the type size more interesting.
};

// Create and read some safe statics for TwoThreads() below to poke at.
static bool GetSafeStaticValue() {
  enum { kMagicConstant = 0x12345678 };
  ION_DECLARE_SAFE_STATIC_POINTER_WITH_CONSTRUCTOR(int, my_int32,
                                                   new int(kMagicConstant));
  ION_DECLARE_SAFE_STATIC_POINTER_WITH_CONSTRUCTOR(bool, my_bool_p,
                                                   new bool(true));
  // Verify that reading both the safe statics and through them (in the pointer
  // case) is thread-safe.
  // Do the reads on separate lines for easier decoding of TSAN errors.
  bool return_value = my_bool_p != static_cast<bool*>(nullptr);
  return_value &= *my_bool_p;
  return_value &= *my_int32 == kMagicConstant;
  return return_value;
}

}  // anonymous namespace

TEST(StaticInitialize, LogInDestructorDoesNotCrash) {
  ION_DECLARE_SAFE_STATIC_POINTER(LogInDestructor, l);
  EXPECT_NE(l, nullptr);
}

TEST(StaticInitialize, InitializeVariables) {
  using ion::base::StaticDeleterDeleter;

  // Save the number of deleters that already exist.
  const size_t offset = StaticDeleterDeleter::GetInstance()->GetDeleterCount();

  // Declare foo as an int*.
  ION_DECLARE_SAFE_STATIC_POINTER(int, foo_ptr);

  EXPECT_EQ(
      "int*",
      StaticDeleterDeleter::GetInstance()->GetDeleterAt(offset)->GetTypeName());
  EXPECT_TRUE(foo_ptr != static_cast<int*>(nullptr));
  // Declare foo as an int* of 10 ints.
  ION_DECLARE_SAFE_STATIC_ARRAY(int, foo_array, 10);
  EXPECT_TRUE(foo_array != static_cast<int*>(nullptr));
  EXPECT_EQ("int*",
            StaticDeleterDeleter::GetInstance()
                ->GetDeleterAt(offset + 1)
                ->GetTypeName());
  // Declare foo_int_array as an array of 10 IntVals with values 0 to 9.
  // This ensures that the array elements are default-constructed and that
  // operator[] does the right thing.
  ION_DECLARE_SAFE_STATIC_ARRAY(IntVal, foo_int_array, 10);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(foo_int_array[i].val, i);
  }
  // Declare foo_struct as a pointer to MyStructDeletedSecond.
  ION_DECLARE_SAFE_STATIC_POINTER(MyStructDeletedSecond, foo_struct);
  EXPECT_EQ("MyStructDeletedSecond*",
            StaticDeleterDeleter::GetInstance()
                ->GetDeleterAt(offset + 3)
                ->GetTypeName());
  EXPECT_EQ(10, foo_struct->a);
  // Declare foo_struct2 as a pointer to MyStructDeletedFirst, calling a
  // non-default constructor.
  ION_DECLARE_SAFE_STATIC_POINTER_WITH_CONSTRUCTOR(
      MyStructDeletedFirst, foo_struct2, (new MyStructDeletedFirst(2)));
  EXPECT_EQ(2, foo_struct2->a);
  EXPECT_EQ("MyStructDeletedFirst*",
            StaticDeleterDeleter::GetInstance()
                ->GetDeleterAt(offset + 4)
                ->GetTypeName());

  EXPECT_TRUE(StaticDeleterDeleter::GetInstance()->GetDeleterAt(offset + 5) ==
              nullptr);
}

TEST(StaticInitialize, Interdependencies) {
  // Declare a dicey situation, where b depends on a1, while a2 depends on b. If
  // StaticDeleters are tied to a particular type, rather than a pointer, one of
  // these types' StaticDeleters will delete all instances of the type before
  // the other, meaning that one of these destructions would attempt to Call()
  // a deleted instance.
  ION_DECLARE_SAFE_STATIC_POINTER_WITH_CONSTRUCTOR(A, a1, new A(nullptr));
  ION_DECLARE_SAFE_STATIC_POINTER_WITH_CONSTRUCTOR(B, b, new B(a1));
  ION_DECLARE_SAFE_STATIC_POINTER_WITH_CONSTRUCTOR(A, a2, new A(b));
}

TEST(StaticInitialize, StaticInDestructorDoesNotDeadlock) {
  ION_DECLARE_SAFE_STATIC_POINTER(StaticInDestructor, s);
  EXPECT_NE(s, nullptr);
}

// No thread support in asmjs means no test coverage for threads.
#if !defined(ION_PLATFORM_ASMJS)

// Test that "safe statics" are indeed "safe": two threads accessing them don't
// race.  Note that this is mostly only useful under TSAN, as no effort is taken
// here to make the test not "get lucky".
TEST(StaticInitialize, TwoThreads) {
  ion::base::ThreadSpawner t1("thread1", &GetSafeStaticValue);
  ion::base::ThreadSpawner t2("thread2", &GetSafeStaticValue);
  // No TSAN errors is our success criteria.
}

#endif  // ION_PLATFORM_ASMJS
