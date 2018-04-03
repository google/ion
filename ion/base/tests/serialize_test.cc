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

#include "ion/base/serialize.h"

// Include some STL containers.
#include <algorithm>
#include <chrono> // NOLINT
#include <deque>
#include <list>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ion/base/stringutils.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace base {

namespace {

// This is a stripped-down version of math::Vector2f since ionbase cannot depend
// on ionmath.
struct Vector2f {
  Vector2f() : x(0.f), y(0.f) {}
  Vector2f(float x_in, float y_in) : x(x_in), y(y_in) {}

  friend std::ostream& operator<<(std::ostream& out, const Vector2f& v) {
    out << "V[" << v.x << ", " << v.y << "]";
    return out;
  }

  friend std::istream& operator>>(std::istream& in, Vector2f& thiz) {
    Vector2f v;
    if (GetExpectedString(in, "V[") &&
        (in >> v.x >> GetExpectedChar<','> >> v.y >> GetExpectedChar<']'>))
      thiz = v;
    return in;
  }

  friend bool operator==(const Vector2f& a, const Vector2f& b) {
    return a.x == b.x && a.y == b.y;
  }
  float x, y;
};

// Helper function to reset a stream to a new string.
void ResetStream(std::istringstream* in, const std::string& str) {
  in->str(str);
  in->clear();
}

// Splits a string containing comma-separated elements and returns them in a
// sorted vector.
const std::vector<std::string> SortContainerStrings(const std::string& str) {
  // Check the start and end of the string.
  EXPECT_TRUE(StartsWith(str, "{ "));
  EXPECT_TRUE(EndsWith(str, " }"));
  // Strip out the beginning and end.
  std::vector<std::string> strings =
      SplitString(str.substr(1, str.length() - 3U), ",");
  std::sort(strings.begin(), strings.end());
  return strings;
}

}  // anonymous namespace

TEST(Serialize, PodTypes) {
  std::istringstream in;

  // Int.
  int i = 42;
  EXPECT_EQ("42", ValueToString(i));
  ResetStream(&in, "123");
  EXPECT_TRUE(StringToValue(in, &i));
  EXPECT_EQ(123, i);

  // Check that non-int strings fail and do not change the value.
  ResetStream(&in, "abc");
  EXPECT_FALSE(StringToValue(in, &i));
  EXPECT_EQ(123, i);
  // The following will work but have truncated values.
  ResetStream(&in, "4.56");
  EXPECT_TRUE(StringToValue(in, &i));
  EXPECT_EQ(4, i);
  ResetStream(&in, "7,89");
  EXPECT_TRUE(StringToValue(in, &i));
  EXPECT_EQ(7, i);

  // Double.
  double d = 42.12;
  EXPECT_EQ("42.12", ValueToString(d));
  ResetStream(&in, "123");
  EXPECT_TRUE(StringToValue(in, &d));
  EXPECT_EQ(123., d);
  ResetStream(&in, "123.456");
  EXPECT_TRUE(StringToValue(in, &d));
  EXPECT_EQ(123.456, d);
  ResetStream(&in, "3.14159e2");
  EXPECT_TRUE(StringToValue(in, &d));
  EXPECT_EQ(314.159, d);
  ResetStream(&in, "281.8E-2");
  EXPECT_TRUE(StringToValue(in, &d));
  EXPECT_EQ(2.818, d);

  ResetStream(&in, "abc");
  EXPECT_FALSE(StringToValue(in, &d));
  EXPECT_EQ(2.818, d);
  ResetStream(&in, "4-.56");
  EXPECT_TRUE(StringToValue(in, &d));
  EXPECT_EQ(4., d);
  ResetStream(&in, "7.23,89");
  EXPECT_TRUE(StringToValue(in, &d));
  EXPECT_EQ(7.23, d);

  // Bool.
  bool b = false;
  EXPECT_EQ("false", ValueToString(b));
  ResetStream(&in, "true");
  EXPECT_TRUE(StringToValue(in, &b));
  EXPECT_TRUE(b);
  EXPECT_EQ("true", ValueToString(b));
  ResetStream(&in, "false");
  EXPECT_TRUE(StringToValue(in, &b));
  EXPECT_FALSE(b);
  ResetStream(&in, "true");
  EXPECT_TRUE(StringToValue(in, &b));
  EXPECT_TRUE(b);

  ResetStream(&in, "abc");
  EXPECT_FALSE(StringToValue(in, &b));
  EXPECT_TRUE(b);
  ResetStream(&in, "1");
  EXPECT_FALSE(StringToValue(in, &b));
  EXPECT_TRUE(b);
  ResetStream(&in, "0");
  EXPECT_FALSE(StringToValue(in, &b));
  EXPECT_TRUE(b);
}

