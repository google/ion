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

#include "ion/analytics/gpuperformance.h"

#if !ION_PRODUCTION

#include <limits>
#include <sstream>

#include "ion/analytics/benchmarkutils.h"
#include "ion/base/logchecker.h"
#include "ion/gfx/attribute.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfx/tests/fakegraphicsmanager.h"
#include "ion/gfx/tests/testscene.h"
#include "ion/gfx/uniform.h"
#include "ion/gfx/uniformblock.h"
#include "absl/base/macros.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace analytics {

using base::DataContainer;
using base::DataContainerPtr;
using gfx::Attribute;
using gfx::AttributeArray;
using gfx::AttributeArrayPtr;
using gfx::BufferObject;
using gfx::BufferObjectPtr;
using gfx::BufferObjectElement;
using gfx::IndexBufferPtr;
using gfx::IndexBuffer;
using gfx::NodePtr;
using gfx::Node;
using gfx::GraphicsManagerPtr;
using gfx::Renderer;
using gfx::RendererPtr;
using gfx::Shader;
using gfx::ShaderPtr;
using gfx::ShaderInputRegistry;
using gfx::ShaderInputRegistryPtr;
using gfx::ShaderProgram;
using gfx::ShaderProgramPtr;
using gfx::ShapePtr;
using gfx::Shape;
using gfx::StateTablePtr;
using gfx::StateTable;
using gfx::Uniform;
using gfx::UniformBlock;
using gfx::UniformBlockPtr;
using math::Range1i;
using math::Range2i;
using math::Point2i;
using math::Vector4f;
using math::Vector2f;

