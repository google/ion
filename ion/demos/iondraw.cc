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

// This is not a unit test - it uses OpenGL to render an ion scene graph in a
// window.

#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

#include "ion/base/invalid.h"
#include "ion/base/logging.h"
#include "ion/demos/hud.h"
#include "ion/demos/utils.h"
#include "ion/demos/viewerdemobase.h"
#include "ion/gfx/attribute.h"
#include "ion/gfx/attributearray.h"
#include "ion/gfx/bufferobject.h"
#include "ion/gfx/image.h"
#include "ion/gfx/indexbuffer.h"
#include "ion/gfx/node.h"
#include "ion/gfx/renderer.h"
#include "ion/gfx/sampler.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfx/shaderprogram.h"
#include "ion/gfx/shape.h"
#include "ion/gfx/statetable.h"
#include "ion/gfx/texture.h"
#include "ion/gfx/uniform.h"
#include "ion/gfxutils/buffertoattributebinder.h"
#include "ion/gfxutils/shapeutils.h"
#include "ion/math/angle.h"
#include "ion/math/matrix.h"
#include "ion/math/range.h"
#include "ion/math/transformutils.h"
#include "ion/math/vector.h"
#include "ion/portgfx/setswapinterval.h"

using ion::math::Anglef;
using ion::math::Matrix4f;
using ion::math::Point2f;
using ion::math::Point2i;
using ion::math::Point3f;
using ion::math::Range2i;
using ion::math::RotationMatrixAxisAngleH;
using ion::math::TranslationMatrix;
using ion::math::Vector2f;
using ion::math::Vector2i;
using ion::math::Vector3f;
using ion::math::Vector4f;