void VerifyString(std::istringstream* in, const std::string& str, int line) {
  std::string new_str;

  SCOPED_TRACE(testing::Message() << "Verifying " << str << " from line "
                                  << line);
  ResetStream(in, ValueToString(str));
  EXPECT_TRUE(StringToValue(*in, &new_str));
  EXPECT_EQ(str, new_str);
}

TEST(Serialize, String) {
  std::istringstream in;

  EXPECT_EQ("\"string\"", ValueToString(std::string("string")));
  EXPECT_EQ("\"two words\"",
            ValueToString(std::string("two words")));
  EXPECT_EQ("\"with \\\" a quote\"",
            ValueToString(std::string("with \" a quote")));
  EXPECT_EQ("\"with \\\\\\\" a quote\"",
            ValueToString(std::string("with \\\" a quote")));
  EXPECT_EQ("\"red fish blue fish\"",
            ValueToString(std::string("red fish blue fish")));

  VerifyString(&in, "with \" a quote", __LINE__);

  std::string str("string");
  ResetStream(&in, "\"red fish blue fish");
  EXPECT_FALSE(StringToValue(in, &str));
  ResetStream(&in, "red fish blue fish\"");
  EXPECT_FALSE(StringToValue(in, &str));
  ResetStream(&in, "red fish\" blue fish\"");
  EXPECT_FALSE(StringToValue(in, &str));
  ResetStream(&in, "");
  EXPECT_FALSE(StringToValue(in, &str));
  EXPECT_EQ("string", str);

  ResetStream(&in, "\"red fish\"\"blue fish\"");
  EXPECT_TRUE(StringToValue(in, &str));
  EXPECT_EQ("red fish", str);
  EXPECT_TRUE(StringToValue(in, &str));
  EXPECT_EQ("blue fish", str);

  ResetStream(&in, "\"\"");
  EXPECT_TRUE(StringToValue(in, &str));
  EXPECT_EQ("", str);

  // This is not a valid string because it is not closed, the non-escaped string
  // is "foo\" (with no ending double quote).
  ResetStream(&in, "\"foo\\\"");
  EXPECT_FALSE(StringToValue(in, &str));
  EXPECT_EQ("", str);

  // The non-escaped string is "foo\"".
  ResetStream(&in, "\"foo\\\"\"");
  EXPECT_TRUE(StringToValue(in, &str));
  EXPECT_EQ("foo\"", str);

  // The non-escaped string is "foo\\".
  ResetStream(&in, "\"foo\\\\\"");
  EXPECT_TRUE(StringToValue(in, &str));
  EXPECT_EQ("foo\\", str);

  // The non-escaped string is "foo\"\\".
  ResetStream(&in, "\"foo\\\"\\\\\"");
  EXPECT_TRUE(StringToValue(in, &str));
  EXPECT_EQ("foo\"\\", str);
}

