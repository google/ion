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

#include <string>

#include "ion/base/datacontainer.h"
#include "ion/base/invalid.h"
#include "ion/base/logging.h"
#include "ion/base/setting.h"
#include "ion/base/settingmanager.h"
#include "ion/base/zipassetmanager.h"
#include "ion/base/zipassetmanagermacros.h"
#include "ion/demos/utils.h"
#include "ion/demos/viewerdemobase.h"
#include "ion/gfx/attributearray.h"
#include "ion/gfx/bufferobject.h"
#include "ion/gfx/image.h"
#include "ion/gfx/indexbuffer.h"
#include "ion/gfx/node.h"
#include "ion/gfx/sampler.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfx/shape.h"
#include "ion/gfx/statetable.h"
#include "ion/gfx/texture.h"
#include "ion/gfxutils/buffertoattributebinder.h"
#include "ion/gfxutils/shapeutils.h"
#include "ion/image/conversionutils.h"
#include "ion/math/utils.h"
#include "ion/math/vector.h"

ION_REGISTER_ASSETS(IonShapeDemoResources);

using ion::gfx::Node;
using ion::gfx::NodePtr;
using ion::gfx::ShaderInputRegistry;
using ion::gfx::ShaderInputRegistryPtr;
using ion::gfx::Shape;
using ion::gfx::ShapePtr;
using ion::gfxutils::RectangleSpec;
using ion::math::Point2i;
using ion::math::Point3f;
using ion::math::Range2i;
using ion::math::Vector2f;
using ion::math::Vector2i;
using ion::math::Vector3f;
using ion::math::Vector4f;

namespace {

//-----------------------------------------------------------------------------
//
// Constants.
//
//-----------------------------------------------------------------------------

// Usage mode for all buffer object data. This is set to kStreamDraw so that
// all buffers can be recreated if necessary.
static const ion::gfx::BufferObject::UsageMode kUsageMode =
    ion::gfx::BufferObject::kStreamDraw;

//-----------------------------------------------------------------------------
//
// This enum defines all of the types of shapes in the demo.
//
//-----------------------------------------------------------------------------

enum ShapeType {
  kRectangleShape,
  kBoxShape,
  kEllipsoidShape,
  kCylinderShape,
};
static const size_t kNumShapeTypes = kCylinderShape + 1;

//-----------------------------------------------------------------------------
//
// The Grid class is used to lay out the shapes easily. The number of columns
// and the spacing between adjacent shapes are constants defined in the
// constructor.
//
//-----------------------------------------------------------------------------

class Grid {
 public:
  Grid();

  // Returns the center of the specified shape.
  const Point3f GetCenter(ShapeType type) const;

  // Returns the radius of the grid, useful for setting up the view.
  float GetRadius() const;

