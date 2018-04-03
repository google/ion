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

#include "ion/base/variant.h"

#include "ion/base/allocatable.h"
#include "ion/base/allocator.h"
#include "ion/base/invalid.h"
#include "ion/base/referent.h"
#include "ion/base/type_structs.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

using ion::base::Variant;

//-----------------------------------------------------------------------------
//
// Helper types.
//
//-----------------------------------------------------------------------------

// Simple base and derived types for testing.
struct BaseType {
  BaseType() : x(-1) {}
  int x;
};
struct DerivedType : public BaseType {};

// Derived Referent class for testing a ReferentPtr in a Variant.
class RefType : public ion::base::Referent {
 public:
  static void ClearNumDeletions() { num_deletions_ = 0; }
  static size_t GetNumDeletions() { return num_deletions_; }
 protected:
  ~RefType() override { ++num_deletions_; }

 private:
  static size_t num_deletions_;
};
using RefTypePtr = ion::base::SharedPtr<RefType>;
size_t RefType::num_deletions_ = 0;

// Struct containing a ReferentPtr.
struct StructWithReferentPtr {
  RefTypePtr ptr;
};

// Allocatable class for tests.
class AllocType : public ion::base::Allocatable {
 public:
  AllocType() {}
};

// Struct allowing different types to be created easily.
template <int N> struct UniqueType {};

//-----------------------------------------------------------------------------
//
// Variant tests.
//
//-----------------------------------------------------------------------------

TEST(Variant, Default) {
  typedef Variant<int, double> TestVariant;
  TestVariant a;

  // A default-constructed instance has no valid type.
  EXPECT_FALSE(a.Is<int>());
  EXPECT_FALSE(a.Is<double>());
}

TEST(Variant, SetAndIs) {
  typedef Variant<int, double> TestVariant;
  TestVariant v1;
  TestVariant v2;
  v1.Set(13);
  v2.Set(13.1);
  EXPECT_TRUE(v1.Is<int>());
  EXPECT_FALSE(v1.Is<double>());
  EXPECT_FALSE(v2.Is<int>());
  EXPECT_TRUE(v2.Is<double>());
  v1.Set(14.5);
  EXPECT_FALSE(v1.Is<int>());
  EXPECT_TRUE(v1.Is<double>());
}

TEST(Variant, SetArrayAndIsArray) {
  typedef Variant<int, double> TestVariant;
  AllocType a;
  TestVariant v1;
  TestVariant v2;

  EXPECT_EQ(0U, v1.GetCount());
  EXPECT_EQ(0U, v2.GetCount());

  v1.InitArray<int>(a.GetNonNullAllocator(), 2);
  EXPECT_EQ(2U, v1.GetCount());
  v1.SetValueAt(0, 1000);
  v1[1] = 20;
  // The array-element comparison operator only works one-way.
  EXPECT_EQ(static_cast<int>(v1[0]), 1000);
  EXPECT_EQ(20, v1.GetValueAt<int>(1));
  int i = v1[1];
  EXPECT_EQ(20, i);
  EXPECT_TRUE(ion::base::IsInvalidReference(v1.GetValueAt<int>(2)));
  EXPECT_TRUE(ion::base::IsInvalidReference(v1.Get<int>()));
  EXPECT_FALSE(v1.Is<int>());
  EXPECT_TRUE(v1.IsArrayOf<int>());
  EXPECT_FALSE(v1.Is<double>());
  EXPECT_FALSE(v1.IsArrayOf<double>());

  v2.InitArray<double>(a.GetNonNullAllocator(), 3);
  EXPECT_EQ(3U, v2.GetCount());
  v2[0] = 1.1;
  v2[1] = 2.;
  v2.SetValueAt<double>(2, 2);
  EXPECT_EQ(v2.GetValueAt<double>(2), 2.);
  v2.SetValueAt(2, 3.14);
  EXPECT_EQ(static_cast<double>(v2[0]), 1.1);
  EXPECT_EQ(static_cast<double>(v2[1]), 2.);
  EXPECT_EQ(v2.GetValueAt<double>(2), 3.14);
  EXPECT_TRUE(ion::base::IsInvalidReference(v2.GetValueAt<int>(1)));
  EXPECT_TRUE(ion::base::IsInvalidReference(v2.Get<double>()));
  EXPECT_TRUE(ion::base::IsInvalidReference(v2.GetValueAt<double>(3)));
  EXPECT_TRUE(ion::base::IsInvalidReference(v2.Get<int>()));
  EXPECT_FALSE(v2.Is<int>());
  EXPECT_FALSE(v2.IsArrayOf<int>());
  EXPECT_FALSE(v2.Is<double>());
  EXPECT_TRUE(v2.IsArrayOf<double>());

  // Check that we can convert back to scalars.
  v1.Set(13);
  v2.Set(13.1);
  EXPECT_EQ(0U, v1.GetCount());
  EXPECT_EQ(0U, v2.GetCount());
  EXPECT_TRUE(v1.Is<int>());
  EXPECT_FALSE(v1.Is<double>());
  EXPECT_FALSE(v2.Is<int>());
  EXPECT_TRUE(v2.Is<double>());
  v1.Set(14.5);
  EXPECT_FALSE(v1.Is<int>());
  EXPECT_TRUE(v1.Is<double>());
}