TEST(Serialize, StlDeque) {
  std::deque<Vector2f> queue;
  std::istringstream in;

  queue.push_back(Vector2f(3.4f, 5.6f));
  queue.push_front(Vector2f(0.1f, 1.2f));
  queue.push_back(Vector2f(7.8f, 9.9f));
  EXPECT_EQ("{ V[0.1, 1.2], V[3.4, 5.6], V[7.8, 9.9] }", ValueToString(queue));

  ResetStream(&in, "{ V[1.2, 3.4] , V[5.6, 7.8] }");
  EXPECT_TRUE(StringToValue(in, &queue));
  EXPECT_EQ(2U, queue.size());
  EXPECT_EQ(Vector2f(1.2f, 3.4f), queue[0]);
  EXPECT_EQ(Vector2f(5.6f, 7.8f), queue[1]);

  ResetStream(&in, "{ V[1.2, 3.4, V[5.6, 7.8] }");
  EXPECT_FALSE(StringToValue(in, &queue));
  ResetStream(&in, "{ V[1.2, 3.4] V[5.6, 7.8] }");
  EXPECT_FALSE(StringToValue(in, &queue));
  ResetStream(&in, "{ [1.2, 3.4] V[5.6, 7.8] }");
  EXPECT_FALSE(StringToValue(in, &queue));
  ResetStream(&in, "{ }");
  EXPECT_FALSE(StringToValue(in, &queue));

  // Check that nothing changed.
  EXPECT_EQ(2U, queue.size());
  EXPECT_EQ(Vector2f(1.2f, 3.4f), queue[0]);
  EXPECT_EQ(Vector2f(5.6f, 7.8f), queue[1]);
}

TEST(Serialize, StlList) {
  std::list<std::string> lst;
  std::istringstream in;

  lst.push_back("string 2");
  lst.push_front("string 1");
  lst.push_back("string 3");
  EXPECT_EQ("{ \"string 1\", \"string 2\", \"string 3\" }", ValueToString(lst));

  ResetStream(&in, "{ \"one\", \"two words\", \"words and a \\\" \" }");
  EXPECT_TRUE(StringToValue(in, &lst));
  EXPECT_EQ("one", *lst.begin());
  EXPECT_EQ("two words", *(++lst.begin()));
  EXPECT_EQ("words and a \" ", *(++(++lst.begin())));

  ResetStream(&in, "{ \"one\", \"two words\", \"words and a }");
  EXPECT_FALSE(StringToValue(in, &lst));
  ResetStream(&in, "{ \"one\", \"two words\" \"words and a\" }");
  EXPECT_FALSE(StringToValue(in, &lst));
  ResetStream(&in, "{ \"one\", \"two words\", \"words and a\" ");
  EXPECT_FALSE(StringToValue(in, &lst));
  ResetStream(&in, "{ \"one\", \"two words\", words and a\" } ");
  EXPECT_FALSE(StringToValue(in, &lst));
  ResetStream(&in, " \"one\", \"two words\", \"words and a\" } ");
  EXPECT_FALSE(StringToValue(in, &lst));
  ResetStream(&in, "{ one\", \"two words\", \"words and a\" } ");
  EXPECT_FALSE(StringToValue(in, &lst));

  // Check that nothing changed.
  EXPECT_EQ("one", *lst.begin());
  EXPECT_EQ("two words", *(++lst.begin()));
  EXPECT_EQ("words and a \" ", *(++(++lst.begin())));
}

TEST(Serialize, StlMap) {
  std::map<std::string, int> mp;
  std::istringstream in;

  mp["key 1"] = 1;
  mp["key 2"] = 2;
  mp["key 3"] = 3;
  EXPECT_EQ("{ \"key 1\" : 1, \"key 2\" : 2, \"key 3\" : 3 }",
            ValueToString(mp));

  ResetStream(&in, "{ \"beans\" : 2, \"slaw\" : 5, \"fried chicken\" : 12 }");
  EXPECT_TRUE(StringToValue(in, &mp));
  EXPECT_EQ(3U, mp.size());
  EXPECT_EQ(2, mp["beans"]);
  EXPECT_EQ(5, mp["slaw"]);
  EXPECT_EQ(12, mp["fried chicken"]);

  ResetStream(&in, "{ \"one\": 1 , \"two\" :2 }");
  EXPECT_TRUE(StringToValue(in, &mp));
  EXPECT_EQ(2U, mp.size());
  EXPECT_EQ(1, mp["one"]);
  EXPECT_EQ(2, mp["two"]);

  ResetStream(&in, "{ \"one\": 1 \"two\" :2 }");
  EXPECT_FALSE(StringToValue(in, &mp));
  ResetStream(&in, "{ \"one\": 1 , \"two\" 2 }");
  EXPECT_FALSE(StringToValue(in, &mp));
  ResetStream(&in, "{ \"one\" 1 , \"two\" :2 }");
  EXPECT_FALSE(StringToValue(in, &mp));
  ResetStream(&in, " \"one\": 1 , \"two\" :2 }");
  EXPECT_FALSE(StringToValue(in, &mp));
  ResetStream(&in, "{ \"one\": 1 , \"two\" :2 ");
  EXPECT_FALSE(StringToValue(in, &mp));
  ResetStream(&in, "{ \"one\": 1 , two :2 }");
  EXPECT_FALSE(StringToValue(in, &mp));
  ResetStream(&in, "{ }");
  EXPECT_FALSE(StringToValue(in, &mp));

  EXPECT_EQ(2U, mp.size());
  EXPECT_EQ(1, mp["one"]);
  EXPECT_EQ(2, mp["two"]);
}

