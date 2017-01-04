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

#ifndef ION_GFX_TEXTUREMANAGER_H_
#define ION_GFX_TEXTUREMANAGER_H_

#include <set>
#include <vector>

#include "base/macros.h"
#include "ion/base/allocatable.h"
#include "ion/math/range.h"

namespace ion {
namespace gfx {

// TextureManager is an internal Ion class, and is not exported for public use.
//
// The TextureManager class associates pointers with texture unit IDs in an LRU
// fashion, but has a fixed number of units it can store. It attempts to
// preserve the mapping of data to units when possible, though that may not be
// possible if there are more requests for units than the TM contains. A TM must
// be initialized with a size of at least 2.
class TextureManager : public base::Allocatable {
 public:
  typedef const void* TextureType;

  // A TextureManager must be initialized with a size of at least 1.
  explicit TextureManager(int max_texture_units);
  ~TextureManager() override;

  // Returns the unit associated with the data pointer. current_unit contains
  // the current association, or -1 if there is not yet one. The returned unit
  // may not be the same as current_unit.
  int GetUnit(TextureType texture, int current_unit);

  // Returns the back unit.
  int GetBackIndex() const { return back_; }
  // Returns the front unit.
  int GetFrontIndex() const { return front_; }
  // Returns the data at index unit.
  TextureType GetTexture(int unit) const;

  // Sets the inclusive range of units that the TextureManager uses. If the
  // range is invalid then this does nothing but log an error. Units must
  // be non-negative, but if it extends beyond the number of units avaiable in
  // hardware, it is clamped to the appropriate range.
  void SetUnitRange(const math::Range1i& units);

 private:
  // Useful for testing an unit is at an end of the queue.
  static const int kEnd;

  struct Item {
    TextureType texture;
    int prev;
    int next;
  };

  // Makes an item the back of the queue.
  void Touch(int unit);

  // The queue of items.
  std::vector<Item> items_;
  // unit of the back item.
  int back_;
  // unit of the front item.
  int front_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(TextureManager);
};

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_TEXTUREMANAGER_H_