namespace {

static const char* kMinimalVertexShaderString = (
    "attribute vec3 aVertex;\n"
    "\n"
    "void main(void) {\n"
    "  gl_Position = vec4(aVertex, 1.);\n"
    "  gl_PointSize = 16.0;\n"
    "}\n");

static const char* kMinimalFragmentShaderString = (
    "#ifdef GL_ES\n"
    "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
    "precision highp float;\n"
    "#else\n"
    "precision mediump float;\n"
    "#endif\n"
    "#endif\n"
    "\n"
    "void main(void) {\n"
    "  gl_FragColor = vec4(0., 1., 0., 1.);\n"
    "}\n");

// Simple struct wrapping a test case.
struct TestCaseData {
  GpuPerformanceTester::Enables enables;
  double node_count;
  double primitive_count;
};

static ShaderProgramPtr BuildShader(
    const std::string& id_string,
    const ShaderInputRegistryPtr& registry_ptr,
    const std::string& vertex_shader_string,
    const std::string& fragment_shader_string) {
  ShaderProgramPtr program(new ShaderProgram(registry_ptr));
  program->SetLabel(id_string);
  program->SetVertexShader(ShaderPtr(new Shader(vertex_shader_string)));
  program->SetFragmentShader(ShaderPtr(new Shader(fragment_shader_string)));
  return program;
}

static NodePtr BuildSingleQuad(const ShaderInputRegistryPtr& reg) {
  Vector2f* vertices = new Vector2f[4];
  static const float kHalfSize = 1.f;
  vertices[0].Set(-kHalfSize, -kHalfSize);
  vertices[1].Set(kHalfSize, -kHalfSize);
  vertices[2].Set(-kHalfSize, kHalfSize);
  vertices[3].Set(kHalfSize, kHalfSize);
  AttributeArrayPtr attribute_array(new AttributeArray);
  ShaderInputRegistryPtr registry = reg.Get() ?
    reg : ShaderInputRegistryPtr(new ShaderInputRegistry);

  BufferObjectPtr buffer_object(new BufferObject);
  DataContainerPtr container =
      DataContainer::Create<Vector2f>(vertices,
          DataContainer::ArrayDeleter<Vector2f>, true,
          buffer_object->GetAllocator());
  buffer_object->SetData(container, sizeof(vertices[0]), 4,
                         BufferObject::kStaticDraw);

  const size_t spec =
      buffer_object->AddSpec(BufferObject::kFloat, 4, 0);
  registry->Add(
      ShaderInputRegistry::AttributeSpec(
          "aVertex",
          gfx::kBufferObjectElementAttribute,
          "Doc string not specified."));
  Attribute a =
      registry->Create<Attribute>(
          "aVertex", BufferObjectElement(buffer_object, spec));
  attribute_array->AddAttribute(a);

  ShapePtr shape(new Shape);
  shape->SetPrimitiveType(Shape::kTriangleStrip);
  shape->SetAttributeArray(attribute_array);
  NodePtr node(new Node);
  node->AddShape(shape);
  return node;
}

static NodePtr BuildQuadNodeStrip() {
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  NodePtr node = BuildSingleQuad(reg);
  node->SetShaderProgram(BuildShader("Minimal Shader",
                                     reg,
                                     kMinimalVertexShaderString,
                                     kMinimalFragmentShaderString));
  return node;
}

static NodePtr BuildQuadNode() {
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  NodePtr node = BuildSingleQuad(reg);
  node->SetShaderProgram(BuildShader("Minimal Shader",
                                     reg,
                                     kMinimalVertexShaderString,
                                     kMinimalFragmentShaderString));
  uint16* indices = new uint16[6];
  indices[0] = 0;
  indices[1] = 1;
  indices[2] = 2;
  indices[3] = 1;
  indices[4] = 2;
  indices[5] = 3;
  DataContainerPtr container =
      DataContainer::Create<uint16>(
          indices, DataContainer::ArrayDeleter<uint16>, true,
          node->GetAllocator());
  IndexBufferPtr index_buffer(new IndexBuffer);
  index_buffer->AddSpec(BufferObject::kUnsignedShort, 1, 0);
  index_buffer->SetData(container, sizeof(indices[0]), 6,
                        BufferObject::kStaticDraw);
  node->GetShapes()[0]->SetIndexBuffer(index_buffer);
  node->GetShapes()[0]->SetPrimitiveType(Shape::kTriangles);
  return node;
}

static bool ScenesEqual(const NodePtr& scene1, const NodePtr& scene2) {
  // Compare counts for deep-copied items (since comparators do not generally
  // exist already for those classes) and compare true equality for
  // shallow-copied items.
  // Compare StateTable (deep copy).
  StateTable* state_table1 = scene1->GetStateTable().Get();
  StateTable* state_table2 = scene2->GetStateTable().Get();
  if ((state_table1 && !state_table2) || (!state_table1 && state_table2))
    return false;
  // Compare Shader (shallow copy).
  if (scene1->GetShaderProgram() != scene2->GetShaderProgram())
    return false;
  // Compare Uniforms (deep copy).
  if (scene1->GetUniforms().size() != scene2->GetUniforms().size())
    return false;
  // Compare UniformBlocks (deep copy).
  if (scene1->GetUniformBlocks().size() != scene2->GetUniformBlocks().size())
    return false;
  // Compare Shapes (shallow copy).
  if (scene1->GetShapes().size() != scene2->GetShapes().size())
    return false;
  for (size_t i = 0; i < scene1->GetShapes().size(); ++i)
    if (scene1->GetShapes()[i] != scene2->GetShapes()[i])
      return false;
  // Compare Enables (deep copy).
  if (scene1->IsEnabled() != scene2->IsEnabled())
    return false;
  // Compare Children.
  if (scene1->GetChildren().size() != scene2->GetChildren().size())
    return false;
  for (size_t i = 0; i < scene1->GetChildren().size(); ++i)
    if (!ScenesEqual(scene1->GetChildren()[i], scene2->GetChildren()[i]))
      return false;
  // Comparison complete.
  return true;
}

// Convenience function to return the Benchmark::Descriptor for an
// AccumulatedVariable in a Benchmark. This is just used to make code shorter
// and easier to read.
static const Benchmark::Descriptor& GetVarDescriptor(
    const Benchmark& benchmark, size_t index) {
  return benchmark.GetAccumulatedVariables()[index].descriptor;
}

}  // namespace

class GPUPerformanceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    gl_context_ = gfx::testing::FakeGlContext::Create(kWidth, kHeight);
    portgfx::GlContext::MakeCurrent(gl_context_);
    gm_.Reset(new gfx::testing::FakeGraphicsManager());
  }

  void TearDown() override {
    gm_.Reset(nullptr);
    gl_context_.Reset(nullptr);
  }

  portgfx::GlContextPtr gl_context_;
  gfx::testing::FakeGraphicsManagerPtr gm_;
  static const int kWidth = 400;
  static const int kHeight = 300;
};

