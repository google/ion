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

#ifndef ION_BASE_VARIANT_H_
#define ION_BASE_VARIANT_H_

#include <cstring>  // For memset().

#include "ion/base/allocationmanager.h"
#include "ion/base/allocator.h"
#include "ion/base/invalid.h"
#include "ion/base/logging.h"
#include "ion/base/static_assert.h"
#include "ion/port/align.h"

namespace ion {
namespace base {

namespace internal_variant_utils {

// Taken from util/gtl/emptytype.h.
template<int N = 0> class EmptyType {};
struct TrueType {
  static const bool value = true;
};
struct FalseType {
  static const bool value = false;
};

template<typename T> struct is_emptytype : FalseType {};
template<int N> struct is_emptytype<EmptyType<N> > : TrueType {};

// Simplification of util/gtl/manual_constructor.h.
template <typename Type>
class ManualConstructor {
 public:
  template <typename T1> inline void Init(const T1& p1) {
#if ION_DEBUG
    val_ =
#endif
    new(space_) Type(p1);
  }
  inline void Init(const base::AllocatorPtr& allocator, size_t count) {
    DCHECK(count);
    DCHECK(allocator.Get());
    const size_t size = count * sizeof(Type);
    void* ptr = allocator->AllocateMemory(size);
    memset(ptr, 0, size);
    *reinterpret_cast<void**>(space_) = ptr;
#if ION_DEBUG
    val_ = static_cast<Type*>(ptr);
#endif
  }
  inline void InitArray(const base::AllocatorPtr& allocator,
                        const ManualConstructor& other, size_t count) {
    DCHECK(count);
    DCHECK(allocator.Get());
    const size_t size = count * sizeof(Type);
    Type* array = static_cast<Type*>(allocator->AllocateMemory(size));
    const Type* other_array =
        *reinterpret_cast<const Type*const*>(other.space_);
    // We can't use a memcpy since some types (e.g., ReferentPtrs) have side
    // effects on assignment.
    for (size_t i = 0; i < count; ++i)
      new(&array[i]) Type(other_array[i]);
    *reinterpret_cast<void**>(space_) = array;
#if ION_DEBUG
    val_ = static_cast<Type*>(array);
#endif
  }

  inline Type* Get() {
    return reinterpret_cast<Type*>(space_);
  }
  inline const Type* Get() const  {
    return reinterpret_cast<const Type*>(space_);
  }
  inline const Type& GetAt(size_t i) const {
    return (*reinterpret_cast<const Type* const*>(space_))[i];
  }
  inline void SetAt(size_t i, const Type& value) {
    (*reinterpret_cast<Type**>(space_))[i] = value;
  }
  inline void Destroy(const base::AllocatorPtr& allocator, size_t count) {
    if (count) {
      DCHECK(allocator.Get());
      if (Type* values = *reinterpret_cast<Type**>(space_)) {
        for (size_t i = 0; i < count; ++i)
          values[i].~Type();
        allocator->DeallocateMemory(values);
      }
    } else {
      reinterpret_cast<Type*>(space_)->~Type();
    }
  }

 private:
  ION_ALIGN(16) char space_[sizeof(Type)];
#if ION_DEBUG
  Type* val_;  // Allows examination of actual value in a debugger.
#endif
};

}  // namespace internal_variant_utils

// The Variant class is similar to boost::variant. It stores one of a limited
// number of types that are passed as template parameters to the class. Up to 40
// types are supported. An instance may be set to a single value or an array of
// any type derived from one of the types. Note that arrays are not dynamically
// resizable; changing the size of an array of values is destructive.
//
// Limitations:
//  - All types must be distinct.
//  - Having multiple types derived from the same type or convertible to the
//    same type may cause compile-time problems.
//  - EmptyType<> should not be used explicitly.
//  - Array resizing is destructive.
//
// Each template type has:
//  - A public typedef exposing the type. For example, T1 is exposed as
//    Type1, T2 as Type2, and so on.
//  - An overloaded Set() function that sets the variant to contain a value of
//    the type or a type that is derived from or otherwise convertible to it.
//
// Examples:
//  Variant<int, double> v;
//  v.Set(13);        // Sets to int with value 13.
//  v.Is<int>();      // Returns true.
//  v.Is<double>();   // Returns false.
//  v.Get<int>();     // Returns 13 (as a const reference).
//  v.Get<double>();  // Returns an InvalidReference.
//  v.Set(11.0);      // Changes to double with value 11.0.
//  v.Is<int>();      // Now returns false.
//  v.Is<double>();   // Now returns true.
//  v.InitArray<double>(allocator, 2);   // v now holds an array of 2 doubles.
//  v.Is<int>();      // Now returns false.
//  v.Is<double>();   // Now returns false.
//  v.IsArrayOf<double>();   // Returns true.
//  v[0] = 1.;        // The decimal point is needed.
//  v.SetValueAt(1, 2.);  // The decimal point is needed.
//  v.SetValueAt<double>(1, 3);  // The template parameter is needed.
//  v.GetValueAt<double>(1);  // Returns 3.
//  v.GetValueAt<int>(1);     // Returns InvalidReference().
//  v.GetValueAt<double>(2);  // Returns InvalidReference().
//  static_cast<double>(v[0]);  // Returns 1.  The static_cast is necessary.
template <typename T1,
          typename T2 = internal_variant_utils::EmptyType<2>,
          typename T3 = internal_variant_utils::EmptyType<3>,
          typename T4 = internal_variant_utils::EmptyType<4>,
          typename T5 = internal_variant_utils::EmptyType<5>,
          typename T6 = internal_variant_utils::EmptyType<6>,
          typename T7 = internal_variant_utils::EmptyType<7>,
          typename T8 = internal_variant_utils::EmptyType<8>,
          typename T9 = internal_variant_utils::EmptyType<9>,
          typename T10 = internal_variant_utils::EmptyType<10>,
          typename T11 = internal_variant_utils::EmptyType<11>,
          typename T12 = internal_variant_utils::EmptyType<12>,
          typename T13 = internal_variant_utils::EmptyType<13>,
          typename T14 = internal_variant_utils::EmptyType<14>,
          typename T15 = internal_variant_utils::EmptyType<15>,
          typename T16 = internal_variant_utils::EmptyType<16>,
          typename T17 = internal_variant_utils::EmptyType<17>,
          typename T18 = internal_variant_utils::EmptyType<18>,
          typename T19 = internal_variant_utils::EmptyType<19>,
          typename T20 = internal_variant_utils::EmptyType<20>,
          typename T21 = internal_variant_utils::EmptyType<21>,
          typename T22 = internal_variant_utils::EmptyType<22>,
          typename T23 = internal_variant_utils::EmptyType<23>,
          typename T24 = internal_variant_utils::EmptyType<24>,
          typename T25 = internal_variant_utils::EmptyType<25>,
          typename T26 = internal_variant_utils::EmptyType<26>,
          typename T27 = internal_variant_utils::EmptyType<27>,
          typename T28 = internal_variant_utils::EmptyType<28>,
          typename T29 = internal_variant_utils::EmptyType<29>,
          typename T30 = internal_variant_utils::EmptyType<30>,
          typename T31 = internal_variant_utils::EmptyType<31>,
          typename T32 = internal_variant_utils::EmptyType<32>,
          typename T33 = internal_variant_utils::EmptyType<33>,
          typename T34 = internal_variant_utils::EmptyType<34>,
          typename T35 = internal_variant_utils::EmptyType<35>,
          typename T36 = internal_variant_utils::EmptyType<36>,
          typename T37 = internal_variant_utils::EmptyType<37>,
          typename T38 = internal_variant_utils::EmptyType<38>,
          typename T39 = internal_variant_utils::EmptyType<39>,
          typename T40 = internal_variant_utils::EmptyType<40> >

class Variant {
 public:
  // Expose the defined types as typedefs.
  typedef T1 Type1;
  typedef T2 Type2;
  typedef T3 Type3;
  typedef T4 Type4;
  typedef T5 Type5;
  typedef T6 Type6;
  typedef T7 Type7;
  typedef T8 Type8;
  typedef T9 Type9;
  typedef T10 Type10;
  typedef T11 Type11;
  typedef T12 Type12;
  typedef T13 Type13;
  typedef T14 Type14;
  typedef T15 Type15;
  typedef T16 Type16;
  typedef T17 Type17;
  typedef T18 Type18;
  typedef T19 Type19;
  typedef T20 Type20;
  typedef T21 Type21;
  typedef T22 Type22;
  typedef T23 Type23;
  typedef T24 Type24;
  typedef T25 Type25;
  typedef T26 Type26;
  typedef T27 Type27;
  typedef T28 Type28;
  typedef T29 Type29;
  typedef T30 Type30;
  typedef T31 Type31;
  typedef T32 Type32;
  typedef T33 Type33;
  typedef T34 Type34;
  typedef T35 Type35;
  typedef T36 Type36;
  typedef T37 Type37;
  typedef T38 Type38;
  typedef T39 Type39;
  typedef T40 Type40;

