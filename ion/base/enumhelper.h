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

#ifndef ION_BASE_ENUMHELPER_H_
#define ION_BASE_ENUMHELPER_H_

#include "base/integral_types.h"
#include "ion/base/indexmap.h"

namespace ion {
namespace base {

// EnumHelper is an internal static class that provides conversions between
// enumerated types, uint32 values, and string representations of each enum. It
// allows all of the enum-related information to be in one place, making
// maintenance easier, and simplifying conversions. For example, consider the
// following example:
//
// enum Values {
//   kValue1,
//   kValue2,
//   kValue3
// };
//
// template <>
// const EnumHelper::EnumData<Values> EnumHelper::GetEnumData() {
//   static const uint32 kValues[] = {
//     1U, 2U, 3U
//   };
//   static const char* kStrings[] = {
//     "Value1", "Value2", "Value3"
//   };
//   return EnumData<Values>(IndexMap<Values, uint32>(kValues, 3), kStrings);
// }
//
// Calling EnumHelper::GetString(kValue1) would return "Value1," while
// EnumHelper::GetConstant(kValue2) would return 2U, and
// EnumHelper::GetEnum<Values>(3U) would return kValue3.
//
// Note that the specialization of GetEnumData() must be in the ion::base
// namespace.
class EnumHelper {
 public:
  // Returns a string corresponding to an enum.
  template <typename EnumType> static const char* GetString(EnumType e) {
    const EnumData<EnumType> data = GetEnumData<EnumType>();
    return e >= 0 && static_cast<size_t>(e) < data.index_map.GetCount() ?
        data.strings[e] : "<INVALID>";
  }

  // Creates and returns an IndexMap instance that can be used to convert
  // between enums of the given type and constants. This function must be
  // specialized for each enum type; each specialization contains a static
  // array of constants in the proper order.
  template <typename EnumType>
  static const IndexMap<EnumType, uint32> GetIndexMap() {
    return GetEnumData<EnumType>().index_map;
  }

  // Returns the number of values corresponding to an enum.
  template <typename EnumType> static size_t GetCount() {
    return GetIndexMap<EnumType>().GetCount();
  }

  // Returns the constant value corresponding to an enum.
  template <typename EnumType> static uint32 GetConstant(EnumType e) {
    return GetIndexMap<EnumType>().GetUnorderedIndex(e);
  }

  // Returns the enum corresponding to a constant value.
  template <typename EnumType> static EnumType GetEnum(uint32 c) {
    return GetIndexMap<EnumType>().GetOrderedIndex(c);
  }

 private:
  // This struct encapsulates the IndexMap and string array for an enum,
  // allowing the full enum definition to be localized.
  template <typename EnumType>
  struct EnumData {
    // The constructor is passed the IndexMap and string array. It assumes the
    // lifetime of the string array is at least as long as that of the EnumData
    // instance.
    EnumData(const IndexMap<EnumType, uint32>& index_map_in,
             const char* strings_in[])
        : index_map(index_map_in), strings(strings_in) {}
    const IndexMap<EnumType, uint32> index_map;        // Enum value -> uint32.
    const char** strings;                              // Enum value -> string.
  };

  // Returns the EnumData for an enum. This must be specialized for each
  // supported enum type.
  template <typename EnumType> static const EnumData<EnumType> GetEnumData();
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_ENUMHELPER_H_