TEST_F(GPUPerformanceTest, Enables) {
  base::LogChecker log_checker;
  GpuPerformanceTester perf(kWidth, kHeight);
  // Check defaults.
  EXPECT_EQ(GpuPerformanceTester::kConstants | GpuPerformanceTester::kBaseline |
                GpuPerformanceTester::kNoDraw |
                GpuPerformanceTester::kMinimumViewport |
                GpuPerformanceTester::kGpuMemory |
                GpuPerformanceTester::kGlTrace,
            perf.GetEnables());
  perf.SetEnables(GpuPerformanceTester::kConstants);
  EXPECT_EQ(GpuPerformanceTester::kConstants, perf.GetEnables());
  perf.SetEnables(GpuPerformanceTester::kBaseline);
  EXPECT_EQ(GpuPerformanceTester::kBaseline, perf.GetEnables());
  EXPECT_FALSE(log_checker.HasAnyMessages());

  perf.SetEnables(GpuPerformanceTester::kMinimumViewport |
                  GpuPerformanceTester::kGpuMemory);
  EXPECT_EQ(
      GpuPerformanceTester::kMinimumViewport | GpuPerformanceTester::kGpuMemory,
      perf.GetEnables());
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // Check error.
  perf.SetEnables(GpuPerformanceTester::kMinimumViewport |
                  GpuPerformanceTester::kGpuMemory |
                  GpuPerformanceTester::kResource);
  EXPECT_EQ(
      GpuPerformanceTester::kMinimumViewport | GpuPerformanceTester::kGpuMemory,
      perf.GetEnables());
  EXPECT_TRUE(log_checker.HasMessage(
      "WARNING", "kResource and kGpuMemory are incompatible"));
}

// 
// test, we could test for exact values (and thus absolutely correct values)
// from the accumulation of benchmark measures.  It has happened that the
// values from the benchmark were valid but not correct, requiring an update
// to the math in the GpuPerformanceTester class.
TEST_F(GPUPerformanceTest, TestRunAllMeasurements) {
  RendererPtr renderer(new ion::gfx::Renderer(gm_));
  gfx::testing::TestScene test_scene;
  NodePtr node = test_scene.GetScene();
  node->SetStateTable(gfx::StateTablePtr(new gfx::StateTable));

  const TestCaseData data[] = {{GpuPerformanceTester::kConstants, 3., 89.},
                               {GpuPerformanceTester::kBaseline, 0., 0.},
                               {GpuPerformanceTester::kNoDraw, 0., 0.},
                               {GpuPerformanceTester::kMinimumViewport, 0., 0.},
                               {GpuPerformanceTester::kResource, 0., 0.},
                               {GpuPerformanceTester::kGpuMemory, 0., 0.},
                               {GpuPerformanceTester::kGlTrace, 0., 0.},
                               {GpuPerformanceTester::kAllEnables, 3., 89.}};

  const size_t count = ABSL_ARRAYSIZE(data);
  for (size_t i = 0; i < count; ++i) {
    GpuPerformanceTester perf(kWidth, kHeight);
    perf.SetEnables(data[i].enables);
    const Benchmark perf_entries =
      perf.RunAllMeasurements(node, gm_, renderer);
    EXPECT_GT(perf_entries.GetConstants().size(), 0U);
    EXPECT_GT(perf_entries.GetAccumulatedVariables().size(), 0U);
    // TestScene; Number of primitives = 89.
    EXPECT_EQ(data[i].primitive_count,
              perf_entries.GetConstants()[GpuPerformanceTester::kPrimitiveCount]
                  .value);
    // TestScene; Number of nodes = 3.
    EXPECT_EQ(
        data[i].node_count,
        perf_entries.GetConstants()[GpuPerformanceTester::kNodeCount].value);
    // Verify we have a description and units for each measure
    const size_t num_variables = perf_entries.GetAccumulatedVariables().size();
    for (size_t i = 0; i < num_variables; ++i) {
      const Benchmark::Descriptor& desc =
          perf_entries.GetAccumulatedVariables()[i].descriptor;
      SCOPED_TRACE(testing::Message()
                   << "Checking entry " << desc.id << " (" << i << ")");
      EXPECT_GT(desc.description.size(), 0U);
      EXPECT_GT(desc.units.size(), 0U);
      EXPECT_GT(desc.group.size(), 0U);
    }
    const NodePtr qsnode = BuildQuadNodeStrip();
    const Benchmark perf_entries_strip =
        perf.RunAllMeasurements(qsnode, gm_, renderer);
    EXPECT_LT(0U, perf_entries_strip.GetAccumulatedVariables().size());
    // TestScene has some invalid Shapes.
    gm_->SetErrorCode(GL_NO_ERROR);
  }
}