  // The default constructor defines a Variant with an invalid tag and leaves
  // the Variant value in an undefined state.
  Variant() : tag_(-1), count_(0U) {}
  ~Variant() { Destroy(); }

  // Each version of Set() sets the variant to contain a value of one of the
  // defined types or to a type that is derived from or otherwise convertible
  // to one of those types.
  void Set(const T1& value) { SetValue<T1>(value, &values_.t1); }
  void Set(const T2& value) { SetValue<T2>(value, &values_.t2); }
  void Set(const T3& value) { SetValue<T3>(value, &values_.t3); }
  void Set(const T4& value) { SetValue<T4>(value, &values_.t4); }
  void Set(const T5& value) { SetValue<T5>(value, &values_.t5); }
  void Set(const T6& value) { SetValue<T6>(value, &values_.t6); }
  void Set(const T7& value) { SetValue<T7>(value, &values_.t7); }
  void Set(const T8& value) { SetValue<T8>(value, &values_.t8); }
  void Set(const T9& value) { SetValue<T9>(value, &values_.t9); }
  void Set(const T10& value) { SetValue<T10>(value, &values_.t10); }
  void Set(const T11& value) { SetValue<T11>(value, &values_.t11); }
  void Set(const T12& value) { SetValue<T12>(value, &values_.t12); }
  void Set(const T13& value) { SetValue<T13>(value, &values_.t13); }
  void Set(const T14& value) { SetValue<T14>(value, &values_.t14); }
  void Set(const T15& value) { SetValue<T15>(value, &values_.t15); }
  void Set(const T16& value) { SetValue<T16>(value, &values_.t16); }
  void Set(const T17& value) { SetValue<T17>(value, &values_.t17); }
  void Set(const T18& value) { SetValue<T18>(value, &values_.t18); }
  void Set(const T19& value) { SetValue<T19>(value, &values_.t19); }
  void Set(const T20& value) { SetValue<T20>(value, &values_.t20); }
  void Set(const T21& value) { SetValue<T21>(value, &values_.t21); }
  void Set(const T22& value) { SetValue<T22>(value, &values_.t22); }
  void Set(const T23& value) { SetValue<T23>(value, &values_.t23); }
  void Set(const T24& value) { SetValue<T24>(value, &values_.t24); }
  void Set(const T25& value) { SetValue<T25>(value, &values_.t25); }
  void Set(const T26& value) { SetValue<T26>(value, &values_.t26); }
  void Set(const T27& value) { SetValue<T27>(value, &values_.t27); }
  void Set(const T28& value) { SetValue<T28>(value, &values_.t28); }
  void Set(const T29& value) { SetValue<T29>(value, &values_.t29); }
  void Set(const T30& value) { SetValue<T30>(value, &values_.t30); }
  void Set(const T31& value) { SetValue<T31>(value, &values_.t31); }
  void Set(const T32& value) { SetValue<T32>(value, &values_.t32); }
  void Set(const T33& value) { SetValue<T33>(value, &values_.t33); }
  void Set(const T34& value) { SetValue<T34>(value, &values_.t34); }
  void Set(const T35& value) { SetValue<T35>(value, &values_.t35); }
  void Set(const T36& value) { SetValue<T36>(value, &values_.t36); }
  void Set(const T37& value) { SetValue<T37>(value, &values_.t37); }
  void Set(const T38& value) { SetValue<T38>(value, &values_.t38); }
  void Set(const T39& value) { SetValue<T39>(value, &values_.t39); }
  void Set(const T40& value) { SetValue<T40>(value, &values_.t40); }

  // Sets the type of this variant to be an array of count Ts. T must be a valid
  // type of this. The passed allocator is used to allocate the memory for the
  // elements; if it is null, then the current default allocator is used. This
  // destroys any existing elements; variants do not resize like STL containers.
  template <typename T>
  void InitArray(const base::AllocatorPtr& allocator, size_t count) {
    Destroy();
    alloc_ = AllocationManager::GetNonNullAllocator(allocator);
    count_ = count;
    tag_ = Tag<T>::kValue;
    MakeArray(static_cast<const T*>(nullptr), count);
  }

  // Copies the variant's type and value from another instance.
  void CopyFrom(const Variant& from) {
    if (&from != this) {
      Destroy();
      alloc_ = from.alloc_;
      count_ = from.count_;
      tag_ = from.tag_;
      CopyValueFrom(from);
    }
  }

  // Returns true if this contains an object of type T, which must be an exact
  // match with one of the template parameter types.
  template <typename T> bool Is() const {
    ION_STATIC_ASSERT(!internal_variant_utils::is_emptytype<T>::value,
                      "Empty type for Variant::Is");
    return !count_ && tag_ == ExactTag<T>::kValue;
  }

  // Returns true if this contains an array of type T, which must be an exact
  // match with one of the template parameter types.
  template <typename T> bool IsArrayOf() const {
    ION_STATIC_ASSERT(!internal_variant_utils::is_emptytype<T>::value,
                      "Empty type for Variant::IsArrayOf");
    return count_ && tag_ == ExactTag<T>::kValue;
  }