namespace {

// Global vars.
static ion::gfx::NodePtr s_textured_shader_node;
static std::vector<ion::gfx::NodePtr> s_nodes_with_uniforms;
static bool s_draw_scene = true;
static bool s_texture_matrix_animation = false;
static bool s_vertex_motion = false;

struct Vertex {
  Vector3f position;
  Vector3f normal;
  Vector2f texture_coords;
};

//-----------------------------------------------------------------------------
//
// Shader program strings.
//
//-----------------------------------------------------------------------------

static const char* kLightingVertexShaderString = (
    "uniform mat4 uProjectionMatrix;\n"
    "uniform mat4 uModelviewMatrix;\n"
    "uniform vec4 uBaseColor;\n"
    "attribute vec3 aVertex;\n"
    "attribute vec3 aNormal;\n"
    "varying vec4 color;\n"
    "\n"
    "void main(void) {\n"
    "  float light0_intensity = 0.9;\n"
    "  float light1_intensity = 0.8;\n"
    "  vec3 dir_to_light0 = normalize(vec3(1., 2., 3.));\n"
    "  vec3 dir_to_light1 = normalize(vec3(-3., -2., -1.));\n"
    "  float l0 = light0_intensity * dot(dir_to_light0, aNormal);\n"
    "  float l1 = light1_intensity * dot(dir_to_light1, aNormal);\n"
    "  float intensity = max(0.0, l0) + max(0.0, l1);\n"
    "  color = intensity * uBaseColor;\n"
    "  gl_Position = uProjectionMatrix * uModelviewMatrix *\n"
    "      vec4(aVertex, 1.);\n"
    "}\n");

static const char* kLightingFragmentShaderString = (
    "#ifdef GL_ES\n"
    "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
    "precision highp float;\n"
    "#else\n"
    "precision mediump float;\n"
    "#endif\n"
    "#endif\n"
    "\n"
    "varying vec4 color;\n"
    "\n"
    "void main(void) {\n"
    "  gl_FragColor = color;\n"
    "}\n");

static const char* kTextureVertexShaderString = (
    "uniform mat4 uProjectionMatrix;\n"
    "uniform mat4 uModelviewMatrix;\n"
    "attribute vec3 aVertex;\n"
    "attribute vec2 aTexCoords;\n"
    "attribute vec3 aNormal;\n"
    "varying vec2  texture_coords;\n"
    "varying float intensity;\n"
    "uniform mat4 texture_matrix;\n"
    "\n"
    "void main(void) {\n"
    "  vec4 tc = texture_matrix * vec4(aTexCoords, 0., 1.);\n"
    "  texture_coords = tc.st;\n"
    "  float light0_intensity = 0.9;\n"
    "  float light1_intensity = 0.8;\n"
    "  vec3 dir_to_light0 = normalize(vec3(1, 2, 3));\n"
    "  vec3 dir_to_light1 = normalize(vec3(-3, -2, -1));\n"
    "  float l0 = light0_intensity * dot(dir_to_light0, aNormal);\n"
    "  float l1 = light1_intensity * dot(dir_to_light1, aNormal);\n"
    "  intensity = max(0.0, l0) + max(0.0, l1);\n"
    "  gl_Position = uProjectionMatrix * uModelviewMatrix * vec4(aVertex, 1);\n"
    "}\n");

static const char* kTextureFragmentShaderString = (
    "#ifdef GL_ES\n"
    "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
    "precision highp float;\n"
    "#else\n"
    "precision mediump float;\n"
    "#endif\n"
    "#endif\n"
    "\n"
    "varying vec2  texture_coords;\n"
    "varying float intensity;\n"
    "uniform sampler2D sampler;\n"
    "\n"
    "void main(void) {\n"
    "  gl_FragColor = intensity * texture2D(sampler, texture_coords);\n"
    "}\n");

//-----------------------------------------------------------------------------
//
// Shape construction.
//
//-----------------------------------------------------------------------------

// Builds a tetrahedron without texture coordinates.
static ion::gfx::BufferObjectPtr BuildTetrahedronBufferObject() {
  const Vector3f p0(1.0f, 1.0f, 1.0f);
  const Vector3f p1(-1.0f, -1.0f, 1.0f);
  const Vector3f p2(-1.0f, 1.0f, -1.0f);
  const Vector3f p3(1.0f, -1.0f, -1.0f);

  Vertex* vertices = new Vertex[12];
  vertices[0].position = p0;
  vertices[1].position = p2;
  vertices[2].position = p1;
  vertices[3].position = p0;
  vertices[4].position = p1;
  vertices[5].position = p3;
  vertices[6].position = p0;
  vertices[7].position = p3;
  vertices[8].position = p2;
  vertices[9].position = p1;
  vertices[10].position = p2;
  vertices[11].position = p3;
  ion::gfx::BufferObjectPtr buffer_object(new ion::gfx::BufferObject);
  ion::base::DataContainerPtr container =
      ion::base::DataContainer::Create<Vertex>(
          vertices, ion::base::DataContainer::ArrayDeleter<Vertex>, false,
          buffer_object->GetAllocator());
  buffer_object->SetData(container, sizeof(vertices[0]), 12U,
                         ion::gfx::BufferObject::kStaticDraw);
  return buffer_object;
}


// Builds a tetrahedron without texture coordinates.
static ion::gfx::ShapePtr BuildTetrahedronShape(
    const ion::gfx::AttributeArrayPtr& attribute_array) {
  ion::gfx::ShapePtr shape(new ion::gfx::Shape);
  shape->SetPrimitiveType(ion::gfx::Shape::kTriangles);
  shape->SetAttributeArray(attribute_array);

  return shape;
}

//-----------------------------------------------------------------------------
//
// Texture image construction.
//
//-----------------------------------------------------------------------------

static ion::gfx::ImagePtr BuildTextureImage() {
  // 20W x 24H RGB pixels.
  static const int kWidth = 20;
  static const int kHeight = 24;
  static const int kRowSize = kWidth * 3;
  static const uint8 pixels[kHeight * kRowSize] = {
#include "ion/demos/smiley-20x24.h"
  };

  // Note that OpenGL starts at the bottom row of the image, so flip the rows
  // first.
  uint8 flipped_pixels[kHeight * kRowSize];
  for (int row = 0; row < kHeight; ++row) {
    std::memcpy(&flipped_pixels[(kHeight - 1 - row) * kRowSize],
                &pixels[row * kRowSize], kRowSize);
  }

  ion::gfx::ImagePtr image(new ion::gfx::Image);
  ion::base::DataContainerPtr container =
      ion::base::DataContainer::CreateAndCopy<uint8>(
          flipped_pixels, kHeight * kRowSize, true, image->GetAllocator());
  image->Set(ion::gfx::Image::kRgb888, kWidth, kHeight, container);
  return image;
}

//-----------------------------------------------------------------------------
//
// Scene graph construction and modification.
//
//-----------------------------------------------------------------------------

static ion::gfx::NodePtr BuildGraph(int width, int height) {
  const ion::gfx::ShaderInputRegistryPtr& global_reg =
      ion::gfx::ShaderInputRegistry::GetGlobalRegistry();

  // The root node uses the default shader.
  ion::gfx::NodePtr root(new ion::gfx::Node);

  // Set up global state.
  ion::gfx::StateTablePtr state_table(new ion::gfx::StateTable(width, height));
  state_table->SetViewport(
      Range2i::BuildWithSize(Point2i(0, 0), Vector2i(width, height)));
  state_table->SetClearColor(Vector4f(0.3f, 0.3f, 0.5f, 1.0f));
  state_table->SetClearDepthValue(1.f);
  state_table->Enable(ion::gfx::StateTable::kDepthTest, true);
  state_table->Enable(ion::gfx::StateTable::kCullFace, true);
  root->SetStateTable(state_table);

  // Untextured cube on the top left using indices and a lighting shader.
  {
    ion::gfx::NodePtr node(new ion::gfx::Node);
    // An empty registry is ok, since there are no local uniforms.
    ion::gfx::ShaderInputRegistryPtr reg(
        new ion::gfx::ShaderInputRegistry);
    reg->IncludeGlobalRegistry();
    node->SetShaderProgram(
        ion::gfx::ShaderProgram::BuildFromStrings(
            "lighting shader", reg, kLightingVertexShaderString,
            kLightingFragmentShaderString, ion::base::AllocatorPtr()));
    demoutils::AddUniformToNode(global_reg, "uProjectionMatrix",
                                Matrix4f::Identity(), node);
    demoutils::AddUniformToNode(global_reg, "uModelviewMatrix",
                                TranslationMatrix(Vector3f(-1.5f, 1.5f, 0.0f)),
                                node);
    demoutils::AddUniformToNode(global_reg, "uBaseColor",
                                Vector4f(0.9f, 0.5f, 0.2f, 1.0f), node);
    root->AddChild(node);

    ion::gfxutils::BoxSpec box_spec;
    box_spec.usage_mode = ion::gfx::BufferObject::kStreamDraw;
    box_spec.vertex_type = ion::gfxutils::ShapeSpec::kPositionNormal;
    box_spec.size.Set(2.f, 2.f, 2.f);
    node->AddShape(ion::gfxutils::BuildBoxShape(box_spec));
    s_nodes_with_uniforms.push_back(node);
  }

  // Untextured tetrahedron on the top right without indices.
  {
    ion::gfx::NodePtr node(new ion::gfx::Node);
    demoutils::AddUniformToNode(global_reg, "uProjectionMatrix",
                                Matrix4f::Identity(), node);
    demoutils::AddUniformToNode(global_reg, "uModelviewMatrix",
                                TranslationMatrix(Vector3f(1.5f, 1.5f, 0.0f)),
                                node);
    demoutils::AddUniformToNode(global_reg, "uBaseColor",
                                Vector4f(0.3f, 0.8f, 0.5f, 1.0f), node);
    root->AddChild(node);

    ion::gfx::BufferObjectPtr tetra_bo = BuildTetrahedronBufferObject();
    ion::gfx::AttributeArrayPtr attribute_array(new ion::gfx::AttributeArray);
    Vertex v;
    ion::gfxutils::BufferToAttributeBinder<Vertex>(v)
        .Bind(v.position, "aVertex")
        .Apply(global_reg, attribute_array, tetra_bo);
    ion::gfx::ShapePtr shape = BuildTetrahedronShape(attribute_array);
    node->AddShape(shape);
    s_nodes_with_uniforms.push_back(node);
  }

  // Textured cube on the bottom left using indices. Texturing requires a
  // texturing shader.
  {
    ion::gfx::NodePtr node(new ion::gfx::Node);

    ion::gfx::ShaderInputRegistryPtr reg(
        new ion::gfx::ShaderInputRegistry);
    reg->IncludeGlobalRegistry();
    reg->Add(ion::gfx::ShaderInputRegistry::UniformSpec(
        "texture_matrix", ion::gfx::kMatrix4x4Uniform,
        "Matrix applied to texture coordinates"));
    reg->Add(ion::gfx::ShaderInputRegistry::UniformSpec(
        "sampler", ion::gfx::kTextureUniform,
        "Smiley texture sampler"));

    node->SetShaderProgram(
        ion::gfx::ShaderProgram::BuildFromStrings(
            "texture shader", reg, kTextureVertexShaderString,
            kTextureFragmentShaderString, ion::base::AllocatorPtr()));

    demoutils::AddUniformToNode(global_reg, "uProjectionMatrix",
                                Matrix4f::Identity(), node);
    demoutils::AddUniformToNode(global_reg, "uModelviewMatrix",
                                TranslationMatrix(Vector3f(-1.5f, -1.5f, 0.0f)),
                                node);
    demoutils::AddUniformToNode(node->GetShaderProgram()->GetRegistry(),
                                "texture_matrix", Matrix4f::Identity(), node);
    ion::gfx::TexturePtr texture(new ion::gfx::Texture);
    texture->SetImage(0U, BuildTextureImage());
    ion::gfx::SamplerPtr sampler(new ion::gfx::Sampler);
    texture->SetSampler(sampler);
    // This is required for textures on iOS. No other texture wrap mode seems to
    // be supported.
    sampler->SetWrapS(ion::gfx::Sampler::kClampToEdge);
    sampler->SetWrapT(ion::gfx::Sampler::kClampToEdge);
    demoutils::AddUniformToNode(node->GetShaderProgram()->GetRegistry(),
                                "sampler", texture, node);
    root->AddChild(node);

    ion::gfxutils::BoxSpec box_spec;
    box_spec.usage_mode = ion::gfx::BufferObject::kStreamDraw;
    box_spec.size.Set(2.f, 2.f, 2.f);
    node->AddShape(ion::gfxutils::BuildBoxShape(box_spec));
    s_textured_shader_node = node;
    s_nodes_with_uniforms.push_back(node);
  }

  return root;
}

static void MoveVertex(uint64 frame_count, size_t index, Vector3f* position) {
  static const uint32 kPhaseLength = 500U;
  const uint64 c = frame_count % (2U * kPhaseLength);
  const float t = 1.0f / static_cast<float>(2U * kPhaseLength);

  if (c < kPhaseLength) {
    // X-growing, Y-shrinking phase.
    position[0] *= 1.0f + t;
    position[1] *= 1.0f - t;
  } else {
    // X-shrinking, Y-growing phase.
    position[0] *= 1.0f - t;
    position[1] *= 1.0f + t;
  }
}

static void MoveVertices(uint64 frame_count, const ion::gfx::NodePtr& root) {
  const ion::base::AllocVector<ion::gfx::ShapePtr>& shapes = root->GetShapes();
  for (size_t i = 0; i < shapes.size(); ++i) {
    const ion::gfx::AttributeArrayPtr& attribute_array =
        shapes[i]->GetAttributeArray();
    if (attribute_array.Get()) {
      // Get the first attribute (which always happens to be position).
      const ion::gfx::Attribute& a = attribute_array->GetAttribute(0U);
      if (a.IsValid()) {
        // Get the vertex buffer.
        const ion::gfx::BufferObjectElement& element =
            a.GetValue<ion::gfx::BufferObjectElement>();
        if (!ion::base::IsInvalidReference(element)) {
          const ion::gfx::BufferObjectPtr& vb = element.buffer_object;
          const ion::base::DataContainerPtr& container = vb->GetData();
          // Get a generic pointer to the data. This assumes the position
          // always comes first in each vertex in the data.
          uint8* data = container->GetMutableData<uint8>();
          const size_t num_verts = vb->GetCount();
          const size_t vertex_size = vb->GetStructSize();
          for (size_t i = 0; i < num_verts; ++i)
            MoveVertex(frame_count, i,
                       reinterpret_cast<Vector3f*>(data + i * vertex_size));
        }
      }
    }
  }

  // Recurse on children.
  const ion::base::AllocVector<ion::gfx::NodePtr>& children =
      root->GetChildren();
  for (size_t i = 0; i < children.size(); ++i)
    MoveVertices(frame_count, children[i]);
}

static void AnimateTextureMatrix(uint64 frame_count,
                                 const ion::gfx::NodePtr& node) {
  static Point3f center(0.5f, 0.5f, 0.0f);
  static float angle_in_degrees = 0.0f;

  // Rotate about the center.
  const Matrix4f texture_matrix =
      TranslationMatrix(center) *
      RotationMatrixAxisAngleH(Vector3f::AxisZ(),
                               Anglef::FromDegrees(angle_in_degrees)) *
      TranslationMatrix(-center);
  demoutils::SetUniformInNode(2, texture_matrix, node);

  angle_in_degrees += 0.08f;
}

}  // anonymous namespace