 private:
  const size_t num_shapes_;
  const size_t num_columns_;
  const size_t num_rows_;
  const float x_spacing_;  // Spacing between shape centers in X dimension.
  const float y_spacing_;  // Spacing between shape centers in Y dimension.
  const float x_center_offset_;  // X offset to center the grid on the origin.
  const float y_center_offset_;  // Y offset to center the grid on the origin.
};

Grid::Grid()
    : num_shapes_(kNumShapeTypes),
      num_columns_(2U),
      num_rows_((num_shapes_ + num_columns_ - 1) / num_columns_),
      x_spacing_(2.0f),
      y_spacing_(2.0f),
      x_center_offset_(0.5f * static_cast<float>(num_columns_) * x_spacing_),
      y_center_offset_(0.5f * static_cast<float>(num_rows_) * y_spacing_) {
}

const Point3f Grid::GetCenter(ShapeType type) const {
  // Invert the row index so that the first shape is at the top.
  const float row = static_cast<float>(
      num_rows_ - static_cast<size_t>(type) / num_columns_ - 1);
  const float col = static_cast<float>(
      static_cast<size_t>(type) % num_columns_);

  // Center the shape within its grid rectangle, then offset the grid rectangle
  // to the origin.
  const float x = (col + 0.5f) * x_spacing_ - x_center_offset_;
  const float y = (row + 0.5f) * y_spacing_ - y_center_offset_;
  return Point3f(x, y, 0.0f);
}

float Grid::GetRadius() const {
  return ion::math::Sqrt(ion::math::Square(x_center_offset_) +
                         ion::math::Square(y_center_offset_));
}

static Grid s_grid;

//-----------------------------------------------------------------------------
//
// Helper functions.
//
//-----------------------------------------------------------------------------

static RectangleSpec::PlaneNormal PlaneNormalFromInt(int i) {
  if (i >= RectangleSpec::kPositiveX && i <= RectangleSpec::kNegativeZ) {
    return static_cast<RectangleSpec::PlaneNormal>(i);
  } else {
    LOG(ERROR) << "Invalid RectangleSpec PlaneNormal value: " << i;
    return RectangleSpec::kPositiveZ;
  }
}

// Converts the given Shape to wireframe, returning a new Shape. This assumes
// the Shape's indices have not been wiped.
static const ShapePtr BuildWireframeShape(const ShapePtr& tri_shape) {
  ShapePtr line_shape(new Shape);

  // Copy the basic stuff.
  line_shape->SetLabel(tri_shape->GetLabel() + " as wireframe");
  line_shape->SetAttributeArray(tri_shape->GetAttributeArray());

  // Draw as lines.
  line_shape->SetPrimitiveType(Shape::kLines);

  // Modify the index buffer to convert triangles to lines.
  DCHECK_EQ(tri_shape->GetPrimitiveType(), Shape::kTriangles);
  line_shape->SetIndexBuffer(
      ion::gfxutils::BuildWireframeIndexBuffer(tri_shape->GetIndexBuffer()));

  return line_shape;
}

// Creates a Shape representing the surface normals of the given Shape. This
// assumes the Shape's vertices and indices have not been wiped.
static const ShapePtr BuildNormalLineShape(const ShapePtr& tri_shape) {
  using ion::gfx::BufferObject;

  // Verify that the shape is exactly as we expect.
  const ion::gfx::AttributeArray& tri_aa = *tri_shape->GetAttributeArray();
  DCHECK_EQ(tri_aa.GetAttributeCount(), 3U);
  DCHECK_EQ(tri_aa.GetBufferAttributeCount(), 3U);
  const ion::gfx::Attribute& pos_attr = tri_aa.GetAttribute(0);
  const ion::gfx::Attribute& norm_attr = tri_aa.GetAttribute(2);
  DCHECK(pos_attr.Is<ion::gfx::BufferObjectElement>());
  DCHECK(norm_attr.Is<ion::gfx::BufferObjectElement>());
  DCHECK_EQ(pos_attr.GetRegistry().GetSpec(pos_attr)->name, "aVertex");
  DCHECK_EQ(norm_attr.GetRegistry().GetSpec(norm_attr)->name, "aNormal");

  // Access the buffer data.
  const ion::gfx::BufferObjectElement& pos_boe =
      pos_attr.GetValue<ion::gfx::BufferObjectElement>();
  const ion::gfx::BufferObjectElement& norm_boe =
      norm_attr.GetValue<ion::gfx::BufferObjectElement>();
  const BufferObject& pos_bo = *pos_boe.buffer_object;
  const BufferObject& norm_bo = *norm_boe.buffer_object;
  const BufferObject::Spec& pos_spec = pos_bo.GetSpec(pos_boe.spec_index);
  const BufferObject::Spec& norm_spec = norm_bo.GetSpec(norm_boe.spec_index);
  const char* pos_data = static_cast<const char*>(pos_bo.GetData()->GetData());
  const char* norm_data =
      static_cast<const char*>(norm_bo.GetData()->GetData());
  const size_t pos_stride = pos_bo.GetStructSize();
  const size_t norm_stride = norm_bo.GetStructSize();
  const size_t vertex_count = pos_bo.GetCount();
  DCHECK_EQ(norm_bo.GetCount(), vertex_count);

  // Set up a new AttributeArray with the vertices forming normal lines.
  static const float kNormalScale = .25f;
  std::vector<Point3f> nl_vertices(2 * vertex_count);
  for (size_t i = 0; i < vertex_count; ++i) {
    const Point3f& pos = *reinterpret_cast<const Point3f*>(
        &pos_data[pos_stride * i + pos_spec.byte_offset]);
    const Vector3f& norm = *reinterpret_cast<const Vector3f*>(
        &norm_data[norm_stride * i + norm_spec.byte_offset]);
    nl_vertices[2 * i + 0] = pos;
    nl_vertices[2 * i + 1] = pos + kNormalScale * norm;
  }
  ion::base::DataContainerPtr dc =
      ion::base::DataContainer::CreateAndCopy<Point3f>(
          &nl_vertices[0], 2 * vertex_count, true, ion::base::AllocatorPtr());
  ion::gfx::BufferObjectPtr buffer_object(new BufferObject);
  buffer_object->SetData(dc, sizeof(nl_vertices[0]), 2 * vertex_count,
                         kUsageMode);
  ion::gfx::AttributeArrayPtr nl_aa(new ion::gfx::AttributeArray);
  Point3f p;
  ion::gfxutils::BufferToAttributeBinder<Point3f>(p)
      .Bind(p, "aVertex")
      .Apply(ShaderInputRegistry::GetGlobalRegistry(), nl_aa, buffer_object);

  // Set up a Shape.
  ShapePtr normal_shape(new Shape);
  normal_shape->SetLabel(tri_shape->GetLabel() + " normals");
  normal_shape->SetPrimitiveType(Shape::kLines);
  normal_shape->SetAttributeArray(nl_aa);

  return normal_shape;
}

// Replaces the indexed shapes in tri_node, line_node, and normal_node with the
// given Shape, a wireframe version of that Shape, and a Shape representing the
// surface normals of that Shape, respectively.
static void ReplaceShape(size_t index, const ShapePtr& shape,
                         const NodePtr& tri_node, const NodePtr& line_node,
                         const NodePtr& normal_node) {
  DCHECK_LT(index, tri_node->GetShapes().size());
  DCHECK_LT(index, line_node->GetShapes().size());
  DCHECK_LT(index, normal_node->GetShapes().size());

  tri_node->ReplaceShape(index, shape);
  line_node->ReplaceShape(index, BuildWireframeShape(shape));
  normal_node->ReplaceShape(index, BuildNormalLineShape(shape));
}

// Builds the Ion graph for the demo.
static const NodePtr BuildGraph(
    int width, int height,
    const ion::gfxutils::ShaderManagerPtr& shader_manager,
    const ion::gfx::ShaderProgramPtr& default_shader,
    const NodePtr& tri_node, const NodePtr& line_node,
    const NodePtr& normal_node) {
  NodePtr root(new Node);

  // Global state.
  ion::gfx::StateTablePtr state_table(new ion::gfx::StateTable(width, height));
  state_table->SetViewport(Range2i::BuildWithSize(Point2i(0, 0),
                                                  Vector2i(width, height)));
  state_table->SetClearColor(Vector4f(0.3f, 0.3f, 0.5f, 1.0f));
  state_table->SetClearDepthValue(1.f);
  state_table->Enable(ion::gfx::StateTable::kDepthTest, true);
  root->SetStateTable(state_table);

  // Sampler.
  ion::gfx::SamplerPtr sampler(new ion::gfx::Sampler);
  // This is required for textures on iOS. No other texture wrap mode seems to
  // be supported.
  sampler->SetWrapS(ion::gfx::Sampler::kClampToEdge);
  sampler->SetWrapT(ion::gfx::Sampler::kClampToEdge);

  // Textures.
  ion::gfx::TexturePtr texture =
      demoutils::LoadTextureAsset("shapes_texture_image.jpg");
  ion::gfx::CubeMapTexturePtr cube_map =
      demoutils::LoadCubeMapAsset("shapes_cubemap_image", ".jpg");
  texture->SetSampler(sampler);
  cube_map->SetSampler(sampler);

  // Shader registry.
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  reg->IncludeGlobalRegistry();
  reg->Add(ShaderInputRegistry::UniformSpec(
        "uTexture", ion::gfx::kTextureUniform, "Texture"));
  reg->Add(ShaderInputRegistry::UniformSpec(
        "uCubeMap", ion::gfx::kCubeMapTextureUniform, "CubeMapTexture"));
  reg->Add(ShaderInputRegistry::UniformSpec(
        "uUseCubeMap", ion::gfx::kIntUniform,
        "Whether to use cubemap or regular texture"));
  demoutils::AddUniformToNode(reg, "uTexture", texture, root);
  demoutils::AddUniformToNode(reg, "uCubeMap", cube_map, root);
  demoutils::AddUniformToNode(reg, "uUseCubeMap", 0, root);

  // Set up the shader that applies the texture to the shapes.
  root->SetShaderProgram(shader_manager->CreateShaderProgram(
      "ShapeDemo shader", reg,
      ion::gfxutils::ShaderSourceComposerPtr(
          new ion::gfxutils::ZipAssetComposer("shapes.vp", false)),
      ion::gfxutils::ShaderSourceComposerPtr(
          new ion::gfxutils::ZipAssetComposer("shapes.fp", false))));

  // The normal node uses the default shader.
  normal_node->SetShaderProgram(default_shader);
  static const Vector4f kNormalColor(.9f, .5f, .9f, 1.f);
  demoutils::AddUniformToNode(ShaderInputRegistry::GetGlobalRegistry(),
                              "uBaseColor", kNormalColor, normal_node);

  // Set up dummy shapes that will be replaced later.
  ShapePtr dummy_shape(new Shape);
  for (size_t i = 0; i < kNumShapeTypes; ++i) {
    tri_node->AddShape(dummy_shape);
    line_node->AddShape(dummy_shape);
    normal_node->AddShape(dummy_shape);
  }

  root->AddChild(tri_node);
  root->AddChild(line_node);
  root->AddChild(normal_node);

  return root;
}

}  // anonymous namespace