  // Returns true if this contains an object of type T or an object which a T
  // can be assigned to.
  //
  // For example:
  //  Variant<float, Base> v;
  //  v.Set(13.2f);
  //  v.IsAssignableTo<float>();  // Returns true (float is already stored).
  //  v.IsAssignableTo<int>();    // Returns true (int -> float conversion).
  //  v.IsAssignableTo<Base>();   // Returns false.
  //  Base b;
  //  v.Set(b);
  //  v.IsAssignableTo<float>();    // Returns false.
  //  v.IsAssignableTo<Base>();     // Returns true.
  //  v.IsAssignableTo<Derived>();  // Returns true (derived from Base).
  template <typename T> bool IsAssignableTo() const {
    ION_STATIC_ASSERT(!internal_variant_utils::is_emptytype<T>::value,
                      "Empty type for Variant::IsAssignableTo");
    return !count_ && tag_ == Tag<T>::kValue;
  }
  // Similar to above but only returns true if the elements of the array that
  // this contains are assignable from T.
  template <typename T> bool ElementsAssignableTo() const {
    ION_STATIC_ASSERT(!internal_variant_utils::is_emptytype<T>::value,
                      "Empty type for Variant::ElementsAssignableTo");
    return count_ && tag_ == Tag<T>::kValue;
  }

  // If this contains an object of type T (which must be one of the defined
  // types), this returns a const reference to it.  Otherwise, it returns an
  // InvalidReference.
  template <typename T> const T& Get() const {
    return Is<T>() ? GetValue(static_cast<const T*>(nullptr)) :
        InvalidReference<T>();
  }

  // If this contains an array of objects of type T (which must be one of the
  // defined types), this returns a const reference to it if the index is valid.
  // Otherwise, it returns an InvalidReference.
  template <typename T> const T& GetValueAt(size_t i) const {
    return IsArrayOf<T>() && i < count_
        ? GetAt(static_cast<const T*>(nullptr), i)
        : InvalidReference<T>();
  }

  // Sets the ith element of the array to the passed value. If the index is
  // invalid or this variant is not an array of type T, nothing will happen.
  template <typename T> void SetValueAt(size_t i, const T& value) {
    if (ElementsAssignableTo<T>() && i < count_)
      SetAt(i, value);
  }

  // Copy constructor and assignment operator. Variant instances must be able
  // to be copied.
  Variant(const Variant& from) : tag_(-1), count_(0U) { CopyFrom(from); }
  Variant& operator=(const Variant& from) {
    CopyFrom(from);
    return *this;
  }

  // Returns the number of array elements this contains, which is 0 when this
  // holds only a scalar value.
  size_t GetCount() const { return count_; }

  // Returns the allocator used to make array allocations.
  const AllocatorPtr& GetArrayAllocator() const { return alloc_; }

  // ArrayAccessor lets callers use operator[] with a Variant in most simple
  // cases, as opposed to using the SetValueAt(i) and GetValueAt(i) functions
  // above. The accessor acts as a proxy between the array operator below on the
  // Variant and the actual getting, setting, or comparison of the value. Note
  // that in some cases a static_cast is required for the accessor to know the
  // array type of the underlying Variant. Examples:
  //   Variant<int, double> v;
  //   const float f = 10.f;
  //   v.InitArray<int>(Allocator(nullptr), 2);
  //   v[0] = 10;
  //   v[1] = static_cast<int>(f);
  //   const int i = v[0];
  //   const float f2 = static_cast<float>(v[1]);

  struct ArrayAccessor {
    ArrayAccessor(Variant* v, size_t i) : variant_(v), i(i) {}
    template <typename T>
    operator const T() const {
      return variant_->template GetValueAt<T>(i);
    }
    template <typename T>
    void operator=(const T& value) {
      variant_->SetValueAt(i, value);
    }
    template <typename T>
    bool operator==(const T& value) const {
      return variant_->template GetValueAt<T>(i) == value;
    }

    Variant* variant_;
    size_t i;
  };
  // Returns an ArrayAccessor object that facilitates the getting of the actual
  // array element from this.
  ArrayAccessor operator[](size_t i) {
    return ArrayAccessor(this, i);
  }

 private:
  // Helper struct for determining tag values.
  template <int N> struct SizedArray { char array[N]; };

  // This overloaded function allows derived and convertible types to be
  // supported when setting values.  It returns an array of a different size
  // for each defined type.  sizeof is applied to the resulting array to set
  // the tag in an instance.
  //
  // A compiler error will be issued unless the passed type is one of the
  // defined types or there is only one unambiguous conversion path from the
  // passed type to one of them.  [It would be nice to be able to use template
  // specialization to do this, but C++ does not allow that within a class
  // scope.]
  static SizedArray<1> GetSizedArrayForTag(const T1&);
  static SizedArray<2> GetSizedArrayForTag(const T2&);
  static SizedArray<3> GetSizedArrayForTag(const T3&);
  static SizedArray<4> GetSizedArrayForTag(const T4&);
  static SizedArray<5> GetSizedArrayForTag(const T5&);
  static SizedArray<6> GetSizedArrayForTag(const T6&);
  static SizedArray<7> GetSizedArrayForTag(const T7&);
  static SizedArray<8> GetSizedArrayForTag(const T8&);
  static SizedArray<9> GetSizedArrayForTag(const T9&);
  static SizedArray<10> GetSizedArrayForTag(const T10&);
  static SizedArray<11> GetSizedArrayForTag(const T11&);
  static SizedArray<12> GetSizedArrayForTag(const T12&);
  static SizedArray<13> GetSizedArrayForTag(const T13&);
  static SizedArray<14> GetSizedArrayForTag(const T14&);
  static SizedArray<15> GetSizedArrayForTag(const T15&);
  static SizedArray<16> GetSizedArrayForTag(const T16&);
  static SizedArray<17> GetSizedArrayForTag(const T17&);
  static SizedArray<18> GetSizedArrayForTag(const T18&);
  static SizedArray<19> GetSizedArrayForTag(const T19&);
  static SizedArray<20> GetSizedArrayForTag(const T20&);
  static SizedArray<21> GetSizedArrayForTag(const T21&);
  static SizedArray<22> GetSizedArrayForTag(const T22&);
  static SizedArray<23> GetSizedArrayForTag(const T23&);
  static SizedArray<24> GetSizedArrayForTag(const T24&);
  static SizedArray<25> GetSizedArrayForTag(const T25&);
  static SizedArray<26> GetSizedArrayForTag(const T26&);
  static SizedArray<27> GetSizedArrayForTag(const T27&);
  static SizedArray<28> GetSizedArrayForTag(const T28&);
  static SizedArray<29> GetSizedArrayForTag(const T29&);
  static SizedArray<30> GetSizedArrayForTag(const T30&);
  static SizedArray<31> GetSizedArrayForTag(const T31&);
  static SizedArray<32> GetSizedArrayForTag(const T32&);
  static SizedArray<33> GetSizedArrayForTag(const T33&);
  static SizedArray<34> GetSizedArrayForTag(const T34&);
  static SizedArray<35> GetSizedArrayForTag(const T35&);
  static SizedArray<36> GetSizedArrayForTag(const T36&);
  static SizedArray<37> GetSizedArrayForTag(const T37&);
  static SizedArray<38> GetSizedArrayForTag(const T38&);
  static SizedArray<39> GetSizedArrayForTag(const T39&);
  static SizedArray<40> GetSizedArrayForTag(const T40&);

