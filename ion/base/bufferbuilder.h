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

#ifndef ION_BASE_BUFFERBUILDER_H_
#define ION_BASE_BUFFERBUILDER_H_

#include <cstring>
#include <memory>
#include <string>
#include <type_traits>

#include "absl/memory/memory.h"

namespace ion {
namespace base {

// This class incrementally builds a byte buffer, returning it in the form of an
// std::string when Build() is called.
class BufferBuilder {
 public:
  BufferBuilder();
  BufferBuilder(const BufferBuilder& other);
  BufferBuilder(BufferBuilder&& other);
  ~BufferBuilder();

  BufferBuilder& operator=(BufferBuilder other);
  void swap(BufferBuilder& other);
  friend void swap(BufferBuilder& lhs, BufferBuilder& rhs) { lhs.swap(rhs); }

  // Append a POD type T.
  template <typename T>
  typename std::enable_if<std::is_pod<T>::value && !std::is_pointer<T>::value,
                          void>::type
  Append(const T& t) {
    if (buffers_ == nullptr) {
      buffers_ = absl::make_unique<Buffer>();
      buffers_tail_ = buffers_.get();
    } else if (buffers_tail_->header.filled_size + sizeof(T) >
               Buffer::kBufferSize) {
      buffers_tail_->header.next = absl::make_unique<Buffer>();
      buffers_tail_ = buffers_tail_->header.next.get();
    }
    std::memcpy(buffers_tail_->buffer + buffers_tail_->header.filled_size,
                reinterpret_cast<const char*>(&t), sizeof(T));
    buffers_tail_->header.filled_size += sizeof(T);
  }

  // Append another BufferBuilder.
  void Append(BufferBuilder other);

  // Append an array of |count| instances of type T.
  template <typename T>
  void AppendArray(const T* t, size_t count) {
    return AppendBytes(reinterpret_cast<const char*>(t), sizeof(T) * count);
  }

  // Return the total size of the buffer so far.
  size_t Size() const;

  // Return the buffer built so far, as a string.
  std::string Build() const;

 private:
  // BufferBuilder is meant to be used on performance-critical paths, so we
  // use a Buffer struct with an intrusive linked-list, rather than an
  // std::list.
  struct Buffer {
    // Header structure for Buffer instances.
    struct Header {
      Header() : filled_size(0) {}
      std::unique_ptr<Buffer> next;
      size_t filled_size;
    };

    // To maintain the size of Buffer exactly as requested, we make the |buffer|
    // char array exactly the right size to fill the rest of the requested size,
    // after the Header structure is included.
    static constexpr size_t kBufferSize = 4096 - sizeof(Header);

    Header header;
    char buffer[kBufferSize];
  };

  void AppendBytes(const char* bytes, size_t size);

  std::unique_ptr<Buffer> buffers_;
  Buffer* buffers_tail_;
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_BUFFERBUILDER_H_