TEST_F(GPUPerformanceTest, TestAccumulateMeasurements) {
  RendererPtr renderer(new ion::gfx::Renderer(gm_));
  const NodePtr qnode = BuildQuadNode();
  GpuPerformanceTester perf(kWidth, kHeight);
  perf.AccumulateMeasurements(qnode, gm_, renderer);
  perf.AccumulateMeasurements(qnode, gm_, renderer);
  perf.AccumulateMeasurements(qnode, gm_, renderer);
  const NodePtr qsnode = BuildQuadNodeStrip();
  perf.AccumulateMeasurements(qsnode, gm_, renderer);
  const Benchmark perf_entries = perf.GetResults();

  EXPECT_GT(perf_entries.GetConstants().size(), 0U);
  EXPECT_GT(perf_entries.GetAccumulatedVariables().size(), 0U);
  // Number of primitives = 8.
  EXPECT_EQ(perf_entries.GetConstants()
            [GpuPerformanceTester::kPrimitiveCount].value, 8.0);
  // Number of nodes = 4.
  EXPECT_EQ(perf_entries.GetConstants()
            [GpuPerformanceTester::kNodeCount].value, 4);
  // Verify we have a description and units for each measure.
  const size_t num_variables = perf_entries.GetAccumulatedVariables().size();
  for (unsigned int i = 0; i < num_variables; ++i) {
    const Benchmark::AccumulatedVariable& v =
        perf_entries.GetAccumulatedVariables()[i];
    SCOPED_TRACE(testing::Message()
                 << "Checking entry " << v.descriptor.id << " (" << i << ")");
    EXPECT_GE(v.mean, 0.0);
    EXPECT_GE(v.maximum, 0.0);
    EXPECT_GE(v.minimum, 0.0);
    EXPECT_GE(v.maximum, v.minimum);
  }

  // Test that things were reset correctly. Everything should be zero now.
  const Benchmark perf_entries_zero = perf.GetResults();
  // Number of primitives = 0.
  EXPECT_EQ(perf_entries_zero.GetConstants()
            [GpuPerformanceTester::kPrimitiveCount].value, 0.0);
  // Number of nodes = 0.
  EXPECT_EQ(perf_entries_zero.GetConstants()
            [GpuPerformanceTester::kNodeCount].value, 0.0);
  // Only checking non-derived values, which should be exactly zero.
  EXPECT_EQ(perf_entries_zero.GetAccumulatedVariables()
            [GpuPerformanceTester::kFramesPerSecond].mean, 0.0);
  EXPECT_EQ(perf_entries_zero.GetAccumulatedVariables()
            [GpuPerformanceTester::kNoDrawCalls].standard_deviation, 0.0);

  // Should be half the stuff as first run.
  perf.AccumulateMeasurements(qnode, gm_, renderer);
  perf.AccumulateMeasurements(qnode, gm_, renderer);
  const Benchmark perf_entries_half = perf.GetResults();
  // Number of primitives = 4.
  EXPECT_EQ(perf_entries_half.GetConstants()
            [GpuPerformanceTester::kPrimitiveCount].value, 4);
  // Number of nodes = 2.
  EXPECT_EQ(perf_entries_half.GetConstants()
            [GpuPerformanceTester::kNodeCount].value, 2);
}