  // This is the same as the above function, except that it requires an exact
  // match with one of the types. This is used when derived/convertible types
  // are not allowed.
  template<typename T> struct ExactMatch {};
  static SizedArray<1> GetSizedArrayForExactTag(const ExactMatch<T1>&);
  static SizedArray<2> GetSizedArrayForExactTag(const ExactMatch<T2>&);
  static SizedArray<3> GetSizedArrayForExactTag(const ExactMatch<T3>&);
  static SizedArray<4> GetSizedArrayForExactTag(const ExactMatch<T4>&);
  static SizedArray<5> GetSizedArrayForExactTag(const ExactMatch<T5>&);
  static SizedArray<6> GetSizedArrayForExactTag(const ExactMatch<T6>&);
  static SizedArray<7> GetSizedArrayForExactTag(const ExactMatch<T7>&);
  static SizedArray<8> GetSizedArrayForExactTag(const ExactMatch<T8>&);
  static SizedArray<9> GetSizedArrayForExactTag(const ExactMatch<T9>&);
  static SizedArray<10> GetSizedArrayForExactTag(const ExactMatch<T10>&);
  static SizedArray<11> GetSizedArrayForExactTag(const ExactMatch<T11>&);
  static SizedArray<12> GetSizedArrayForExactTag(const ExactMatch<T12>&);
  static SizedArray<13> GetSizedArrayForExactTag(const ExactMatch<T13>&);
  static SizedArray<14> GetSizedArrayForExactTag(const ExactMatch<T14>&);
  static SizedArray<15> GetSizedArrayForExactTag(const ExactMatch<T15>&);
  static SizedArray<16> GetSizedArrayForExactTag(const ExactMatch<T16>&);
  static SizedArray<17> GetSizedArrayForExactTag(const ExactMatch<T17>&);
  static SizedArray<18> GetSizedArrayForExactTag(const ExactMatch<T18>&);
  static SizedArray<19> GetSizedArrayForExactTag(const ExactMatch<T19>&);
  static SizedArray<20> GetSizedArrayForExactTag(const ExactMatch<T20>&);
  static SizedArray<21> GetSizedArrayForExactTag(const ExactMatch<T21>&);
  static SizedArray<22> GetSizedArrayForExactTag(const ExactMatch<T22>&);
  static SizedArray<23> GetSizedArrayForExactTag(const ExactMatch<T23>&);
  static SizedArray<24> GetSizedArrayForExactTag(const ExactMatch<T24>&);
  static SizedArray<25> GetSizedArrayForExactTag(const ExactMatch<T25>&);
  static SizedArray<26> GetSizedArrayForExactTag(const ExactMatch<T26>&);
  static SizedArray<27> GetSizedArrayForExactTag(const ExactMatch<T27>&);
  static SizedArray<28> GetSizedArrayForExactTag(const ExactMatch<T28>&);
  static SizedArray<29> GetSizedArrayForExactTag(const ExactMatch<T29>&);
  static SizedArray<30> GetSizedArrayForExactTag(const ExactMatch<T30>&);
  static SizedArray<31> GetSizedArrayForExactTag(const ExactMatch<T31>&);
  static SizedArray<32> GetSizedArrayForExactTag(const ExactMatch<T32>&);
  static SizedArray<33> GetSizedArrayForExactTag(const ExactMatch<T33>&);
  static SizedArray<34> GetSizedArrayForExactTag(const ExactMatch<T34>&);
  static SizedArray<35> GetSizedArrayForExactTag(const ExactMatch<T35>&);
  static SizedArray<36> GetSizedArrayForExactTag(const ExactMatch<T36>&);
  static SizedArray<37> GetSizedArrayForExactTag(const ExactMatch<T37>&);
  static SizedArray<38> GetSizedArrayForExactTag(const ExactMatch<T38>&);
  static SizedArray<39> GetSizedArrayForExactTag(const ExactMatch<T39>&);
  static SizedArray<40> GetSizedArrayForExactTag(const ExactMatch<T40>&);

  // The Tag and ExactTag structs are used to set the tag_ field in a Variant
  // instance, using the kValue member. Tag allows derived/convertible types to
  // be used, while ExactTag requires an exact match.
  template <typename T> struct Tag {
    static const int kValue =
        sizeof(GetSizedArrayForTag(*static_cast<const T*>(nullptr)).array);
  };
  template <typename T> struct ExactTag {
    static const int kValue =
        sizeof(GetSizedArrayForExactTag(ExactMatch<T>()).array);
  };

  // Destroys any stored object.
  void Destroy() {
    switch (tag_) {
      case Tag<T1>::kValue: values_.t1.Destroy(alloc_, count_); break;
      case Tag<T2>::kValue: values_.t2.Destroy(alloc_, count_); break;
      case Tag<T3>::kValue: values_.t3.Destroy(alloc_, count_); break;
      case Tag<T4>::kValue: values_.t4.Destroy(alloc_, count_); break;
      case Tag<T5>::kValue: values_.t5.Destroy(alloc_, count_); break;
      case Tag<T6>::kValue: values_.t6.Destroy(alloc_, count_); break;
      case Tag<T7>::kValue: values_.t7.Destroy(alloc_, count_); break;
      case Tag<T8>::kValue: values_.t8.Destroy(alloc_, count_); break;
      case Tag<T9>::kValue: values_.t9.Destroy(alloc_, count_); break;
      case Tag<T10>::kValue: values_.t10.Destroy(alloc_, count_); break;
      case Tag<T11>::kValue: values_.t11.Destroy(alloc_, count_); break;
      case Tag<T12>::kValue: values_.t12.Destroy(alloc_, count_); break;
      case Tag<T13>::kValue: values_.t13.Destroy(alloc_, count_); break;
      case Tag<T14>::kValue: values_.t14.Destroy(alloc_, count_); break;
      case Tag<T15>::kValue: values_.t15.Destroy(alloc_, count_); break;
      case Tag<T16>::kValue: values_.t16.Destroy(alloc_, count_); break;
      case Tag<T17>::kValue: values_.t17.Destroy(alloc_, count_); break;
      case Tag<T18>::kValue: values_.t18.Destroy(alloc_, count_); break;
      case Tag<T19>::kValue: values_.t19.Destroy(alloc_, count_); break;
      case Tag<T20>::kValue: values_.t20.Destroy(alloc_, count_); break;
      case Tag<T21>::kValue: values_.t21.Destroy(alloc_, count_); break;
      case Tag<T22>::kValue: values_.t22.Destroy(alloc_, count_); break;
      case Tag<T23>::kValue: values_.t23.Destroy(alloc_, count_); break;
      case Tag<T24>::kValue: values_.t24.Destroy(alloc_, count_); break;
      case Tag<T25>::kValue: values_.t25.Destroy(alloc_, count_); break;
      case Tag<T26>::kValue: values_.t26.Destroy(alloc_, count_); break;
      case Tag<T27>::kValue: values_.t27.Destroy(alloc_, count_); break;
      case Tag<T28>::kValue: values_.t28.Destroy(alloc_, count_); break;
      case Tag<T29>::kValue: values_.t29.Destroy(alloc_, count_); break;
      case Tag<T30>::kValue: values_.t30.Destroy(alloc_, count_); break;
      case Tag<T31>::kValue: values_.t31.Destroy(alloc_, count_); break;
      case Tag<T32>::kValue: values_.t32.Destroy(alloc_, count_); break;
      case Tag<T33>::kValue: values_.t33.Destroy(alloc_, count_); break;
      case Tag<T34>::kValue: values_.t34.Destroy(alloc_, count_); break;
      case Tag<T35>::kValue: values_.t35.Destroy(alloc_, count_); break;
      case Tag<T36>::kValue: values_.t36.Destroy(alloc_, count_); break;
      case Tag<T37>::kValue: values_.t37.Destroy(alloc_, count_); break;
      case Tag<T38>::kValue: values_.t38.Destroy(alloc_, count_); break;
      case Tag<T39>::kValue: values_.t39.Destroy(alloc_, count_); break;
      case Tag<T40>::kValue: values_.t40.Destroy(alloc_, count_); break;
      default: break;
    }
    alloc_.Reset(nullptr);
  }

