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

#include "ion/base/varianttyperesolver.h"

#include "base/integral_types.h"
#include "ion/base/type_structs.h"
#include "ion/base/variant.h"
#include "ion/math/vector.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

// Simple base and derived types for testing.
struct BaseType {};
struct DerivedType : public BaseType {};
struct MoreDerivedType : public DerivedType {};
struct OtherDerivedType : public BaseType {};

// Struct allowing different types to be created easily.
template <int N> struct UniqueType {};

template <typename ExpectedType, typename VariantType, typename TestType>
static bool TestResolver() {
  using ion::base::VariantTypeResolver;
  typedef typename VariantTypeResolver<VariantType, TestType>::Type ResultType;
  return ion::base::IsSameType<ExpectedType, ResultType>::value;
}

TEST(VariantTypeResolver, Simple) {
  typedef ion::base::Variant<int, double, BaseType> TestVariant;

  // Test exact type matches.
  EXPECT_TRUE((TestResolver<int, TestVariant, int>()));
  EXPECT_TRUE((TestResolver<double, TestVariant, double>()));
  EXPECT_TRUE((TestResolver<BaseType, TestVariant, BaseType>()));

  // Test derived types.
  EXPECT_TRUE((TestResolver<BaseType, TestVariant, DerivedType>()));
  EXPECT_TRUE((TestResolver<BaseType, TestVariant, MoreDerivedType>()));

  // Unsupported types should be void, even if they are convertible to
  // supported types.
  EXPECT_TRUE((TestResolver<void, TestVariant, int16>()));
  EXPECT_TRUE((TestResolver<void, TestVariant, float>()));
  EXPECT_TRUE((TestResolver<void, TestVariant, char*>()));
}

TEST(VariantTypeResolver, BaseVsDerived) {
  typedef ion::base::Variant<int, DerivedType> TestVariant;

  // Test exact type matches.
  EXPECT_TRUE((TestResolver<int, TestVariant, int>()));
  EXPECT_TRUE((TestResolver<DerivedType, TestVariant, DerivedType>()));

  // Test derived type.
  EXPECT_TRUE((TestResolver<DerivedType, TestVariant, MoreDerivedType>()));

  // BaseType should result in void; DerivedType cannot be converted to it.
  EXPECT_TRUE((TestResolver<void, TestVariant, BaseType>()));
}

TEST(VariantTypeResolver, MultipleDerivedTypes) {
  typedef ion::base::Variant<int, BaseType> TestVariant;

  // Test exact type matches.
  EXPECT_TRUE((TestResolver<int, TestVariant, int>()));
  EXPECT_TRUE((TestResolver<BaseType, TestVariant, BaseType>()));

  // Test both derived types.
  EXPECT_TRUE((TestResolver<BaseType, TestVariant, DerivedType>()));
  EXPECT_TRUE((TestResolver<BaseType, TestVariant, OtherDerivedType>()));
}

TEST(VariantTypeResolver, SiblingTypes) {
  typedef ion::base::Variant<int, DerivedType> TestVariant;

  // Sibling type should not work.
  EXPECT_TRUE((TestResolver<void, TestVariant, OtherDerivedType>()));
}