TEST_F(GPUPerformanceTest, IdTokenTest) {
  RendererPtr renderer(new ion::gfx::Renderer(gm_));
  const NodePtr qnode = BuildQuadNode();
  GpuPerformanceTester perf(kWidth, kHeight);
  const Benchmark perf_entries =
    perf.RunAllMeasurements(qnode, gm_, renderer);
  // 
  // standardized.
  typedef GpuPerformanceTester GPT;

  // Constants.
  EXPECT_EQ(perf_entries.GetConstants()[GPT::kNodeCount].descriptor.id,
            std::string("Node Count"));
  EXPECT_EQ(perf_entries.GetConstants()[GPT::kShapeCount].descriptor.id,
            std::string("Shape Count"));
  EXPECT_EQ(perf_entries.GetConstants()[GPT::kDrawCount].descriptor.id,
            std::string("Draw Count"));
  EXPECT_EQ(perf_entries.GetConstants()[GPT::kVertexCount].descriptor.id,
            std::string("Vertex Count"));
  EXPECT_EQ(perf_entries.GetConstants()[GPT::kPrimitiveCount].descriptor.id,
            std::string("Primitive Count"));
  EXPECT_EQ(perf_entries.GetConstants()[GPT::kTriangleCount].descriptor.id,
            std::string("Triangle Count"));
  EXPECT_EQ(perf_entries.GetConstants()[GPT::kLineCount].descriptor.id,
            std::string("Line Count"));
  EXPECT_EQ(perf_entries.GetConstants()[GPT::kPointCount].descriptor.id,
            std::string("Point Count"));
  EXPECT_EQ(perf_entries.GetConstants()[GPT::kTrianglePercent].descriptor.id,
            std::string("Triangle Percent"));
  EXPECT_EQ(perf_entries.GetConstants()[GPT::kLinePercent].descriptor.id,
            std::string("Line Percent"));
  EXPECT_EQ(perf_entries.GetConstants()[GPT::kPointPercent].descriptor.id,
            std::string("Point Percent"));
  EXPECT_EQ(perf_entries.GetConstants()[GPT::kVerticesPerShape].descriptor.id,
            std::string("Vertices Per Shape"));
  EXPECT_EQ(perf_entries.GetConstants()[GPT::kPrimitivesPerShape].descriptor.id,
            std::string("Primitives Per Shape"));
  EXPECT_EQ(perf_entries.GetConstants()[GPT::kTrialCount].descriptor.id,
            std::string("Trial Count"));
  EXPECT_EQ(perf_entries.GetConstants()[GPT::kBindShaderCount].descriptor.id,
            std::string("Bind Shader Count"));
  EXPECT_EQ(perf_entries.GetConstants()[GPT::kBindTextureCount].descriptor.id,
            std::string("Bind Texture Count"));
  EXPECT_EQ(perf_entries.GetConstants()[GPT::kSetUniformCount].descriptor.id,
            std::string("Set Uniform Count"));
  EXPECT_EQ(perf_entries.GetConstants()[GPT::kBufferMemory].descriptor.id,
            std::string("Buffer Memory"));
  EXPECT_EQ(perf_entries.GetConstants()[GPT::kFboMemory].descriptor.id,
            std::string("FBO Memory"));
  EXPECT_EQ(perf_entries.GetConstants()[GPT::kTextureMemory].descriptor.id,
            std::string("Texture Memory"));
  EXPECT_EQ(perf_entries.GetConstants()[GPT::kTotalGpuMemory].descriptor.id,
            std::string("Total GPU Memory"));

  // Variables.
  EXPECT_EQ(GetVarDescriptor(perf_entries, GPT::kFramesPerSecond).id,
            std::string("Frames Per Second"));
  EXPECT_EQ(GetVarDescriptor(perf_entries, GPT::kNodesPerSecond).id,
            std::string("Nodes Per Second"));
  EXPECT_EQ(GetVarDescriptor(perf_entries, GPT::kShapesPerSecond).id,
            std::string("Shapes Per Second"));
  EXPECT_EQ(GetVarDescriptor(perf_entries, GPT::kDrawCallsPerSecond).id,
            std::string("Draw Calls Per Second"));
  EXPECT_EQ(GetVarDescriptor(perf_entries, GPT::kVerticesPerSecond).id,
            std::string("Vertices Per Second"));
  EXPECT_EQ(GetVarDescriptor(perf_entries, GPT::kPrimitivesPerSecond).id,
            std::string("Primitives Per Second"));
  EXPECT_EQ(GetVarDescriptor(perf_entries, GPT::kPixelsPerSecond).id,
            std::string("Pixels Per Second"));
  EXPECT_EQ(GetVarDescriptor(perf_entries, GPT::kRenderTime).id,
            std::string("Render Time"));
  EXPECT_EQ(GetVarDescriptor(perf_entries, GPT::kResourceCreation).id,
            std::string("Resource Creation"));
  EXPECT_EQ(GetVarDescriptor(perf_entries, GPT::kNoDrawCalls).id,
            std::string("No Draw Calls"));
  EXPECT_EQ(GetVarDescriptor(perf_entries, GPT::kMinViewport).id,
            std::string("Min Viewport"));
  EXPECT_EQ(GetVarDescriptor(perf_entries, GPT::kTransformRate).id,
            std::string("Transform Rate"));
  EXPECT_EQ(GetVarDescriptor(perf_entries, GPT::kFillRate).id,
            std::string("Fill Rate"));
  EXPECT_EQ(GetVarDescriptor(perf_entries, GPT::kTraversalPercent).id,
            std::string("Traversal Percent"));
  EXPECT_EQ(GetVarDescriptor(perf_entries, GPT::kTransformPercent).id,
            std::string("Transform Percent"));
  EXPECT_EQ(GetVarDescriptor(perf_entries, GPT::kFillPercent).id,
            std::string("Fill Percent"));
}

