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

#ifndef ION_TEXT_TESTS_MOCKFONTIMAGE_H_
#define ION_TEXT_TESTS_MOCKFONTIMAGE_H_

#include <vector>

#include "base/integral_types.h"
#include "ion/base/datacontainer.h"
#include "ion/gfx/image.h"
#include "ion/math/range.h"
#include "ion/math/vector.h"
#include "ion/text/font.h"
#include "ion/text/fontimage.h"

namespace ion {
namespace text {
namespace testing {

// MockFontImage is a version of StaticFontImage that bypasses normal FontImage
// creation and returns simple texture coordinates for testing purposes. It
// defines texture coordinates for all lower-case ASCII letters. The texture
// coordinates for the i'th letter range from i to i+1 in both s and t.  The
// image is a 64x64 RGB image with undefined contents.
class MockFontImage : public StaticFontImage {
 public:
  // The default constructor uses an empty Font.
  MockFontImage()
      : StaticFontImage(FontPtr(), kImageSize, BuildImageData(FontPtr())) {}
  // This constructor takes a Font instance.
  explicit MockFontImage(const FontPtr& font)
      : StaticFontImage(font, kImageSize, BuildImageData(font)) {}
  ~MockFontImage() override {}

 private:
  static const size_t kImageSize = 64U;

  // Helpers below are all static because they are called in the initializer for
  // the super-class, so can't really depend on anything being initialized yet.

  // Builds and returns an ImageData instance mocking the StaticFontImage.
  static const ImageData BuildImageData(const FontPtr& font) {
    ImageData image_data(base::AllocatorPtr(nullptr));
    image_data.texture->SetImage(0U, BuildImage());
    for (GlyphIndex i = 'a'; i < 'z'; ++i) {
      image_data.glyph_set.insert(i);
    }
    BuildCharRectangles(font, &image_data.texture_rectangle_map);
    return image_data;
  }

  // Builds and returns a square kImageSize RGB image. The contents of the
  // image do not really matter.
  static const gfx::ImagePtr BuildImage() {
    gfx::ImagePtr image(new gfx::Image());
    std::vector<uint8> data_buf(3 * kImageSize * kImageSize, 0U);
    image->Set(gfx::Image::kRgb888, kImageSize, kImageSize,
               base::DataContainer::CreateAndCopy<uint8>(
                   &data_buf[0], data_buf.size(), false,
                   image->GetAllocator()));
    return image;
  }

  // Builds and returns a vector of texture coordinate rectangles for all
  // lower-case ASCII characters.
  static void BuildCharRectangles(const FontPtr& font,
                                  ImageData::TexRectMap* rects) {
    if (!font.Get()) {
      return;
    }
    for (int i = 'a'; i < 'z'; ++i) {
      const float f0 = static_cast<float>(i);
      const float f1 = f0 + 1.f;
      (*rects)[font->GetDefaultGlyphForChar(i)] =
          math::Range2f(math::Point2f(f0, f0), math::Point2f(f1, f1));
    }
  }
};

}  // namespace testing
}  // namespace text
}  // namespace ion

#endif  // ION_TEXT_TESTS_MOCKFONTIMAGE_H_
