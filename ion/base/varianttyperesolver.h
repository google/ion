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

#ifndef ION_BASE_VARIANTTYPERESOLVER_H_
#define ION_BASE_VARIANTTYPERESOLVER_H_

#include "ion/base/type_structs.h"
#include "ion/base/variant.h"

namespace ion {
namespace base {

namespace internal {

//-----------------------------------------------------------------------------
//
// Helper structs.
//
//-----------------------------------------------------------------------------

// This struct contains a Type typedef that is T iff T is the same as Base or
// is derived from it and void otherwise.
template <typename T, typename Base>
struct IsSameOrDerivedFrom {
  typedef typename ConditionalType<IsSameType<Base, T>::value ||
                                   IsBaseOf<Base, T>::value,
                                   T, void>::Type Type;
};

// This struct is specialized for each of the defined types supported by a
// Variant. The Type defined in exactly one of these is used as the resolved
// type. If no specialized version matches (because the TypeToResolve is not
// compatible with any of the defined types), Type will be void.
template <typename VariantType, typename TypeToResolve, typename Base = void>
struct ResolverHelper {
  typedef void Type;
};

#define ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(VariantSubType)                \
template <typename VariantType, typename TypeToResolve>                       \
struct ResolverHelper<                                                        \
  VariantType, TypeToResolve,                                                 \
  typename IsSameOrDerivedFrom<TypeToResolve,                                 \
                               typename VariantType::VariantSubType>::Type> { \
  typedef typename VariantType::VariantSubType Type;                          \
}

ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type1);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type2);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type3);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type4);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type5);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type6);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type7);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type8);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type9);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type10);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type11);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type12);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type13);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type14);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type15);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type16);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type17);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type18);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type19);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type20);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type21);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type22);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type23);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type24);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type25);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type26);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type27);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type28);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type29);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type30);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type31);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type32);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type33);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type34);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type35);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type36);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type37);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type38);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type39);
ION_SPECIALIZE_VARIANT_RESOLVER_HELPER(Type40);

#undef ION_SPECIALIZE_VARIANT_RESOLVER_HELPER

}  // namespace internal

//-----------------------------------------------------------------------------
//
// The VariantTypeResolver struct allows users of the Variant class to
// determine which type defined by a particular Variant can be used to store a
// value of a particular type. For a defined Variant type T,
// VariantTypeResolver<VariantType, T>::Type will be T if T is one of the
// defined types or is a type derived from one of the defined types. Note that
// other type conversions are not allowed.
//
// For example, suppose you have:
//    typedef Variant<int, double, BaseClass> MyVariant;
//
// Then:
//    VariantTypeResolver<MyVariant, int>::Type
//        will be int because int is one of the defined types.
//    VariantTypeResolver<MyVariant, double>::Type
//        will be double for the same reason.
//    VariantTypeResolver<MyVariant, float>::Type
//        will be void, because float is not one of the defined types (and no
//        conversions are allowed).
//    VariantTypeResolver<MyVariant, BaseClass>::Type
//        will be BaseClass.
//    VariantTypeResolver<MyVariant, DerivedClass>::Type
//        will be BaseClass if DerivedClass is derived from BaseClass.
//
//-----------------------------------------------------------------------------

template <typename VariantType, typename TypeToResolve>
struct VariantTypeResolver {
  typedef typename internal::ResolverHelper<
    VariantType, TypeToResolve, TypeToResolve>::Type Type;
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_VARIANTTYPERESOLVER_H_
