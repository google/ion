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

#ifndef ION_BASE_STRINGTABLE_H_
#define ION_BASE_STRINGTABLE_H_

#include <memory>
#include <mutex>  // NOLINT(build/c++11)
#include <string>
#include <utility>
#include <vector>

#include "ion/base/referent.h"
#include "ion/base/weakreferent.h"

namespace ion {
namespace base {

// This class implements a mapping of strings to incrementing integer indexes.
// The indexes are suitable for indexing into a dense table, which can be
// retrieved using StringTable::GetTable().  This class is thread-safe, and to
// improve concurrent performance it also exposes a StringTable::View class
// which may be held per-thread to cache lookups of strings.
//
// Note that for efficiency, this class implements a hash table directly instead
// of delegating to an std::unordered_map<>; in multithreaded contention this
// implementation has been tested to be about 4x as fast as an externally locked
// unordered_map.  In particular:
//
// * When std::unordered_map<> is used with strings, std::string must be used as
//   the key type.  This forces map lookups to gratuitously construct an
//   std::string regardless of whether the key exists in the map or not, which
//   requires dynamic allocation and a string copy.
// * Using the same hash function for StringTable::View and StringTable means
//   the hash on a string can be computed once per lookup.
// * StringTable holds its internal node pointers in an std::vector<>, so the
//   hash table can be just another vector of indices into ther internal node
//   vector.
class StringTable : public base::Referent {
 private:
  // A struct which records one string entry in the StringTable.
  struct StringEntry;

 public:
  using StringIndex = uint32;
  static constexpr StringIndex kInvalidIndex = 0xFFFFFFFF;

  // This class implements a caching view on the StringTable.  If possible it
  // serves lookups out of its cache, and thus avoids a locking call to
  // StringTable::FindIndex()/GetString().  It is itself not thread-safe, and an
  // unique View instance should be held per-thread, or externally synchronized.
  class View : public base::WeakReferent {
   public:
    // Finds the index associated with a string, or inserts it if it does not
    // already exist.
    StringTable::StringIndex FindIndex(const char* string);
    StringTable::StringIndex FindIndex(const char* string, size_t len);

    // Gets the string associated with an index.
    std::string GetString(StringTable::StringIndex index) const;

   private:
    friend class StringTable;

    // Create a View on |string_table|.  |cache_size| is the size of the
    // internal cache, and will be rounded up to the next power of 2.
    View(const base::SharedPtr<StringTable>& string_table, size_t cache_size);

    // Finds the StringEntry for a given |string|, or inserts it if it does not
    // already exist.  |hash| is the computed hashed value of the string.
    const StringTable::StringEntry* FindEntry(const char* string, size_t hash,
                                              size_t len);

    std::vector<const StringTable::StringEntry*> hash_array_;
    const base::SharedPtr<StringTable> string_table_;
  };

  using ViewPtr = base::SharedPtr<View>;

  StringTable();
  ~StringTable() override;

  // The StringTable cannot be copied or moved, as this leads to data race
  // conditions with Views open on the StringTable from other threads.
  StringTable(const StringTable& other) = delete;
  StringTable(StringTable&& other) = delete;
  StringTable& operator=(StringTable other) = delete;

  // Create a View on this StringTable.  |cache_size| is the size of the View's
  // cache; it will be rounded up to the nearest power of two.
  ViewPtr CreateView(size_t cache_size);

  // Get the number of strings in this StringTable.
  size_t GetSize() const;

  // Finds the index associated with a string, or insert it if it does not
  // already exist.
  StringIndex FindIndex(const char* string);

  // Similar to FindIndex() above, with a specified length.  Every character in
  // |string| up to |len| will be hashed, including any '\0' characters.
  StringIndex FindIndex(const char* string, size_t len);

  // Get the string associated with an index.
  std::string GetString(StringIndex index) const;

  // Gets the strings in this StringTable as a linear table.  StringIndexes can
  // be used to index into this table.
  std::vector<std::string> GetTable() const;

 private:
  // Finds the StringEntry for a given |string|, or inserts it if it does not
  // already exist.  |hash| is the computed hashed value of the string.
  const StringEntry* FindEntry(const char* string, size_t hash, size_t len);

  // Rehash the hash table for the given new size, if necessary.
  void MaybeRehash(size_t new_size);

  // The mapping of indexes to StringEntry instances.
  std::vector<std::unique_ptr<StringEntry>> index_map_;

  // The hash array for mapping strings to StringEntry instances.
  std::vector<StringIndex> hash_array_;

  // Mutex which protects the state of string entries.
  mutable std::mutex string_entry_mutex_;
};

using StringTablePtr = base::SharedPtr<StringTable>;

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_STRINGTABLE_H_