TEST(Serialize, StlUnorderedMap) {
  std::unordered_map<std::string, int> mp;
  std::istringstream in;

  mp["key 1"] = 1;
  mp["key 2"] = 2;
  mp["key 3"] = 3;
  // The keys could be in any order since the map is unordered.
  const std::vector<std::string> strings =
      SortContainerStrings(ValueToString(mp));
  EXPECT_EQ(3U, strings.size());
  EXPECT_EQ(" \"key 1\" : 1", strings[0]);
  EXPECT_EQ(" \"key 2\" : 2", strings[1]);
  EXPECT_EQ(" \"key 3\" : 3", strings[2]);

  ResetStream(&in, "{ \"beans\" : 2, \"slaw\" : 5, \"fried chicken\" : 12 }");
  EXPECT_TRUE(StringToValue(in, &mp));
  EXPECT_EQ(3U, mp.size());
  EXPECT_EQ(2, mp["beans"]);
  EXPECT_EQ(5, mp["slaw"]);
  EXPECT_EQ(12, mp["fried chicken"]);

  ResetStream(&in, "{ \"one\": 1 , \"two\" :2 }");
  EXPECT_TRUE(StringToValue(in, &mp));
  EXPECT_EQ(2U, mp.size());
  EXPECT_EQ(1, mp["one"]);
  EXPECT_EQ(2, mp["two"]);

  ResetStream(&in, "{ \"alpha\": 3 \"beta\" :2 }");
  EXPECT_FALSE(StringToValue(in, &mp));
  ResetStream(&in, "{ \"alpha\": 3 , \"beta\" 2 }");
  EXPECT_FALSE(StringToValue(in, &mp));
  ResetStream(&in, "{ \"alpha\" 3 , \"beta\" :2 }");
  EXPECT_FALSE(StringToValue(in, &mp));
  ResetStream(&in, " \"alpha\": 3 , \"beta\" :2 }");
  EXPECT_FALSE(StringToValue(in, &mp));
  ResetStream(&in, "{ \"alpha\": 3 , \"beta\" :2 ");
  EXPECT_FALSE(StringToValue(in, &mp));
  ResetStream(&in, "{ \"alpha\": 3 , beta :2 }");
  EXPECT_FALSE(StringToValue(in, &mp));
  ResetStream(&in, "{ }");
  EXPECT_FALSE(StringToValue(in, &mp));

  // Check that nothing changed.
  EXPECT_EQ(2U, mp.size());
  EXPECT_EQ(1, mp["one"]);
  EXPECT_EQ(2, mp["two"]);
}

TEST(Serialize, StlSet) {
  std::set<float> st;
  std::istringstream in;

  st.insert(1.f);
  st.insert(11.f);
  st.insert(111.f);
  EXPECT_EQ("{ 1, 11, 111 }", ValueToString(st));

  ResetStream(&in, "{ 1.2, 2.1, 1.2 }");
  EXPECT_TRUE(StringToValue(in, &st));
  EXPECT_EQ(2U, st.size());
  EXPECT_EQ(1U, st.count(1.2f));
  EXPECT_EQ(1U, st.count(2.1f));

  ResetStream(&in, "{3.4 , 5.6 ,7.8}");
  EXPECT_TRUE(StringToValue(in, &st));
  EXPECT_EQ(3U, st.size());
  EXPECT_EQ(1U, st.count(3.4f));
  EXPECT_EQ(1U, st.count(5.6f));
  EXPECT_EQ(1U, st.count(7.8f));

  ResetStream(&in, "{ 1.23 , 4.56 ");
  EXPECT_FALSE(StringToValue(in, &st));
  ResetStream(&in, "1.23 , 4.56 }");
  EXPECT_FALSE(StringToValue(in, &st));
  ResetStream(&in, "{ 1.23 4.56 }");
  EXPECT_FALSE(StringToValue(in, &st));
  ResetStream(&in, "{ abc , 4.56 }");
  EXPECT_FALSE(StringToValue(in, &st));

  // Check that nothing changed.
  EXPECT_EQ(3U, st.size());
  EXPECT_EQ(1U, st.count(3.4f));
  EXPECT_EQ(1U, st.count(5.6f));
  EXPECT_EQ(1U, st.count(7.8f));
}