TEST(Variant, SetConvertible) {
  {
    typedef Variant<int, double> TestVariant;
    TestVariant v;

    // Set from int - should be an int.
    v.Set(11);
    EXPECT_TRUE(v.Is<int>());
    EXPECT_FALSE(v.Is<double>());
    // Set from float or double - should be a double.
    v.Set(15.0f);
    EXPECT_FALSE(v.Is<int>());
    EXPECT_TRUE(v.Is<double>());
    v.Set(17.3);
    EXPECT_FALSE(v.Is<int>());
    EXPECT_TRUE(v.Is<double>());
  }

  {
    // With float and double as possibilities, should use the exact type.
    typedef Variant<int, float, double> TestVariant;
    TestVariant v;

    // Set from int - should be an int.
    v.Set(11);
    EXPECT_TRUE(v.Is<int>());
    EXPECT_FALSE(v.Is<float>());
    EXPECT_FALSE(v.Is<double>());
    // Set from float - should be a float.
    v.Set(15.0f);
    EXPECT_FALSE(v.Is<int>());
    EXPECT_TRUE(v.Is<float>());
    EXPECT_FALSE(v.Is<double>());
    // Set from double - should be a double.
    v.Set(17.3);
    EXPECT_FALSE(v.Is<int>());
    EXPECT_FALSE(v.Is<float>());
    EXPECT_TRUE(v.Is<double>());
  }
}

TEST(Variant, SetDerived) {
  typedef Variant<int, double, BaseType> TestVariant;
  TestVariant v;
  EXPECT_FALSE(v.Is<int>());
  EXPECT_FALSE(v.Is<double>());
  EXPECT_FALSE(v.Is<BaseType>());
  BaseType b;
  v.Set(b);
  EXPECT_FALSE(v.Is<int>());
  EXPECT_FALSE(v.Is<double>());
  EXPECT_TRUE(v.Is<BaseType>());
  DerivedType d;
  v.Set(d);
  EXPECT_FALSE(v.Is<int>());
  EXPECT_FALSE(v.Is<double>());
  EXPECT_TRUE(v.Is<BaseType>());
}

