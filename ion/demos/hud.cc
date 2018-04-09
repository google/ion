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

#include "ion/demos/hud.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <vector>

#include "ion/base/invalid.h"
#include "ion/base/logging.h"
#include "ion/base/zipassetmanager.h"
#include "ion/base/zipassetmanagermacros.h"
#include "ion/demos/utils.h"
#include "ion/gfx/bufferobject.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfx/statetable.h"
#include "ion/gfxutils/shadermanager.h"
#include "ion/math/matrix.h"
#include "ion/math/range.h"
#include "ion/math/transformutils.h"
#include "ion/port/timer.h"
#include "ion/text/basicbuilder.h"
#include "ion/text/font.h"
#include "ion/text/fontimage.h"
#include "ion/text/layout.h"

// Resources for the HUD.
ION_REGISTER_ASSETS(IonDemoHud);

//-----------------------------------------------------------------------------
//
// Helper functions.
//
//-----------------------------------------------------------------------------

// Builds the root node for the HUD.
static const ion::gfx::NodePtr BuildHudRootNode(int width, int height) {
  ion::gfx::NodePtr node(new ion::gfx::Node);

  // Set an orthographic projection matrix and identity modelview matrix.
  const ion::gfx::ShaderInputRegistryPtr& global_reg =
      ion::gfx::ShaderInputRegistry::GetGlobalRegistry();
  const ion::math::Matrix4f proj =
      ion::math::OrthographicMatrixFromFrustum(0.0f, 1.0f, 0.0f, 1.0f,
                                               -1.0f, 1.0f);
  demoutils::AddUniformToNode(global_reg, "uProjectionMatrix", proj, node);
  demoutils::AddUniformToNode(global_reg, "uModelviewMatrix",
                              ion::math::Matrix4f::Identity(), node);

  ion::gfx::StateTablePtr state_table(new ion::gfx::StateTable);
  state_table->SetViewport(
      ion::math::Range2i::BuildWithSize(ion::math::Point2i(0, 0),
                                        ion::math::Vector2i(width, height)));
  node->SetStateTable(state_table);

  return node;
}

//-----------------------------------------------------------------------------
//
// Hud::FpsHelper class. This is used to compute frames per second and convert
// the result to a string.
//
//-----------------------------------------------------------------------------

class Hud::FpsHelper {
 public:
  FpsHelper();

  // Initializes the instance with specific precision values. By default, the
  // values are 6 and 3.
  void SetPrecision(int num_integral_digits, int num_fractional_digits);

  // Updates the FPS calculation for a new frame and returns the new FPS value
  // if at least 1 second has passed since the last time it was returned.
  // Otherwise, it returns 0.
  double GetFps();

  // Returns a text string representing the given FPS value.
  const std::string GetText(double fps) const;

  // Resets the helper to its pre-Update() state.
  void Reset() {
    timer_active_ = false;
    frames_since_last_report_ = 0;
  }

 private:
  // FPS text display variables.
  int num_integral_digits_;
  int num_fractional_digits_;

  // FPS calculation variables.
  ion::port::Timer timer_;
  bool timer_active_;
  uint32 frames_since_last_report_;
};

Hud::FpsHelper::FpsHelper()
    : num_integral_digits_(6),
      num_fractional_digits_(3),
      timer_active_(false),
      frames_since_last_report_(0) {}

void Hud::FpsHelper::SetPrecision(int num_integral_digits,
                                  int num_fractional_digits) {
  num_integral_digits_ = num_integral_digits;
  num_fractional_digits_ = num_fractional_digits;
}

double Hud::FpsHelper::GetFps() {
  double fps = 0.0;
  if (!timer_active_) {
    // This is the very first call to GetFps() since the instance was created or
    // reset, so don't accumulate the time.
    timer_active_ = true;
    timer_.Reset();
  } else {
    // Accumulate one frame and see if the timer is past 1 second.
    frames_since_last_report_++;
    const double time = timer_.GetInS();
    if (time >= 1.0) {
      fps = frames_since_last_report_ / time;
      frames_since_last_report_ = 0;
      timer_.Reset();
    }
  }
  return fps;
}

const std::string Hud::FpsHelper::GetText(double fps) const {
  std::string text;
  if (fps <= 0.0) {
    // Use stand-in characters until a real FPS value is computed.
    text = std::string(num_integral_digits_, '*') + '.' +
           std::string(num_fractional_digits_, '*');
  } else {
    char s[32];
    snprintf(s, sizeof(s), "%*.*f",
             num_integral_digits_ + num_fractional_digits_,
             num_fractional_digits_, fps);
    text = s;
  }
  return text;
}