//-----------------------------------------------------------------------------
//
// IonDraw class.
//
//-----------------------------------------------------------------------------

class IonDraw : public ViewerDemoBase {
 public:
  IonDraw(int width, int height);
  ~IonDraw() override;
  void Resize(int width, int height) override;
  void Update() override {}
  void RenderFrame() override;
  void Keyboard(int key, int x, int y, bool is_press) override;
  std::string GetDemoClassName() const override { return "IonDraw"; }

 private:
  void UpdateViewUniforms() override;

  ion::gfx::NodePtr root_;
  Hud hud_;
};

IonDraw::IonDraw(int width, int height)
    : ViewerDemoBase(width, height),
      hud_(ion::text::FontManagerPtr(new ion::text::FontManager),
           GetShaderManager(), width, height) {
  root_ = BuildGraph(width, height);

  // Set up viewing.
  SetTrackballRadius(4.0f);
  SetNodeWithViewUniforms(root_);
  UpdateViewUniforms();

  Hud::TextRegion fps_region;
  fps_region.resize_policy = Hud::TextRegion::kFixedSize;
  fps_region.target_point.Set(0.5f, 0.02f);
  fps_region.target_size.Set(0.15f, 0.025f);
  fps_region.horizontal_alignment = ion::text::kAlignHCenter;
  fps_region.vertical_alignment = ion::text::kAlignBottom;
  hud_.InitFps(4, 2, fps_region);
  hud_.GetRootNode()->SetLabel("HUD");

  ion::portgfx::SetSwapInterval(0);

  // Set up the remote handlers.
  std::vector<ion::gfx::NodePtr> tracked_nodes;
  tracked_nodes.push_back(root_);
  tracked_nodes.push_back(hud_.GetRootNode());
  InitRemoteHandlers(tracked_nodes);
}