//-----------------------------------------------------------------------------
//
// ShapeDemo class.
//
//-----------------------------------------------------------------------------

class IonShapeDemo : public ViewerDemoBase {
 public:
  IonShapeDemo(int width, int height);
  ~IonShapeDemo() override {}
  void Resize(int width, int height) override;
  void Update() override {}
  void RenderFrame() override;
  void Keyboard(int key, int x, int y, bool is_press) override {}
  std::string GetDemoClassName() const override { return "ShapeDemo"; }

 private:
  void InitSettings();
  void ChangeTexture(ion::base::SettingBase*);
  void EnableBackFaceCulling(ion::base::SettingBase*);
  void EnableNormals(ion::base::SettingBase*);
  void EnableWireframe(ion::base::SettingBase*);
  void UpdateShape(size_t index, const ion::gfx::ShapePtr& shape);
  void UpdateRectangle(ion::base::SettingBase*);
  void UpdateBox(ion::base::SettingBase*);
  void UpdateEllipsoid(ion::base::SettingBase*);
  void UpdateCylinder(ion::base::SettingBase*);

  ion::gfx::NodePtr root_;         // Root of graph.
  ion::gfx::NodePtr tri_node_;     // Node containing shapes using triangles.
  ion::gfx::NodePtr line_node_;    // Node containing shapes using lines.
  ion::gfx::NodePtr normal_node_;  // Node containing surface normal lines.