  void MakeArray(const T1*, size_t count) { values_.t1.Init(alloc_, count); }
  void MakeArray(const T2*, size_t count) { values_.t2.Init(alloc_, count); }
  void MakeArray(const T3*, size_t count) { values_.t3.Init(alloc_, count); }
  void MakeArray(const T4*, size_t count) { values_.t4.Init(alloc_, count); }
  void MakeArray(const T5*, size_t count) { values_.t5.Init(alloc_, count); }
  void MakeArray(const T6*, size_t count) { values_.t6.Init(alloc_, count); }
  void MakeArray(const T7*, size_t count) { values_.t7.Init(alloc_, count); }
  void MakeArray(const T8*, size_t count) { values_.t8.Init(alloc_, count); }
  void MakeArray(const T9*, size_t count) { values_.t9.Init(alloc_, count); }
  void MakeArray(const T10*, size_t count) { values_.t10.Init(alloc_, count); }
  void MakeArray(const T11*, size_t count) { values_.t11.Init(alloc_, count); }
  void MakeArray(const T12*, size_t count) { values_.t12.Init(alloc_, count); }
  void MakeArray(const T13*, size_t count) { values_.t13.Init(alloc_, count); }
  void MakeArray(const T14*, size_t count) { values_.t14.Init(alloc_, count); }
  void MakeArray(const T15*, size_t count) { values_.t15.Init(alloc_, count); }
  void MakeArray(const T16*, size_t count) { values_.t16.Init(alloc_, count); }
  void MakeArray(const T17*, size_t count) { values_.t17.Init(alloc_, count); }
  void MakeArray(const T18*, size_t count) { values_.t18.Init(alloc_, count); }
  void MakeArray(const T19*, size_t count) { values_.t19.Init(alloc_, count); }
  void MakeArray(const T20*, size_t count) { values_.t20.Init(alloc_, count); }
  void MakeArray(const T21*, size_t count) { values_.t21.Init(alloc_, count); }
  void MakeArray(const T22*, size_t count) { values_.t22.Init(alloc_, count); }
  void MakeArray(const T23*, size_t count) { values_.t23.Init(alloc_, count); }
  void MakeArray(const T24*, size_t count) { values_.t24.Init(alloc_, count); }
  void MakeArray(const T25*, size_t count) { values_.t25.Init(alloc_, count); }
  void MakeArray(const T26*, size_t count) { values_.t26.Init(alloc_, count); }
  void MakeArray(const T27*, size_t count) { values_.t27.Init(alloc_, count); }
  void MakeArray(const T28*, size_t count) { values_.t28.Init(alloc_, count); }
  void MakeArray(const T29*, size_t count) { values_.t29.Init(alloc_, count); }
  void MakeArray(const T30*, size_t count) { values_.t30.Init(alloc_, count); }
  void MakeArray(const T31*, size_t count) { values_.t31.Init(alloc_, count); }
  void MakeArray(const T32*, size_t count) { values_.t32.Init(alloc_, count); }
  void MakeArray(const T33*, size_t count) { values_.t33.Init(alloc_, count); }
  void MakeArray(const T34*, size_t count) { values_.t34.Init(alloc_, count); }
  void MakeArray(const T35*, size_t count) { values_.t35.Init(alloc_, count); }
  void MakeArray(const T36*, size_t count) { values_.t36.Init(alloc_, count); }
  void MakeArray(const T37*, size_t count) { values_.t37.Init(alloc_, count); }
  void MakeArray(const T38*, size_t count) { values_.t38.Init(alloc_, count); }
  void MakeArray(const T39*, size_t count) { values_.t39.Init(alloc_, count); }
  void MakeArray(const T40*, size_t count) { values_.t40.Init(alloc_, count); }

  // Sets one of the union members to the given value.
  template <typename T> void SetValue(
      const T& value, internal_variant_utils::ManualConstructor<T>* target) {
    ION_STATIC_ASSERT(!internal_variant_utils::is_emptytype<T>::value,
                      "Empty type for Variant::Set");
    Destroy();
    count_ = 0U;
    tag_ = Tag<T>::kValue;
    target->Init(value);
  }

