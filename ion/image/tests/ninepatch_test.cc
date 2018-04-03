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

#include "ion/image/ninepatch.h"

#include <sstream>

#include "ion/base/allocator.h"
#include "ion/base/datacontainer.h"
#include "ion/base/invalid.h"
#include "ion/base/zipassetmanager.h"
#include "ion/base/zipassetmanagermacros.h"
#include "ion/image/conversionutils.h"
#include "ion/math/range.h"
#include "ion/math/vector.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

ION_REGISTER_ASSETS(Images);

namespace ion {
namespace image {

namespace {

using base::AllocatorPtr;
using gfx::Image;
using gfx::ImagePtr;
using math::Point2ui;
using math::Range1ui;
using math::Range2ui;
using math::Vector2ui;

// Some common colors.
static const uint32 kEmpty = 0x00000000;
static const uint32 kBlack = 0xff000000;
static const uint32 kRed = 0xff0000ff;
static const uint32 kGreen = 0xff00ff00;
static const uint32 kBlue = 0xffff0000;
static const uint32 kYellow = 0xff00ffff;
static const uint32 kPurple = 0xffff00ff;
static const uint32 kLtRed = 0xff0000cc;
static const uint32 kLtGreen = 0xff00cc00;
static const uint32 kLtBlue = 0xffcc0000;
static const uint32 kLtYellow = 0xff00cccc;
static const uint32 kLtPurple = 0xffcc00cc;
static const uint32 kMidRed = 0xff000099;
static const uint32 kMidGreen = 0xff009900;
static const uint32 kMidBlue = 0xff990000;
static const uint32 kMidYellow = 0xff009999;
static const uint32 kMidPurple = 0xff990099;
static const uint32 kDarkRed = 0xff000066;
static const uint32 kDarkBlue = 0xff006600;
static const uint32 kDarkGreen = 0xff660000;
static const uint32 kDarkYellow = 0xff006666;
static const uint32 kDarkPurple = 0xff660066;
static const uint32 kBlkRed = 0xff000033;
static const uint32 kBlkBlue = 0xff003300;
static const uint32 kBlkGreen = 0xff330000;
static const uint32 kBlkYellow = 0xff003333;
static const uint32 kBlkPurple = 0xff330033;

// Creates and returns an image with the passed dimensions.
static const ImagePtr CreateImage(uint32 width, uint32 height) {
  ImagePtr image(new Image);
  uint32* pixels = new uint32[width * height];
  image->Set(Image::kRgba8888, width, height,
             base::DataContainer::Create<uint32>(
                 pixels, base::DataContainer::ArrayDeleter<uint32>, true,
                 AllocatorPtr()));
  return image;
}

// Loads the image with the passed name from zipassets.
static const ImagePtr LoadAssetImage(const std::string& name) {
  const std::string& data = base::ZipAssetManager::GetFileData(name);
  CHECK(!base::IsInvalidReference(data));
  ImagePtr image = ConvertFromExternalImageData(
      &data[0], data.length(), false, false, AllocatorPtr());
  return image;
}

// Sets all pixels in the passed image to value.
static void FillImage(const ImagePtr& image, uint32 value) {
  uint32* pixels = image->GetData()->GetMutableData<uint32>();
  const uint32 length = image->GetWidth() * image->GetHeight();
  for (uint32 i = 0; i < length; ++i)
    pixels[i] = value;
}

// Returns the pixel value at [x,y] in image.
static uint32 GetPixel(const ImagePtr& image, uint32 x, uint32 y) {
  if (x < image->GetWidth() && y < image->GetHeight()) {
    const uint32* pixels = image->GetData()->GetData<uint32>();
    return pixels[y * image->GetWidth() + x];
  }
  return 0U;
}

// Sets the pixel at [x,y] in image to value.
static void SetPixel(const ImagePtr& image, uint32 x, uint32 y, uint32 value) {
  if (x < image->GetWidth() && y < image->GetHeight()) {
    uint32* pixels = image->GetData()->GetMutableData<uint32>();
    pixels[y * image->GetWidth() + x] = value;
  }
}

// Returns whether two uint32 colors are equal.
::testing::AssertionResult AssertColorsEqual(const char* color1_expr,
                                             const char* color2_expr,
                                             uint32 color1,
                                             uint32 color2) {
  if (color1 == color2)
    return ::testing::AssertionSuccess();

  std::ostringstream error_str;
  error_str << "  Actual: 0x" << std::hex << color2 << " (" << color2_expr
            << ")\nExpected: 0x" << color1 << " (" << color1_expr << ")";
  return ::testing::AssertionFailure(::testing::Message()
                                     << "Color mismatch at " << color2_expr
                                     << ":\n" << error_str.str());
}

}  // anonymous namespace

TEST(NinePatch, EmptySource) {
  ImagePtr image;
  NinePatchPtr empty_patch(new NinePatch(image));
  EXPECT_EQ(Vector2ui(0, 0), empty_patch->GetMinimumSize());
  EXPECT_EQ(Range2ui(Point2ui(0, 0), Point2ui(16, 16)),
            empty_patch->GetPaddingBox(16, 16));
  const ImagePtr pixmap = empty_patch->BuildImage(16, 16, AllocatorPtr());
  EXPECT_EQ(16U, pixmap->GetWidth());
  EXPECT_EQ(16U, pixmap->GetHeight());
}

TEST(NinePatch, ImagesWipeable) {
  ImagePtr image;
  NinePatchPtr empty_patch(new NinePatch(image));
  ImagePtr pixmap = empty_patch->BuildImage(16, 16, AllocatorPtr());
  EXPECT_TRUE(pixmap->GetData()->IsWipeable());
  empty_patch->SetBuildWipeable(false);
  pixmap = empty_patch->BuildImage(16, 16, AllocatorPtr());
  EXPECT_FALSE(pixmap->GetData()->IsWipeable());
  empty_patch->SetBuildWipeable(true);
  pixmap = empty_patch->BuildImage(16, 16, AllocatorPtr());
  EXPECT_TRUE(pixmap->GetData()->IsWipeable());
}

TEST(NinePatch, BadImage) {
  ImagePtr image(new Image);
  std::unique_ptr<uint32[]> pixels(new uint32[8 * 8]);

  // No data.
  image->Set(Image::kRgb888, 8, 8, base::DataContainerPtr());
  {
    NinePatchPtr empty_patch(new NinePatch(image));
    EXPECT_EQ(Vector2ui(0, 0), empty_patch->GetMinimumSize());
    EXPECT_EQ(Range2ui(Point2ui(0, 0), Point2ui(16, 16)),
              empty_patch->GetPaddingBox(16, 16));
    const ImagePtr pixmap = empty_patch->BuildImage(16, 16, AllocatorPtr());
    EXPECT_EQ(16U, pixmap->GetWidth());
    EXPECT_EQ(16U, pixmap->GetHeight());
  }

  // Bad format.
  image->Set(Image::kRgb888, 8, 8,
             base::DataContainer::Create<uint32>(pixels.get(), nullptr, false,
                                                 AllocatorPtr()));
  {
    NinePatchPtr empty_patch(new NinePatch(image));
    EXPECT_EQ(Vector2ui(0, 0), empty_patch->GetMinimumSize());
    EXPECT_EQ(Range2ui(Point2ui(0, 0), Point2ui(16, 16)),
              empty_patch->GetPaddingBox(16, 16));
    const ImagePtr pixmap = empty_patch->BuildImage(16, 16, AllocatorPtr());
    EXPECT_EQ(16U, pixmap->GetWidth());
    EXPECT_EQ(16U, pixmap->GetHeight());
  }

  // Zero width.
  image->Set(Image::kRgba8888, 0, 8,
             base::DataContainer::Create<uint32>(pixels.get(), nullptr, false,
                                                 AllocatorPtr()));
  {
    NinePatchPtr empty_patch(new NinePatch(image));
    EXPECT_EQ(Vector2ui(0, 0), empty_patch->GetMinimumSize());
    EXPECT_EQ(Range2ui(Point2ui(0, 0), Point2ui(16, 16)),
              empty_patch->GetPaddingBox(16, 16));
    const ImagePtr pixmap = empty_patch->BuildImage(16, 16, AllocatorPtr());
    EXPECT_EQ(16U, pixmap->GetWidth());
    EXPECT_EQ(16U, pixmap->GetHeight());
  }

  // Zero height.
  image->Set(Image::kRgba8888, 8, 0,
             base::DataContainer::Create<uint32>(pixels.get(), nullptr, false,
                                                 AllocatorPtr()));
  {
    NinePatchPtr empty_patch(new NinePatch(image));
    EXPECT_EQ(Vector2ui(0, 0), empty_patch->GetMinimumSize());
    EXPECT_EQ(Range2ui(Point2ui(0, 0), Point2ui(16, 16)),
              empty_patch->GetPaddingBox(16, 16));
    const ImagePtr pixmap = empty_patch->BuildImage(16, 16, AllocatorPtr());
    EXPECT_EQ(16U, pixmap->GetWidth());
    EXPECT_EQ(16U, pixmap->GetHeight());
  }

  // NULL data.
  image->Set(Image::kRgba8888, 8, 8,
             base::DataContainer::Create<uint32>(nullptr, nullptr, false,
                                                 AllocatorPtr()));
  {
    NinePatchPtr empty_patch(new NinePatch(image));
    EXPECT_EQ(Vector2ui(0, 0), empty_patch->GetMinimumSize());
    EXPECT_EQ(Range2ui(Point2ui(0, 0), Point2ui(16, 16)),
              empty_patch->GetPaddingBox(16, 16));
    const ImagePtr pixmap = empty_patch->BuildImage(16, 16, AllocatorPtr());
    EXPECT_EQ(16U, pixmap->GetWidth());
    EXPECT_EQ(16U, pixmap->GetHeight());
  }
}

TEST(NinePatch, StretchRegions) {
  ImagePtr image = CreateImage(12, 8);
  FillImage(image, kEmpty);
  // An image with no stretch regions cannot be stretched.
  {
    NinePatchPtr nine_patch(new NinePatch(image));
    EXPECT_EQ(0U, nine_patch->regions_h_.size());
    EXPECT_EQ(0U, nine_patch->regions_v_.size());
  }

  SetPixel(image, 0, 2, kBlack);
  SetPixel(image, 0, 3, kBlack);
  SetPixel(image, 0, 4, kBlack);
  SetPixel(image, 0, 5, kBlack);
  SetPixel(image, 2, 0, kBlack);
  SetPixel(image, 3, 0, kBlack);
  SetPixel(image, 4, 0, kBlack);
  SetPixel(image, 5, 0, kBlack);
  SetPixel(image, 6, 0, kBlack);
  SetPixel(image, 7, 0, kBlack);
  SetPixel(image, 8, 0, kBlack);
  SetPixel(image, 9, 0, kBlack);
  {
    NinePatchPtr nine_patch(new NinePatch(image));
    EXPECT_EQ(4U, nine_patch->regions_h_[2]);
    EXPECT_EQ(8U, nine_patch->regions_v_[2]);
  }

  FillImage(image, kEmpty);
  SetPixel(image, 0, 2, kBlack);
  SetPixel(image, 0, 3, kBlack);
  SetPixel(image, 0, 5, kBlack);
  SetPixel(image, 3, 0, kBlack);
  SetPixel(image, 4, 0, kBlack);
  SetPixel(image, 7, 0, kBlack);
  {
    NinePatchPtr nine_patch(new NinePatch(image));
    EXPECT_EQ(2U, nine_patch->regions_h_[2]);
    EXPECT_EQ(1U, nine_patch->regions_h_[5]);
    EXPECT_EQ(2U, nine_patch->regions_v_[3]);
    EXPECT_EQ(1U, nine_patch->regions_v_[7]);
  }

  FillImage(image, kEmpty);
  SetPixel(image, 0, 1, kBlack);
  SetPixel(image, 0, 2, kBlack);
  SetPixel(image, 0, 3, kBlack);
  SetPixel(image, 1, 0, kBlack);
  SetPixel(image, 2, 0, kBlack);
  SetPixel(image, 3, 0, kBlack);
  {
    NinePatchPtr nine_patch(new NinePatch(image));
    EXPECT_EQ(3U, nine_patch->regions_h_[1]);
    EXPECT_EQ(3U, nine_patch->regions_v_[1]);
  }

  FillImage(image, kEmpty);
  SetPixel(image, 0, 4, kBlack);
  SetPixel(image, 0, 5, kBlack);
  SetPixel(image, 0, 6, kBlack);
  SetPixel(image, 8, 0, kBlack);
  SetPixel(image, 9, 0, kBlack);
  SetPixel(image, 10, 0, kBlack);
  {
    NinePatchPtr nine_patch(new NinePatch(image));
    EXPECT_EQ(3U, nine_patch->regions_h_[4]);
    EXPECT_EQ(3U, nine_patch->regions_v_[8]);
  }

  FillImage(image, kEmpty);
  SetPixel(image, 0, 0, kBlack);  // out of bounds
  SetPixel(image, 0, 1, kBlack);
  SetPixel(image, 0, 2, kBlack);
  SetPixel(image, 0, 3, kBlack);
  SetPixel(image, 1, 0, kBlack);
  SetPixel(image, 2, 0, kBlack);
  SetPixel(image, 3, 0, kBlack);
  {
    NinePatchPtr nine_patch(new NinePatch(image));
    EXPECT_EQ(3U, nine_patch->regions_h_[1]);
    EXPECT_EQ(3U, nine_patch->regions_v_[1]);
  }

  FillImage(image, kEmpty);
  SetPixel(image, 0, 4, kBlack);
  SetPixel(image, 0, 5, kBlack);
  SetPixel(image, 0, 6, kBlack);
  SetPixel(image, 0, 7, kBlack);  // out of bounds
  SetPixel(image, 8, 0, kBlack);
  SetPixel(image, 9, 0, kBlack);
  SetPixel(image, 10, 0, kBlack);
  SetPixel(image, 11, 0, kBlack);  // out of bounds
  {
    NinePatchPtr nine_patch(new NinePatch(image));
    EXPECT_EQ(3U, nine_patch->regions_h_[4]);
    EXPECT_EQ(3U, nine_patch->regions_v_[8]);
  }
  FillImage(image, kBlack);
  {
    NinePatchPtr nine_patch(new NinePatch(image));
    EXPECT_EQ(6U, nine_patch->regions_h_[1]);
    EXPECT_EQ(10U, nine_patch->regions_v_[1]);
  }
}

TEST(NinePatch, MinimumSize) {
  ImagePtr image = CreateImage(12, 8);
  FillImage(image, kEmpty);
  {
    NinePatchPtr nine_patch(new NinePatch(image));
    EXPECT_EQ(Vector2ui(10, 6), nine_patch->GetMinimumSize());
  }
  FillImage(image, kEmpty);
  SetPixel(image, 0, 4, kBlack);
  SetPixel(image, 6, 0, kBlack);
  {
    NinePatchPtr nine_patch(new NinePatch(image));
    EXPECT_EQ(Vector2ui(9, 5), nine_patch->GetMinimumSize());
  }
  FillImage(image, kEmpty);
  SetPixel(image, 0, 3, kBlack);
  SetPixel(image, 0, 4, kBlack);
  SetPixel(image, 0, 5, kBlack);
  SetPixel(image, 4, 0, kBlack);
  SetPixel(image, 5, 0, kBlack);
  SetPixel(image, 6, 0, kBlack);
  SetPixel(image, 7, 0, kBlack);
  SetPixel(image, 8, 0, kBlack);
  {
    NinePatchPtr nine_patch(new NinePatch(image));
    EXPECT_EQ(Vector2ui(5, 3), nine_patch->GetMinimumSize());
  }
  FillImage(image, kEmpty);
  SetPixel(image, 0, 3, kBlack);
  SetPixel(image, 0, 5, kBlack);
  SetPixel(image, 4, 0, kBlack);
  SetPixel(image, 5, 0, kBlack);
  SetPixel(image, 7, 0, kBlack);
  SetPixel(image, 8, 0, kBlack);
  {
    NinePatchPtr nine_patch(new NinePatch(image));
    EXPECT_EQ(Vector2ui(6, 4), nine_patch->GetMinimumSize());
  }
  FillImage(image, kBlack);
  {
    NinePatchPtr nine_patch(new NinePatch(image));
    EXPECT_EQ(Vector2ui(0, 0), nine_patch->GetMinimumSize());
  }
}

TEST(NinePatch, PaddingBox) {
  ImagePtr image = CreateImage(12, 8);
  FillImage(image, kEmpty);
  SetPixel(image, 11, 4, kBlack);
  SetPixel(image, 5, 7, kBlack);
  {
    NinePatchPtr nine_patch(new NinePatch(image));
    EXPECT_EQ(Range2ui::BuildWithSize(Point2ui(5, 4), Vector2ui(1, 1)),
              nine_patch->padding_);
  }

  FillImage(image, kEmpty);
  SetPixel(image, 11, 2, kBlack);
  SetPixel(image, 11, 3, kBlack);
  SetPixel(image, 11, 4, kBlack);
  SetPixel(image, 11, 5, kBlack);
  SetPixel(image, 2, 7, kBlack);
  SetPixel(image, 3, 7, kBlack);
  SetPixel(image, 4, 7, kBlack);
  SetPixel(image, 5, 7, kBlack);
  SetPixel(image, 6, 7, kBlack);
  SetPixel(image, 7, 7, kBlack);
  SetPixel(image, 8, 7, kBlack);
  SetPixel(image, 9, 7, kBlack);
  {
    NinePatchPtr nine_patch(new NinePatch(image));
    EXPECT_EQ(Range2ui::BuildWithSize(Point2ui(2, 2), Vector2ui(8, 4)),
              nine_patch->padding_);
  }

  FillImage(image, kEmpty);
  SetPixel(image, 11, 2, kBlack);
  SetPixel(image, 11, 5, kBlack);
  SetPixel(image, 2, 7, kBlack);
  SetPixel(image, 9, 7, kBlack);
  {
    NinePatchPtr nine_patch(new NinePatch(image));
    EXPECT_EQ(Range2ui::BuildWithSize(Point2ui(2, 2), Vector2ui(8, 4)),
              nine_patch->padding_);
  }

  FillImage(image, kBlack);
  {
    NinePatchPtr nine_patch(new NinePatch(image));
    EXPECT_EQ(Range2ui::BuildWithSize(Point2ui(1, 1), Vector2ui(10, 6)),
              nine_patch->padding_);
  }
}

TEST(NinePatch, GetPaddingBox) {
  ImagePtr image = CreateImage(12, 8);
  FillImage(image, kEmpty);
  {
    NinePatchPtr nine_patch(new NinePatch(image));
    EXPECT_EQ(Range2ui(Point2ui(0, 0), Point2ui(10, 6)),
              nine_patch->GetPaddingBox(10, 6));
    EXPECT_EQ(Range2ui(Point2ui(0, 0), Point2ui(11, 7)),
              nine_patch->GetPaddingBox(11, 7));
    EXPECT_EQ(Range2ui(Point2ui(0, 0), Point2ui(50, 50)),
              nine_patch->GetPaddingBox(50, 50));
    EXPECT_EQ(Range2ui(Point2ui(0, 0), Point2ui(50, 6)),
              nine_patch->GetPaddingBox(50, 6));
    EXPECT_EQ(Range2ui(Point2ui(0, 0), Point2ui(10, 50)),
              nine_patch->GetPaddingBox(10, 50));
    EXPECT_EQ(Range2ui(Point2ui(0, 0), Point2ui(10, 2)),
              nine_patch->GetPaddingBox(10, 2));
    EXPECT_EQ(Range2ui(Point2ui(0, 0), Point2ui(2, 2)),
              nine_patch->GetPaddingBox(2, 2));
    EXPECT_EQ(Range2ui(Point2ui(0, 0), Point2ui(0, 0)),
              nine_patch->GetPaddingBox(0, 0));
  }
  // Image with no stretch, but 8x4 padding box specified.
  SetPixel(image, 11, 2, kBlack);
  SetPixel(image, 11, 3, kBlack);
  SetPixel(image, 11, 4, kBlack);
  SetPixel(image, 11, 5, kBlack);
  SetPixel(image, 2, 7, kBlack);
  SetPixel(image, 3, 7, kBlack);
  SetPixel(image, 4, 7, kBlack);
  SetPixel(image, 5, 7, kBlack);
  SetPixel(image, 6, 7, kBlack);
  SetPixel(image, 7, 7, kBlack);
  SetPixel(image, 8, 7, kBlack);
  SetPixel(image, 9, 7, kBlack);
  {
    NinePatchPtr nine_patch(new NinePatch(image));
    EXPECT_EQ(Range2ui::BuildWithSize(Point2ui(1, 1), Vector2ui(8, 4)),
              nine_patch->GetPaddingBox(10, 6));
    EXPECT_EQ(Range2ui::BuildWithSize(Point2ui(1, 1), Vector2ui(9, 5)),
              nine_patch->GetPaddingBox(11, 7));
    EXPECT_EQ(Range2ui::BuildWithSize(Point2ui(1, 1), Vector2ui(48, 48)),
              nine_patch->GetPaddingBox(50, 50));
    EXPECT_EQ(Range2ui::BuildWithSize(Point2ui(1, 1), Vector2ui(48, 4)),
              nine_patch->GetPaddingBox(50, 6));
    EXPECT_EQ(Range2ui::BuildWithSize(Point2ui(1, 1), Vector2ui(8, 48)),
              nine_patch->GetPaddingBox(10, 50));
    EXPECT_EQ(Range2ui::BuildWithSize(Point2ui(1, 1), Vector2ui(8, 4)),
              nine_patch->GetPaddingBox(10, 2));
    EXPECT_EQ(Range2ui::BuildWithSize(Point2ui(1, 1), Vector2ui(8, 4)),
              nine_patch->GetPaddingBox(2, 2));
    EXPECT_EQ(Range2ui::BuildWithSize(Point2ui(1, 1), Vector2ui(8, 4)),
              nine_patch->GetPaddingBox(0, 0));
  }
  // Now image has 5 px stretchable horizontal region.
  // And 1 px stretchable vertical region.
  SetPixel(image, 4, 0, kBlack);
  SetPixel(image, 5, 0, kBlack);
  SetPixel(image, 6, 0, kBlack);
  SetPixel(image, 7, 0, kBlack);
  SetPixel(image, 8, 0, kBlack);
  SetPixel(image, 0, 5, kBlack);
  {
    NinePatchPtr nine_patch(new NinePatch(image));
    EXPECT_EQ(Range2ui::BuildWithSize(Point2ui(1, 1), Vector2ui(8, 4)),
              nine_patch->GetPaddingBox(10, 6));
    EXPECT_EQ(Range2ui::BuildWithSize(Point2ui(1, 1), Vector2ui(9, 5)),
              nine_patch->GetPaddingBox(11, 7));
    EXPECT_EQ(Range2ui::BuildWithSize(Point2ui(1, 1), Vector2ui(48, 48)),
              nine_patch->GetPaddingBox(50, 50));
    EXPECT_EQ(Range2ui::BuildWithSize(Point2ui(1, 1), Vector2ui(48, 4)),
              nine_patch->GetPaddingBox(50, 6));
    EXPECT_EQ(Range2ui::BuildWithSize(Point2ui(1, 1), Vector2ui(8, 48)),
              nine_patch->GetPaddingBox(10, 50));
    EXPECT_EQ(Range2ui::BuildWithSize(Point2ui(1, 1), Vector2ui(8, 3)),
              nine_patch->GetPaddingBox(10, 2));
    EXPECT_EQ(Range2ui::BuildWithSize(Point2ui(1, 1), Vector2ui(3, 3)),
              nine_patch->GetPaddingBox(2, 2));
    EXPECT_EQ(Range2ui::BuildWithSize(Point2ui(1, 1), Vector2ui(3, 3)),
              nine_patch->GetPaddingBox(0, 0));
  }
}

TEST(NinePatch, GetSizeToFitContent) {
  ImagePtr image = CreateImage(12, 8);
  FillImage(image, kEmpty);
  {
    NinePatchPtr nine_patch(new NinePatch(image));
    EXPECT_EQ(Vector2ui(10, 6), nine_patch->GetSizeToFitContent(10, 6));
    EXPECT_EQ(Vector2ui(11, 7), nine_patch->GetSizeToFitContent(11, 7));
    EXPECT_EQ(Vector2ui(50, 50), nine_patch->GetSizeToFitContent(50, 50));
    EXPECT_EQ(Vector2ui(50, 6), nine_patch->GetSizeToFitContent(50, 6));
    EXPECT_EQ(Vector2ui(10, 50), nine_patch->GetSizeToFitContent(10, 50));
    EXPECT_EQ(Vector2ui(10, 2), nine_patch->GetSizeToFitContent(10, 2));
    EXPECT_EQ(Vector2ui(2, 2), nine_patch->GetSizeToFitContent(2, 2));
    EXPECT_EQ(Vector2ui(0, 0), nine_patch->GetSizeToFitContent(0, 0));
  }
  // Image with no stretch, but 8x4 padding box specified.
  SetPixel(image, 11, 2, kBlack);
  SetPixel(image, 11, 3, kBlack);
  SetPixel(image, 11, 4, kBlack);
  SetPixel(image, 11, 5, kBlack);
  SetPixel(image, 2, 7, kBlack);
  SetPixel(image, 3, 7, kBlack);
  SetPixel(image, 4, 7, kBlack);
  SetPixel(image, 5, 7, kBlack);
  SetPixel(image, 6, 7, kBlack);
  SetPixel(image, 7, 7, kBlack);
  SetPixel(image, 8, 7, kBlack);
  SetPixel(image, 9, 7, kBlack);
  {
    NinePatchPtr nine_patch(new NinePatch(image));
    EXPECT_EQ(Vector2ui(10, 6), nine_patch->GetSizeToFitContent(7, 3));
    EXPECT_EQ(Vector2ui(10, 6), nine_patch->GetSizeToFitContent(7, 4));
    EXPECT_EQ(Vector2ui(10, 7), nine_patch->GetSizeToFitContent(7, 5));
    EXPECT_EQ(Vector2ui(10, 6), nine_patch->GetSizeToFitContent(8, 3));
    EXPECT_EQ(Vector2ui(10, 6), nine_patch->GetSizeToFitContent(8, 4));
    EXPECT_EQ(Vector2ui(10, 7), nine_patch->GetSizeToFitContent(8, 5));
    EXPECT_EQ(Vector2ui(11, 6), nine_patch->GetSizeToFitContent(9, 3));
    EXPECT_EQ(Vector2ui(11, 6), nine_patch->GetSizeToFitContent(9, 4));
    EXPECT_EQ(Vector2ui(11, 7), nine_patch->GetSizeToFitContent(9, 5));
    EXPECT_EQ(Vector2ui(10, 6), nine_patch->GetSizeToFitContent(0, 0));
  }
  // Now image has 5 px stretchable horizontal region.
  // And 1 px stretchable vertical region.
  SetPixel(image, 4, 0, kBlack);
  SetPixel(image, 5, 0, kBlack);
  SetPixel(image, 6, 0, kBlack);
  SetPixel(image, 7, 0, kBlack);
  SetPixel(image, 8, 0, kBlack);
  SetPixel(image, 0, 5, kBlack);
  {
    NinePatchPtr nine_patch(new NinePatch(image));
    EXPECT_EQ(Vector2ui(9, 5), nine_patch->GetSizeToFitContent(7, 3));
    EXPECT_EQ(Vector2ui(9, 6), nine_patch->GetSizeToFitContent(7, 4));
    EXPECT_EQ(Vector2ui(9, 7), nine_patch->GetSizeToFitContent(7, 5));
    EXPECT_EQ(Vector2ui(10, 5), nine_patch->GetSizeToFitContent(8, 3));
    EXPECT_EQ(Vector2ui(10, 6), nine_patch->GetSizeToFitContent(8, 4));
    EXPECT_EQ(Vector2ui(10, 7), nine_patch->GetSizeToFitContent(8, 5));
    EXPECT_EQ(Vector2ui(11, 5), nine_patch->GetSizeToFitContent(9, 3));
    EXPECT_EQ(Vector2ui(11, 6), nine_patch->GetSizeToFitContent(9, 4));
    EXPECT_EQ(Vector2ui(11, 7), nine_patch->GetSizeToFitContent(9, 5));
    EXPECT_EQ(Vector2ui(5, 5), nine_patch->GetSizeToFitContent(0, 0));
  }
}

TEST(NinePatch, SimpleImage) {
  ImagePtr image = CreateImage(5, 5);
  FillImage(image, kEmpty);
  // Test trying to create images of the same size as the original image when
  // there are no stretch regions.
  {
    NinePatchPtr nine_patch(new NinePatch(image));
    ImagePtr sized = nine_patch->BuildImage(5, 5, AllocatorPtr());
    EXPECT_EQ(5U, sized->GetWidth());
    EXPECT_EQ(5U, sized->GetHeight());
    for (uint32 y = 0; y < 5; ++y) {
      for (uint32 x = 0; x < 5; ++x) {
        EXPECT_PRED_FORMAT2(AssertColorsEqual, kEmpty,
                            GetPixel(sized, x, y));
      }
    }

    sized = nine_patch->BuildImage(10, 10, AllocatorPtr());
    EXPECT_EQ(10U, sized->GetWidth());
    EXPECT_EQ(10U, sized->GetHeight());
    for (uint32 y = 0; y < 10; ++y) {
      for (uint32 x = 0; x < 10; ++x) {
        EXPECT_PRED_FORMAT2(AssertColorsEqual, kEmpty,
                            GetPixel(sized, x, y));
      }
    }
  }
  SetPixel(image, 0, 2, kBlack);
  SetPixel(image, 2, 0, kBlack);
  SetPixel(image, 4, 2, kBlack);
  SetPixel(image, 2, 4, kBlack);

  SetPixel(image, 1, 1, kRed);
  SetPixel(image, 2, 1, kGreen);
  SetPixel(image, 3, 1, kBlue);
  SetPixel(image, 1, 2, kMidRed);
  SetPixel(image, 2, 2, kMidGreen);
  SetPixel(image, 3, 2, kMidBlue);
  SetPixel(image, 1, 3, kDarkRed);
  SetPixel(image, 2, 3, kDarkBlue);
  SetPixel(image, 3, 3, kDarkGreen);

  NinePatchPtr nine_patch(new NinePatch(image));
  ImagePtr sized = nine_patch->BuildImage(3, 3, AllocatorPtr());
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kRed, GetPixel(sized, 0, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kGreen, GetPixel(sized, 1, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kBlue, GetPixel(sized, 2, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidRed, GetPixel(sized, 0, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidGreen, GetPixel(sized, 1, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidBlue, GetPixel(sized, 2, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kDarkRed, GetPixel(sized, 0, 2));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kDarkBlue, GetPixel(sized, 1, 2));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kDarkGreen, GetPixel(sized, 2, 2));

  sized = nine_patch->BuildImage(5, 5, AllocatorPtr());
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kRed, GetPixel(sized, 0, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kGreen, GetPixel(sized, 1, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kGreen, GetPixel(sized, 2, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kGreen, GetPixel(sized, 3, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kBlue, GetPixel(sized, 4, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidRed, GetPixel(sized, 0, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidGreen, GetPixel(sized, 1, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidGreen, GetPixel(sized, 2, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidGreen, GetPixel(sized, 3, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidBlue, GetPixel(sized, 4, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidRed, GetPixel(sized, 0, 2));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidGreen, GetPixel(sized, 1, 2));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidGreen, GetPixel(sized, 2, 2));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidGreen, GetPixel(sized, 3, 2));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidBlue, GetPixel(sized, 4, 2));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidRed, GetPixel(sized, 0, 3));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidGreen, GetPixel(sized, 1, 3));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidGreen, GetPixel(sized, 2, 3));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidGreen, GetPixel(sized, 3, 3));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidBlue, GetPixel(sized, 4, 3));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kDarkRed, GetPixel(sized, 0, 4));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kDarkBlue, GetPixel(sized, 1, 4));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kDarkBlue, GetPixel(sized, 2, 4));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kDarkBlue, GetPixel(sized, 3, 4));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kDarkGreen, GetPixel(sized, 4, 4));

