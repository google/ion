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

#include <chrono> // NOLINT

#include "ion/base/setting.h"

#include "ion/base/logging.h"
#include "ion/base/threadspawner.h"
#include "ion/port/barrier.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace base {

namespace {

// A simple class to help verify that listeners are called when a setting's
// value changes.
class Listener {
 public:
  static void Callback(SettingBase* setting) {
    call_count_++;
  }

  // Returns if the callback was called and resets the called flag.
  static bool WasCalledOnce() {
    const bool called = call_count_ == 1;
    call_count_ = 0;
    return called;
  }

 private:
  static int call_count_;
};

// Another listener class to test that only the correct listener function is
// called.
class Listener2 {
 public:
  static void Callback(SettingBase* setting) {
    call_count_ = true;
  }

  // Returns if the callback was called and resets the called flag.
  static bool WasCalledOnce() {
    const bool called = call_count_ == 1;
    call_count_ = 0;
    return called;
  }

 private:
  static int call_count_;
};

// Yet one more listener class to check that std::bind functions are called
// correctly.
class Listener3 {
 public:
  Listener3() : call_count_(0) {}

  void Callback(SettingBase* setting) {
    call_count_++;
  }

  // Returns if the callback was called and resets the called flag.
  bool WasCalledOnce() {
    const bool called = call_count_ == 1;
    call_count_ = false;
    return called;
  }

 private:
  int call_count_;
};

int Listener::call_count_ = 0;
int Listener2::call_count_ = 0;

class BGThreadSetting {
 public:
  BGThreadSetting()
    : barrier_(2),
      spawner_("bgthread_setting", std::bind(&BGThreadSetting::Func, this)) {}

  bool Func() {
    barrier_.Wait();  // Make sure the thread has started.
    Setting<bool> bgthread_setting("bgthread_setting", true, "dummy");
    barrier_.Wait();
    return true;
  }

  void Wait() {
    barrier_.Wait();
  }

 private:
  port::Barrier barrier_;
  ThreadSpawner spawner_;
};

}  // anonymous namespace

// This test does not make any explicit tests; it is designed to verify that
// Settings can be constructed in multiple threads without ThreadSanitizer
// warnings. Therefore, this test is only useful when run with ThreadSanitizer.
#if !defined(ION_PLATFORM_ASMJS)
TEST(Setting, MultiThreaded) {
  BGThreadSetting bgthread;

  bgthread.Wait();  // Make sure the thread has started
  Setting<bool> mainthread_setting("mainthread_setting", true, "dummy");
  bgthread.Wait();  // Make sure the thread has constructed its setting.
}
#endif