TEST_F(GPUPerformanceTest, PrintTests) {
  RendererPtr renderer(new ion::gfx::Renderer(gm_));
  const NodePtr qnode = BuildQuadNode();
  GpuPerformanceTester perf(kWidth, kHeight);
  const Benchmark perf_entries =
    perf.RunAllMeasurements(qnode, gm_, renderer);
  // Verify that no string fields have commas in them.
  size_t entry_count = perf_entries.GetAccumulatedVariables().size();
  for (size_t i = 0; i < entry_count; ++i) {
    const Benchmark::Descriptor& desc = GetVarDescriptor(perf_entries, i);
    EXPECT_EQ(std::string::npos, desc.id.find(","));
    EXPECT_EQ(std::string::npos, desc.group.find(","));
    EXPECT_EQ(std::string::npos, desc.description.find(","));
  }
  // CSV Print.
  std::stringstream ss_csv;
  OutputBenchmarkAsCsv(perf_entries, ss_csv);
  std::string csv_string = ss_csv.str();
  EXPECT_GT(csv_string.find("Group"), 0U);
  EXPECT_GT(csv_string.find("Triangles Per Second,"), 0U);

  // Terse Print.
  std::stringstream ss_pretty;
  OutputBenchmarkPretty("Test Pretty", true, perf_entries, ss_pretty);
  std::string pretty_string = ss_pretty.str();
  EXPECT_GT(pretty_string.find("\"Renderable"), 0U);
  EXPECT_GT(pretty_string.find("Fill Rate"), 0U);
}

TEST_F(GPUPerformanceTest, InstanceCopy) {
  NodePtr node1 = BuildQuadNode();
  NodePtr node2 = BuildQuadNodeStrip();
  StateTablePtr dummy_state_table = StateTablePtr(new StateTable);
  node1->SetStateTable(dummy_state_table);
  Uniform uniform1, uniform2, uniform3;
  node1->AddUniform(uniform1);
  node1->AddUniform(uniform2);
  node2->AddUniform(uniform3);
  node1->AddUniformBlock(UniformBlockPtr(new UniformBlock()));
  node2->AddUniformBlock(UniformBlockPtr(new UniformBlock()));
  node2->AddUniformBlock(UniformBlockPtr(new UniformBlock()));
  node1->Enable(true);
  node2->Enable(false);
  node1->AddChild(node2);
  NodePtr scene1 = node1;
  NodePtr scene2 = GpuPerformanceTester::InstanceCopy(scene1);
  EXPECT_TRUE(ScenesEqual(scene1, scene2));
}

}  // namespace analytics
}  // namespace ion

#endif  // !ION_PRODUCTION
