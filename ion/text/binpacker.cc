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

#include "ion/text/binpacker.h"

#include <algorithm>
#include <limits>

#include "base/integral_types.h"
#include "ion/base/logging.h"
#include "absl/memory/memory.h"

namespace ion {
namespace text {

using math::Vector2ui;

//-----------------------------------------------------------------------------
//
// The BinPacker::Skyline is an internal helper class that implements
// the Skyline Bottom-Left bin-packing algorithm.
//
//-----------------------------------------------------------------------------

class BinPacker::Skyline {
 public:
  // For convenience.
  typedef BinPacker::Rectangle Rectangle;

  explicit Skyline(const Vector2ui& bin_size);
  Skyline(const Skyline& from)  // Copy constructor.
      : bin_size_(from.bin_size_), levels_(from.levels_) {}

  const Vector2ui& GetBinSize() const { return bin_size_; }

  // Inserts a single rectangle into the bin, setting its position. Returns
  // false if the rectangle does not fit.
  bool Insert(Rectangle* rect);

 private:
  // A Level represents one level (horizontal line segment) of the skyline.
  struct Level {
    Level() : x(0), y(0), width(0) {}
    Level(int x_in, int y_in, int width_in)
        : x(x_in), y(y_in), width(width_in) {}

    int32 x;       // Leftmost X position.
    int32 y;       // Y coordinate of the Skyline level.
    int32 width;   // Skyline width.
  };

  // Returns the index of the best skyline level in which a rectangle of the
  // given size would fit, or base::kInvalidIndex if it can't fit at all. If it
  // fits, it sets rect->bottom_left to the correct position for the rectangle.
  bool FindLevel(Rectangle* rect, size_t* level_index) const;

  // Adds a level to the skyline starting at the given index.
  // 
  // improve comments here and within functions.
  void AddLevel(size_t index, const Rectangle& rect);

  // If a rectangle of the given size fits in the indexed level, this adds it
  // to all levels it intersects and sets y to the top of topmost level it
  // intersected. Returns false if the rectangle did not fit.
  bool RectangleFits(size_t level_index, const Vector2ui& size,
                     int32* y) const;

  // Merges all skyline levels that are at the same height.
  void MergeLevels();

  const Vector2ui bin_size_;
  std::vector<Level> levels_;
};

BinPacker::Skyline::Skyline(const Vector2ui& bin_size)
    : bin_size_(bin_size) {
  // Insert a Level covering the full width of the bin.
  levels_.push_back(Level(0, 0, bin_size[0]));
}

bool BinPacker::Skyline::Insert(Rectangle* rect) {
  size_t level_index;
  if (FindLevel(rect, &level_index)) {
    AddLevel(level_index, *rect);
    return true;
  }
  return false;
}

bool BinPacker::Skyline::FindLevel(Rectangle* rect, size_t* level_index) const {
  size_t best_index = base::kInvalidIndex;
  int32 best_width = std::numeric_limits<int32>::max();
  int32 best_height = std::numeric_limits<int32>::max();

  // Look at each level that the rectangle can fit into, and choose the one
  // with the best fit.
  const size_t num_levels = levels_.size();
  for (size_t i = 0; i < num_levels; ++i) {
    int32 y;
    if (RectangleFits(i, rect->size, &y)) {
      const Level& level = levels_[i];
      const int32 height = y + static_cast<int32>(rect->size[1]);
      if (height < best_height ||
          (height == best_height && level.width < best_width)) {
        best_height = height;
        best_index = i;
        best_width = level.width;
        rect->bottom_left.Set(level.x, y);
      }
    }
  }
  if (best_index == base::kInvalidIndex) {
    return false;
  } else {
    *level_index = best_index;
    return true;
  }
}

void BinPacker::Skyline::AddLevel(size_t index, const Rectangle& rect) {
  const Level level(rect.bottom_left[0],
                    rect.bottom_left[1] + rect.size[1],
                    rect.size[0]);
  levels_.insert(levels_.begin() + index, level);

  DCHECK_LE(level.x + level.width, static_cast<int32>(bin_size_[0]));
  DCHECK_LE(level.y, static_cast<int32>(bin_size_[1]));

  for (size_t i = index + 1; i < levels_.size(); ++i) {
    const Level& prev_level = levels_[i - 1];
    Level& cur_level = levels_[i];
    DCHECK_LE(prev_level.x, cur_level.x);
    const int x_diff = prev_level.x + prev_level.width - cur_level.x;
    if (x_diff <= 0)
      break;
    cur_level.x += x_diff;
    cur_level.width -= x_diff;
    if (cur_level.width > 0)
      break;
    levels_.erase(levels_.begin() + i);
    --i;
  }
  MergeLevels();
}

bool BinPacker::Skyline::RectangleFits(
    size_t level_index, const Vector2ui& size, int32* y) const {
  const int x = levels_[level_index].x;
  if (x + size[0] > bin_size_[0])
    return false;

  int width_remaining = size[0];
  size_t i = level_index;
  *y = levels_[level_index].y;
  while (width_remaining > 0) {
    DCHECK_LT(i, levels_.size());
    const Level& level = levels_[i];
    *y = std::max(*y, level.y);
    if (*y + size[1] > bin_size_[1])
      return false;
    width_remaining -= level.width;
    ++i;
  }
  return true;
}

void BinPacker::Skyline::MergeLevels() {
  for (size_t i = 1; i < levels_.size(); ++i) {
    if (levels_[i - 1].y == levels_[i].y) {
      levels_[i - 1].width += levels_[i].width;
      levels_.erase(levels_.begin() + i);
      --i;
    }
  }
}

//-----------------------------------------------------------------------------
//
// BinPacker functions.
//
//-----------------------------------------------------------------------------

BinPacker::BinPacker() : num_rectangles_packed_(0) {}

BinPacker::BinPacker(const BinPacker& from) { *this = from; }

BinPacker::~BinPacker() {}

BinPacker& BinPacker::operator=(const BinPacker& from) {
  rectangles_ = from.rectangles_;
  if (from.skyline_) skyline_ = absl::make_unique<Skyline>(*from.skyline_);
  num_rectangles_packed_ = from.num_rectangles_packed_;
  return *this;
}

bool BinPacker::Pack(const Vector2ui& bin_size) {
  if (!skyline_ || skyline_->GetBinSize() != bin_size) {
    skyline_ = absl::make_unique<Skyline>(bin_size);
    num_rectangles_packed_ = 0;
  }

  // The Skyline operates on one rectangle at a time, so this is a simple loop
  // that sets the position of each new rectangle as it is inserted.
  for (size_t i = num_rectangles_packed_; i < rectangles_.size(); ++i) {
    // Stop if the rectangle can't be inserted.
    if (!skyline_->Insert(&rectangles_[i]))
      break;
    ++num_rectangles_packed_;
  }
  return num_rectangles_packed_ == rectangles_.size();
}

}  // namespace text
}  // namespace ion