//-----------------------------------------------------------------------------
//
// Hud::TextHelper class.
//
//-----------------------------------------------------------------------------

class Hud::TextHelper {
 public:
  TextHelper(const ion::text::FontManagerPtr& font_manager,
             const ion::gfxutils::ShaderManagerPtr& shader_manager,
             int width, int height);

  // Initializes a font, returning a pointer to a Font. Logs a message and
  // returns a null pointer on error.
  ion::text::FontPtr InitFont(const std::string& font_name,
                              size_t size_in_pixels, size_t sdf_padding) {
    return demoutils::InitFont(font_manager_, font_name,
                               size_in_pixels, sdf_padding);
  }

  // Initializes and returns a StaticFontImage that uses the given Font and
  // GlyphSet. It caches it in the FontManager so that subsequent calls
  // with the same key use the same instance.
  const ion::text::FontImagePtr InitFontImage(
      const std::string& key, const ion::text::FontPtr& font,
      const ion::text::GlyphSet& glyph_set);

  // Adds a text string. The returned ID is used to identify the text in
  // subsequent calls to the TextHelper. Returns kInvalidIndex on error.
  size_t AddText(const ion::text::FontImagePtr& font_image,
                 const TextRegion& region, const std::string& text);

  // Returns the Node for a given ID, or NULL if the ID is not valid.
  const ion::gfx::NodePtr GetNode(size_t id) {
    return id < specs_.size() ? specs_[id].node : ion::gfx::NodePtr();
  }

  // Responds to a window resize, depending on each text string's ResizePolicy.
  void Resize(int width, int height);

  // Enables the Node for the text with the given ID.
  void EnableText(size_t id, bool enable) {
    if (id < specs_.size())
      specs_[id].node->Enable(enable);
  }

  // Returns whether the Node for the text with the given ID is enabled.
  bool IsTextEnabled(size_t id) const {
    return id < specs_.size() && specs_[id].node->IsEnabled();
  }

  // Modifies the text with the given ID to display a new text string..
  void ChangeText(size_t id, const std::string& new_text);

 private:
  // Stores information needed to process each text string.
  struct TextSpec {
    Hud::TextRegion region;
    std::string text;
    ion::text::BasicBuilderPtr builder;
    ion::gfx::NodePtr node;
  };

  // FontManager used for initializing fonts.
  ion::text::FontManagerPtr font_manager_;

  // ShaderManager used for composing text shaders.
  ion::gfxutils::ShaderManagerPtr shader_manager_;

  // Width and height of the HUD, which are used to manage fixed-size regions.
  int width_;
  int height_;

  // Data for each text string added.
  std::vector<TextSpec> specs_;
};

Hud::TextHelper::TextHelper(
    const ion::text::FontManagerPtr& font_manager,
    const ion::gfxutils::ShaderManagerPtr& shader_manager,
    int width, int height)
    : font_manager_(font_manager),
      shader_manager_(shader_manager),
      width_(width),
      height_(height) {
  if (!IonDemoHud::RegisterAssets()) {
    LOG(ERROR) << "Unable to register HUD assets";
  }
}

const ion::text::FontImagePtr Hud::TextHelper::InitFontImage(
    const std::string& key, const ion::text::FontPtr& font,
    const ion::text::GlyphSet& glyph_set) {
  ion::text::FontImagePtr font_image = font_manager_->GetCachedFontImage(key);
  if (!font_image.Get()) {
    ion::text::StaticFontImagePtr sfi(
        new ion::text::StaticFontImage(font, 256U, glyph_set));
    if (!sfi->GetImageData().texture.Get()) {
      LOG(ERROR) << "Unable to create HUD FontImage";
    } else {
      font_manager_->CacheFontImage(key, sfi);
      font_image = sfi;
    }
  }
  return font_image;
}

size_t Hud::TextHelper::AddText(const ion::text::FontImagePtr& font_image,
                                const TextRegion& region,
                                const std::string& text) {
  size_t id = ion::base::kInvalidIndex;
  if (font_image.Get()) {
    ion::text::BasicBuilderPtr builder(new ion::text::BasicBuilder(
        font_image, shader_manager_, ion::base::AllocatorPtr()));
    const ion::text::Layout layout =
        font_image->GetFont()->BuildLayout(text, region);
    if (builder->Build(layout, ion::gfx::BufferObject::kStreamDraw)) {
      TextSpec text_spec;
      text_spec.region = region;
      text_spec.text = text;
      text_spec.builder = builder;
      text_spec.node = builder->GetNode();
      id = specs_.size();
      specs_.push_back(text_spec);
    }
  }
  return id;
}