TEST(Serialize, StlUnorderedSet) {
  std::unordered_set<double> st;
  std::istringstream in;

  st.insert(1.);
  st.insert(11.);
  st.insert(111.);
  // The keys could be in any order since the set is unordered.
  const std::vector<std::string> strings =
      SortContainerStrings(ValueToString(st));
  EXPECT_EQ(3U, strings.size());
  EXPECT_EQ(" 1", strings[0]);
  EXPECT_EQ(" 11", strings[1]);
  EXPECT_EQ(" 111", strings[2]);

  ResetStream(&in, "{ 1.2, 2.1, 1.2 }");
  EXPECT_TRUE(StringToValue(in, &st));
  EXPECT_EQ(2U, st.size());
  EXPECT_EQ(1U, st.count(1.2));
  EXPECT_EQ(1U, st.count(2.1));

  ResetStream(&in, "{3.4 , 5.6 ,7.8}");
  EXPECT_TRUE(StringToValue(in, &st));
  EXPECT_EQ(3U, st.size());
  EXPECT_EQ(1U, st.count(3.4));
  EXPECT_EQ(1U, st.count(5.6));
  EXPECT_EQ(1U, st.count(7.8));

  ResetStream(&in, "{ 1.23 , 4.56 ");
  EXPECT_FALSE(StringToValue(in, &st));
  ResetStream(&in, "1.23 , 4.56 }");
  EXPECT_FALSE(StringToValue(in, &st));
  ResetStream(&in, "{ 1.23 4.56 }");
  EXPECT_FALSE(StringToValue(in, &st));
  ResetStream(&in, "{ abc , 4.56 }");
  EXPECT_FALSE(StringToValue(in, &st));

  // Check that nothing changed.
  EXPECT_EQ(3U, st.size());
  EXPECT_EQ(1U, st.count(3.4));
  EXPECT_EQ(1U, st.count(5.6));
  EXPECT_EQ(1U, st.count(7.8));
}

TEST(Serialize, StlVector) {
  std::vector<int> vec;
  std::istringstream in;

  vec.push_back(765);
  vec.push_back(4);
  vec.push_back(22);
  EXPECT_EQ("{ 765, 4, 22 }", ValueToString(vec));

  ResetStream(&in, "{ 2, 8, 17 }");
  EXPECT_TRUE(StringToValue(in, &vec));
  EXPECT_EQ(3U, vec.size());
  EXPECT_EQ(2, vec[0]);
  EXPECT_EQ(8, vec[1]);
  EXPECT_EQ(17, vec[2]);

  ResetStream(&in, "{ 1 2, 3 }");
  EXPECT_FALSE(StringToValue(in, &vec));
  ResetStream(&in, "{ 1, 2, 3 ");
  EXPECT_FALSE(StringToValue(in, &vec));
  ResetStream(&in, "1, 2, 3 }");
  EXPECT_FALSE(StringToValue(in, &vec));

  // Check that nothing changed.
  EXPECT_EQ(3U, vec.size());
  EXPECT_EQ(2, vec[0]);
  EXPECT_EQ(8, vec[1]);
  EXPECT_EQ(17, vec[2]);
}

TEST(Serialize, StringConvenience) {
  int i;
  EXPECT_TRUE(StringToValue(std::string("-14"), &i));
  EXPECT_EQ(-14, i);

  double d;
  EXPECT_TRUE(StringToValue(std::string("123.5"), &d));
  EXPECT_EQ(123.5, d);

  EXPECT_FALSE(StringToValue(std::string("x43"), &i));

  std::vector<int> vec;
  EXPECT_TRUE(StringToValue(std::string("{ 3, 5, 8 }"), &vec));
  EXPECT_EQ(3, vec[0]);
  EXPECT_EQ(5, vec[1]);
  EXPECT_EQ(8, vec[2]);
}