TEST(Setting, BasicUsage) {
  // Int.
  Setting<int> int_setting("int", 12, "an int");
  int* mutable_int = int_setting.GetMutableValue();
  EXPECT_EQ(12, *mutable_int);
  EXPECT_EQ(12, int_setting);
  EXPECT_EQ("an int", int_setting.GetDocString());
  EXPECT_EQ(12, int_setting.GetValue());
  int_setting = 21;
  EXPECT_EQ(21, int_setting);
  int_setting.SetValue(42);
  EXPECT_EQ(42, int_setting);
  const int test_int = int_setting;
  EXPECT_EQ(42, test_int);
  EXPECT_EQ(42, *mutable_int);

  EXPECT_EQ("42", int_setting.ToString());
  EXPECT_TRUE(int_setting.FromString("123"));
  EXPECT_EQ(123, int_setting);

  // Check that non-int strings fail and do not change the value.
  EXPECT_FALSE(int_setting.FromString("abc"));
  EXPECT_EQ(123, int_setting);
  // The following will work but have truncated values.
  EXPECT_TRUE(int_setting.FromString("4.56"));
  EXPECT_EQ(4, int_setting);
  EXPECT_TRUE(int_setting.FromString("7,89"));
  EXPECT_EQ(7, int_setting);

  // Double.
  Setting<double> double_setting("double", 12.34, "a double");
  double* mutable_double = double_setting.GetMutableValue();
  EXPECT_EQ(12.34, static_cast<double>(double_setting));
  EXPECT_EQ(12.34, *mutable_double);
  EXPECT_EQ("a double", double_setting.GetDocString());
  EXPECT_EQ(12.34, double_setting.GetValue());
  double_setting = 23.56;
  EXPECT_EQ(23.56, static_cast<double>(double_setting));
  double_setting.SetValue(42.12);
  EXPECT_EQ(42.12, static_cast<double>(double_setting));
  const double test_double = double_setting;
  EXPECT_EQ(42.12, test_double);
  EXPECT_EQ(42.12, *mutable_double);

  EXPECT_EQ("42.12", double_setting.ToString());
  EXPECT_TRUE(double_setting.FromString("123"));
  EXPECT_EQ(123., static_cast<double>(double_setting));
  EXPECT_TRUE(double_setting.FromString("123.456"));
  EXPECT_EQ(123.456, static_cast<double>(double_setting));
  EXPECT_TRUE(double_setting.FromString("3.14159e2"));
  EXPECT_EQ(314.159, static_cast<double>(double_setting));
  EXPECT_TRUE(double_setting.FromString("281.8E-2"));
  EXPECT_EQ(2.818, static_cast<double>(double_setting));

  EXPECT_FALSE(double_setting.FromString("abc"));
  EXPECT_EQ(2.818, static_cast<double>(double_setting));
  EXPECT_TRUE(double_setting.FromString("4-.56"));
  EXPECT_EQ(4., static_cast<double>(double_setting));
  EXPECT_TRUE(double_setting.FromString("7.23,89"));
  EXPECT_EQ(7.23, static_cast<double>(double_setting));

  // Chrono Duration
  typedef std::chrono::duration<int64_t, std::nano> nsduration;
  Setting<nsduration> ns_setting("ns", nsduration(1978), "a duration");
  nsduration* mutable_dur = ns_setting.GetMutableValue();
  EXPECT_EQ(nsduration(1978), ns_setting);
  EXPECT_EQ(nsduration(1978), *mutable_dur);
  EXPECT_EQ("a duration", ns_setting.GetDocString());
  EXPECT_EQ(nsduration(1978), ns_setting.GetValue());
  ns_setting = nsduration(1980);
  EXPECT_EQ(nsduration(1980), ns_setting);
  ns_setting.SetValue(nsduration(2000));
  EXPECT_EQ(nsduration(2000), ns_setting);
}

TEST(Setting, AtomicSettings) {
  // Int.
  Setting<std::atomic<int> > int_setting("int", 12, "an int");
  std::atomic<int>* mutable_int = int_setting.GetMutableValue();
  EXPECT_EQ(12, int_setting);
  EXPECT_EQ(12, static_cast<int>(*mutable_int));
  EXPECT_EQ("an int", int_setting.GetDocString());
  EXPECT_EQ(12, int_setting.GetValue());
  int_setting = 21;
  EXPECT_EQ(21, int_setting);
  int_setting.SetValue(42);
  EXPECT_EQ(42, int_setting);
  const int test_int = int_setting;
  EXPECT_EQ(42, test_int);
  EXPECT_EQ(42, static_cast<int>(*mutable_int));

  EXPECT_EQ("42", int_setting.ToString());
  EXPECT_TRUE(int_setting.FromString("123"));
  EXPECT_EQ(123, int_setting);

  // Check that non-int strings fail and do not change the value.
  EXPECT_FALSE(int_setting.FromString("abc"));
  EXPECT_EQ(123, int_setting);
  // The following will work but have truncated values.
  EXPECT_TRUE(int_setting.FromString("4.56"));
  EXPECT_EQ(4, int_setting);
  EXPECT_TRUE(int_setting.FromString("7,89"));
  EXPECT_EQ(7, int_setting);

  // Bool.
  Setting<std::atomic<bool> > bool_setting("bool", false, "a bool");
  std::atomic<bool>* mutable_bool = bool_setting.GetMutableValue();
  EXPECT_FALSE(static_cast<bool>(bool_setting));
  EXPECT_FALSE(static_cast<bool>(*mutable_bool));
  EXPECT_EQ("a bool", bool_setting.GetDocString());
  EXPECT_FALSE(bool_setting.GetValue());
  bool_setting = true;
  EXPECT_TRUE(static_cast<bool>(bool_setting));
  bool_setting = false;
  EXPECT_FALSE(static_cast<bool>(bool_setting));
  bool_setting.SetValue(true);
  EXPECT_TRUE(static_cast<bool>(bool_setting));
  const bool test_bool = bool_setting;
  EXPECT_TRUE(test_bool);
  EXPECT_TRUE(static_cast<bool>(*mutable_bool));

  EXPECT_EQ("true", bool_setting.ToString());
  EXPECT_TRUE(bool_setting.FromString("false"));
  EXPECT_FALSE(static_cast<bool>(bool_setting));
  EXPECT_TRUE(bool_setting.FromString("true"));
  EXPECT_TRUE(static_cast<bool>(bool_setting));

  // Check that non-bool strings fail and do not change the value.
  EXPECT_FALSE(bool_setting.FromString("abc"));
  EXPECT_TRUE(static_cast<bool>(bool_setting));
  EXPECT_FALSE(bool_setting.FromString("4.56"));
  EXPECT_TRUE(static_cast<bool>(bool_setting));
  EXPECT_FALSE(bool_setting.FromString("7,89"));
  EXPECT_TRUE(static_cast<bool>(bool_setting));
}

