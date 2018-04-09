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

#include "ion/base/stringtable.h"

#include <cstring>

#include "ion/base/lockguards.h"
#include "ion/base/logging.h"

namespace ion {
namespace base {
namespace {

constexpr size_t kInitialSize = 32;

// Hash a '\0'-terminated string and perform a strlen() at the same time.
size_t HashString(const char* string, size_t* len) {
  // FNV-1a hash.  Simple and fast.
  uint32 hash = 0x811C9DC5;
  const char* c = string;
  while (*c != '\0') {
    hash = hash ^ *c;
    hash = hash * 0x1000193;
    ++c;
  }
  *len = c - string;
  return static_cast<size_t>(hash);
}

// Hash a string with a specified string length.
size_t HashString(const char* string, size_t len) {
  // FNV-1a hash.  Simple and fast.
  uint32 hash = 0x811C9DC5;
  for (size_t i = 0; i < len; ++i) {
    hash = hash ^ string[i];
    hash = hash * 0x1000193;
  }
  return static_cast<size_t>(hash);
}

}  // namespace

struct StringTable::StringEntry {
  // The stored string.
  std::unique_ptr<const char[]> string;

  // This string's StringIndex.
  StringIndex index;

  // The next StringEntry in the hashmap, as indexed by |hash_array_|.
  StringIndex hash_next;
};

StringTable::View::View(const StringTablePtr& string_table, size_t cache_size)
    : string_table_(string_table) {
  // Round up |cache_size| to the next power of 2.
  size_t shifted_cache_size = cache_size - 1;
  size_t rounded_cache_size = 1;
  while (shifted_cache_size != 0) {
    rounded_cache_size <<= static_cast<size_t>(1);
    shifted_cache_size >>= static_cast<size_t>(1);
  }
  hash_array_.resize(rounded_cache_size, nullptr);
}

StringTable::StringIndex StringTable::View::FindIndex(const char* string) {
  size_t len = 0;
  const size_t hash = HashString(string, &len);
  return FindEntry(string, hash, len)->index;
}

StringTable::StringIndex StringTable::View::FindIndex(const char* string,
                                                      size_t len) {
  const size_t hash = HashString(string, len);
  return FindEntry(string, hash, len)->index;
}

std::string StringTable::View::GetString(StringTable::StringIndex index) const {
  return string_table_->GetString(index);
}

const StringTable::StringEntry* StringTable::View::FindEntry(const char* string,
                                                             size_t hash,
                                                             size_t len) {
  // Lookup the string in the cache first.
  const size_t hash_index = hash & (hash_array_.size() - 1);
  auto entry = hash_array_[hash_index];
  if (entry != nullptr && std::strcmp(entry->string.get(), string) == 0) {
    return entry;
  }

  // Delegate to the StringTable and update the cache.
  entry = string_table_->FindEntry(string, hash, len);
  hash_array_[hash_index] = entry;
  return entry;
}

constexpr StringTable::StringIndex StringTable::kInvalidIndex;

StringTable::StringTable() : hash_array_(kInitialSize, kInvalidIndex) {}

StringTable::~StringTable() = default;

StringTable::ViewPtr StringTable::CreateView(size_t cache_size) {
  ViewPtr view(new View(StringTablePtr(this), cache_size));
  return view;
}

size_t StringTable::GetSize() const {
  std::lock_guard<std::mutex> lock(string_entry_mutex_);
  return index_map_.size();
}

StringTable::StringIndex StringTable::FindIndex(const char* string) {
  size_t len = 0;
  const size_t hash = HashString(string, &len);
  return FindEntry(string, hash, len)->index;
}

StringTable::StringIndex StringTable::FindIndex(const char* string,
                                                size_t len) {
  return FindEntry(string, HashString(string, len), len)->index;
}

std::string StringTable::GetString(StringIndex index) const {
  std::lock_guard<std::mutex> lock(string_entry_mutex_);
  return index_map_[index]->string.get();
}

std::vector<std::string> StringTable::GetTable() const {
  std::lock_guard<std::mutex> lock(string_entry_mutex_);
  std::vector<std::string> strings;
  strings.reserve(index_map_.size());
  for (const auto& item : index_map_) {
    strings.emplace_back(item->string.get());
  }
  return strings;
}

const StringTable::StringEntry* StringTable::FindEntry(const char* string,
                                                       size_t hash,
                                                       size_t len) {
  std::lock_guard<std::mutex> lock(string_entry_mutex_);

  // First search for the string.
  const size_t hash_index = hash & (hash_array_.size() - 1);
  StringIndex current = hash_array_[hash_index];
  while (current != kInvalidIndex) {
    const StringEntry* entry = index_map_[current].get();
    if (std::strcmp(entry->string.get(), string) == 0) {
      return entry;
    }
    current = entry->hash_next;
  }

  // The string is not found; insert it into |index_map_| and |hash_array_|.
  MaybeRehash(index_map_.size() + 1);
  const size_t rehash_index = hash & (hash_array_.size() - 1);

  const StringIndex index = static_cast<StringIndex>(index_map_.size());
  std::unique_ptr<char[]> buffer(new char[len + 1]);
  std::memcpy(buffer.get(), string, len);
  buffer[len] = '\0';
  index_map_.emplace_back(
      new StringEntry{std::move(buffer), index, hash_array_[rehash_index]});
  hash_array_[rehash_index] = index;
  return index_map_.back().get();
}

void StringTable::MaybeRehash(size_t new_size) {
  // Maximum load factor: 0.75.
  if (new_size < (hash_array_.size() * 3) / 4) {
    return;
  }

  // Rehash all items in |index_map_|.
  hash_array_ = std::vector<StringIndex>(hash_array_.size() * 2, kInvalidIndex);
  for (auto& item : index_map_) {
    size_t len = 0;
    const size_t hash = HashString(item->string.get(), &len);
    const size_t hash_index = hash & (hash_array_.size() - 1);
    item->hash_next = hash_array_[hash_index];
    hash_array_[hash_index] = item->index;
  }
}

}  // namespace base
}  // namespace ion
