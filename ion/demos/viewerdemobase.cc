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

#include "ion/demos/viewerdemobase.h"

#include "ion/base/logging.h"
#include "ion/demos/utils.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/math/matrix.h"
#include "ion/math/matrixutils.h"
#include "ion/math/transformutils.h"
#include "ion/math/utils.h"
#include "absl/memory/memory.h"

// Production builds will typically not need the Remote library, as it is mostly
// intended for on-the-fly debugging and development.
#define ION_ENABLE_REMOTE (!ION_PRODUCTION && !ION_SCUBA)

#if ION_ENABLE_REMOTE
#include "ion/remote/calltracehandler.h"
#include "ion/remote/nodegraphhandler.h"
#include "ion/remote/remoteserver.h"
#include "ion/remote/resourcehandler.h"
#include "ion/remote/settinghandler.h"
#include "ion/remote/shaderhandler.h"
#include "ion/remote/tracinghandler.h"
#endif

using ion::math::Anglef;
using ion::math::Matrix4f;
using ion::math::Point2f;
using ion::math::Point3f;
using ion::math::Vector2f;
using ion::math::Vector3f;

//-----------------------------------------------------------------------------
//
// The ViewerDemoBase::ViewInfo struct maintains all the data necessary for
// interactive viewing.
//
//-----------------------------------------------------------------------------

struct ViewerDemoBase::ViewInfo {
  ViewInfo()
      // See UpdateViewUniforms() for why 60 degrees is used.
      : field_of_view_angle(Anglef::FromDegrees(60.0f)),
        tilt_angle(Anglef::FromDegrees(30.0f)),
        rotation_angle(Anglef::FromDegrees(30.0f)),
        last_mouse_pos(Vector2f::Zero()) {}

  Anglef field_of_view_angle;
  Anglef tilt_angle;
  Anglef rotation_angle;
  Vector2f last_mouse_pos;
};

//-----------------------------------------------------------------------------
//
// ViewerDemoBase functions.
//
//-----------------------------------------------------------------------------

ViewerDemoBase::ViewerDemoBase(int viewport_width, int viewport_height)
    : frame_(new ion::gfxutils::Frame),
      graphics_manager_(new ion::gfx::GraphicsManager),
      renderer_(new ion::gfx::Renderer(graphics_manager_)),
      shader_manager_(new ion::gfxutils::ShaderManager()),
      viewport_size_(viewport_width, viewport_height),
      trackball_radius_(1.0f),
      view_info_(new ViewInfo) {}

ViewerDemoBase::~ViewerDemoBase() {}

void ViewerDemoBase::ProcessMotion(float x, float y, bool is_press) {
  const Vector2f new_pos(x, y);
  if (!is_press) {
    static const Anglef kRotationAngle = Anglef::FromDegrees(.25f);
    const Vector2f delta = new_pos - view_info_->last_mouse_pos;
    view_info_->rotation_angle += delta[0] * kRotationAngle;
    view_info_->tilt_angle += delta[1] * kRotationAngle;
    UpdateViewUniforms();
  }
  view_info_->last_mouse_pos = new_pos;
}

void ViewerDemoBase::ProcessScale(float scale) {
  // Set the base field of view to a reasonable angle based on the radius.
  static const float kBaseViewAngle = 60.0f;
  static const float kMinViewAngle = 2.0f;
  static const float kMaxViewAngle = 175.0f;
  view_info_->field_of_view_angle = Anglef::FromDegrees(
      ion::math::Clamp(scale * kBaseViewAngle, kMinViewAngle, kMaxViewAngle));
  UpdateViewUniforms();
}

void ViewerDemoBase::Resize(int width, int height) {
  viewport_size_.Set(width, height);
  UpdateViewUniforms();
}

void ViewerDemoBase::Render() {
  frame_->Begin();
  RenderFrame();
  frame_->End();
}

void ViewerDemoBase::InitRemoteHandlers(
    const std::vector<ion::gfx::NodePtr>& nodes_to_track) {
#if ION_ENABLE_REMOTE

#if defined(ION_PLATFORM_ASMJS) || defined(ION_PLATFORM_NACL)
  remote_ = absl::make_unique<ion::remote::RemoteServer>(0);
  remote_->SetEmbedLocalSourcedFiles(true);
#else
  remote_ = absl::make_unique<ion::remote::RemoteServer>(1234);
#endif
  ion::remote::NodeGraphHandlerPtr ngh(new ion::remote::NodeGraphHandler);
  ngh->SetFrame(frame_);
  for (size_t i = 0; i < nodes_to_track.size(); ++i)
    ngh->AddNode(nodes_to_track[i]);
  remote_->RegisterHandler(ngh);
  remote_->RegisterHandler(
      ion::remote::HttpServer::RequestHandlerPtr(
          new ion::remote::CallTraceHandler()));
  remote_->RegisterHandler(
      ion::remote::HttpServer::RequestHandlerPtr(
          new ion::remote::ResourceHandler(renderer_)));
  remote_->RegisterHandler(
      ion::remote::HttpServer::RequestHandlerPtr(
          new ion::remote::SettingHandler()));
  remote_->RegisterHandler(
      ion::remote::HttpServer::RequestHandlerPtr(
          new ion::remote::ShaderHandler(shader_manager_, renderer_)));
  remote_->RegisterHandler(
      ion::remote::HttpServer::RequestHandlerPtr(
          new ion::remote::TracingHandler(frame_, renderer_)));

#endif
}