TEST(Setting, TypeDescriptor) {
  Setting<double> double_setting("double", 12.34, "a double");
  EXPECT_TRUE(double_setting.GetTypeDescriptor().empty());
  double_setting.SetTypeDescriptor("Some string");
  EXPECT_EQ("Some string", double_setting.GetTypeDescriptor());

  // Bool settings should have the descriptor set automatically.
  Setting<bool> bool_setting("bool", true, "a bool");
  EXPECT_EQ("bool", bool_setting.GetTypeDescriptor());

  Setting<std::atomic<bool> > atomic_bool_setting("atomicbool", false,
                                                 "an atomic bool");
  EXPECT_EQ("bool", atomic_bool_setting.GetTypeDescriptor());
}

TEST(Setting, Listeners) {
  Setting<int> int_setting("int", 12);
  int_setting.RegisterListener("callback", Listener::Callback);
  int_setting.RegisterListener("callback2", Listener2::Callback);
  Listener3 listener;
  int_setting.RegisterListener(
      "callback3",
      std::bind(&Listener3::Callback, &listener, std::placeholders::_1));

  EXPECT_EQ(12, int_setting);
  EXPECT_EQ(12, int_setting.GetValue());
  EXPECT_FALSE(Listener::WasCalledOnce());
  EXPECT_FALSE(Listener2::WasCalledOnce());
  EXPECT_FALSE(listener.WasCalledOnce());
  int_setting.SetValue(42);
  EXPECT_EQ(42, int_setting);
  EXPECT_TRUE(Listener::WasCalledOnce());
  EXPECT_TRUE(Listener2::WasCalledOnce());
  EXPECT_TRUE(listener.WasCalledOnce());

  // Disable listeners.
  int_setting.EnableListener("callback2", false);
  int_setting.EnableListener("callback3", false);
  int_setting.SetValue(31);
  EXPECT_TRUE(Listener::WasCalledOnce());
  EXPECT_FALSE(Listener2::WasCalledOnce());
  EXPECT_FALSE(listener.WasCalledOnce());
  int_setting.EnableListener("callback3", true);
  int_setting.SetValue(32);
  EXPECT_EQ(32, int_setting);
  EXPECT_TRUE(Listener::WasCalledOnce());
  EXPECT_FALSE(Listener2::WasCalledOnce());
  EXPECT_TRUE(listener.WasCalledOnce());
  int_setting.EnableListener("callback2", true);

  int_setting.UnregisterListener("callback");
  int_setting.SetValue(26);
  EXPECT_EQ(26, int_setting);
  EXPECT_FALSE(Listener::WasCalledOnce());
  EXPECT_TRUE(Listener2::WasCalledOnce());
  EXPECT_TRUE(listener.WasCalledOnce());

  int_setting.UnregisterListener("callback3");
  int_setting.SetValue(1234);
  EXPECT_EQ(1234, int_setting);
  EXPECT_FALSE(Listener::WasCalledOnce());
  EXPECT_TRUE(Listener2::WasCalledOnce());
  EXPECT_FALSE(listener.WasCalledOnce());

  // Nothing happens if you try to remove a non-existent listener.
  int_setting.UnregisterListener("not a listener");
  int_setting.SetValue(123);
  EXPECT_EQ(123, int_setting);
  EXPECT_FALSE(Listener::WasCalledOnce());
  EXPECT_TRUE(Listener2::WasCalledOnce());
  EXPECT_FALSE(listener.WasCalledOnce());

  // Listeners can be overridden.
  int_setting.RegisterListener("callback2", Listener::Callback);
  int_setting.UnregisterListener("not a listener");
  int_setting.SetValue(12);
  EXPECT_EQ(12, int_setting);
  EXPECT_TRUE(Listener::WasCalledOnce());
  EXPECT_FALSE(Listener2::WasCalledOnce());
  EXPECT_FALSE(listener.WasCalledOnce());
}