  // Copies the value from another instance.
  void CopyValueFrom(const Variant& from) {
    if (from.count_) {
      switch (tag_) {
        case Tag<T1>::kValue:
          values_.t1.InitArray(alloc_, from.values_.t1, count_);
          break;
        case Tag<T2>::kValue:
          values_.t2.InitArray(alloc_, from.values_.t2, count_);
          break;
        case Tag<T3>::kValue:
          values_.t3.InitArray(alloc_, from.values_.t3, count_);
          break;
        case Tag<T4>::kValue:
          values_.t4.InitArray(alloc_, from.values_.t4, count_);
          break;
        case Tag<T5>::kValue:
          values_.t5.InitArray(alloc_, from.values_.t5, count_);
          break;
        case Tag<T6>::kValue:
          values_.t6.InitArray(alloc_, from.values_.t6, count_);
          break;
        case Tag<T7>::kValue:
          values_.t7.InitArray(alloc_, from.values_.t7, count_);
          break;
        case Tag<T8>::kValue:
          values_.t8.InitArray(alloc_, from.values_.t8, count_);
          break;
        case Tag<T9>::kValue:
          values_.t9.InitArray(alloc_, from.values_.t9, count_);
          break;
        case Tag<T10>::kValue:
          values_.t10.InitArray(alloc_, from.values_.t10, count_);
          break;
        case Tag<T11>::kValue:
          values_.t11.InitArray(alloc_, from.values_.t11, count_);
          break;
        case Tag<T12>::kValue:
          values_.t12.InitArray(alloc_, from.values_.t12, count_);
          break;
        case Tag<T13>::kValue:
          values_.t13.InitArray(alloc_, from.values_.t13, count_);
          break;
        case Tag<T14>::kValue:
          values_.t14.InitArray(alloc_, from.values_.t14, count_);
          break;
        case Tag<T15>::kValue:
          values_.t15.InitArray(alloc_, from.values_.t15, count_);
          break;
        case Tag<T16>::kValue:
          values_.t16.InitArray(alloc_, from.values_.t16, count_);
          break;
        case Tag<T17>::kValue:
          values_.t17.InitArray(alloc_, from.values_.t17, count_);
          break;
        case Tag<T18>::kValue:
          values_.t18.InitArray(alloc_, from.values_.t18, count_);
          break;
        case Tag<T19>::kValue:
          values_.t19.InitArray(alloc_, from.values_.t19, count_);
          break;
        case Tag<T20>::kValue:
          values_.t20.InitArray(alloc_, from.values_.t20, count_);
          break;
        case Tag<T21>::kValue:
          values_.t21.InitArray(alloc_, from.values_.t21, count_);
          break;
        case Tag<T22>::kValue:
          values_.t22.InitArray(alloc_, from.values_.t22, count_);
          break;
        case Tag<T23>::kValue:
          values_.t23.InitArray(alloc_, from.values_.t23, count_);
          break;
        case Tag<T24>::kValue:
          values_.t24.InitArray(alloc_, from.values_.t24, count_);
          break;
        case Tag<T25>::kValue:
          values_.t25.InitArray(alloc_, from.values_.t25, count_);
          break;
        case Tag<T26>::kValue:
          values_.t26.InitArray(alloc_, from.values_.t26, count_);
          break;
        case Tag<T27>::kValue:
          values_.t27.InitArray(alloc_, from.values_.t27, count_);
          break;
        case Tag<T28>::kValue:
          values_.t28.InitArray(alloc_, from.values_.t28, count_);
          break;
        case Tag<T29>::kValue:
          values_.t29.InitArray(alloc_, from.values_.t29, count_);
          break;
        case Tag<T30>::kValue:
          values_.t30.InitArray(alloc_, from.values_.t30, count_);
          break;
        case Tag<T31>::kValue:
          values_.t31.InitArray(alloc_, from.values_.t31, count_);
          break;
        case Tag<T32>::kValue:
          values_.t32.InitArray(alloc_, from.values_.t32, count_);
          break;
        case Tag<T33>::kValue:
          values_.t33.InitArray(alloc_, from.values_.t33, count_);
          break;
        case Tag<T34>::kValue:
          values_.t34.InitArray(alloc_, from.values_.t34, count_);
          break;
        case Tag<T35>::kValue:
          values_.t35.InitArray(alloc_, from.values_.t35, count_);
          break;
        case Tag<T36>::kValue:
          values_.t36.InitArray(alloc_, from.values_.t36, count_);
          break;
        case Tag<T37>::kValue:
          values_.t37.InitArray(alloc_, from.values_.t37, count_);
          break;
        case Tag<T38>::kValue:
          values_.t38.InitArray(alloc_, from.values_.t38, count_);
          break;
        case Tag<T39>::kValue:
          values_.t39.InitArray(alloc_, from.values_.t39, count_);
          break;
        case Tag<T40>::kValue:
          values_.t40.InitArray(alloc_, from.values_.t40, count_);
          break;
        default:
#if !defined(ION_COVERAGE)  // COV_NF_START
          // This should never be reachable: if the "from" instance is an array
          // of values, it must have a valid tag.
          DCHECK(false) << "Invalid tag in array variant";
#endif  // COV_NF_END
          break;
      }
    } else {
      switch (tag_) {
        case Tag<T1>::kValue:
          values_.t1.Init(*(from.values_.t1.Get()));
          break;
        case Tag<T2>::kValue:
          values_.t2.Init(*(from.values_.t2.Get()));
          break;
        case Tag<T3>::kValue:
          values_.t3.Init(*(from.values_.t3.Get()));
          break;
        case Tag<T4>::kValue:
          values_.t4.Init(*(from.values_.t4.Get()));
          break;
        case Tag<T5>::kValue:
          values_.t5.Init(*(from.values_.t5.Get()));
          break;
        case Tag<T6>::kValue:
          values_.t6.Init(*(from.values_.t6.Get()));
          break;
        case Tag<T7>::kValue:
          values_.t7.Init(*(from.values_.t7.Get()));
          break;
        case Tag<T8>::kValue:
          values_.t8.Init(*(from.values_.t8.Get()));
          break;
        case Tag<T9>::kValue:
          values_.t9.Init(*(from.values_.t9.Get()));
          break;
        case Tag<T10>::kValue:
          values_.t10.Init(*(from.values_.t10.Get()));
          break;
        case Tag<T11>::kValue:
          values_.t11.Init(*(from.values_.t11.Get()));
          break;
        case Tag<T12>::kValue:
          values_.t12.Init(*(from.values_.t12.Get()));
          break;
        case Tag<T13>::kValue:
          values_.t13.Init(*(from.values_.t13.Get()));
          break;
        case Tag<T14>::kValue:
          values_.t14.Init(*(from.values_.t14.Get()));
          break;
        case Tag<T15>::kValue:
          values_.t15.Init(*(from.values_.t15.Get()));
          break;
        case Tag<T16>::kValue:
          values_.t16.Init(*(from.values_.t16.Get()));
          break;
        case Tag<T17>::kValue:
          values_.t17.Init(*(from.values_.t17.Get()));
          break;
        case Tag<T18>::kValue:
          values_.t18.Init(*(from.values_.t18.Get()));
          break;
        case Tag<T19>::kValue:
          values_.t19.Init(*(from.values_.t19.Get()));
          break;
        case Tag<T20>::kValue:
          values_.t20.Init(*(from.values_.t20.Get()));
          break;
        case Tag<T21>::kValue:
          values_.t21.Init(*(from.values_.t21.Get()));
          break;
        case Tag<T22>::kValue:
          values_.t22.Init(*(from.values_.t22.Get()));
          break;
        case Tag<T23>::kValue:
          values_.t23.Init(*(from.values_.t23.Get()));
          break;
        case Tag<T24>::kValue:
          values_.t24.Init(*(from.values_.t24.Get()));
          break;
        case Tag<T25>::kValue:
          values_.t25.Init(*(from.values_.t25.Get()));
          break;
        case Tag<T26>::kValue:
          values_.t26.Init(*(from.values_.t26.Get()));
          break;
        case Tag<T27>::kValue:
          values_.t27.Init(*(from.values_.t27.Get()));
          break;
        case Tag<T28>::kValue:
          values_.t28.Init(*(from.values_.t28.Get()));
          break;
        case Tag<T29>::kValue:
          values_.t29.Init(*(from.values_.t29.Get()));
          break;
        case Tag<T30>::kValue:
          values_.t30.Init(*(from.values_.t30.Get()));
          break;
        case Tag<T31>::kValue:
          values_.t31.Init(*(from.values_.t31.Get()));
          break;
        case Tag<T32>::kValue:
          values_.t32.Init(*(from.values_.t32.Get()));
          break;
        case Tag<T33>::kValue:
          values_.t33.Init(*(from.values_.t33.Get()));
          break;
        case Tag<T34>::kValue:
          values_.t34.Init(*(from.values_.t34.Get()));
          break;
        case Tag<T35>::kValue:
          values_.t35.Init(*(from.values_.t35.Get()));
          break;
        case Tag<T36>::kValue:
          values_.t36.Init(*(from.values_.t36.Get()));
          break;
        case Tag<T37>::kValue:
          values_.t37.Init(*(from.values_.t37.Get()));
          break;
        case Tag<T38>::kValue:
          values_.t38.Init(*(from.values_.t38.Get()));
          break;
        case Tag<T39>::kValue:
          values_.t39.Init(*(from.values_.t39.Get()));
          break;
        case Tag<T40>::kValue:
          values_.t40.Init(*(from.values_.t40.Get()));
          break;
        default:
          break;
      }
    }
  }

