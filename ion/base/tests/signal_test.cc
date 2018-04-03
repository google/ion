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

#include "ion/base/signal.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace base {

class SignalTest : public ::testing::Test {
 public:
  SignalTest() : int_value_(0), float_value_(0.f) {}

  void SlotOne(int a, float b) {
    int_value_ = a;
    float_value_ = b;
  }
  void SlotTwo(int a) {
    int_value_ = a + 1;
  }

 protected:
  int int_value_;
  float float_value_;
};

TEST_F(SignalTest, Empty) {
  // Invoking a signal without any slots should not crash or deadlock.
  Signal<void(char)> signal;
  signal.Emit('a');
}

TEST_F(SignalTest, SlotOrder) {
  using std::placeholders::_1;
  using std::placeholders::_2;

  // Test that the slots are invoked in the order of connection.
  Signal<void(int, float)> signal;
  Connection connection1 =
      signal.Connect(std::bind(&SignalTest::SlotOne, this, _1, _2));
  Connection connection2 =
      signal.Connect(std::bind(&SignalTest::SlotTwo, this, _1));
  signal.Emit(4, 5.0f);
  EXPECT_EQ(5, int_value_);
  EXPECT_EQ(5.0f, float_value_);

  connection1.Disconnect();
  signal.Emit(10, 20.0f);
  EXPECT_EQ(11, int_value_);
  EXPECT_EQ(5.f, float_value_);

  connection2.Disconnect();
  signal.Emit(123, 456.0f);
  EXPECT_EQ(11, int_value_);
  EXPECT_EQ(5.0f, float_value_);
}

TEST_F(SignalTest, ConnectionDestruction) {
  {
    // Destroying an empty Connection should not crash.
    Connection connection;
  }
  // Destroying the connection after its signal should also not crash.
  Connection connection;
  {
    using std::placeholders::_1;
    using std::placeholders::_2;
    Signal<void(int, float)> signal;
    connection = signal.Connect(std::bind(&SignalTest::SlotOne, this, _1, _2));
    signal.Emit(7, 8.f);
    EXPECT_EQ(7, int_value_);
    EXPECT_EQ(8.f, float_value_);
  }
}

TEST_F(SignalTest, ConnectionDetach) {
  using std::placeholders::_1;
  using std::placeholders::_2;

  Signal<void(int, float)> signal;
  signal.Connect(std::bind(&SignalTest::SlotOne, this, _1, _2)).Detach();
  signal.Emit(-20, 15.f);
  EXPECT_EQ(-20, int_value_);
  EXPECT_EQ(15.f, float_value_);
}

TEST_F(SignalTest, DisconnectionFromSafeEmit) {
  using std::placeholders::_1;

  Signal<void(int)> signal;
  Connection connection;
  connection = signal.Connect([&connection, this](int v) {
    float_value_ = static_cast<float>(v);
    connection.Disconnect();
  });
  Connection connection2 =
      signal.Connect(std::bind(&SignalTest::SlotTwo, this, _1));

  signal.SafeEmit(9);
  signal.Emit(10);
  EXPECT_EQ(11, int_value_);
  EXPECT_EQ(9.f, float_value_);
}

}  // namespace base
}  // namespace ion