  sized = nine_patch->BuildImage(2, 2, AllocatorPtr());
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kRed, GetPixel(sized, 0, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kBlue, GetPixel(sized, 1, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kDarkRed, GetPixel(sized, 0, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kDarkGreen, GetPixel(sized, 1, 1));

  sized = nine_patch->BuildImage(5, 3, AllocatorPtr());
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kRed, GetPixel(sized, 0, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kGreen, GetPixel(sized, 1, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kGreen, GetPixel(sized, 2, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kGreen, GetPixel(sized, 3, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kBlue, GetPixel(sized, 4, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidRed, GetPixel(sized, 0, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidGreen, GetPixel(sized, 1, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidGreen, GetPixel(sized, 2, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidGreen, GetPixel(sized, 3, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidBlue, GetPixel(sized, 4, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kDarkRed, GetPixel(sized, 0, 2));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kDarkBlue, GetPixel(sized, 1, 2));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kDarkBlue, GetPixel(sized, 2, 2));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kDarkBlue, GetPixel(sized, 3, 2));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kDarkGreen, GetPixel(sized, 4, 2));
}

TEST(NinePatch, ComplexImage) {
  ImagePtr image = CreateImage(7, 5);
  FillImage(image, kEmpty);
  SetPixel(image, 0, 2, kBlack);
  SetPixel(image, 2, 0, kBlack);
  SetPixel(image, 4, 0, kBlack);
  SetPixel(image, 6, 2, kBlack);
  SetPixel(image, 2, 4, kBlack);
  SetPixel(image, 3, 4, kBlack);
  SetPixel(image, 4, 4, kBlack);

  SetPixel(image, 1, 1, kRed);
  SetPixel(image, 2, 1, kGreen);
  SetPixel(image, 3, 1, kBlue);
  SetPixel(image, 4, 1, kYellow);
  SetPixel(image, 5, 1, kPurple);
  SetPixel(image, 1, 2, kMidRed);
  SetPixel(image, 2, 2, kMidGreen);
  SetPixel(image, 3, 2, kMidBlue);
  SetPixel(image, 4, 2, kMidYellow);
  SetPixel(image, 5, 2, kMidPurple);
  SetPixel(image, 1, 3, kDarkRed);
  SetPixel(image, 2, 3, kDarkBlue);
  SetPixel(image, 3, 3, kDarkGreen);
  SetPixel(image, 4, 3, kDarkYellow);
  SetPixel(image, 5, 3, kDarkPurple);

  NinePatchPtr nine_patch(new NinePatch(image));
  ImagePtr sized = nine_patch->BuildImage(5, 3, AllocatorPtr());
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kRed, GetPixel(sized, 0, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kGreen, GetPixel(sized, 1, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kBlue, GetPixel(sized, 2, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kYellow, GetPixel(sized, 3, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kPurple, GetPixel(sized, 4, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidRed, GetPixel(sized, 0, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidGreen, GetPixel(sized, 1, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidBlue, GetPixel(sized, 2, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidYellow, GetPixel(sized, 3, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidPurple, GetPixel(sized, 4, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kDarkRed, GetPixel(sized, 0, 2));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kDarkBlue, GetPixel(sized, 1, 2));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kDarkGreen, GetPixel(sized, 2, 2));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kDarkYellow, GetPixel(sized, 3, 2));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kDarkPurple, GetPixel(sized, 4, 2));

  sized = nine_patch->BuildImage(7, 3, AllocatorPtr());
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kRed, GetPixel(sized, 0, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kGreen, GetPixel(sized, 1, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kGreen, GetPixel(sized, 2, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kBlue, GetPixel(sized, 3, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kYellow, GetPixel(sized, 4, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kYellow, GetPixel(sized, 5, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kPurple, GetPixel(sized, 6, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidRed, GetPixel(sized, 0, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidGreen, GetPixel(sized, 1, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidGreen, GetPixel(sized, 2, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidBlue, GetPixel(sized, 3, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidYellow, GetPixel(sized, 4, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidYellow, GetPixel(sized, 5, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidPurple, GetPixel(sized, 6, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kDarkRed, GetPixel(sized, 0, 2));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kDarkBlue, GetPixel(sized, 1, 2));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kDarkBlue, GetPixel(sized, 2, 2));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kDarkGreen, GetPixel(sized, 3, 2));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kDarkYellow, GetPixel(sized, 4, 2));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kDarkYellow, GetPixel(sized, 5, 2));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kDarkPurple, GetPixel(sized, 6, 2));
}

TEST(NinePatch, VeryComplexImage) {
  ImagePtr image = CreateImage(7, 8);
  FillImage(image, kEmpty);
  SetPixel(image, 2, 0, kBlack);
  SetPixel(image, 4, 0, kBlack);
  SetPixel(image, 0, 2, kBlack);
  SetPixel(image, 0, 4, kBlack);
  SetPixel(image, 0, 5, kBlack);
  SetPixel(image, 2, 7, kBlack);
  SetPixel(image, 3, 7, kBlack);
  SetPixel(image, 4, 7, kBlack);
  SetPixel(image, 6, 2, kBlack);
  SetPixel(image, 6, 3, kBlack);
  SetPixel(image, 6, 4, kBlack);
  SetPixel(image, 6, 5, kBlack);

  SetPixel(image, 1, 1, kRed);
  SetPixel(image, 2, 1, kGreen);
  SetPixel(image, 3, 1, kBlue);
  SetPixel(image, 4, 1, kYellow);
  SetPixel(image, 5, 1, kPurple);
  SetPixel(image, 1, 2, kLtRed);
  SetPixel(image, 2, 2, kLtGreen);
  SetPixel(image, 3, 2, kLtBlue);
  SetPixel(image, 4, 2, kLtYellow);
  SetPixel(image, 5, 2, kLtPurple);
  SetPixel(image, 1, 3, kMidRed);
  SetPixel(image, 2, 3, kMidGreen);
  SetPixel(image, 3, 3, kMidBlue);
  SetPixel(image, 4, 3, kMidYellow);
  SetPixel(image, 5, 3, kMidPurple);
  SetPixel(image, 1, 4, kDarkRed);
  SetPixel(image, 2, 4, kDarkBlue);
  SetPixel(image, 3, 4, kDarkGreen);
  SetPixel(image, 4, 4, kDarkYellow);
  SetPixel(image, 5, 4, kDarkPurple);
  SetPixel(image, 1, 5, kBlkRed);
  SetPixel(image, 2, 5, kBlkBlue);
  SetPixel(image, 3, 5, kBlkGreen);
  SetPixel(image, 4, 5, kBlkYellow);
  SetPixel(image, 5, 5, kBlkPurple);
  SetPixel(image, 1, 6, kRed);
  // Test a few special values.
  SetPixel(image, 2, 6, 0xcc00ff00);
  SetPixel(image, 3, 6, 0x99ff0000);
  SetPixel(image, 4, 6, 0x6600ffff);
  SetPixel(image, 5, 6, 0x33ff00ff);

  NinePatchPtr nine_patch(new NinePatch(image));
  ImagePtr sized = nine_patch->BuildImage(3, 3, AllocatorPtr());
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kRed, GetPixel(sized, 0, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kBlue, GetPixel(sized, 1, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kPurple, GetPixel(sized, 2, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidRed, GetPixel(sized, 0, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidBlue, GetPixel(sized, 1, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidPurple, GetPixel(sized, 2, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kRed, GetPixel(sized, 0, 2));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, 0x99ff0000, GetPixel(sized, 1, 2));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, 0x33ff00ff, GetPixel(sized, 2, 2));

  sized = nine_patch->BuildImage(5, 6, AllocatorPtr());
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kRed, GetPixel(sized, 0, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kGreen, GetPixel(sized, 1, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kBlue, GetPixel(sized, 2, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kYellow, GetPixel(sized, 3, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kPurple, GetPixel(sized, 4, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kLtRed, GetPixel(sized, 0, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kLtGreen, GetPixel(sized, 1, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kLtBlue, GetPixel(sized, 2, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kLtYellow, GetPixel(sized, 3, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kLtPurple, GetPixel(sized, 4, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidRed, GetPixel(sized, 0, 2));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidGreen, GetPixel(sized, 1, 2));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidBlue, GetPixel(sized, 2, 2));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidYellow, GetPixel(sized, 3, 2));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidPurple, GetPixel(sized, 4, 2));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kDarkRed, GetPixel(sized, 0, 3));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kDarkBlue, GetPixel(sized, 1, 3));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kDarkGreen, GetPixel(sized, 2, 3));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kDarkYellow, GetPixel(sized, 3, 3));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kDarkPurple, GetPixel(sized, 4, 3));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kBlkRed, GetPixel(sized, 0, 4));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kBlkBlue, GetPixel(sized, 1, 4));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kBlkGreen, GetPixel(sized, 2, 4));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kBlkYellow, GetPixel(sized, 3, 4));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kBlkPurple, GetPixel(sized, 4, 4));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kRed, GetPixel(sized, 0, 5));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, 0xcc00ff00, GetPixel(sized, 1, 5));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, 0x99ff0000, GetPixel(sized, 2, 5));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, 0x6600ffff, GetPixel(sized, 3, 5));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, 0x33ff00ff, GetPixel(sized, 4, 5));
}

TEST(NinePatch, NonIntegerStretchRatios) {
  // All of the other tests in this suite use sizes that allow all stretch
  // regions to expand to integer multiples of their source sizes. For example,
  // if you have a 5x5 nine-patch image with two one-pixel wide horizontal
  // stretch regions, and you try to build a 9x5 image, each stretch region
  // should be 3px wide. If you expand the image to 10x5, however, it is unclear
  // which stretch region should become 4px wide, and different rendering
  // engines do it differently. This test verifies that:
  //   - If the source image is opaque, no transparent regions should be present
  //     in the sized image,
  //   - If the source image uses only two colors, those colors (and no others)
  //     should be present in the sized image.
  ImagePtr image = CreateImage(7, 5);
  FillImage(image, kEmpty);
  SetPixel(image, 0, 2, kBlack);
  SetPixel(image, 2, 0, kBlack);
  SetPixel(image, 4, 0, kBlack);

  SetPixel(image, 1, 1, kRed);
  SetPixel(image, 2, 1, kGreen);
  SetPixel(image, 3, 1, kRed);
  SetPixel(image, 4, 1, kGreen);
  SetPixel(image, 5, 1, kRed);
  SetPixel(image, 1, 2, kRed);
  SetPixel(image, 2, 2, kGreen);
  SetPixel(image, 3, 2, kRed);
  SetPixel(image, 4, 2, kGreen);
  SetPixel(image, 5, 2, kRed);
  SetPixel(image, 1, 3, kRed);
  SetPixel(image, 2, 3, kGreen);
  SetPixel(image, 3, 3, kRed);
  SetPixel(image, 4, 3, kGreen);
  SetPixel(image, 5, 3, kRed);

  NinePatchPtr nine_patch(new NinePatch(image));
  ImagePtr sized = nine_patch->BuildImage(6, 4, AllocatorPtr());
  for (uint32 y = 0; y < sized->GetHeight(); ++y) {
    for (uint32 x = 0; x < sized->GetWidth(); ++x) {
      EXPECT_TRUE(kRed == GetPixel(sized, x, y) ||
                  kGreen == GetPixel(sized, x, y));
    }
  }
}

TEST(NinePatch, PremultipliedAlpha) {
  ImagePtr image = CreateImage(5, 5);
  FillImage(image, kEmpty);
  SetPixel(image, 0, 2, kBlack);
  SetPixel(image, 2, 0, kBlack);
  SetPixel(image, 4, 2, kBlack);
  SetPixel(image, 2, 4, kBlack);

  SetPixel(image, 1, 1, kRed);
  SetPixel(image, 2, 1, kGreen);
  SetPixel(image, 3, 1, kBlue);
  SetPixel(image, 1, 2, kMidRed);
  SetPixel(image, 2, 2, kMidGreen);
  SetPixel(image, 3, 2, kMidBlue);
  // Test premultiplied alpha values.
  SetPixel(image, 1, 3, 0x99000066);
  SetPixel(image, 2, 3, 0x99006600);
  SetPixel(image, 3, 3, 0x99660000);

  NinePatchPtr nine_patch(new NinePatch(image));
  ImagePtr sized = nine_patch->BuildImage(3, 3, AllocatorPtr());
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kRed, GetPixel(sized, 0, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kGreen, GetPixel(sized, 1, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kBlue, GetPixel(sized, 2, 0));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidRed, GetPixel(sized, 0, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidGreen, GetPixel(sized, 1, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, kMidBlue, GetPixel(sized, 2, 1));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, 0x99000066, GetPixel(sized, 0, 2));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, 0x99006600, GetPixel(sized, 1, 2));
  EXPECT_PRED_FORMAT2(AssertColorsEqual, 0x99660000, GetPixel(sized, 2, 2));
}

TEST(NinePatch, ExpectedContents) {
  Images::RegisterAssets();

  // Load an image, create a larger version, and make sure it matches a golden
  // image.
  const ImagePtr image = LoadAssetImage("tooltip.9.png");
  const ImagePtr expected_image = LoadAssetImage("tooltip_120x48.png");
  NinePatchPtr nine_patch(new NinePatch(image));
  const ImagePtr sized = nine_patch->BuildImage(120, 48, AllocatorPtr());

  const uint32 width = sized->GetWidth();
  const uint32 height = sized->GetHeight();
  EXPECT_EQ(expected_image->GetWidth(), width);
  EXPECT_EQ(expected_image->GetHeight(), height);

  for (uint32 y = 0; y < height; ++y) {
    for (uint32 x = 0; x < width; ++x) {
      EXPECT_PRED_FORMAT2(AssertColorsEqual, GetPixel(expected_image, x, y),
                          GetPixel(sized, x, y));
    }
  }
}

}  // namespace image
}  // namespace ion