  // Overloaded function to access the value of the correct member of the
  // union.
  const T1& GetValue(const T1*) const { return *values_.t1.Get(); }
  const T2& GetValue(const T2*) const { return *values_.t2.Get(); }
  const T3& GetValue(const T3*) const { return *values_.t3.Get(); }
  const T4& GetValue(const T4*) const { return *values_.t4.Get(); }
  const T5& GetValue(const T5*) const { return *values_.t5.Get(); }
  const T6& GetValue(const T6*) const { return *values_.t6.Get(); }
  const T7& GetValue(const T7*) const { return *values_.t7.Get(); }
  const T8& GetValue(const T8*) const { return *values_.t8.Get(); }
  const T9& GetValue(const T9*) const { return *values_.t9.Get(); }
  const T10& GetValue(const T10*) const { return *values_.t10.Get(); }
  const T11& GetValue(const T11*) const { return *values_.t11.Get(); }
  const T12& GetValue(const T12*) const { return *values_.t12.Get(); }
  const T13& GetValue(const T13*) const { return *values_.t13.Get(); }
  const T14& GetValue(const T14*) const { return *values_.t14.Get(); }
  const T15& GetValue(const T15*) const { return *values_.t15.Get(); }
  const T16& GetValue(const T16*) const { return *values_.t16.Get(); }
  const T17& GetValue(const T17*) const { return *values_.t17.Get(); }
  const T18& GetValue(const T18*) const { return *values_.t18.Get(); }
  const T19& GetValue(const T19*) const { return *values_.t19.Get(); }
  const T20& GetValue(const T20*) const { return *values_.t20.Get(); }
  const T21& GetValue(const T21*) const { return *values_.t21.Get(); }
  const T22& GetValue(const T22*) const { return *values_.t22.Get(); }
  const T23& GetValue(const T23*) const { return *values_.t23.Get(); }
  const T24& GetValue(const T24*) const { return *values_.t24.Get(); }
  const T25& GetValue(const T25*) const { return *values_.t25.Get(); }
  const T26& GetValue(const T26*) const { return *values_.t26.Get(); }
  const T27& GetValue(const T27*) const { return *values_.t27.Get(); }
  const T28& GetValue(const T28*) const { return *values_.t28.Get(); }
  const T29& GetValue(const T29*) const { return *values_.t29.Get(); }
  const T30& GetValue(const T30*) const { return *values_.t30.Get(); }
  const T31& GetValue(const T31*) const { return *values_.t31.Get(); }
  const T32& GetValue(const T32*) const { return *values_.t32.Get(); }
  const T33& GetValue(const T33*) const { return *values_.t33.Get(); }
  const T34& GetValue(const T34*) const { return *values_.t34.Get(); }
  const T35& GetValue(const T35*) const { return *values_.t35.Get(); }
  const T36& GetValue(const T36*) const { return *values_.t36.Get(); }
  const T37& GetValue(const T37*) const { return *values_.t37.Get(); }
  const T38& GetValue(const T38*) const { return *values_.t38.Get(); }
  const T39& GetValue(const T39*) const { return *values_.t39.Get(); }
  const T40& GetValue(const T40*) const { return *values_.t40.Get(); }

  // Overloaded function to get the i-th element of the proper type. No range
  // checking is performed; that must be done before these are called.
  const T1& GetAt(const T1*, size_t i) const { return values_.t1.GetAt(i); }
  const T2& GetAt(const T2*, size_t i) const { return values_.t2.GetAt(i); }
  const T3& GetAt(const T3*, size_t i) const { return values_.t3.GetAt(i); }
  const T4& GetAt(const T4*, size_t i) const { return values_.t4.GetAt(i); }
  const T5& GetAt(const T5*, size_t i) const { return values_.t5.GetAt(i); }
  const T6& GetAt(const T6*, size_t i) const { return values_.t6.GetAt(i); }
  const T7& GetAt(const T7*, size_t i) const { return values_.t7.GetAt(i); }
  const T8& GetAt(const T8*, size_t i) const { return values_.t8.GetAt(i); }
  const T9& GetAt(const T9*, size_t i) const { return values_.t9.GetAt(i); }
  const T10& GetAt(const T10*, size_t i) const { return values_.t10.GetAt(i); }
  const T11& GetAt(const T11*, size_t i) const { return values_.t11.GetAt(i); }
  const T12& GetAt(const T12*, size_t i) const { return values_.t12.GetAt(i); }
  const T13& GetAt(const T13*, size_t i) const { return values_.t13.GetAt(i); }
  const T14& GetAt(const T14*, size_t i) const { return values_.t14.GetAt(i); }
  const T15& GetAt(const T15*, size_t i) const { return values_.t15.GetAt(i); }
  const T16& GetAt(const T16*, size_t i) const { return values_.t16.GetAt(i); }
  const T17& GetAt(const T17*, size_t i) const { return values_.t17.GetAt(i); }
  const T18& GetAt(const T18*, size_t i) const { return values_.t18.GetAt(i); }
  const T19& GetAt(const T19*, size_t i) const { return values_.t19.GetAt(i); }
  const T20& GetAt(const T20*, size_t i) const { return values_.t20.GetAt(i); }
  const T21& GetAt(const T21*, size_t i) const { return values_.t21.GetAt(i); }
  const T22& GetAt(const T22*, size_t i) const { return values_.t22.GetAt(i); }
  const T23& GetAt(const T23*, size_t i) const { return values_.t23.GetAt(i); }
  const T24& GetAt(const T24*, size_t i) const { return values_.t24.GetAt(i); }
  const T25& GetAt(const T25*, size_t i) const { return values_.t25.GetAt(i); }
  const T26& GetAt(const T26*, size_t i) const { return values_.t26.GetAt(i); }
  const T27& GetAt(const T27*, size_t i) const { return values_.t27.GetAt(i); }
  const T28& GetAt(const T28*, size_t i) const { return values_.t28.GetAt(i); }
  const T29& GetAt(const T29*, size_t i) const { return values_.t29.GetAt(i); }
  const T30& GetAt(const T30*, size_t i) const { return values_.t30.GetAt(i); }
  const T31& GetAt(const T31*, size_t i) const { return values_.t31.GetAt(i); }
  const T32& GetAt(const T32*, size_t i) const { return values_.t32.GetAt(i); }
  const T33& GetAt(const T33*, size_t i) const { return values_.t33.GetAt(i); }
  const T34& GetAt(const T34*, size_t i) const { return values_.t34.GetAt(i); }
  const T35& GetAt(const T35*, size_t i) const { return values_.t35.GetAt(i); }
  const T36& GetAt(const T36*, size_t i) const { return values_.t36.GetAt(i); }
  const T37& GetAt(const T37*, size_t i) const { return values_.t37.GetAt(i); }
  const T38& GetAt(const T38*, size_t i) const { return values_.t38.GetAt(i); }
  const T39& GetAt(const T39*, size_t i) const { return values_.t39.GetAt(i); }
  const T40& GetAt(const T40*, size_t i) const { return values_.t40.GetAt(i); }