  // Rectangle settings.
  ion::base::Setting<int> rectangle_plane_normal_;
  ion::base::Setting<ion::math::Vector2f> rectangle_size_;

  // Box settings.
  ion::base::Setting<ion::math::Vector3f> box_size_;

  // Ellipsoid settings.
  ion::base::Setting<float> ellipsoid_longitude_degrees_start_;
  ion::base::Setting<float> ellipsoid_longitude_degrees_end_;
  ion::base::Setting<float> ellipsoid_latitude_degrees_start_;
  ion::base::Setting<float> ellipsoid_latitude_degrees_end_;
  ion::base::Setting<size_t> ellipsoid_band_count_;
  ion::base::Setting<size_t> ellipsoid_sector_count_;
  ion::base::Setting<ion::math::Vector3f> ellipsoid_size_;

  // Cylinder settings.
  ion::base::Setting<bool> cylinder_has_top_cap_;
  ion::base::Setting<bool> cylinder_has_bottom_cap_;
  ion::base::Setting<size_t> cylinder_shaft_band_count_;
  ion::base::Setting<size_t> cylinder_cap_band_count_;
  ion::base::Setting<size_t> cylinder_sector_count_;
  ion::base::Setting<float> cylinder_top_radius_;
  ion::base::Setting<float> cylinder_bottom_radius_;
  ion::base::Setting<float> cylinder_height_;

