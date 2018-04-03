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

#ifndef ION_TEXT_BINPACKER_H_
#define ION_TEXT_BINPACKER_H_

#include <memory>
#include <vector>

#include "ion/base/invalid.h"
#include "ion/math/vector.h"

namespace ion {
namespace text {

// This class implements generic 2D bin-packing using a modified version of the
// Skyline Bottom-Left algorithm available at:
//     http://clb.demon.fi/files/RectangleBinPack
//
// Modifications include:
//  - Not allowing rotations of rectangles.
class BinPacker {
 public:
  // Structure representing a rectangle to pack into the bin.
  struct Rectangle {
    Rectangle() = delete;

    // Constructor that takes an ID and size.
    Rectangle(uint64 id_in, const math::Vector2ui& size_in)
        : id(id_in), size(size_in), bottom_left(math::Point2ui::Zero()) {}

    uint64 id;                   // Input client identifier for rectangle.
    math::Vector2ui size;        // Input 2D size.
    math::Point2ui bottom_left;  // Output position of rectangle.
  };

  BinPacker();
  ~BinPacker();

  // Allow BinPacker to be copied.
  BinPacker(const BinPacker& from);
  BinPacker& operator=(const BinPacker& from);

  // Adds a rectangle of the given size to pack into the bin. It is up to the
  // client to manage IDs responsibly; duplicates are not detected here.
  void AddRectangle(uint64 id, const math::Vector2ui& size) {
    rectangles_.push_back(Rectangle(id, size));
  }

  // Tries to pack all of the rectangles into a bin of the given size. Returns
  // true if they were able to fit. This can be called incrementally; only the
  // rectangles added since the last call to Pack() will be processed.
  bool Pack(const math::Vector2ui& bin_size);

  // Returns the vector of rectangles (including positions) resulting from the
  // last call to Pack(). If Pack() returned false, this vector will not be
  // useful.
  const std::vector<Rectangle>& GetRectangles() const {
    return rectangles_;
  }

 private:
  // Nested class used to do the hard work.
  class Skyline;

  std::vector<Rectangle> rectangles_;
  std::unique_ptr<Skyline> skyline_;
  // Number of rectangles already packed.
  size_t num_rectangles_packed_;
};

}  // namespace text
}  // namespace ion

#endif  // ION_TEXT_BINPACKER_H_
