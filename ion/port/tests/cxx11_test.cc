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

// This file tests C++11 features to verify they work (and keep working)
// on all Ion-supported platforms.

#include <map>
#include <memory>
#include <random>
#include <tuple>
#include <type_traits>
#include <vector>

#include "absl/memory/memory.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

// std::atomic functionality is tested in atomic_test.cc

namespace {

// Tests returning a unique_ptr
std::unique_ptr<int> MakeUniqueInt(int val) {
  std::unique_ptr<int> ptr(new int(val));
  return ptr;
}

// Tests variadic classes and type traits
template <typename ...Types>
class VarTemplate;

template <typename T, typename ...Types>
class VarTemplate<T, Types...>: public VarTemplate<Types...> {
 public:
  using VarTemplate<Types...>::ReturnTrueForPointers;
  bool ReturnTrueForPointers(T arg) { return std::is_pointer<T>::value; }
};

template <typename T>
class VarTemplate<T> {
 public:
  bool ReturnTrueForPointers(T arg) { return std::is_pointer<T>::value; }
};

// Tests variadic functions
template <typename T>
int Sum(T val) {
  return val;
}

template <typename T, typename ...Args>
int Sum(T val, Args ...more) {
  return val + Sum<Args...>(more...);
}

// Tests perfect forwarding
class TestObj {
 public:
  TestObj() : moved_(false), copied_(false) {}
  TestObj(const TestObj& orig) : moved_(false), copied_(true) {
    orig.copied_ = true;
  }
  TestObj(TestObj&& orig) : moved_(true), copied_(false) {  // NOLINT
    orig.moved_ = true;
  }

  bool moved_;
  mutable bool copied_;
};

template <typename ...Args>
class MoverOrCopier {
 public:
  template <typename ...Args2>
  MoverOrCopier(Args2&& ...args)  // NOLINT
      : vals_(std::forward<Args2>(args)...) {}
  std::tuple<Args...> vals_;
};

// Tests that " = delete" doesn't trigger a compiler error.
class DeletedAssignOperator {
 public:
  void operator=(const DeletedAssignOperator&) = delete;
};

// Tests that the 'override' keyword compiles. QNX and NaCl don't actually
// support 'override', but we #define it away to make it work.
class TestOverrideBase {
 public:
  virtual int Virtual() { return 0; }
  virtual int PureVirtual() = 0;
  virtual int PureVirtualConcreteOverride() = 0;
  virtual ~TestOverrideBase() {}
};

class TestOverrideDerived : public TestOverrideBase {
 public:
  int Virtual() override { return 1; }
  int PureVirtual() override = 0;
  int PureVirtualConcreteOverride() override { return 1; }
  ~TestOverrideDerived() override {}
};

class TestOverrideTwiceDerived : public TestOverrideDerived {
 public:
  int Virtual() override { return 2; }
  int PureVirtual() override { return 2; }
  int PureVirtualConcreteOverride() override { return 2; }
  ~TestOverrideTwiceDerived() override {}
};

// Classes for testing 'final' keyword.  The commented-out lines would be legal
// if the superclass didn't use 'final' in the method or struct declaration.
struct FinalKeywordA {
  virtual int foo() final { return 1; }  // NOLINT
  virtual int bar() { return 2; }
  virtual ~FinalKeywordA() {}
};

struct FinalKeywordB final : FinalKeywordA {
  // Error: FinalKeywordA::foo() is final.
  // int foo() override { return 3; }
  int bar() override { return 4; }
};

// Error: FinalKeywordB is final.
// struct FinalKeywordC : FinalKeywordB {};

}  // anonymous namespace

TEST(Cxx11, ReturnUnique) {
  std::unique_ptr<int> ptr = MakeUniqueInt(5);
  EXPECT_TRUE(ptr);
  EXPECT_TRUE(ptr.get() != static_cast<int*>(nullptr));
  EXPECT_EQ(*ptr, 5);
}

TEST(Cxx11, MoveUnique) {
  std::unique_ptr<double> ptr1(new double(3.14159));
  std::unique_ptr<double> ptr2;

  ptr2 = std::move(ptr1);
  EXPECT_TRUE(ptr1.get() == static_cast<double*>(nullptr));
  EXPECT_TRUE(ptr2.get() != static_cast<double*>(nullptr));
  EXPECT_FALSE(ptr1);
}