  // Other settings.
  ion::base::Setting<bool> check_errors_;
  ion::base::Setting<bool> draw_as_wireframe_;
  ion::base::Setting<bool> draw_normals_;
  ion::base::Setting<bool> enable_back_face_culling_;
  ion::base::Setting<bool> use_cube_map_texture_;
};

IonShapeDemo::IonShapeDemo(int width, int height)
    : ViewerDemoBase(width, height),

      // Rectangle settings.
      rectangle_plane_normal_("shapedemo/rectangle/rectangle_plane_normal",
                              RectangleSpec::kPositiveZ,
                              "Normal to plane containing rectangle"),
      rectangle_size_("shapedemo/rectangle/rectangle_size", Vector2f::Fill(1.f),
                      "Size of rectangle shape"),

      // Box settings.
      box_size_("shapedemo/box/box_size", Vector3f::Fill(1.f),
                "Size of box shape"),

      // Ellipsoid settings.
      ellipsoid_longitude_degrees_start_(
          "shapedemo/ellipsoid/ellipsoid_longitude_degrees_start",
          0.f, "Start longitude angle in degrees."),
      ellipsoid_longitude_degrees_end_(
          "shapedemo/ellipsoid/ellipsoid_longitude_degrees_end",
          360.f, "End longitude angle in degrees."),
      ellipsoid_latitude_degrees_start_(
          "shapedemo/ellipsoid/ellipsoid_latitude_degrees_start",
          -90.f, "Start latitude angle in degrees."),
      ellipsoid_latitude_degrees_end_(
          "shapedemo/ellipsoid/ellipsoid_latitude_degrees_end",
          90.f, "End longitude angle in degrees."),
      ellipsoid_band_count_("shapedemo/ellipsoid/ellipsoid_band_count",
                            10U, "Number of latitude bands in ellipsoid shape"),
      ellipsoid_sector_count_("shapedemo/ellipsoid/ellipsoid_sector_count", 10U,
                              "Number of longitude sectors in ellipsoid shape"),
      ellipsoid_size_("shapedemo/ellipsoid/ellipsoid_size",
                      Vector3f::Fill(1.f), "Size of ellipsoid shape"),

      // Cylinder settings.
      cylinder_has_top_cap_("shapedemo/cylinder/cylinder_has_top_cap", true,
                            "Whether cylinder shape has a top cap"),
      cylinder_has_bottom_cap_("shapedemo/cylinder/cylinder_has_bottom_cap",
                               true, "Whether cylinder shape has a bottom cap"),
      cylinder_shaft_band_count_(
          "shapedemo/cylinder/cylinder_shaft_band_count", 1U,
          "Number of bands in shaft of cylinder shape"),
      cylinder_cap_band_count_("shapedemo/cylinder/cylinder_cap_band_count", 1U,
                               "Number of bands in caps of cylinder shape"),
      cylinder_sector_count_(
          "shapedemo/cylinder/cylinder_sector_count", 10U,
          "Number of longitudinal sectors in cylinder shape"),
      cylinder_top_radius_("shapedemo/cylinder/cylinder_top_radius", .5f,
                           "Radius of top of cylinder shape"),
      cylinder_bottom_radius_("shapedemo/cylinder/cylinder_bottom_radius", .5f,
                              "Radius of bottom of cylinder shape"),
      cylinder_height_("shapedemo/cylinder/cylinder_height", 1.f,
                       "Height of cylinder shape"),

      // Other settings.
      check_errors_("shapedemo/check_errors", false,
                    "Enable OpenGL error checking"),
      draw_as_wireframe_("shapedemo/draw_as_wireframe", false,
                         "Draw shapes as wire-frame"),
      draw_normals_("shapedemo/draw_normals", false,
                    "Draw surface normals as lines on shapes"),
      enable_back_face_culling_("shapedemo/enable_back_face_culling", true,
                                "Enable back-face culling"),
      use_cube_map_texture_("shapedemo/use_cube_map_texture", false,
                            "Use a CubeMapTexture or a regular Texture") {
  // Load assets.
  IonShapeDemoResources::RegisterAssets();

  // Build the Ion graph.
  tri_node_ = new Node;
  line_node_ = new Node;
  normal_node_ = new Node;
  root_ = BuildGraph(width, height, GetShaderManager(),
                     GetRenderer()->GetDefaultShaderProgram(),
                     tri_node_, line_node_, normal_node_);

  // Add the shapes.
  UpdateRectangle(nullptr);
  UpdateBox(nullptr);
  UpdateEllipsoid(nullptr);
  UpdateCylinder(nullptr);

  // Initialize other state.
  EnableBackFaceCulling(nullptr);
  EnableNormals(nullptr);
  EnableWireframe(nullptr);

  // Set up viewing.
  SetTrackballRadius(s_grid.GetRadius());
  SetNodeWithViewUniforms(root_);

  // Set up the remote handlers.
  InitRemoteHandlers(std::vector<ion::gfx::NodePtr>(1, root_));

  // Set up the settings.
  InitSettings();

  // Initialize the uniforms and matrices in the graph.
  UpdateViewUniforms();
}

