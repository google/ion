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

#include "ion/gfx/texturemanager.h"

#include <algorithm>
#include <cstring>

#include "ion/base/logging.h"

namespace ion {
namespace gfx {

const int TextureManager::kEnd = -1;

// TextureManager implements an LRU queue using an linked-list implemented with
// an array of fixed size (no pointers).
//
// Each item has a link to its previous and next item, though the front item's
// prev and the back item's next point to invalid indices (kEnd).
// For example, a queue of size 4 stars out looking like:
//   front_ = 0
//   back_ = 3
//   items[0].prev = kEnd
//   items[0].next = 1
//   items[1].prev = 0
//   items[1].next = 2
//   items[2].prev = 1
//   items[2].next = 3
//   items[3].prev = 2
//   items[3].next = kEnd
// and the order of items is: 0, 1, 2, 3.
// and all of the Texture pointers are NULL. When an item is Touch()ed, that
// becomes the back of the list, and the pointers update accordingly. Suppose
// that item 1 is Touch()ed, then the list becomes:
//   front_ = 0
//   back_ = 1
//   items[0].prev = kEnd
//   items[0].next = 2
//   items[1].prev = 3
//   items[1].next = kEnd
//   items[2].prev = 1
//   items[2].next = 3
//   items[3].prev = 2
//   items[3].next = 1
// and the order of items is now 0, 2, 3, 1. Note that since 1 is now the back
// of the queue, 3's next now points to 1.
TextureManager::TextureManager(int max_texture_units)
    : items_(max_texture_units), back_(max_texture_units - 1), front_(0) {
  DCHECK_EQ(items_.size(), static_cast<size_t>(max_texture_units));
  DCHECK_GE(max_texture_units, 1) << "TextureManager was initialized with < 1 "
                                     "texture units. This could mean that "
                                     "there is no valid GL context bound.";
  SetUnitRange(math::Range1i(0, max_texture_units - 1));
}

TextureManager::~TextureManager() {}

TextureManager::TextureType TextureManager::GetTexture(int unit) const {
  DCHECK_GE(unit, 0);
  DCHECK_LT(unit, static_cast<int>(items_.size()));
  return items_[unit].texture;
}

int TextureManager::GetUnit(TextureType texture, int current_unit) {
  DCHECK_LT(current_unit, static_cast<int>(items_.size()));
  if (current_unit != kEnd && items_[current_unit].texture == texture) {
    // Touch the current unit (which moves it to the back).
    Touch(current_unit);
  } else {
    // Get a new unit.
    Touch(front_);  // This changes front_ to back_.
  }
  // Update the pointer.
  items_[back_].texture = texture;
  // Return the back index.
  return back_;
}

// Makes an item the back of the queue.
void TextureManager::Touch(int unit) {
  DCHECK_LT(unit, static_cast<int>(items_.size()));
  if (unit == back_)
    return;

  Item& this_item = items_[unit];
  // Remove this_item from linked list.
  if (this_item.prev != kEnd) {
    // The prev item gets this's next.
    Item& prev_item = items_[this_item.prev];
    prev_item.next = this_item.next;
  } else {
    front_ = this_item.next;
  }
  if (this_item.next != kEnd) {
    // The next item gets this's prev.
    Item& next_item = items_[this_item.next];
    next_item.prev = this_item.prev;
  }
  // Note that no else clause needed here because if (unit == back_) return;
  // catches this situation early.

  // Link this_item onto back of linked list.
  Item& back_item = items_[back_];
  back_item.next = unit;
  this_item.prev = back_;
  this_item.next = kEnd;
  back_ = unit;
}

void TextureManager::SetUnitRange(const math::Range1i& units) {
  if (units.GetMinPoint() < 0) {
    LOG(ERROR) << "The minimum unit for TextureManager to use must be >= 0.";
    return;
  }

  front_ = std::min(static_cast<int>(units.GetMinPoint()),
                    static_cast<int>(items_.size() - 1U));
  back_ = std::min(static_cast<int>(units.GetMaxPoint()),
                   static_cast<int>(items_.size() - 1U));

  // Reset everything.
  memset(&items_[0], 0, sizeof(Item) * items_.size());

  // Initialize the links and data pointers.
  items_[front_].prev = kEnd;
  items_[front_].texture = nullptr;
  items_[front_].next = (front_ < back_) ? front_ + 1 : kEnd;
  items_[back_].prev = (front_ < back_) ? back_ - 1 : kEnd;
  items_[back_].texture = nullptr;
  items_[back_].next = kEnd;
  for (int i = front_ + 1; i < back_; ++i) {
    items_[i].prev = i - 1;
    items_[i].texture = nullptr;
    items_[i].next = i + 1;
  }
}

}  // namespace gfx
}  // namespace ion
