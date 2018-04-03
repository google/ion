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

#ifndef ION_IMAGE_EXPORTJPEG_H_
#define ION_IMAGE_EXPORTJPEG_H_

#include <vector>

#include "base/integral_types.h"
#include "ion/gfx/image.h"

namespace ion {
namespace image {

// Converts an existing Image to data in JPEG format using the provided
// |quality| setting, returning a vector.  If |flip_vertically| is true, the
// resulting image is inverted in the Y dimension. The vector will be empty if
// the conversion is not possible for any reason.
ION_API const std::vector<uint8> ConvertToJpeg(
    const gfx::ImagePtr& image, bool flip_vertically, int quality);

}  // namespace image
}  // namespace ion

#endif  // ION_IMAGE_EXPORTJPEG_H_