TEST(Cxx11, UniqueInVector) {
  std::vector<std::unique_ptr<int>> v;
  std::vector<std::unique_ptr<int>> v2(10);
  std::unique_ptr<int> ptr(new int(3));

  v.emplace_back(new int(1));
  EXPECT_EQ(v.size(), 1U);
  EXPECT_EQ(*v[0], 1);
  v.push_back(absl::make_unique<int>(2));
  EXPECT_EQ(*v[1], 2);
  v.push_back(std::move(ptr));
  EXPECT_EQ(*v[2], 3);
  EXPECT_EQ(v.size(), 3U);

  ptr = absl::make_unique<int>(4);
  v2.push_back(std::move(ptr));
  EXPECT_EQ(*v2.back(), 4);
  v2.push_back(absl::make_unique<int>(5));
  EXPECT_EQ(*v2.back(), 5);
  v2.resize(30);
  v2.resize(2);
  v2 = std::move(v);
  EXPECT_EQ(v2.size(), 3U);
  EXPECT_EQ(*v2[0], 1);
  EXPECT_EQ(*v2[1], 2);
  EXPECT_EQ(*v2[2], 3);
}

TEST(Cxx11, UniqueInMap) {
  std::map<int, std::unique_ptr<int>> m;
  std::map<int, std::unique_ptr<int>> m2;
  std::unique_ptr<int> ptr(new int(0));

  m[0] = std::move(ptr);
  m[1] = absl::make_unique<int>(1);

  // Unlike std::vector::emplace_back(), we can't provide a raw int-ptr as an
  // argument to emplace().  In other words, this fails to compile:
  //   m.emplace(1, new int(1));
  //
  // This seems to be because the unique_ptr constructor is marked as explicit;
  // evidence for this is that the following also fails:
  //   std::pair<int, std::unique_ptr<int>> p{1, new int(1)};
  //
  // It is not clear whether this behavior is compliant with the C++11 standard.
  m.emplace(2, absl::make_unique<int>(2));
  ptr = absl::make_unique<int>(3);
  m.emplace(3, std::move(ptr));
  ptr = absl::make_unique<int>(4);
  m.insert(std::make_pair(4, std::move(ptr)));
  m.insert(std::make_pair(5, absl::make_unique<int>(5)));

  EXPECT_EQ(6U, m.size());
  for (const auto& pair : m)
    EXPECT_EQ(pair.first, *pair.second);

  // Add a value '5' to m2; this will be gone after we assign m to m2.
  m2[6] = absl::make_unique<int>(6);
  m2 = std::move(m);

  EXPECT_EQ(0U, m.size());
  EXPECT_EQ(6U, m2.size());
  for (const auto& pair : m)
    EXPECT_EQ(pair.first, *pair.second);
}

TEST(Cxx11, AutoKeyword) {
  std::vector<int> v;
  v.push_back(0);
  v.push_back(1);

  int counter = 0;
  for (auto it = v.begin(); it != v.end(); ++it, ++counter) {
    EXPECT_EQ(*it, counter);
  }
}

TEST(Cxx11, FinalKeyword) {
  EXPECT_EQ(1, FinalKeywordB().foo());
}

TEST(Cxx11, VariadicClassesAndTraits) {
  VarTemplate<int, double, int*, double*> tester;
  int* iptr = nullptr;
  double* dptr = nullptr;
  EXPECT_FALSE(tester.ReturnTrueForPointers(5));
  EXPECT_FALSE(tester.ReturnTrueForPointers(5.4));
  EXPECT_TRUE(tester.ReturnTrueForPointers(iptr));
  EXPECT_TRUE(tester.ReturnTrueForPointers(dptr));
}

TEST(Cxx11, VariadicFunctions) {
  EXPECT_EQ(Sum(1, 2, 3), 6);
  EXPECT_EQ(Sum(4), 4);
}

TEST(Cxx11, PerfectForwarding) {
  TestObj o1, o2;
  MoverOrCopier<TestObj, TestObj, TestObj> mc(o1, std::move(o2), TestObj());
  EXPECT_TRUE(o1.copied_);
  EXPECT_FALSE(o1.moved_);
  EXPECT_TRUE(o2.moved_);
  EXPECT_FALSE(o2.copied_);
  EXPECT_TRUE(std::get<0>(mc.vals_).copied_);
  EXPECT_FALSE(std::get<0>(mc.vals_).moved_);
  EXPECT_TRUE(std::get<1>(mc.vals_).moved_);
  EXPECT_FALSE(std::get<1>(mc.vals_).copied_);
  EXPECT_TRUE(std::get<2>(mc.vals_).moved_);
  EXPECT_FALSE(std::get<2>(mc.vals_).copied_);
}