TEST(Variant, IsAssignableToElementsAssignableTo) {
  typedef Variant<int, double, BaseType> TestVariant;
  TestVariant v;
  v.Set(13);
  EXPECT_TRUE(v.IsAssignableTo<int>());
  EXPECT_FALSE(v.IsAssignableTo<double>());
  EXPECT_FALSE(v.IsAssignableTo<BaseType>());
  EXPECT_FALSE(v.ElementsAssignableTo<BaseType>());
  EXPECT_FALSE(v.ElementsAssignableTo<DerivedType>());

  v.Set(13.0);
  EXPECT_FALSE(v.IsAssignableTo<int>());
  EXPECT_TRUE(v.IsAssignableTo<double>());
  EXPECT_FALSE(v.IsAssignableTo<BaseType>());
  EXPECT_FALSE(v.ElementsAssignableTo<BaseType>());
  EXPECT_FALSE(v.ElementsAssignableTo<DerivedType>());

  AllocType a;
  v.InitArray<int>(a.GetNonNullAllocator(), 2);
  EXPECT_TRUE(v.ElementsAssignableTo<int>());
  EXPECT_FALSE(v.ElementsAssignableTo<double>());
  EXPECT_FALSE(v.ElementsAssignableTo<BaseType>());
  EXPECT_FALSE(v.ElementsAssignableTo<DerivedType>());
  EXPECT_FALSE(v.IsAssignableTo<int>());
  EXPECT_FALSE(v.IsAssignableTo<double>());
  EXPECT_FALSE(v.IsAssignableTo<BaseType>());
  EXPECT_FALSE(v.IsAssignableTo<DerivedType>());

  v.InitArray<double>(a.GetNonNullAllocator(), 2);
  EXPECT_FALSE(v.ElementsAssignableTo<int>());
  EXPECT_TRUE(v.ElementsAssignableTo<double>());
  EXPECT_FALSE(v.ElementsAssignableTo<BaseType>());
  EXPECT_FALSE(v.ElementsAssignableTo<DerivedType>());
  EXPECT_FALSE(v.IsAssignableTo<int>());
  EXPECT_FALSE(v.IsAssignableTo<double>());
  EXPECT_FALSE(v.IsAssignableTo<BaseType>());
  EXPECT_FALSE(v.IsAssignableTo<DerivedType>());

  BaseType b;
  v.Set(b);
  EXPECT_FALSE(v.ElementsAssignableTo<int>());
  EXPECT_FALSE(v.ElementsAssignableTo<double>());
  EXPECT_FALSE(v.ElementsAssignableTo<BaseType>());
  EXPECT_FALSE(v.ElementsAssignableTo<DerivedType>());
  EXPECT_FALSE(v.IsAssignableTo<int>());
  EXPECT_FALSE(v.IsAssignableTo<double>());
  EXPECT_TRUE(v.IsAssignableTo<BaseType>());
  EXPECT_TRUE(v.IsAssignableTo<DerivedType>());

  DerivedType d;
  v.Set(d);
  EXPECT_FALSE(v.ElementsAssignableTo<int>());
  EXPECT_FALSE(v.ElementsAssignableTo<double>());
  EXPECT_FALSE(v.ElementsAssignableTo<BaseType>());
  EXPECT_FALSE(v.ElementsAssignableTo<DerivedType>());
  EXPECT_FALSE(v.IsAssignableTo<int>());
  EXPECT_FALSE(v.IsAssignableTo<double>());
  EXPECT_TRUE(v.IsAssignableTo<BaseType>());
  EXPECT_TRUE(v.IsAssignableTo<DerivedType>());

  v.InitArray<BaseType>(a.GetNonNullAllocator(), 2);
  EXPECT_EQ(a.GetNonNullAllocator().Get(), v.GetArrayAllocator().Get());
  EXPECT_FALSE(v.ElementsAssignableTo<int>());
  EXPECT_FALSE(v.ElementsAssignableTo<double>());
  EXPECT_TRUE(v.ElementsAssignableTo<BaseType>());
  EXPECT_TRUE(v.ElementsAssignableTo<DerivedType>());
  EXPECT_FALSE(v.IsAssignableTo<int>());
  EXPECT_FALSE(v.IsAssignableTo<double>());
  EXPECT_FALSE(v.IsAssignableTo<BaseType>());
  EXPECT_FALSE(v.IsAssignableTo<DerivedType>());

  v.InitArray<DerivedType>(a.GetNonNullAllocator(), 2);
  EXPECT_EQ(a.GetNonNullAllocator().Get(), v.GetArrayAllocator().Get());
  EXPECT_FALSE(v.ElementsAssignableTo<int>());
  EXPECT_FALSE(v.ElementsAssignableTo<double>());
  EXPECT_TRUE(v.ElementsAssignableTo<BaseType>());
  EXPECT_TRUE(v.ElementsAssignableTo<DerivedType>());
  EXPECT_FALSE(v.IsAssignableTo<int>());
  EXPECT_FALSE(v.IsAssignableTo<double>());
  EXPECT_FALSE(v.IsAssignableTo<BaseType>());
  EXPECT_FALSE(v.IsAssignableTo<DerivedType>());
}