void Hud::TextHelper::Resize(int width, int height) {
  // Update the sizes in all fixed-size regions.
  const size_t num_specs = specs_.size();
  for (size_t i = 0; i < num_specs; ++i) {
    TextSpec& spec = specs_[i];
    TextRegion& region = spec.region;
    // Scale the target size to compensate for the new size. However, some
    // platforms (e.g., Android) may be initialized with a zero size, so do not
    // scale in that case.
    if (region.resize_policy == TextRegion::kFixedSize && width_ && height_) {
      region.target_size[0] *= static_cast<float>(width_) /
                               static_cast<float>(width);
      region.target_size[1] *= static_cast<float>(height_) /
                               static_cast<float>(height);
    }
    ChangeText(i, spec.text);
  }
  width_ = width;
  height_ = height;
}

void Hud::TextHelper::ChangeText(size_t id, const std::string& new_text) {
  if (id < specs_.size()) {
    TextSpec& spec = specs_[id];
    spec.builder->Build(
        spec.builder->GetFont()->BuildLayout(new_text, spec.region),
        ion::gfx::BufferObject::kStreamDraw);
    spec.text = new_text;
  }
}

//-----------------------------------------------------------------------------
//
// Hud class functions.
//
//-----------------------------------------------------------------------------

Hud::Hud(const ion::text::FontManagerPtr& font_manager,
         const ion::gfxutils::ShaderManagerPtr& shader_manager,
         int width, int height)
    : root_(BuildHudRootNode(width, height)),
      fps_helper_(new FpsHelper),
      text_helper_(new TextHelper(font_manager, shader_manager, width, height)),
      fps_text_id_(ion::base::kInvalidIndex) {}

Hud::~Hud() {}

void Hud::InitFps(int num_integral_digits, int num_fractional_digits,
                  const TextRegion& region) {
  ion::text::FontPtr font = text_helper_->InitFont("Hud", 32U, 4U);
  if (font.Get()) {
    fps_helper_->SetPrecision(num_integral_digits, num_fractional_digits);
    // Create a StaticFontImage using only the characters needed for the FPS
    // text.
    ion::text::GlyphSet glyph_set(ion::base::AllocatorPtr(nullptr));
    font->AddGlyphsForAsciiCharacterRange('0', '9', &glyph_set);
    font->AddGlyphsForAsciiCharacterRange('*', '*', &glyph_set);
    font->AddGlyphsForAsciiCharacterRange('.', '.', &glyph_set);
    ion::text::FontImagePtr font_image(
        text_helper_->InitFontImage("HUD FPS", font, glyph_set));

    fps_text_id_ = text_helper_->AddText(font_image, region,
                                         fps_helper_->GetText(0.0));
    if (fps_text_id_ == ion::base::kInvalidIndex) {
      LOG(ERROR) << "Unable to add FPS text to HUD";
    } else {
      // Disable FPS display by default.
      text_helper_->EnableText(fps_text_id_, false);
      root_->AddChild(text_helper_->GetNode(fps_text_id_));
    }
  }
}

void Hud::EnableFps(bool enable) {
  if (!enable)
    fps_helper_->Reset();
  if (fps_text_id_ != ion::base::kInvalidIndex) {
    // If enabling, make sure the invalid-FPS text is displayed.
    if (enable)
      text_helper_->ChangeText(fps_text_id_, fps_helper_->GetText(0.0));
    text_helper_->EnableText(fps_text_id_, enable);
  }
}

bool Hud::IsFpsEnabled() const {
  return fps_text_id_ != ion::base::kInvalidIndex &&
      text_helper_->IsTextEnabled(fps_text_id_);
}

void Hud::Resize(int width, int height) {
  if (fps_text_id_ != ion::base::kInvalidIndex)
    text_helper_->Resize(width, height);
  root_->GetStateTable()->SetViewport(
      ion::math::Range2i::BuildWithSize(ion::math::Point2i(0, 0),
                                        ion::math::Vector2i(width, height)));
}

void Hud::Update() {
  if (IsFpsEnabled()) {
    double fps = fps_helper_->GetFps();
    if (fps != 0.0)
      text_helper_->ChangeText(fps_text_id_, fps_helper_->GetText(fps));
  }
}
