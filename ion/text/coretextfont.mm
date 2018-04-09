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

#include "ion/text/coretextfont.h"

#include <mutex>  // NOLINT(build/c++11)

// Though functionally very similar, the objective-c font object on Mac is
// NSFont and on iOS is UIFont, and they are found in different headers.
#if defined(ION_PLATFORM_MAC)
#import <AppKit/AppKit.h>

typedef NSFont OBJC_FONT;

#else  // defined(ION_PLATFORM_MAC)
#import <CoreText/CoreText.h>
#import <UIKit/UIKit.h>

typedef UIFont OBJC_FONT;

#endif  // defined(ION_PLATFORM_MAC)

#include "ion/math/matrix.h"
#include "ion/math/rangeutils.h"
#include "ion/text/layout.h"

namespace ion {
namespace text {

namespace {

using math::Matrix2d;
using math::Point2d;
using math::Point2f;
using math::Point3d;
using math::Point3f;
using math::Range2d;
using math::Range2f;
using math::Vector2d;
using math::Vector2f;

// The width and height of the rectangular area in which CoreText is asked to
// layout the incoming text. This needs to be larger than any expected output
// layout, so that CoreText won't wrap or clip its results.
static const CGFloat kPathSizePixels = 10000;

// Returns true if a target size passed to a layout function is valid. To be
// valid, neither component can be negative.
static bool IsSizeValid(const Vector2f& target_size) {
  const float width = target_size[0];
  const float height = target_size[1];
  return (width >= 0.0f && height >= 0.0f);
}

// This is the transform that takes CoreText glyph coordinates and converts them
// into those that can be used in ion::text::Layout::Glyph::Quads.
struct Transform {
  Vector2d translation;
  Vector2d scale;
  Point2f position;
  Vector2f size;
};

// Calculates the transform that should be applied to glyph locations due to
// the input |options|, and for the given CoreText line data.
static Transform CalculateLayoutOptionsTransform(
    CFArrayRef lines,
    CFIndex line_count,
    const CGPoint* line_origins,
    const LayoutOptions& options,
    const Font* font) {
  // Work out the range enclosing the bounds of all the lines.
  const CTLineBoundsOptions bounds_option = options.metrics_based_alignment ?
      kCTLineBoundsExcludeTypographicLeading : kCTLineBoundsUseGlyphPathBounds;
  math::Range2d line_bounds_range;
  for (CFIndex line_index = 0; line_index < line_count; ++line_index) {
    CTLineRef line = (CTLineRef)CFArrayGetValueAtIndex(lines, line_index);
    CGRect line_bounds = CTLineGetBoundsWithOptions(line, bounds_option);
    line_bounds.origin.x += line_origins[line_index].x;
    line_bounds.origin.y += line_origins[line_index].y;
    if (options.metrics_based_alignment) {
      // Some fonts do not have descent defined and default to y_min, which
      // can exceed the font size. Use the same approximation from ascender to
      // estimate the descender and correct this offest.
      CGFloat descent;
      CTLineGetTypographicBounds(line, nullptr, &descent, nullptr);
      const CGFloat descender = static_cast<CGFloat>(
          font->GetSizeInPixels() - font->GetFontMetrics().ascender);
      line_bounds.size.height = static_cast<CGFloat>(font->GetSizeInPixels());
      line_bounds.origin.y += descent - descender;
    }
    line_bounds_range.ExtendByPoint(Point2d(CGRectGetMinX(line_bounds),
                                            CGRectGetMinY(line_bounds)));
    line_bounds_range.ExtendByPoint(Point2d(CGRectGetMaxX(line_bounds),
                                            CGRectGetMaxY(line_bounds)));
  }

  // The height as it should be considered for transformation isn't the bounds
  // of the lines as calculated above, which take into account the presence or
  // absence of descenders and ascenders in the text. It's a simpler calculation
  // as follows.
  const double rect_height =
      font->GetFontMetrics().line_advance_height * (line_count - 1) + font->GetSizeInPixels();

  // Compute the scale based on the text size in pixels and LayoutOptions::target_size. If both the
  // target size dimensions are 0, then do the layout in pixels with no scaling. If only one of the
  // target size dimensions is 0, use the other dimension's scale.
  Vector2d scale_for_target_size;
  if (options.target_size == Vector2f::Zero()) {
    scale_for_target_size = Vector2d::Fill(1.0);
  } else if (options.target_size[0] == 0.0f) {
    DCHECK_GT(options.target_size[1], 0.0f);
    const double s = options.target_size[1] / rect_height;
    scale_for_target_size.Set(s, s);
  } else if (options.target_size[1] == 0.0f) {
    DCHECK_GT(options.target_size[0], 0.0f);
    const double s = options.target_size[0] / line_bounds_range.GetSize()[0];
    scale_for_target_size.Set(s, s);
  } else {
    scale_for_target_size.Set(options.target_size[0] / line_bounds_range.GetSize()[0],
                              options.target_size[1] / rect_height);
  }

  // Calculate the translation needed to align the text relative to the origin rather than where
  // CoreText placed it within the CGPath.
  Vector2d translation_for_path;
  double position_x = 0.0;  // Get the left edge of the text rectangle.
  if (line_count) {
    // Horizontal alignment has been done by CoreText within the path that it
    // was passed. All that remains is to center it around the origin rather
    // than the edges of the path rectangle.
    switch (options.horizontal_alignment) {
      case kAlignLeft:
        translation_for_path[0] = 0;
        break;
      case kAlignHCenter:
        translation_for_path[0] = -kPathSizePixels * 0.5;
        position_x = -line_bounds_range.GetSize()[0] / 2.0;
        break;
      case kAlignRight:
        translation_for_path[0] = -kPathSizePixels;
        position_x = -line_bounds_range.GetSize()[0];
        break;
    }

    // Center vertically as required.
    switch (options.vertical_alignment) {
      case kAlignTop:
        translation_for_path[1] = -line_bounds_range.GetMaxPoint()[1];
        break;
      case kAlignVCenter:
        translation_for_path[1] =
            -(line_bounds_range.GetMinPoint()[1] + line_bounds_range.GetMaxPoint()[1]) * 0.5;
        break;
      case kAlignBaseline:
        translation_for_path[1] = -line_origins[0].y;
        break;
      case kAlignBottom:
        translation_for_path[1] = -line_bounds_range.GetMinPoint()[1];
        break;
    }
  }

  // Combine scale_for_target_size, translation_for_path and LayoutOptions::target_point.
  Transform result;
  result.scale = scale_for_target_size;
  result.translation[0] =
      translation_for_path[0] * scale_for_target_size[0] + options.target_point[0];
  result.translation[1] =
      translation_for_path[1] * scale_for_target_size[1] + options.target_point[1];

  // Calculate the text rectangle's bottom-left position and size.
  result.position[0] =
      static_cast<float>(position_x * scale_for_target_size[0] + options.target_point[0]);
  result.position[1] = static_cast<float>(result.translation[1] +
      line_bounds_range.GetMinPoint()[1] * scale_for_target_size[1]);
  result.size[0] =
      static_cast<float>(line_bounds_range.GetSize()[0] * scale_for_target_size[0]);
  result.size[1] =
      static_cast<float>(line_bounds_range.GetSize()[1] * scale_for_target_size[1]);
  return result;
}

// Builds a Layout::Quad in the XY plane for |rect|.
static const Layout::Quad BuildXyQuad(const Range2d& rect) {
  const Point2f& min(rect.GetMinPoint());
  const Point2f& max(rect.GetMaxPoint());
  return Layout::Quad(Point3f(min[0], min[1], 0.0f),
                      Point3f(max[0], min[1], 0.0f),
                      Point3f(max[0], max[1], 0.0f),
                      Point3f(min[0], max[1], 0.0f));
}

// Applies |transform| to |point|.
static Point2d ApplyTransform(const Transform& transform,
                              const Point2d& point) {
  return Point2d(point[0] * transform.scale[0] + transform.translation[0],
                 point[1] * transform.scale[1] + transform.translation[1]);
}

// Because CoreTextFonts may render glyphs from more than one CTFont, the
// GlyphIndex passed to the rest of Ion must combine both the CoreText CGGlyph
// and some representation of what CoreText font the glyph comes from. These
// following functions map between GlyphIndex, CGGlyph and a uint16 font index.
// This first function combines a |glyph| and |font_index| to create a
// GlyphIndex.
static GlyphIndex CGGlyphFontIndexToGlyphIndex(CGGlyph glyph,
                                               uint16 font_index) {
  ION_STATIC_ASSERT(sizeof(CGGlyph) <= sizeof(uint16),
                    "CGGlyph size too large for bit-packing operation.");
  ION_STATIC_ASSERT(sizeof(CGGlyph) + sizeof(uint16) <= sizeof(GlyphIndex),
                    "CGGlyph and uint16 too large to pack into GlyphIndex.");
  return static_cast<GlyphIndex>(glyph) | (static_cast<GlyphIndex>(font_index) << 16);
}

// Extracts the system CGGlyph from an ion::text::GlyphIndex.
static CGGlyph GlyphIndexToCGGlyph(GlyphIndex glyph_index) {
  return static_cast<CGGlyph>(glyph_index);
}

// Extracts the font index from an ion::text::GlyphIndex.
static uint16 GlyphIndexToFontIndex(GlyphIndex glyph_index) {
  return static_cast<uint16>(glyph_index >> 16);
}

// Adds a glyph to the |layout|. Glyphs contained in CTLines have a rectangular
// |bounds|, a |position| relative to their containing CTLine and the CTLine has
// an origin |line_origin| relative to the whole text being rendered. This
// method also applies the |sdf_padding| as necessary.
static void AddGlyphToLayout(CGGlyph glyph,
                             uint16 font_index,
                             const Transform& transform,
                             const CGRect& bounds,
                             const CGPoint& position,
                             const CGPoint& line_origin,
                             float sdf_padding,
                             Layout* layout) {
  const Point2d glyph_min(CGRectGetMinX(bounds) + position.x + line_origin.x,
                          CGRectGetMinY(bounds) + position.y + line_origin.y);
  const Point2d glyph_max(CGRectGetMaxX(bounds) + position.x + line_origin.x,
                          CGRectGetMaxY(bounds) + position.y + line_origin.y);

  Range2d transformed_glyph(ApplyTransform(transform, glyph_min),
                            ApplyTransform(transform, glyph_max));
  const Range2f tight_bounds(Point2f(transformed_glyph.GetMinPoint()),
                             Point2f(transformed_glyph.GetMaxPoint()));

  // Scale nonuniformly about the Quad center to compensate for the padding.
  if (sdf_padding != 0) {
    const double padding = 2.0 * sdf_padding;
    const double w = CGRectGetWidth(bounds);
    const double h = CGRectGetHeight(bounds);
    if (w > 0. && h > 0.) {
      const Vector2d scale((w + padding) / w, (h + padding) / h);
      transformed_glyph =
          math::ScaleRangeNonUniformly(transformed_glyph, scale);
    }
  }

  const Vector2f offset(float(CGRectGetMinX(bounds) * transform.scale[0]),
                        float(CGRectGetMinY(bounds) * transform.scale[1]));

  CHECK(layout->AddGlyph(
      Layout::Glyph(CGGlyphFontIndexToGlyphIndex(glyph, font_index),
                    BuildXyQuad(transformed_glyph),
                    tight_bounds,
                    offset)));
}

}  // anonymous namespace

//-----------------------------------------------------------------------------
//
// Helper class. The only reason for this class is to avoid having either a
// CTFontRef exposed in coretextfont.h, or an awkward cast from (for example)
// a void * member of CoreTextFont. Given that the class exists, however, it
// implements all non-trivial functionality of CoreTextFont.
//
//-----------------------------------------------------------------------------

class CoreTextFont::Helper {
 public:
  Helper(const CoreTextFont& owning_font, const void* data, size_t data_size);
  ~Helper();