const ion::math::Anglef ViewerDemoBase::GetTiltAngle() const {
  return view_info_->tilt_angle;
}

const ion::math::Anglef ViewerDemoBase::GetRotationAngle() const {
  return view_info_->rotation_angle;
}

void ViewerDemoBase::SetTiltAngle(const ion::math::Anglef& angle) {
  view_info_->tilt_angle = angle;
}

void ViewerDemoBase::SetRotationAngle(const ion::math::Anglef& angle) {
  view_info_->rotation_angle = angle;
}

void ViewerDemoBase::SetNodeWithViewUniforms(const ion::gfx::NodePtr& node) {
  if (!node.Get()) {
    LOG(ERROR) << "NULL node passed to ViewerDemoBase::SetNodeWithUniforms().";
    return;
  }
  const ion::gfx::ShaderInputRegistryPtr& global_reg =
      ion::gfx::ShaderInputRegistry::GetGlobalRegistry();
  uniform_indices_[kViewportSizeIndex] =
      demoutils::AddUniformToNode(global_reg, "uViewportSize",
                                  viewport_size_, node);
  uniform_indices_[kProjectionMatrixIndex] =
      demoutils::AddUniformToNode(global_reg, "uProjectionMatrix",
                                  Matrix4f::Identity(), node);
  uniform_indices_[kModelviewMatrixIndex] =
      demoutils::AddUniformToNode(global_reg, "uModelviewMatrix",
                                  Matrix4f::Identity(), node);
  uniform_indices_[kCameraPositionIndex] =
      demoutils::AddUniformToNode(global_reg, "uCameraPosition",
                                  Vector3f::Zero(), node);
  view_node_ = node;
}

void ViewerDemoBase::UpdateViewUniforms() {
  if (!view_node_.Get()) {
    LOG(ERROR) << "ViewerDemoBase::UpdateViewUniforms() called with NULL node.";
    return;
  }

  // Viewport size.
  demoutils::SetUniformInNode(uniform_indices_[kViewportSizeIndex],
                              viewport_size_, view_node_);

  //
  // The projection and modelview matrices are set up to view a sphere with the
  // specified radius centered at the origin. The eyepoint is at a distance of
  // twice the radius from the origin. The default field-of-view angle is 60
  // degrees, which works well for this configuration (30-60-90 triangle). The
  // near and far planes are positioned at the edges of the sphere.
  // 
  //

  // Projection matrix.
  const float aspect_ratio = static_cast<float>(viewport_size_[0]) /
                             static_cast<float>(viewport_size_[1]);
  const float near_distance = trackball_radius_;
  const float far_distance = 10.0f * trackball_radius_;
  const Matrix4f persp = ion::math::PerspectiveMatrixFromView(
      view_info_->field_of_view_angle, aspect_ratio,
      near_distance, far_distance);
  // Translate the center of the viewport to the origin.
  // 
  const Point2f pan_location(0.5f, 0.5f);
  const Matrix4f trans = ion::math::TranslationMatrix(
      Vector3f(pan_location[0] - 0.5f, pan_location[1] - 0.5f, 0.0f));
  const Matrix4f proj = trans * persp;
  demoutils::SetUniformInNode(
      uniform_indices_[kProjectionMatrixIndex], proj, view_node_);

  // Modelview matrix.  Local transformations are on the left, so this is in
  // reverse order of how the matrices are applied to the scene.
  const Point3f scene_center = Point3f::Zero();
  const float camera_z = 2.0f * trackball_radius_;
  const Matrix4f view =
      ion::math::TranslationMatrix(Vector3f(0.0f, 0.0f, -camera_z)) *
      ion::math::RotationMatrixAxisAngleH(
          Vector3f::AxisX(), view_info_->tilt_angle) *
      ion::math::RotationMatrixAxisAngleH(
          Vector3f::AxisY(), view_info_->rotation_angle) *
      ion::math::TranslationMatrix(-scene_center);
  demoutils::SetUniformInNode(uniform_indices_[kModelviewMatrixIndex],
                              view, view_node_);
  ion::math::Matrix4f view_inverse = ion::math::Inverse(view);
  auto camera_position = ion::math::Vector3f(
      view_inverse(0, 3), view_inverse(1, 3), view_inverse(2, 3));
  demoutils::SetUniformInNode(uniform_indices_[kCameraPositionIndex],
                              camera_position, view_node_);
}

const Matrix4f ViewerDemoBase::GetMatrixFromUniform(UniformIndex which) const {
  if (view_node_.Get() && uniform_indices_[which] != ion::base::kInvalidIndex) {
    const ion::gfx::Uniform& u =
        view_node_->GetUniforms()[uniform_indices_[which]];
    return u.GetValue<Matrix4f>();
  } else {
    return Matrix4f::Identity();
  }
}