TEST(Serialize, Chrono) {
  using std::chrono::nanoseconds;
  using std::chrono::microseconds;
  using std::chrono::milliseconds;
  using std::chrono::seconds;
  using std::chrono::minutes;

  nanoseconds nsdur;
  microseconds usdur;
  milliseconds msdur;
  seconds sdur;
  minutes mdur;

  // False indicates a non-integral tick count or a missing unit.
  EXPECT_FALSE(StringToValue(std::string("foo"), &nsdur));
  EXPECT_FALSE(StringToValue(std::string("14"), &nsdur));
  EXPECT_FALSE(StringToValue(std::string("14.5 ns"), &nsdur));

  // Test simple cases that don't need ratio conversion.
  EXPECT_TRUE(StringToValue(std::string("14 ns"), &nsdur));
  EXPECT_EQ(nanoseconds(14), nsdur);
  EXPECT_EQ("14 ns", ValueToString(nsdur));

  EXPECT_TRUE(StringToValue(std::string("14 us"), &usdur));
  EXPECT_EQ(microseconds(14), usdur);
  EXPECT_EQ("14 us", ValueToString(usdur));

  EXPECT_TRUE(StringToValue(std::string("14 ms"), &msdur));
  EXPECT_EQ(milliseconds(14), msdur);
  EXPECT_EQ("14 ms", ValueToString(msdur));

  EXPECT_TRUE(StringToValue(std::string("14 s"), &sdur));
  EXPECT_EQ(seconds(14), sdur);
  EXPECT_EQ("14 s", ValueToString(sdur));

  // The number of spaces (or omitting them altogether) between the tick count
  // and the unit in the input string does not matter.
  EXPECT_TRUE(StringToValue(std::string("14s"), &sdur));
  EXPECT_EQ(seconds(14), sdur);
  EXPECT_EQ("14 s", ValueToString(sdur));

  EXPECT_TRUE(StringToValue(std::string("14   s"), &sdur));
  EXPECT_EQ(seconds(14), sdur);
  EXPECT_EQ("14 s", ValueToString(sdur));

  // A zero duration causes us to print a value of zero seconds.
  EXPECT_TRUE(StringToValue(std::string("0 s"), &sdur));
  EXPECT_EQ(seconds(0), sdur);
  EXPECT_EQ("0 s", ValueToString(seconds(0)));
  EXPECT_EQ("0 s", ValueToString(seconds(-0)));

  // Test cases that will cause ratio conversion.
  EXPECT_TRUE(StringToValue(std::string("14000 ns"), &nsdur));
  EXPECT_EQ(nanoseconds(14000), nsdur);
  EXPECT_EQ("14 us", ValueToString(nsdur));

  EXPECT_TRUE(StringToValue(std::string("14000000 ns"), &nsdur));
  EXPECT_EQ(nanoseconds(14000000), nsdur);
  EXPECT_EQ("14 ms", ValueToString(nsdur));

  EXPECT_TRUE(StringToValue(std::string("14001000 ns"), &nsdur));
  EXPECT_EQ(nanoseconds(14001000), nsdur);
  EXPECT_EQ("14001 us", ValueToString(nsdur));

  // Note that we don't convert to any units bigger than seconds.
  EXPECT_TRUE(StringToValue(std::string("60 s"), &mdur));
  EXPECT_EQ(minutes(1), mdur);
  EXPECT_EQ("60 s", ValueToString(mdur));

  // Negative values should be preserved.
  EXPECT_TRUE(StringToValue(std::string("-14 ns"), &nsdur));
  EXPECT_EQ(nanoseconds(-14), nsdur);
  EXPECT_EQ("-14 ns", ValueToString(nsdur));

  EXPECT_TRUE(StringToValue(std::string("-14000 ns"), &nsdur));
  EXPECT_EQ(microseconds(-14), nsdur);
  EXPECT_EQ("-14 us", ValueToString(nsdur));
}

}  // namespace base
}  // namespace ion