  // For the given string, return its layout rendered with the current font.
  CTFrameRef CreateFrame(const std::string& text,
                         HorizontalAlignment horizontal_alignment,
                         float line_spacing) const;

  // The following methods correspond directly to CoreTextFont methods, see
  // comments in the base Font class.
  const Font::FontMetrics GetFontMetrics(size_t size_in_pixels) const;
  bool LoadGlyphGrid(GlyphIndex glyph_index, GlyphGrid* glyph_grid);
  GlyphIndex GetDefaultGlyphForChar(CharIndex char_index);
  const Layout BuildLayout(const std::string& text, const LayoutOptions& options, const Font* font);
  std::string GetCTFontName() const;
  void AddFallbackFont(const FontPtr& fallback);

 private:
  // A thread-safe bi-directional mapping is maintained here between uint16 font
  // indices and CTFonts.
  uint16 FontToFontIndex(CTFontRef font);
  CTFontRef FontIndexToFont(uint16 glyph_index);

  typedef base::AllocMap<CTFontRef, uint16> FontToIndexMap;
  typedef base::AllocMap<uint16, CTFontRef> IndexToFontMap;

  FontToIndexMap font_to_index_map_;
  IndexToFontMap index_to_font_map_;
  // Guards all font<->index data fields above.
  std::mutex mutex_;