TEST(Variant, Get) {
  using ion::base::IsInvalidReference;

  typedef Variant<int, double, BaseType> TestVariant;
  TestVariant v;

  v.Set(17);
  EXPECT_EQ(17, v.Get<int>());
  EXPECT_TRUE(IsInvalidReference(v.Get<double>()));
  EXPECT_TRUE(IsInvalidReference(v.Get<BaseType>()));

  v.Set(17.5);
  EXPECT_TRUE(IsInvalidReference(v.Get<int>()));
  EXPECT_EQ(17.5, v.Get<double>());
  EXPECT_TRUE(IsInvalidReference(v.Get<BaseType>()));

  BaseType b;
  b.x = 143;
  v.Set(b);
  EXPECT_TRUE(IsInvalidReference(v.Get<int>()));
  EXPECT_TRUE(IsInvalidReference(v.Get<double>()));
  EXPECT_EQ(143, v.Get<BaseType>().x);

  DerivedType d;
  d.x = 21;
  v.Set(d);
  EXPECT_TRUE(IsInvalidReference(v.Get<int>()));
  EXPECT_TRUE(IsInvalidReference(v.Get<double>()));
  EXPECT_EQ(21, v.Get<BaseType>().x);
}

TEST(Variant, CopyFrom) {
  typedef Variant<int, double, BaseType> TestVariant;
  TestVariant v1, v2;
  v1.Set(17);
  v2.CopyFrom(v1);
  EXPECT_EQ(17, v2.Get<int>());
  EXPECT_TRUE(v2.Is<int>());

  v1.Set(12.2);
  v2.CopyFrom(v1);
  EXPECT_EQ(12.2, v2.Get<double>());
  EXPECT_TRUE(v2.Is<double>());

  DerivedType d;
  d.x = 55;
  v1.Set(d);
  v2.CopyFrom(v1);
  EXPECT_TRUE(v2.Is<BaseType>());
  EXPECT_EQ(55, v2.Get<BaseType>().x);

  // Copy default-constructed instance.
  TestVariant v3;
  v2.CopyFrom(v3);
  EXPECT_FALSE(v2.Is<int>());
  EXPECT_FALSE(v2.Is<double>());
  EXPECT_FALSE(v2.Is<BaseType>());

  TestVariant v4;
  v4.InitArray<int>(ion::base::AllocatorPtr(), 2);
  v4[0] = 1;
  v4[1] = 2;

  TestVariant v5;
  v5.CopyFrom(v4);
  EXPECT_TRUE(v5.IsArrayOf<int>());
  EXPECT_EQ(1, v5.GetValueAt<int>(0));
  EXPECT_EQ(2, v5.GetValueAt<int>(1));
}

TEST(Variant, CopyAndAssign) {
  typedef Variant<int, double, BaseType> TestVariant;
  TestVariant v1;
  v1.Set(17);

  TestVariant v2(v1);
  EXPECT_EQ(17, v2.Get<int>());
  EXPECT_TRUE(v2.Is<int>());

  TestVariant v3 = v1;
  EXPECT_EQ(17, v3.Get<int>());
  EXPECT_TRUE(v3.Is<int>());

  TestVariant v4;
  v4.InitArray<int>(ion::base::AllocatorPtr(), 2);
  v4[0] = 1;
  v4[1] = 2;

  TestVariant v5 = v4;
  EXPECT_TRUE(v5.IsArrayOf<int>());
  EXPECT_EQ(1, v5.GetValueAt<int>(0));
  EXPECT_EQ(2, v5.GetValueAt<int>(1));
}