void IonShapeDemo::Resize(int width, int height) {
  ViewerDemoBase::Resize(width, height);

  DCHECK(root_->GetStateTable().Get());
  root_->GetStateTable()->SetViewport(
      Range2i::BuildWithSize(Point2i(0, 0), Vector2i(width, height)));
}

void IonShapeDemo::RenderFrame() {
  GetGraphicsManager()->EnableErrorChecking(check_errors_);
  GetRenderer()->DrawScene(root_);
}

void IonShapeDemo::InitSettings() {
  using std::bind;
  using std::placeholders::_1;
  // Set up listeners for settings that require rebuilding.
  ion::base::SettingManager::RegisterGroupListener(
      "shapedemo/rectangle", "ShapeDemo",
      bind(&IonShapeDemo::UpdateRectangle, this, _1));
  ion::base::SettingManager::RegisterGroupListener(
      "shapedemo/box", "ShapeDemo",
      bind(&IonShapeDemo::UpdateBox, this, _1));
  ion::base::SettingManager::RegisterGroupListener(
      "shapedemo/ellipsoid", "ShapeDemo",
      bind(&IonShapeDemo::UpdateEllipsoid, this, _1));
  ion::base::SettingManager::RegisterGroupListener(
      "shapedemo/cylinder", "ShapeDemo",
      bind(&IonShapeDemo::UpdateCylinder, this, _1));

  // Set up other listeners.
  draw_as_wireframe_.RegisterListener(
      "ShapeDemo", bind(&IonShapeDemo::EnableWireframe, this, _1));
  draw_normals_.RegisterListener(
      "ShapeDemo", bind(&IonShapeDemo::EnableNormals, this, _1));
  enable_back_face_culling_.RegisterListener(
      "ShapeDemo", bind(&IonShapeDemo::EnableBackFaceCulling, this, _1));
  use_cube_map_texture_.RegisterListener(
      "ShapeDemo", bind(&IonShapeDemo::ChangeTexture, this, _1));

  // Set up strings for enum settings so they use dropboxes.
  rectangle_plane_normal_.SetTypeDescriptor("enum:+X|-X|+Y|-Y|+Z|-Z");
}

void IonShapeDemo::ChangeTexture(ion::base::SettingBase*) {
  const size_t index = root_->GetUniformIndex("uUseCubeMap");
  DCHECK_NE(index, ion::base::kInvalidIndex);
  root_->SetUniformValue<int>(index, use_cube_map_texture_);
}