  CTFontRef coretext_font_;
  CGPathRef path_;
};

CoreTextFont::Helper::Helper(const CoreTextFont& owning_font,
                             const void* data, size_t data_size)
  : font_to_index_map_(owning_font.GetAllocator()),
    index_to_font_map_(owning_font.GetAllocator()),
    path_(CGPathCreateWithRect(
        CGRectMake(0, 0, kPathSizePixels, kPathSizePixels),
        &CGAffineTransformIdentity)) {
  const size_t size_in_pixels = owning_font.GetSizeInPixels();
  if (data && data_size) {
    CGDataProviderRef data_provider =
        CGDataProviderCreateWithData(nullptr, data, data_size, nullptr);
    CGFontRef cg_font = CGFontCreateWithDataProvider(data_provider);
    coretext_font_ =
        CTFontCreateWithGraphicsFont(cg_font, size_in_pixels, nullptr, nullptr);
    CFRelease(cg_font);
    CFRelease(data_provider);
  } else {
    CFStringRef name_ref = CFStringCreateWithCString(
        nullptr, owning_font.GetName().c_str(), kCFStringEncodingUTF8);
    coretext_font_ = CTFontCreateWithName(name_ref, size_in_pixels, nullptr);
    CFRelease(name_ref);
  }
}

CoreTextFont::Helper::~Helper() {
  CFRelease(coretext_font_);
  CFRelease(path_);

  for (auto it = font_to_index_map_.begin(); it != font_to_index_map_.end(); ++it) {
    CFRelease(it->first);
  }
}

CTFrameRef CoreTextFont::Helper::CreateFrame(
    const std::string& text, HorizontalAlignment horizontal_alignment, float line_spacing) const {
  // Create a CFAttributedString from |string|, with the current font.
  CFStringRef cf_string = CFStringCreateWithBytes(
      nullptr, reinterpret_cast<const UInt8 *>(text.data()), text.size(),
      kCFStringEncodingUTF8, false);
  if (!cf_string) {
    LOG(ERROR) << "CreateFrame failed on: " << text;
    return nullptr;
  }
  NSMutableParagraphStyle* paragraph_style =
      [[NSMutableParagraphStyle alloc] init];
  paragraph_style.lineHeightMultiple = line_spacing;
#if defined(ION_PLATFORM_MAC)
  switch (horizontal_alignment) {
    case kAlignLeft:
      paragraph_style.alignment = NSLeftTextAlignment;
      break;
    case kAlignHCenter:
      paragraph_style.alignment = NSCenterTextAlignment;
      break;
    case kAlignRight:
      paragraph_style.alignment = NSRightTextAlignment;
      break;
  }
#else  // defined(ION_PLATFORM_MAC)
  switch (horizontal_alignment) {
    case kAlignLeft:
      paragraph_style.alignment = NSTextAlignmentLeft;
      break;
    case kAlignHCenter:
      paragraph_style.alignment = NSTextAlignmentCenter;
      break;
    case kAlignRight:
      paragraph_style.alignment = NSTextAlignmentRight;
      break;
  }
#endif  // defined(ION_PLATFORM_MAC)
  NSDictionary* attributes =
      @{ NSFontAttributeName: (__bridge OBJC_FONT *)coretext_font_,
         NSParagraphStyleAttributeName: paragraph_style };

  CFAttributedStringRef attrString =
      CFAttributedStringCreate(nullptr, cf_string, (CFDictionaryRef)attributes);
  CFRelease(cf_string);

  // Create a CTFrame; this is the step that lays out the glyphs.
  CTFramesetterRef framesetter =
      CTFramesetterCreateWithAttributedString(attrString);
  CFRelease(attrString);
  CTFrameRef frame = CTFramesetterCreateFrame(
      framesetter, CFRangeMake(0, 0), path_, nullptr);
  CFRelease(framesetter);
  return frame;
}

const Font::FontMetrics CoreTextFont::Helper::GetFontMetrics(
    size_t size_in_pixels) const {
  FontMetrics metrics;

  // Calculation of font metrics as per http://goo.gl/MleUbS.
  // 
  // be verified more; it may apply only to Mac.
  CGFloat ascent = CTFontGetAscent(coretext_font_);
  CGFloat descent = CTFontGetDescent(coretext_font_);
  CGFloat leading = CTFontGetLeading(coretext_font_);

  if (leading < 0)
    leading = 0;

  leading = static_cast<CGFloat>(floor(leading + 0.5));
  const float ascender = static_cast<float>(floor(ascent + 0.5));
  const float global_glyph_height = static_cast<float>(ascender + floor(descent + 0.5));
  metrics.line_advance_height = static_cast<float>(global_glyph_height + leading);
  // Some fonts do not contain the correct ascender or descender values, but
  // instead only the maximum and minimum y values, which will exceed the size.
  // To handle these cases, approximate the ascender with the ratio of ascender
  // to (ascender + descender) and scale by size.
  metrics.ascender = static_cast<float>(ascender * size_in_pixels / global_glyph_height);
  return metrics;
}

bool CoreTextFont::Helper::LoadGlyphGrid(GlyphIndex glyph_index,
                                         GlyphGrid* glyph_grid) {
  CGGlyph glyph = GlyphIndexToCGGlyph(glyph_index);
  CGRect bounds;
  CTFontRef font = FontIndexToFont(GlyphIndexToFontIndex(glyph_index));
  CTFontGetBoundingRectsForGlyphs(
      font, kCTFontDefaultOrientation, &glyph, &bounds, 1);
  const size_t pixel_width = static_cast<size_t>(ceil(CGRectGetWidth(bounds)));
  const size_t pixel_height =
      static_cast<size_t>(ceil(CGRectGetHeight(bounds)));

  if (pixel_width != 0 && pixel_height != 0) {
    // Render the glyph.

    // Create a CGContext backed by an array of bytes.
    std::unique_ptr<uint8[]> bitmapData(new uint8[pixel_height * pixel_width]);
    memset(bitmapData.get(), 0, pixel_height * pixel_width);
    CGContextRef cg_context = CGBitmapContextCreate(
        bitmapData.get(), pixel_width, pixel_height,
        8, pixel_width, nullptr, kCGImageAlphaOnly);
    CGContextSetTextMatrix(cg_context, CGAffineTransformIdentity);

    // Render the glyph into the above CGContext.
    CGPoint point = CGPointMake(-CGRectGetMinX(bounds), -CGRectGetMinY(bounds));
    CTFontDrawGlyphs(font, &glyph, &point, 1, cg_context);

    // Inspect the array of bytes to calculate the contents of the GlyphData's
    // grid.
    static const double kScale = 1.0 / 255.0;
    glyph_grid->pixels = base::Array2<double>(pixel_width, pixel_height);
    for (size_t x = 0; x < pixel_width; x++) {
      for (size_t y = 0; y < pixel_height; y++) {
        glyph_grid->pixels.Set(
            x, y, (double)bitmapData[x + y * pixel_width] * kScale);
      }
    }

    CGContextRelease(cg_context);
  }
  return true;
}

GlyphIndex CoreTextFont::Helper::GetDefaultGlyphForChar(CharIndex char_index) {
  // Note that this only supports 16-bit unicode and also uses
  // CTFontGetGlyphsForCharacters which does not provide font fallback.
  // However, this should be sufficient for its use as documented in the base
  // class.
  CGGlyph glyph;
  unichar un = (unichar)char_index;
  uint16 font_index = FontToFontIndex(coretext_font_);
  return CTFontGetGlyphsForCharacters(coretext_font_, &un, &glyph, 1) ?
      CGGlyphFontIndexToGlyphIndex(glyph, font_index) : 0;
}

const Layout CoreTextFont::Helper::BuildLayout(const std::string& text,
                                               const LayoutOptions& options,
                                               const Font* font) {
  CTFrameRef frame = CreateFrame(text, options.horizontal_alignment, options.line_spacing);
  Layout layout;
  if (frame == nil || !IsSizeValid(options.target_size)) {
    return layout;
  }
  CFArrayRef lines = CTFrameGetLines(frame);
  const CFIndex line_count = CFArrayGetCount(lines);
  std::unique_ptr<CGPoint[]> line_origins(new CGPoint[line_count]);
  CTFrameGetLineOrigins(frame, CFRangeMake(0, line_count), line_origins.get());

  Transform layout_options_transform = CalculateLayoutOptionsTransform(
      lines, line_count, line_origins.get(), options, font);
  layout.SetLineAdvanceHeight(
      font->GetFontMetrics().line_advance_height *
      static_cast<float>(layout_options_transform.scale[1]) *
      options.line_spacing);
  layout.SetPosition(layout_options_transform.position);
  layout.SetSize(layout_options_transform.size);

  for (CFIndex line_index = 0; line_index < line_count; ++line_index) {
    CTLineRef line = (CTLineRef)CFArrayGetValueAtIndex(lines, line_index);
    CFArrayRef runs = CTLineGetGlyphRuns(line);

    const CFIndex run_count = CFArrayGetCount(runs);
    for (CFIndex run_index = 0; run_index < run_count; ++run_index) {
      CTRunRef run = (CTRunRef)CFArrayGetValueAtIndex(runs, run_index);

      // Check if this run is the correct font.
      NSDictionary* run_attributes = (NSDictionary*)CTRunGetAttributes(run);
      CTFontRef run_font =
          (__bridge CTFontRef)[run_attributes objectForKey:NSFontAttributeName];
      uint16 font_index = FontToFontIndex(run_font);

      const CFIndex glyph_count = CTRunGetGlyphCount(run);

      std::unique_ptr<CGPoint[]> positions(new CGPoint[glyph_count]);
      CTRunGetPositions(run, CFRangeMake(0, glyph_count), positions.get());

      std::unique_ptr<CGGlyph[]> glyphs(new CGGlyph[glyph_count]);
      CTRunGetGlyphs(run, CFRangeMake(0, glyph_count), glyphs.get());

      std::unique_ptr<CGRect[]> bounds(new CGRect[glyph_count]);
      CTFontGetBoundingRectsForGlyphs(run_font, kCTFontDefaultOrientation,
                                      glyphs.get(), bounds.get(), glyph_count);

      for (CFIndex glyph_index = 0; glyph_index < glyph_count; ++glyph_index) {
        AddGlyphToLayout(glyphs[glyph_index],
                         font_index,
                         layout_options_transform,
                         bounds[glyph_index],
                         positions[glyph_index],
                         line_origins[line_index],
                         font->GetSdfPadding(),
                         &layout);
      }
    }
  }
  CFRelease(frame);

  return layout;
}

std::string CoreTextFont::Helper::GetCTFontName() const {
  return [CFBridgingRelease(CTFontCopyFullName(coretext_font_)) UTF8String];
}

uint16 CoreTextFont::Helper::FontToFontIndex(CTFontRef font) {
  std::lock_guard<std::mutex> guard(mutex_);
  auto existing = font_to_index_map_.find(font);
  if (existing == font_to_index_map_.end()) {
    // It is assumed that there would never be enough fallback fonts mapped to
    // cause font_index to overflow a uint16.
    CHECK_LT(font_to_index_map_.size(), kuint16max);
    uint16 font_index = static_cast<uint16>(font_to_index_map_.size());
    font_to_index_map_[font] = font_index;
    index_to_font_map_[font_index] = font;
    CFRetain((CTFontRef)font);
    return font_index;
  }
  return existing->second;
}

CTFontRef CoreTextFont::Helper::FontIndexToFont(uint16 font_index) {
  std::lock_guard<std::mutex> guard(mutex_);
  return index_to_font_map_[font_index];
}

void CoreTextFont::Helper::AddFallbackFont(const FontPtr& fallback) {
  LOG(WARNING) << "Fallback support for CoreTextFont is not implemented";
}

//-----------------------------------------------------------------------------
//
// CoreTextFont functions.
//
//-----------------------------------------------------------------------------

CoreTextFont::CoreTextFont(
    const std::string& name, size_t size_in_pixels, size_t sdf_padding,
    const void* data, size_t data_size)
    : Font(name, size_in_pixels, sdf_padding),
      helper_(new Helper(*this, data, data_size)) {
  SetFontMetrics(helper_->GetFontMetrics(size_in_pixels));
}

CoreTextFont::~CoreTextFont() {}

bool CoreTextFont::LoadGlyphGrid(GlyphIndex glyph_index,
                                 GlyphGrid* glyph_grid) const {
  return helper_->LoadGlyphGrid(glyph_index, glyph_grid);
}

GlyphIndex CoreTextFont::GetDefaultGlyphForChar(CharIndex char_index) const {
  return helper_->GetDefaultGlyphForChar(char_index);
}

const Layout CoreTextFont::BuildLayout(const std::string& text,
                                       const LayoutOptions& options) const {
  return helper_->BuildLayout(text, options, this);
}

std::string CoreTextFont::GetCTFontName() const {
  return helper_->GetCTFontName();
}

void CoreTextFont::AddFallbackFont(const FontPtr& fallback) {
  helper_->AddFallbackFont(fallback);
}

#undef OBJC_FONT

}  // namespace text
}  // namespace ion
