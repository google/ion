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

#ifndef ION_DEMOS_HUD_H_
#define ION_DEMOS_HUD_H_

#include <memory>

#include "ion/gfx/node.h"
#include "ion/gfxutils/shadermanager.h"
#include "ion/math/vector.h"
#include "ion/text/fontmanager.h"
#include "ion/text/layout.h"

// This class implements a very simple HUD (heads-up display) for Ion demos.
// Right now it provides just a simple frames-per-second display.
class Hud {
 public:
  // This struct defines a region in which to display text in the HUD; It adds
  // a ResizePolicy to the standard ion::text::LayoutOptions.
  //
  // Every piece of text added to the HUD requires its own region. Region
  // coordinates are specified in normalized window coordinates, which range
  // from 0 to 1 in X and Y. The view is orthographic along the -Z axis.
  struct TextRegion : public ion::text::LayoutOptions {
    // This enum specifies how a region responds to resizing of the HUD window.
    enum ResizePolicy {
      kFixedSize,     // Region stays the same size when the HUD is resized.
      kRelativeSize,  // Region resizes when the HUD is resized.
    };

    TextRegion() : resize_policy(kRelativeSize) {
      // Override default LayoutOptions alignment.
      horizontal_alignment = ion::text::kAlignHCenter;
      vertical_alignment = ion::text::kAlignVCenter;
    }

    // How the region responds to window resizing.
    ResizePolicy resize_policy;
  };

  Hud(const ion::text::FontManagerPtr& font_manager,
      const ion::gfxutils::ShaderManagerPtr& shader_manager,
      int width, int height);
  virtual ~Hud();

  // Initializes the display of frames-per-second text.
  void InitFps(int num_integral_digits, int num_fractional_digits,
               const TextRegion& region);

  // Enables or disables the display of frames-per-second text.
  void EnableFps(bool enable);
  bool IsFpsEnabled() const;

  // Tells the HUD the current window size in pixels.
  void Resize(int width, int height);

  // This must be called every frame for the fps display to work.
  void Update();

  // Returns the root node for the HUD graph. Render this node to show the HUD.
  const ion::gfx::NodePtr& GetRootNode() const { return root_; }

 private:
  // Nested class that manages fps calculation.
  class FpsHelper;
  // Nested class that manages text rendering.
  class TextHelper;

  // Root node of the HUD graph.
  ion::gfx::NodePtr root_;

  std::unique_ptr<FpsHelper> fps_helper_;
  std::unique_ptr<TextHelper> text_helper_;

  // TextHelper ID for the FPS text.
  size_t fps_text_id_;
};

#endif  // ION_DEMOS_HUD_H_