IonDraw::~IonDraw() {
  s_textured_shader_node.Reset(nullptr);
  s_nodes_with_uniforms.clear();
}

void IonDraw::Keyboard(int key, int x, int y, bool is_press) {
  if (!is_press)
    return;
  switch (key) {
    case 'q':
      exit(0);
      break;

    case 'e':     // Toggle OpenGL error checking.
      GetGraphicsManager()->EnableErrorChecking(
          !GetGraphicsManager()->IsErrorCheckingEnabled());
      LOG(INFO) << "OpenGL error-checking is now "
                << (GetGraphicsManager()->IsErrorCheckingEnabled() ?
                    "on" : "off");
      break;

    case 'h':     // Enable HUD showing frames per second.
      hud_.EnableFps(!hud_.IsFpsEnabled());
      break;

    case 'm':     // Toggle texture matrix animation.
      s_texture_matrix_animation = !s_texture_matrix_animation;
      break;

    case 's':     // Toggle scene.
      s_draw_scene = !s_draw_scene;
      break;

    case 'v':     // Toggle vertex motion.
      s_vertex_motion = !s_vertex_motion;
      break;

    default:
      break;
  }
}

void IonDraw::Resize(int width, int height) {
  ViewerDemoBase::Resize(width, height);
  hud_.Resize(width, height);

  DCHECK(root_->GetStateTable().Get());
  root_->GetStateTable()->SetViewport(
      Range2i::BuildWithSize(Point2i(0, 0), Vector2i(width, height)));
}

void IonDraw::RenderFrame() {
  if (s_vertex_motion)
    MoveVertices(GetFrame()->GetCounter(), root_);
  if (s_texture_matrix_animation)
    AnimateTextureMatrix(GetFrame()->GetCounter(), s_textured_shader_node);

  if (s_draw_scene)
    GetRenderer()->DrawScene(root_);
  hud_.Update();
  GetRenderer()->DrawScene(hud_.GetRootNode());
}

void IonDraw::UpdateViewUniforms() {
  ViewerDemoBase::UpdateViewUniforms();

  // Replace the projection matrix in all non-root nodes with uniforms.  This
  // assumes that uProjectionMatrix is the first uniform in all such nodes.
  const Matrix4f proj = GetProjectionMatrix();
  for (size_t i = 0; i < s_nodes_with_uniforms.size(); ++i) {
    demoutils::SetUniformInNode(0, proj, s_nodes_with_uniforms[i]);
  }
}

std::unique_ptr<DemoBase> CreateDemo(int width, int height) {
  return std::unique_ptr<DemoBase>(new IonDraw(width, height));
}
