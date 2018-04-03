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

#include <string>
#include <unordered_map>

#include "third_party/googletest/googletest/include/gtest/gtest.h"

// These tests are to verify that unordered_map works on the local platform.
// Tests are based on:
// http://www.cplusplus.com/reference/unordered_map/unordered_map/operator[]/

TEST(UnorderedMap, StringMap) {
  typedef std::unordered_map<std::string, std::string> MapType;
  MapType mymap;

  mymap["Bakery"] = "Barbara";  // new element inserted
  mymap["Seafood"] = "Lisa";    // new element inserted
  mymap["Produce"] = "John";    // new element inserted

  std::string name = mymap["Bakery"];  // existing element accessed (read)
  mymap["Seafood"] = name;             // existing element accessed (written)

  mymap["Bakery"] =
      mymap["Produce"];  // existing elements accessed (read/written)

  name = mymap["Deli"];  // non-existing element: new element "Deli" inserted!

  mymap["Produce"] =
      mymap["Gifts"];  // new element "Gifts" inserted, "Produce" written

  EXPECT_EQ("John", mymap["Bakery"]);
  EXPECT_EQ("Barbara", mymap["Seafood"]);
  EXPECT_EQ("", mymap["Produce"]);
  EXPECT_EQ("", mymap["Deli"]);
  EXPECT_EQ("", mymap["Gifts"]);
  EXPECT_EQ(mymap.end(), mymap.find("Doesn't exist"));
}

TEST(UnorderedMap, SizeT) {
  typedef std::unordered_map<size_t, std::string> MapType;
  MapType mymap;

  mymap[31245] = "Barbara";  // new element inserted
  mymap[223] = "Lisa";    // new element inserted
  mymap[943] = "John";    // new element inserted

  std::string name = mymap[31245];  // existing element accessed (read)
  mymap[223] = name;             // existing element accessed (written)

  mymap[31245] =
      mymap[943];  // existing elements accessed (read/written)

  name = mymap[4563244];  // non-existing element: new element "Deli" inserted!

  mymap[943] =
      mymap[65753];  // new element "Gifts" inserted, "Produce" written

  EXPECT_EQ("John", mymap[31245]);
  EXPECT_EQ("Barbara", mymap[223]);
  EXPECT_EQ("", mymap[943]);
  EXPECT_EQ("", mymap[4563244]);
  EXPECT_EQ("", mymap[65753]);
  EXPECT_EQ(mymap.end(), mymap.find(384572034U));
}