TEST(Cxx11, DeletedAssignOperator) {
  DeletedAssignOperator obj;
  DeletedAssignOperator obj2(obj);
  // Silly expectation just to encourage the compiler not to elide the test
  // wholesale.
  EXPECT_FALSE(&obj == &obj2);
}

TEST(Cxx11, OverrideKeyword) {
  TestOverrideTwiceDerived derived;
  TestOverrideDerived* derived_ptr = &derived;
  TestOverrideTwiceDerived* base_ptr = &derived;
  // Test to make sure the overriding actually worked and to make sure the
  // compiler doesn't elide the test wholesale.
  EXPECT_EQ(2, derived.Virtual());
  EXPECT_EQ(2, derived.PureVirtual());
  EXPECT_EQ(2, derived.PureVirtualConcreteOverride());
  EXPECT_EQ(2, derived_ptr->Virtual());
  EXPECT_EQ(2, derived_ptr->PureVirtual());
  EXPECT_EQ(2, derived_ptr->PureVirtualConcreteOverride());
  EXPECT_EQ(2, base_ptr->Virtual());
  EXPECT_EQ(2, base_ptr->PureVirtual());
  EXPECT_EQ(2, base_ptr->PureVirtualConcreteOverride());
}

// "Nullptr" isn't defined on QNX and NaCl but we #define it to NULL so that it
// can be used.
TEST(Cxx11, NullptrKeyword) {
  TestObj* o_ptr = nullptr;
  // Silly test just to make sure the compiler doesn't strip this test.
  EXPECT_TRUE(o_ptr == nullptr);
}

TEST(Cxx11, Tuple) {
  int int_val = 0;;
  double double_val = 0.0;

  std::tuple<int, char, double> a_tuple;
  // Test packing values into a tuple.
  a_tuple = std::make_tuple(10, 'a', 2.6);
  EXPECT_EQ(10, std::get<0>(a_tuple));
  EXPECT_EQ('a', std::get<1>(a_tuple));
  EXPECT_EQ(2.6, std::get<2>(a_tuple));

  // Test unpacking a tuple into values.
  std::tie(int_val, std::ignore, double_val) = a_tuple;
  EXPECT_EQ(10, int_val);
  EXPECT_EQ(2.6, double_val);

  // std::forward_as_tuple does not work on QNX or NaCl.
}

#if !defined(ION_PLATFORM_QNX)
// Fails to compile on QNX.
TEST(Cxx11, Lambdas) {
  int data[] = {-3, -2, 1, 4};
  std::vector<int> data_vec(data, data + 4);
  std::sort(data_vec.begin(), data_vec.end(),
            [](int a, int b) { return abs(a) < abs(b); });
  EXPECT_EQ(1, data_vec[0]);
  EXPECT_EQ(-2, data_vec[1]);
  EXPECT_EQ(-3, data_vec[2]);
  EXPECT_EQ(4, data_vec[3]);

  int a = 5;
  int b = 5;
  auto lambda = [a, &b] { b = a + b; };  // capture "a" by val, "b" by ref
  a = 100000;  // doesn't change captured "a" val
  b = 123;     // changes captured ref to "b"
  lambda();
  EXPECT_EQ(128, b);
}

// Fails to compile on QNX.
TEST(Cxx11, RangeBasedFor) {
  int total = 0;
  int data[] = {1, 2, 3, 4};

  // Test with std::vector.
  std::vector<int> data_vec(data, data + 4);
  for (auto num : data_vec)
    total += num;
  EXPECT_EQ(10, total);

  // Test with std::set.
  std::set<int> data_set;
  data_set.insert(data_vec.begin(), data_vec.end());
  for (auto num : data_set)
    total -= num;
  EXPECT_EQ(0, total);
}

TEST(Cxx11, Random) {
  std::default_random_engine int_generator;
  std::uniform_int_distribution<int> int_distribution(1, 6);
  int i = int_distribution(int_generator);
  EXPECT_GE(i, 1);
  EXPECT_LE(i, 6);

  std::default_random_engine double_generator;
  std::uniform_real_distribution<double> double_distribution(0, 1);
  double d = double_distribution(double_generator);
  EXPECT_GE(d, 0);
  EXPECT_LE(d, 1);
}

#endif  // !defined(ION_PLATFORM_QNX)