void IonShapeDemo::EnableBackFaceCulling(ion::base::SettingBase*) {
  DCHECK(root_->GetStateTable().Get());
  root_->GetStateTable()->Enable(ion::gfx::StateTable::kCullFace,
                                 enable_back_face_culling_);
}

void IonShapeDemo::EnableNormals(ion::base::SettingBase*) {
  normal_node_->Enable(draw_normals_);
}

void IonShapeDemo::EnableWireframe(ion::base::SettingBase*) {
  tri_node_->Enable(!draw_as_wireframe_);
  line_node_->Enable(draw_as_wireframe_);
}

void IonShapeDemo::UpdateShape(size_t index, const ion::gfx::ShapePtr& shape) {
  ReplaceShape(index, shape, tri_node_, line_node_, normal_node_);
}

void IonShapeDemo::UpdateRectangle(ion::base::SettingBase*) {
  RectangleSpec rect_spec;
  rect_spec.usage_mode = kUsageMode;
  rect_spec.translation = s_grid.GetCenter(kRectangleShape);
  rect_spec.plane_normal = PlaneNormalFromInt(rectangle_plane_normal_);
  rect_spec.size = rectangle_size_;
  UpdateShape(kRectangleShape, ion::gfxutils::BuildRectangleShape(rect_spec));
}

void IonShapeDemo::UpdateBox(ion::base::SettingBase*) {
  ion::gfxutils::BoxSpec box_spec;
  box_spec.usage_mode = kUsageMode;
  box_spec.translation = s_grid.GetCenter(kBoxShape);
  box_spec.size = box_size_;
  UpdateShape(kBoxShape, ion::gfxutils::BuildBoxShape(box_spec));
}

void IonShapeDemo::UpdateEllipsoid(ion::base::SettingBase*) {
  ion::gfxutils::EllipsoidSpec ellipsoid_spec;
  ellipsoid_spec.usage_mode = kUsageMode;
  ellipsoid_spec.translation = s_grid.GetCenter(kEllipsoidShape);
  ellipsoid_spec.size = ellipsoid_size_;
  ellipsoid_spec.band_count = ellipsoid_band_count_;
  ellipsoid_spec.sector_count = ellipsoid_sector_count_;
  ellipsoid_spec.longitude_start = ion::math::Anglef::FromDegrees(
      ellipsoid_longitude_degrees_start_);
  ellipsoid_spec.longitude_end = ion::math::Anglef::FromDegrees(
      ellipsoid_longitude_degrees_end_);
  ellipsoid_spec.latitude_start = ion::math::Anglef::FromDegrees(
      ellipsoid_latitude_degrees_start_);
  ellipsoid_spec.latitude_end = ion::math::Anglef::FromDegrees(
      ellipsoid_latitude_degrees_end_);

  UpdateShape(kEllipsoidShape,
              ion::gfxutils::BuildEllipsoidShape(ellipsoid_spec));
}

void IonShapeDemo::UpdateCylinder(ion::base::SettingBase*) {
  ion::gfxutils::CylinderSpec cylinder_spec;
  cylinder_spec.usage_mode = kUsageMode;
  cylinder_spec.translation = s_grid.GetCenter(kCylinderShape);
  cylinder_spec.has_top_cap = cylinder_has_top_cap_;
  cylinder_spec.has_bottom_cap = cylinder_has_bottom_cap_;
  cylinder_spec.shaft_band_count = cylinder_shaft_band_count_;
  cylinder_spec.cap_band_count = cylinder_cap_band_count_;
  cylinder_spec.sector_count = cylinder_sector_count_;
  cylinder_spec.top_radius = cylinder_top_radius_;
  cylinder_spec.bottom_radius = cylinder_bottom_radius_;
  cylinder_spec.height = cylinder_height_;
  UpdateShape(kCylinderShape, ion::gfxutils::BuildCylinderShape(cylinder_spec));
}

std::unique_ptr<DemoBase> CreateDemo(int width, int height) {
  return std::unique_ptr<DemoBase>(new IonShapeDemo(width, height));
}