TEST(Setting, SettingGroup) {
  // Create a group.
  SettingGroup group("group/group2");
  EXPECT_EQ("group/group2", group.GetGroupName());

  // Add some settings to the group.
  Setting<int> int_setting(&group, "int", 12);
  EXPECT_EQ("group/group2/int", int_setting.GetName());
  Setting<float> float_setting(&group, "float", 1.2f);
  EXPECT_EQ("group/group2/float", float_setting.GetName());

  // Test with trailing slashes.
  SettingGroup group3("group/group3/");
  EXPECT_EQ("group/group3", group3.GetGroupName());
  SettingGroup group4("group4///");
  EXPECT_EQ("group4", group4.GetGroupName());
  SettingGroup empty("///");
  EXPECT_EQ("", empty.GetGroupName());

  // Test groups within groups.
  SettingGroup group5("group5");
  SettingGroup group6(group5, "//group6/");
  EXPECT_EQ("group5/group6", group6.GetGroupName());
}

TEST(EnvironmentSetting, EnvironmentSettings) {
  EnvironmentSetting<int> int_setting("int", "int_value", 42, "int env");
  EXPECT_EQ(42, int_setting);
  EXPECT_EQ("int env", int_setting.GetDocString());

  port::SetEnvironmentVariableValue("int_value", "23");
  EnvironmentSetting<int> int_setting2("int2", "int_value", 42, "");
  EXPECT_EQ("", int_setting2.GetDocString());
  EXPECT_EQ(23, int_setting2);

  EnvironmentSetting<double> double_setting(
      "double", "double_value", 123.32, "");
  EXPECT_EQ(123.32, static_cast<double>(double_setting));
  port::SetEnvironmentVariableValue("double_value", "41.143");
  EnvironmentSetting<double> double_setting2(
      "double2", "double_value", 123.32, "");
  EXPECT_EQ(41.143, static_cast<double>(double_setting2));

  std::vector<int> vec;
  vec.push_back(16);
  vec.push_back(10);
  vec.push_back(4);
  EnvironmentSetting<std::vector<int> > vec_setting(
      "vec", "vec_value", vec, "");
  EXPECT_EQ(vec, vec_setting);

  std::vector<int> vec2;
  vec2.push_back(123);
  vec2.push_back(9);
  vec2.push_back(72);
  port::SetEnvironmentVariableValue("vec_value", "{123, 9, 72}");
  EnvironmentSetting<std::vector<int> > vec_setting2(
      "vec2", "vec_value", vec, "");
  EXPECT_EQ(vec2, vec_setting2);
}

TEST(ScopedSettingValue, ScopedSettingValue) {
  Setting<int> setting("mysetting", 5);
  EXPECT_EQ(5, setting);
  {
    ScopedSettingValue<int> scoped_setting(&setting, 7);
    EXPECT_EQ(7, setting);
  }
  // We should be back to the original value after ScopedSettingValue
  // is destroyed.
  EXPECT_EQ(5, setting);
}

}  // namespace base
}  // namespace ion
