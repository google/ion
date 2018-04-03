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

#include "ion/base/type_structs.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

struct BaseType {};
struct DerivedType1 : public BaseType {};
struct DerivedType2 : public BaseType {};
struct NonTrivialDestructor {
 public:
  ~NonTrivialDestructor() {}
};

typedef BaseType BaseTypeAlias1;
typedef BaseType BaseTypeAlias2;

TEST(TypeStructs, BoolType) {
  using ion::base::BoolType;
  EXPECT_TRUE(BoolType<true>::value);
  EXPECT_FALSE(BoolType<false>::value);

  EXPECT_TRUE(BoolType<4 == 2 * 2>::value);
  EXPECT_FALSE(BoolType<sizeof(char) == 3>::value);
}

TEST(TypeStructs, IsSameType) {
  using ion::base::IsSameType;
  EXPECT_TRUE((IsSameType<int, int>::value));
  EXPECT_TRUE((IsSameType<char, char>::value));
  EXPECT_FALSE((IsSameType<char, int>::value));
  EXPECT_TRUE((IsSameType<BaseType, BaseType>::value));
  EXPECT_TRUE((IsSameType<BaseType, BaseTypeAlias1>::value));
  EXPECT_TRUE((IsSameType<BaseTypeAlias1, BaseTypeAlias2>::value));
  EXPECT_FALSE((IsSameType<BaseType, DerivedType1>::value));
  EXPECT_FALSE((IsSameType<DerivedType1, DerivedType2>::value));
}

TEST(TypeStructs, IsBaseOf) {
  using ion::base::IsBaseOf;
  EXPECT_FALSE((IsBaseOf<int, int>::value));
  EXPECT_FALSE((IsBaseOf<int, char>::value));
  EXPECT_FALSE((IsBaseOf<BaseType, int>::value));
  EXPECT_FALSE((IsBaseOf<BaseType, BaseType>::value));
  EXPECT_TRUE((IsBaseOf<BaseType, DerivedType1>::value));
  EXPECT_TRUE((IsBaseOf<BaseType, DerivedType2>::value));
  EXPECT_FALSE((IsBaseOf<DerivedType1, BaseType>::value));
  EXPECT_FALSE((IsBaseOf<DerivedType1, DerivedType2>::value));
}

TEST(TypeStructs, IsConvertible) {
  using ion::base::IsConvertible;
  EXPECT_TRUE((IsConvertible<int, int>::value));
  EXPECT_FALSE((IsConvertible<int, char>::value));
  EXPECT_FALSE((IsConvertible<BaseType, int>::value));
  EXPECT_TRUE((IsConvertible<BaseType, BaseType>::value));
  EXPECT_FALSE((IsConvertible<BaseType, DerivedType1>::value));
  EXPECT_FALSE((IsConvertible<BaseType, DerivedType2>::value));
  EXPECT_TRUE((IsConvertible<DerivedType1, BaseType>::value));
  EXPECT_TRUE((IsConvertible<DerivedType2, BaseType>::value));
  EXPECT_FALSE((IsConvertible<DerivedType1, DerivedType2>::value));
}

TEST(TypeStructs, ConditionalType) {
  using ion::base::ConditionalType;
  using ion::base::IsSameType;
  EXPECT_TRUE((IsSameType<int,
                          ConditionalType<true, int, char>::Type>::value));
  EXPECT_TRUE((IsSameType<char,
                          ConditionalType<false, int, char>::Type>::value));
}

TEST(TypeStructs, HasTrivialDestructor) {
  using ion::base::HasTrivialDestructor;
  EXPECT_TRUE(HasTrivialDestructor<BaseType>::value);
  EXPECT_TRUE(HasTrivialDestructor<DerivedType1>::value);
  EXPECT_FALSE(HasTrivialDestructor<NonTrivialDestructor>::value);
}