TEST(Variant, Referent) {
  // Verify that a ReferentPtr can be stored in a Variant with no ill effects.
  typedef Variant<int, RefTypePtr, StructWithReferentPtr> TestVariant;

  // Destroying an int should have no effect on RefType.
  {
    TestVariant v;
    v.Set(13);
  }
  EXPECT_EQ(0U, RefType::GetNumDeletions());

  // Destroying a NULL RefTypePtr should be fine.
  {
    TestVariant v;
    v.Set(RefTypePtr());
  }
  EXPECT_EQ(0U, RefType::GetNumDeletions());

  // Destroying a NULL RefTypePtr should also be fine and should result in the
  // destruction of the RefType.
  {
    TestVariant v;
    v.Set(RefTypePtr(new RefType));
  }
  EXPECT_EQ(1U, RefType::GetNumDeletions());
  RefType::ClearNumDeletions();

  // Setting to a different pointer should delete the first one.
  {
    TestVariant v;
    v.Set(RefTypePtr(new RefType));
    EXPECT_EQ(0U, RefType::GetNumDeletions());
    v.Set(RefTypePtr(new RefType));
    EXPECT_EQ(1U, RefType::GetNumDeletions());
  }
  EXPECT_EQ(2U, RefType::GetNumDeletions());
  RefType::ClearNumDeletions();

  // Try a Variant containing a struct containing a ReferentPtr.
  {
    TestVariant v;
    StructWithReferentPtr s;
    s.ptr.Reset(new RefType);
    v.Set(s);
    EXPECT_EQ(0U, RefType::GetNumDeletions());
  }
  EXPECT_EQ(1U, RefType::GetNumDeletions());
  RefType::ClearNumDeletions();

  // Try an array of ReferentPtrs.
  {
    std::vector<RefTypePtr> vec;
    vec.push_back(RefTypePtr(new RefType));
    vec.push_back(RefTypePtr(new RefType));

    TestVariant v;
    v.InitArray<RefTypePtr>(ion::base::AllocatorPtr(), 2U);
    v.SetValueAt(0, vec[0]);
    v.SetValueAt(1, vec[1]);
    EXPECT_EQ(0U, RefType::GetNumDeletions());
    EXPECT_EQ(2, v.GetValueAt<RefTypePtr>(0)->GetRefCount());
    EXPECT_EQ(2, v.GetValueAt<RefTypePtr>(1)->GetRefCount());

    {
      TestVariant v2;
      v2.InitArray<RefTypePtr>(ion::base::AllocatorPtr(), 2U);
      v2.SetValueAt(0, vec[0]);
      v2.SetValueAt(1, vec[1]);
      EXPECT_EQ(0U, RefType::GetNumDeletions());
      EXPECT_EQ(3, v.GetValueAt<RefTypePtr>(0)->GetRefCount());
      EXPECT_EQ(3, v.GetValueAt<RefTypePtr>(1)->GetRefCount());
    }
    EXPECT_EQ(0U, RefType::GetNumDeletions());
    EXPECT_EQ(2, v.GetValueAt<RefTypePtr>(0)->GetRefCount());
    EXPECT_EQ(2, v.GetValueAt<RefTypePtr>(1)->GetRefCount());

    {
      TestVariant v2 = v;
      EXPECT_EQ(0U, RefType::GetNumDeletions());
      EXPECT_EQ(3, v.GetValueAt<RefTypePtr>(0)->GetRefCount());
      EXPECT_EQ(3, v.GetValueAt<RefTypePtr>(1)->GetRefCount());
    }
    EXPECT_EQ(0U, RefType::GetNumDeletions());
    EXPECT_EQ(2, v.GetValueAt<RefTypePtr>(0)->GetRefCount());
    EXPECT_EQ(2, v.GetValueAt<RefTypePtr>(1)->GetRefCount());

    // This clears the storage in v, but the vec still holds references to the
    // values.
    v.Set(0);
    EXPECT_EQ(0U, RefType::GetNumDeletions());
    EXPECT_EQ(1, vec[0]->GetRefCount());
    EXPECT_EQ(1, vec[1]->GetRefCount());

    {
      TestVariant v2;
      v2.InitArray<RefTypePtr>(ion::base::AllocatorPtr(), 2U);
      v2.SetValueAt(0, vec[0]);
      v2.SetValueAt(1, vec[1]);
      EXPECT_EQ(0U, RefType::GetNumDeletions());
      EXPECT_EQ(2, v2.GetValueAt<RefTypePtr>(0)->GetRefCount());
      EXPECT_EQ(2, v2.GetValueAt<RefTypePtr>(1)->GetRefCount());

      vec.clear();
      EXPECT_EQ(1, v2.GetValueAt<RefTypePtr>(0)->GetRefCount());
      EXPECT_EQ(1, v2.GetValueAt<RefTypePtr>(1)->GetRefCount());
      EXPECT_EQ(0U, RefType::GetNumDeletions());
    }
    EXPECT_EQ(2U, RefType::GetNumDeletions());
  }
  RefType::ClearNumDeletions();
}

