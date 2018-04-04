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

#ifndef ION_DEMOS_VIEWERDEMOBASE_H_
#define ION_DEMOS_VIEWERDEMOBASE_H_

#include <memory>
#include <vector>

#include "ion/demos/demobase.h"
#include "ion/gfx/graphicsmanager.h"
#include "ion/gfx/node.h"
#include "ion/gfx/renderer.h"
#include "ion/gfxutils/frame.h"
#include "ion/gfxutils/shadermanager.h"
#include "ion/math/angle.h"
#include "ion/math/matrix.h"
#include "ion/math/vector.h"

//-----------------------------------------------------------------------------
//
// The ViewerDemoBase class can be used as a base class for any demo that does
// interactive trackball-type viewing. It processes all mouse motion to update
// the view. It can also set up all supported remote handlers.
//
//-----------------------------------------------------------------------------

class ViewerDemoBase : public DemoBase {
 public:
  ~ViewerDemoBase() override;

  //---------------------------------------------------------------------------
  // DemoBase API implementation.

  // Implements these to manage view changes.
  void ProcessMotion(float x, float y, bool is_press) override;
  void ProcessScale(float scale) override;

  // Implements this to maintain the proper width and height for viewing.
  void Resize(int width, int height) override;

  // Implements this to call Begin() and End() on the Frame, then invoke
  // DoRender().
  void Render() override;

 protected:
  // The constructor is passed the initial width and height of the viewport.
  ViewerDemoBase(int viewport_width, int viewport_height);

  // Derived classes must define this to implement rendering of a frame.
  virtual void RenderFrame() = 0;

  //---------------------------------------------------------------------------
  // Manager and handler state.

  // Sets up all supported remote handlers. A vector of nodes for the
  // NodeGraphHandler to track must be supplied.
  void InitRemoteHandlers(const std::vector<ion::gfx::NodePtr>& nodes_to_track);

  // Returns the Frame set up by the constructor.
  const ion::gfxutils::FramePtr& GetFrame() const { return frame_; }

  // Returns the GraphicsManager set up by the constructor.
  const ion::gfx::GraphicsManagerPtr& GetGraphicsManager() const {
    return graphics_manager_;
  }

  // Returns the Renderer set up by the constructor.
  const ion::gfx::RendererPtr& GetRenderer() const { return renderer_; }

  // Returns the ShaderManager set up by the constructor.
  const ion::gfxutils::ShaderManagerPtr& GetShaderManager() const {
    return shader_manager_;
  }

  //---------------------------------------------------------------------------
  // Viewing operations.

  // Returns the current viewport size.
  const ion::math::Vector2i& GetViewportSize() const { return viewport_size_; }

  // Sets/returns the radius defining the size of the trackball. It is 1 by
  // default.
  void SetTrackballRadius(float radius) { trackball_radius_ = radius; }
  float GetTrackballRadius() const { return trackball_radius_; }

  // Sets/returns the tilt and rotation angles. They are both 30 degrees by
  // default.
  const ion::math::Anglef GetTiltAngle() const;
  const ion::math::Anglef GetRotationAngle() const;
  void SetTiltAngle(const ion::math::Anglef& angle);
  void SetRotationAngle(const ion::math::Anglef& angle);

  // Derived classes must call this function to add the uniforms related to
  // viewing parameters to the given node, which must not be NULL. This class
  // assumes the node to be persistent; call this again if the node is
  // replaced.
  void SetNodeWithViewUniforms(const ion::gfx::NodePtr& node);

  // This is called to update the uniforms in the node passed to
  // SetNodeWithViewUniforms() based on the current view. Derived classes may
  // redefine it to also add their own view processing.
  virtual void UpdateViewUniforms();

  // Returns the current projection or modelview matrix. Returns an identity
  // matrix if SetNodeWithViewUniforms() was not called with a valid node
  // pointer.
  const ion::math::Matrix4f GetProjectionMatrix() const {
    return GetMatrixFromUniform(kProjectionMatrixIndex);
  }
  const ion::math::Matrix4f GetModelviewMatrix() const {
    return GetMatrixFromUniform(kModelviewMatrixIndex);
  }

 private:
  enum UniformIndex {
    kViewportSizeIndex,
    kProjectionMatrixIndex,
    kModelviewMatrixIndex,
    kCameraPositionIndex,
    kNumIndices
  };

  const ion::math::Matrix4f GetMatrixFromUniform(UniformIndex which) const;

  ion::gfxutils::FramePtr frame_;
  ion::gfx::GraphicsManagerPtr graphics_manager_;
  ion::gfx::RendererPtr renderer_;
  ion::gfxutils::ShaderManagerPtr shader_manager_;

  ion::math::Vector2i viewport_size_;
  float trackball_radius_;

  // This struct maintains all the necessary viewing parameters.
  struct ViewInfo;
  std::unique_ptr<ViewInfo> view_info_;

  // Node containing the view uniforms.
  ion::gfx::NodePtr view_node_;

  // Indices of uniforms in view_node_.
  size_t uniform_indices_[kNumIndices];
};

#endif  // ION_DEMOS_VIEWERDEMOBASE_H_