TEST(VariantTypeResolver, AllTypes) {
  typedef ion::base::Variant<
    UniqueType<1>, UniqueType<2>, UniqueType<3>, UniqueType<4>,
    UniqueType<5>, UniqueType<6>, UniqueType<7>, UniqueType<8>,
    UniqueType<9>, UniqueType<10>, UniqueType<11>, UniqueType<12>,
    UniqueType<13>, UniqueType<14>, UniqueType<15>, UniqueType<16>,
    UniqueType<17>, UniqueType<18>, UniqueType<19>, UniqueType<20>,
    UniqueType<21>, UniqueType<22>, UniqueType<23>, UniqueType<24>,
    UniqueType<25>, UniqueType<26>, UniqueType<27>, UniqueType<28>,
    UniqueType<29>, UniqueType<30>, UniqueType<31>, UniqueType<32>,
    UniqueType<33>, UniqueType<34>, UniqueType<35>, UniqueType<36>,
    UniqueType<37>, UniqueType<38>, UniqueType<39>, UniqueType<40>
    > TestVariant;
    EXPECT_TRUE((TestResolver<UniqueType<1>, TestVariant, UniqueType<1> >()));
    EXPECT_TRUE((TestResolver<UniqueType<2>, TestVariant, UniqueType<2> >()));
    EXPECT_TRUE((TestResolver<UniqueType<3>, TestVariant, UniqueType<3> >()));
    EXPECT_TRUE((TestResolver<UniqueType<4>, TestVariant, UniqueType<4> >()));
    EXPECT_TRUE((TestResolver<UniqueType<5>, TestVariant, UniqueType<5> >()));
    EXPECT_TRUE((TestResolver<UniqueType<6>, TestVariant, UniqueType<6> >()));
    EXPECT_TRUE((TestResolver<UniqueType<7>, TestVariant, UniqueType<7> >()));
    EXPECT_TRUE((TestResolver<UniqueType<8>, TestVariant, UniqueType<8> >()));
    EXPECT_TRUE((TestResolver<UniqueType<9>, TestVariant, UniqueType<9> >()));
    EXPECT_TRUE((TestResolver<UniqueType<10>, TestVariant, UniqueType<10> >()));
    EXPECT_TRUE((TestResolver<UniqueType<11>, TestVariant, UniqueType<11> >()));
    EXPECT_TRUE((TestResolver<UniqueType<12>, TestVariant, UniqueType<12> >()));
    EXPECT_TRUE((TestResolver<UniqueType<13>, TestVariant, UniqueType<13> >()));
    EXPECT_TRUE((TestResolver<UniqueType<14>, TestVariant, UniqueType<14> >()));
    EXPECT_TRUE((TestResolver<UniqueType<15>, TestVariant, UniqueType<15> >()));
    EXPECT_TRUE((TestResolver<UniqueType<16>, TestVariant, UniqueType<16> >()));
    EXPECT_TRUE((TestResolver<UniqueType<17>, TestVariant, UniqueType<17> >()));
    EXPECT_TRUE((TestResolver<UniqueType<18>, TestVariant, UniqueType<18> >()));
    EXPECT_TRUE((TestResolver<UniqueType<19>, TestVariant, UniqueType<19> >()));
    EXPECT_TRUE((TestResolver<UniqueType<20>, TestVariant, UniqueType<20> >()));
    EXPECT_TRUE((TestResolver<UniqueType<21>, TestVariant, UniqueType<21> >()));
    EXPECT_TRUE((TestResolver<UniqueType<22>, TestVariant, UniqueType<22> >()));
    EXPECT_TRUE((TestResolver<UniqueType<23>, TestVariant, UniqueType<23> >()));
    EXPECT_TRUE((TestResolver<UniqueType<24>, TestVariant, UniqueType<24> >()));
    EXPECT_TRUE((TestResolver<UniqueType<25>, TestVariant, UniqueType<25> >()));
    EXPECT_TRUE((TestResolver<UniqueType<26>, TestVariant, UniqueType<26> >()));
    EXPECT_TRUE((TestResolver<UniqueType<27>, TestVariant, UniqueType<27> >()));
    EXPECT_TRUE((TestResolver<UniqueType<28>, TestVariant, UniqueType<28> >()));
    EXPECT_TRUE((TestResolver<UniqueType<29>, TestVariant, UniqueType<29> >()));
    EXPECT_TRUE((TestResolver<UniqueType<30>, TestVariant, UniqueType<30> >()));
    EXPECT_TRUE((TestResolver<UniqueType<31>, TestVariant, UniqueType<31> >()));
    EXPECT_TRUE((TestResolver<UniqueType<32>, TestVariant, UniqueType<32> >()));
    EXPECT_TRUE((TestResolver<UniqueType<33>, TestVariant, UniqueType<33> >()));
    EXPECT_TRUE((TestResolver<UniqueType<34>, TestVariant, UniqueType<34> >()));
    EXPECT_TRUE((TestResolver<UniqueType<35>, TestVariant, UniqueType<35> >()));
    EXPECT_TRUE((TestResolver<UniqueType<36>, TestVariant, UniqueType<36> >()));
    EXPECT_TRUE((TestResolver<UniqueType<37>, TestVariant, UniqueType<37> >()));
    EXPECT_TRUE((TestResolver<UniqueType<38>, TestVariant, UniqueType<38> >()));
    EXPECT_TRUE((TestResolver<UniqueType<39>, TestVariant, UniqueType<39> >()));
    EXPECT_TRUE((TestResolver<UniqueType<40>, TestVariant, UniqueType<40> >()));
    EXPECT_TRUE((TestResolver<void, TestVariant, UniqueType<41> >()));
}

TEST(VariantTypeResolver, VectorTypes) {
  using ion::math::Vector2ui16;
  using ion::math::Vector4d;
  using ion::math::VectorBase2ui16;
  using ion::math::VectorBase4d;
  typedef ion::base::Variant<float, VectorBase2ui16, VectorBase4d> TestVariant;

  EXPECT_TRUE((TestResolver<void, TestVariant, double>()));
  EXPECT_TRUE((TestResolver<float, TestVariant, float>()));
  EXPECT_TRUE((TestResolver<VectorBase4d, TestVariant, VectorBase4d>()));
  EXPECT_TRUE((TestResolver<VectorBase4d, TestVariant, Vector4d>()));
  EXPECT_TRUE((TestResolver<VectorBase2ui16, TestVariant, VectorBase2ui16>()));
  EXPECT_TRUE((TestResolver<VectorBase2ui16, TestVariant, Vector2ui16>()));
}