TEST(Variant, AllTypes) {
  typedef Variant<
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
  using ion::base::IsSameType;
  EXPECT_TRUE((IsSameType<UniqueType<1>, TestVariant::Type1>::value));
  EXPECT_TRUE((IsSameType<UniqueType<2>, TestVariant::Type2>::value));
  EXPECT_TRUE((IsSameType<UniqueType<3>, TestVariant::Type3>::value));
  EXPECT_TRUE((IsSameType<UniqueType<4>, TestVariant::Type4>::value));
  EXPECT_TRUE((IsSameType<UniqueType<5>, TestVariant::Type5>::value));
  EXPECT_TRUE((IsSameType<UniqueType<6>, TestVariant::Type6>::value));
  EXPECT_TRUE((IsSameType<UniqueType<7>, TestVariant::Type7>::value));
  EXPECT_TRUE((IsSameType<UniqueType<8>, TestVariant::Type8>::value));
  EXPECT_TRUE((IsSameType<UniqueType<9>, TestVariant::Type9>::value));
  EXPECT_TRUE((IsSameType<UniqueType<10>, TestVariant::Type10>::value));
  EXPECT_TRUE((IsSameType<UniqueType<11>, TestVariant::Type11>::value));
  EXPECT_TRUE((IsSameType<UniqueType<12>, TestVariant::Type12>::value));
  EXPECT_TRUE((IsSameType<UniqueType<13>, TestVariant::Type13>::value));
  EXPECT_TRUE((IsSameType<UniqueType<14>, TestVariant::Type14>::value));
  EXPECT_TRUE((IsSameType<UniqueType<15>, TestVariant::Type15>::value));
  EXPECT_TRUE((IsSameType<UniqueType<16>, TestVariant::Type16>::value));
  EXPECT_TRUE((IsSameType<UniqueType<17>, TestVariant::Type17>::value));
  EXPECT_TRUE((IsSameType<UniqueType<18>, TestVariant::Type18>::value));
  EXPECT_TRUE((IsSameType<UniqueType<19>, TestVariant::Type19>::value));
  EXPECT_TRUE((IsSameType<UniqueType<20>, TestVariant::Type20>::value));
  EXPECT_TRUE((IsSameType<UniqueType<21>, TestVariant::Type21>::value));
  EXPECT_TRUE((IsSameType<UniqueType<22>, TestVariant::Type22>::value));
  EXPECT_TRUE((IsSameType<UniqueType<23>, TestVariant::Type23>::value));
  EXPECT_TRUE((IsSameType<UniqueType<24>, TestVariant::Type24>::value));
  EXPECT_TRUE((IsSameType<UniqueType<25>, TestVariant::Type25>::value));
  EXPECT_TRUE((IsSameType<UniqueType<26>, TestVariant::Type26>::value));
  EXPECT_TRUE((IsSameType<UniqueType<27>, TestVariant::Type27>::value));
  EXPECT_TRUE((IsSameType<UniqueType<28>, TestVariant::Type28>::value));
  EXPECT_TRUE((IsSameType<UniqueType<29>, TestVariant::Type29>::value));
  EXPECT_TRUE((IsSameType<UniqueType<30>, TestVariant::Type30>::value));
  EXPECT_TRUE((IsSameType<UniqueType<31>, TestVariant::Type31>::value));
  EXPECT_TRUE((IsSameType<UniqueType<32>, TestVariant::Type32>::value));
  EXPECT_TRUE((IsSameType<UniqueType<33>, TestVariant::Type33>::value));
  EXPECT_TRUE((IsSameType<UniqueType<34>, TestVariant::Type34>::value));
  EXPECT_TRUE((IsSameType<UniqueType<35>, TestVariant::Type35>::value));
  EXPECT_TRUE((IsSameType<UniqueType<36>, TestVariant::Type36>::value));
  EXPECT_TRUE((IsSameType<UniqueType<37>, TestVariant::Type37>::value));
  EXPECT_TRUE((IsSameType<UniqueType<38>, TestVariant::Type38>::value));
  EXPECT_TRUE((IsSameType<UniqueType<39>, TestVariant::Type39>::value));
  EXPECT_TRUE((IsSameType<UniqueType<40>, TestVariant::Type40>::value));

  // Create and copy instances of all types (single-valued and array) for full
  // test coverage.
  TestVariant v1, v2, va;
  ion::base::AllocatorPtr al;

#define TEST_VARIANT_TYPE(n)                    \
  v1.Set(UniqueType<n>());                      \
  va.InitArray<UniqueType<n> >(al, 1U);         \
  v2 = v1;                                      \
  v1 = va

  TEST_VARIANT_TYPE(1);
  TEST_VARIANT_TYPE(2);
  TEST_VARIANT_TYPE(3);
  TEST_VARIANT_TYPE(4);
  TEST_VARIANT_TYPE(5);
  TEST_VARIANT_TYPE(6);
  TEST_VARIANT_TYPE(7);
  TEST_VARIANT_TYPE(8);
  TEST_VARIANT_TYPE(9);
  TEST_VARIANT_TYPE(10);
  TEST_VARIANT_TYPE(11);
  TEST_VARIANT_TYPE(12);
  TEST_VARIANT_TYPE(13);
  TEST_VARIANT_TYPE(14);
  TEST_VARIANT_TYPE(15);
  TEST_VARIANT_TYPE(16);
  TEST_VARIANT_TYPE(17);
  TEST_VARIANT_TYPE(18);
  TEST_VARIANT_TYPE(19);
  TEST_VARIANT_TYPE(20);
  TEST_VARIANT_TYPE(21);
  TEST_VARIANT_TYPE(22);
  TEST_VARIANT_TYPE(23);
  TEST_VARIANT_TYPE(24);
  TEST_VARIANT_TYPE(25);
  TEST_VARIANT_TYPE(26);
  TEST_VARIANT_TYPE(27);
  TEST_VARIANT_TYPE(28);
  TEST_VARIANT_TYPE(29);
  TEST_VARIANT_TYPE(30);
  TEST_VARIANT_TYPE(31);
  TEST_VARIANT_TYPE(32);
  TEST_VARIANT_TYPE(33);
  TEST_VARIANT_TYPE(34);
  TEST_VARIANT_TYPE(35);
  TEST_VARIANT_TYPE(36);
  TEST_VARIANT_TYPE(37);
  TEST_VARIANT_TYPE(38);
  TEST_VARIANT_TYPE(39);
  TEST_VARIANT_TYPE(40);

#undef TEST_VARIANT_TYPE
}
