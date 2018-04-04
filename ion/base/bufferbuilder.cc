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

#include "ion/base/bufferbuilder.h"

#include <algorithm>

#include "absl/memory/memory.h"

namespace ion {
namespace base {

BufferBuilder::BufferBuilder() : buffers_(), buffers_tail_(nullptr) {
  static_assert((sizeof(Buffer) & (sizeof(Buffer) - 1)) == 0,
                "sizeof(Buffer) is not a power of 2");
}

BufferBuilder::BufferBuilder(const BufferBuilder& other) : BufferBuilder() {
  std::unique_ptr<Buffer>* buffer_ptr = &buffers_;
  for (const Buffer* buffer = other.buffers_.get(); buffer != nullptr;
       buffer = buffer->header.next.get()) {
    *buffer_ptr = absl::make_unique<Buffer>();
    (*buffer_ptr)->header.filled_size = buffer->header.filled_size;
    std::memcpy((*buffer_ptr)->buffer, buffer->buffer,
                buffer->header.filled_size);
    buffers_tail_ = (*buffer_ptr).get();
    buffer_ptr = &(*buffer_ptr)->header.next;
  }
}

BufferBuilder::BufferBuilder(BufferBuilder&& other) : BufferBuilder() {
  swap(other);
}

BufferBuilder::~BufferBuilder() {
  // Manually delete the linked-list buffers, one-by-one. This is to avoid stack
  // overflow when each unique_ptr destructor recursively destroys the buffer it
  // links to.
  Buffer* buffer = buffers_.release();
  while (buffer != nullptr) {
    Buffer* to_delete = buffer;
    buffer = buffer->header.next.release();
    delete to_delete;
  }
}

void BufferBuilder::swap(BufferBuilder& other) {
  using std::swap;
  swap(buffers_, other.buffers_);
  swap(buffers_tail_, other.buffers_tail_);
}

size_t BufferBuilder::Size() const {
  size_t size = 0;
  for (const Buffer* buffer = buffers_.get(); buffer != nullptr;
       buffer = buffer->header.next.get()) {
    size += buffer->header.filled_size;
  }
  return size;
}

std::string BufferBuilder::Build() const {
  std::string ret;
  ret.reserve(Size());
  size_t offset = 0;
  for (const Buffer* buffer = buffers_.get(); buffer != nullptr;
       buffer = buffer->header.next.get()) {
    ret.insert(offset, buffer->buffer, buffer->header.filled_size);
    offset += buffer->header.filled_size;
  }
  return ret;
}

void BufferBuilder::Append(BufferBuilder other) {
  // Note that |other| is taken by-value, so we can can append the list safely
  // below.
  buffers_tail_->header.next = std::move(other.buffers_);
  buffers_tail_ = other.buffers_tail_;
  other.buffers_tail_ = nullptr;
}

void BufferBuilder::AppendBytes(const char* bytes, size_t size) {
  size_t offset = 0;
  if (buffers_ == nullptr) {
    buffers_ = absl::make_unique<Buffer>();
    buffers_tail_ = buffers_.get();
  }
  while (offset < size) {
    if (buffers_tail_->header.filled_size == Buffer::kBufferSize) {
      buffers_tail_->header.next = absl::make_unique<Buffer>();
      buffers_tail_ = buffers_tail_->header.next.get();
    }

    using std::min;
    const size_t to_copy = min(
        size - offset, Buffer::kBufferSize - buffers_tail_->header.filled_size);
    std::memcpy(buffers_tail_->buffer + buffers_tail_->header.filled_size,
                bytes + offset, to_copy);
    buffers_tail_->header.filled_size += to_copy;
    offset += to_copy;
  }
}

}  // namespace base
}  // namespace ion
