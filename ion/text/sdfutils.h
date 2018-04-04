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

#ifndef ION_TEXT_SDFUTILS_H_
#define ION_TEXT_SDFUTILS_H_

//
// This file contains functions dealing with creation and modification of
// signed distance field (SDF) grids.
//

#include "ion/base/array2.h"

namespace ion {
namespace text {

// Creates a signed distance field (SDF) grid from a grid representing an
// antialiased image (such as a font glyph). The values in the input grid are
// assumed to be in the range [0,1]. This returns a grid in which each element
// represents the signed distance from that element to the nearest pixel forming
// an edge of the image. (Edges are inferred by the algorithm from the
// antialiased pixel values.) The padding parameter specifies how many pixels
// are added to the left, right, top, and bottom of the original image so that
// the distance field can taper off correctly.  Output elements are positive
// outside the foreground of the input image and negative inside it.  Output
// elements have grid-distance as their units, so are bounded in absolute value
// by sqrt(height^2+width^2) (after padding).
const base::Array2<double> ComputeSdfGrid(
    const base::Array2<double>& image_grid, size_t padding);

}  // namespace text
}  // namespace ion

#endif  // ION_TEXT_SDFUTILS_H_