  // Sets the ith element of the array to the passed value. If the index is
  // invalid or this variant is not an array, nothing will happen.
  void SetAt(size_t i, const T1& v) { values_.t1.SetAt(i, v); }
  void SetAt(size_t i, const T2& v) { values_.t2.SetAt(i, v); }
  void SetAt(size_t i, const T3& v) { values_.t3.SetAt(i, v); }
  void SetAt(size_t i, const T4& v) { values_.t4.SetAt(i, v); }
  void SetAt(size_t i, const T5& v) { values_.t5.SetAt(i, v); }
  void SetAt(size_t i, const T6& v) { values_.t6.SetAt(i, v); }
  void SetAt(size_t i, const T7& v) { values_.t7.SetAt(i, v); }
  void SetAt(size_t i, const T8& v) { values_.t8.SetAt(i, v); }
  void SetAt(size_t i, const T9& v) { values_.t9.SetAt(i, v); }
  void SetAt(size_t i, const T10& v) { values_.t10.SetAt(i, v); }
  void SetAt(size_t i, const T11& v) { values_.t11.SetAt(i, v); }
  void SetAt(size_t i, const T12& v) { values_.t12.SetAt(i, v); }
  void SetAt(size_t i, const T13& v) { values_.t13.SetAt(i, v); }
  void SetAt(size_t i, const T14& v) { values_.t14.SetAt(i, v); }
  void SetAt(size_t i, const T15& v) { values_.t15.SetAt(i, v); }
  void SetAt(size_t i, const T16& v) { values_.t16.SetAt(i, v); }
  void SetAt(size_t i, const T17& v) { values_.t17.SetAt(i, v); }
  void SetAt(size_t i, const T18& v) { values_.t18.SetAt(i, v); }
  void SetAt(size_t i, const T19& v) { values_.t19.SetAt(i, v); }
  void SetAt(size_t i, const T20& v) { values_.t20.SetAt(i, v); }
  void SetAt(size_t i, const T21& v) { values_.t21.SetAt(i, v); }
  void SetAt(size_t i, const T22& v) { values_.t22.SetAt(i, v); }
  void SetAt(size_t i, const T23& v) { values_.t23.SetAt(i, v); }
  void SetAt(size_t i, const T24& v) { values_.t24.SetAt(i, v); }
  void SetAt(size_t i, const T25& v) { values_.t25.SetAt(i, v); }
  void SetAt(size_t i, const T26& v) { values_.t26.SetAt(i, v); }
  void SetAt(size_t i, const T27& v) { values_.t27.SetAt(i, v); }
  void SetAt(size_t i, const T28& v) { values_.t28.SetAt(i, v); }
  void SetAt(size_t i, const T29& v) { values_.t29.SetAt(i, v); }
  void SetAt(size_t i, const T30& v) { values_.t30.SetAt(i, v); }
  void SetAt(size_t i, const T31& v) { values_.t31.SetAt(i, v); }
  void SetAt(size_t i, const T32& v) { values_.t32.SetAt(i, v); }
  void SetAt(size_t i, const T33& v) { values_.t33.SetAt(i, v); }
  void SetAt(size_t i, const T34& v) { values_.t34.SetAt(i, v); }
  void SetAt(size_t i, const T35& v) { values_.t35.SetAt(i, v); }
  void SetAt(size_t i, const T36& v) { values_.t36.SetAt(i, v); }
  void SetAt(size_t i, const T37& v) { values_.t37.SetAt(i, v); }
  void SetAt(size_t i, const T38& v) { values_.t38.SetAt(i, v); }
  void SetAt(size_t i, const T39& v) { values_.t39.SetAt(i, v); }
  void SetAt(size_t i, const T40& v) { values_.t40.SetAt(i, v); }

  // The Tag indicates what type is actually stored here.
  int tag_;
  // The count is 0 for scalar values and positive for arrays.
  size_t count_;
  base::AllocatorPtr alloc_;

  // This stores all possible variants.
  union {
    // Use ManualConstructor<> so that we can explicitly control the
    // construction/destruction.
    internal_variant_utils::ManualConstructor<T1> t1;
    internal_variant_utils::ManualConstructor<T2> t2;
    internal_variant_utils::ManualConstructor<T3> t3;
    internal_variant_utils::ManualConstructor<T4> t4;
    internal_variant_utils::ManualConstructor<T5> t5;
    internal_variant_utils::ManualConstructor<T6> t6;
    internal_variant_utils::ManualConstructor<T7> t7;
    internal_variant_utils::ManualConstructor<T8> t8;
    internal_variant_utils::ManualConstructor<T9> t9;
    internal_variant_utils::ManualConstructor<T10> t10;
    internal_variant_utils::ManualConstructor<T11> t11;
    internal_variant_utils::ManualConstructor<T12> t12;
    internal_variant_utils::ManualConstructor<T13> t13;
    internal_variant_utils::ManualConstructor<T14> t14;
    internal_variant_utils::ManualConstructor<T15> t15;
    internal_variant_utils::ManualConstructor<T16> t16;
    internal_variant_utils::ManualConstructor<T17> t17;
    internal_variant_utils::ManualConstructor<T18> t18;
    internal_variant_utils::ManualConstructor<T19> t19;
    internal_variant_utils::ManualConstructor<T20> t20;
    internal_variant_utils::ManualConstructor<T21> t21;
    internal_variant_utils::ManualConstructor<T22> t22;
    internal_variant_utils::ManualConstructor<T23> t23;
    internal_variant_utils::ManualConstructor<T24> t24;
    internal_variant_utils::ManualConstructor<T25> t25;
    internal_variant_utils::ManualConstructor<T26> t26;
    internal_variant_utils::ManualConstructor<T27> t27;
    internal_variant_utils::ManualConstructor<T28> t28;
    internal_variant_utils::ManualConstructor<T29> t29;
    internal_variant_utils::ManualConstructor<T30> t30;
    internal_variant_utils::ManualConstructor<T31> t31;
    internal_variant_utils::ManualConstructor<T32> t32;
    internal_variant_utils::ManualConstructor<T33> t33;
    internal_variant_utils::ManualConstructor<T34> t34;
    internal_variant_utils::ManualConstructor<T35> t35;
    internal_variant_utils::ManualConstructor<T36> t36;
    internal_variant_utils::ManualConstructor<T37> t37;
    internal_variant_utils::ManualConstructor<T38> t38;
    internal_variant_utils::ManualConstructor<T39> t39;
    internal_variant_utils::ManualConstructor<T40> t40;
  } values_;
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_VARIANT_H_
