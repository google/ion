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

#include <algorithm>
#include <iomanip>
#include <limits>

#include "ion/gfx/attribute.h"
#include "ion/gfx/bufferobject.h"
#include "ion/gfx/indexbuffer.h"
#include "ion/gfx/shape.h"
#include "ion/gfx/statetable.h"
#include "ion/gfx/tracecallextractor.h"
#include "ion/math/range.h"
#include "ion/math/vector.h"
#include "ion/port/timer.h"

namespace ion {
using analytics::Benchmark;
using base::AllocatorPtr;
using gfx::Attribute;
using gfx::BufferObjectElement;
using gfx::ImagePtr;
using gfx::IndexBufferPtr;
using gfx::IndexBuffer;
using gfx::NodePtr;
using gfx::Node;
using gfx::GraphicsManagerPtr;
using gfx::Renderer;
using gfx::RendererPtr;
using gfx::ShapePtr;
using gfx::Shape;
using gfx::StateTablePtr;
using gfx::StateTable;
using math::Range1i;
using math::Range2i;
using math::Point2i;
using math::Vector4f;

namespace analytics {

namespace {

// On Android, glFinish seems to have significant overhead, so perform multiple
// draw iterations per glFinish to improve accuracy.
static const int kInnerTrialCount = 20;

static const double kMinValue = 1e-20;
static const double kMaxValue = 1e20;

static const double kToPercent = 1e2;
static const double kToKilo = 1e-3;
static const double kToMega = 1e-6;
static const double kToMilli = 1e3;

static const char *kUseProgramString = "UseProgram";
static const char *kBindTextureString = "BindTexture";
static const char *kUniformString = "Uniform";

// Bring Measurement into this scope.
typedef GpuPerformanceTester::Measurement Measurement;


static const char kSceneConstantsGroup[] = "Scene constants";

static const Benchmark::StaticDescriptor kNodeCountDescriptor(
    "Node Count", kSceneConstantsGroup, "Nodes in Ion scene graph", "nodes");

static const Benchmark::StaticDescriptor kShapeCountDescriptor(
    "Shape Count", kSceneConstantsGroup,
    "Shapes in Ion scene graph", "shapes");

static const Benchmark::StaticDescriptor kDrawCountDescriptor(
    "Draw Count", kSceneConstantsGroup,
    "Draw calls in Ion scene graph", "draw calls");

static const Benchmark::StaticDescriptor kVertexCountDescriptor(
    "Vertex Count", kSceneConstantsGroup,
    "Vertices in scene", "vertices");

static const Benchmark::StaticDescriptor kPrimitiveCountDescriptor(
    "Primitive Count", kSceneConstantsGroup,
    "Renderable elements in scene: Triangles; points; etc.", "primitives");

static const Benchmark::StaticDescriptor kTriangleCountDescriptor(
    "Triangle Count", kSceneConstantsGroup,
    "Renderable triangles in scene", "triangles");

static const Benchmark::StaticDescriptor kLineCountDescriptor(
    "Line Count", kSceneConstantsGroup,
    "Renderable lines in scene", "lines");

static const Benchmark::StaticDescriptor kPointCountDescriptor(
    "Point Count", kSceneConstantsGroup,
    "Renderable points in scene", "points");

static const Benchmark::StaticDescriptor kPatchCountDescriptor(
    "Patch Count", kSceneConstantsGroup,
    "Renderable patches  in scene", "patches");

static const Benchmark::StaticDescriptor kTrianglePercentDescriptor(
    "Triangle Percent", kSceneConstantsGroup,
    "Percent of primitives that are triangles", "%");

static const Benchmark::StaticDescriptor kLinePercentDescriptor(
    "Line Percent", kSceneConstantsGroup,
    "Percent of primitives that are lines", "%");

static const Benchmark::StaticDescriptor kPointPercentDescriptor(
    "Point Percent", kSceneConstantsGroup,
    "Percent of primitives that are points", "%");

static const Benchmark::StaticDescriptor kPatchPercentDescriptor(
    "Patch Percent", kSceneConstantsGroup,
    "Percent of primitives that are patches", "%");

static const Benchmark::StaticDescriptor kVerticesPerShapeDescriptor(
    "Vertices Per Shape", kSceneConstantsGroup,
    "Average number of vertices per shape (draw call)", "vertices/shape");

static const Benchmark::StaticDescriptor kPrimitivesPerShapeDescriptor(
    "Primitives Per Shape", kSceneConstantsGroup,
    "Average number of primitives per shape (draw call)", "primitives/shape");

static const Benchmark::StaticDescriptor kTrialCountDescriptor(
    "Trial Count", kSceneConstantsGroup,
    "Number of trials used to compute averages.", "frames");

static const Benchmark::StaticDescriptor kBindShaderCountDescriptor(
    "Bind Shader Count", kSceneConstantsGroup,
    "Number of bind shader calls.", "binds");

static const Benchmark::StaticDescriptor kBindTextureCountDescriptor(
    "Bind Texture Count", kSceneConstantsGroup,
    "Number of bind shader calls.", "binds");

static const Benchmark::StaticDescriptor kSetUniformCountDescriptor(
    "Set Uniform Count", kSceneConstantsGroup,
    "Number of uniform value set calls.", "set uniforms");

static const Benchmark::StaticDescriptor kBufferMemoryDescriptor(
    "Buffer Memory", kSceneConstantsGroup,
    "GPU Buffer memory used during the frame", "MB");

static const Benchmark::StaticDescriptor kFboMemoryDescriptor(
    "FBO Memory", kSceneConstantsGroup,
    "GPU Framebuffer Object memory used during the frame", "MB");

static const Benchmark::StaticDescriptor kTextureMemoryDescriptor(
    "Texture Memory", kSceneConstantsGroup,
    "GPU texture memory used during the frame", "MB");

static const Benchmark::StaticDescriptor kFramebufferMemoryDescriptor(
    "Framebuffer Memory", kSceneConstantsGroup, "GPU Framebuffer memory", "MB");

static const Benchmark::StaticDescriptor kTotalGpuMemoryDescriptor(
    "Total GPU Memory", kSceneConstantsGroup,
    "Total GPU memory used during the frame (excluding frame buffer)", "MB");

static const char kSceneRatesGroup[] = "Scene Rates";

static const Benchmark::StaticDescriptor kFramesPerSecondDescriptor(
    "Frames Per Second", kSceneRatesGroup, "Frames per second.",
    "frames/s");

static const Benchmark::StaticDescriptor kNodesPerSecondDescriptor(
    "Nodes Per Second", kSceneRatesGroup,
    "Nodes per second.", "Knodes/s");

static const Benchmark::StaticDescriptor kShapesPerSecondDescriptor(
    "Shapes Per Second", kSceneRatesGroup,
    "Shapes per second.", "Kshapes/s");

static const Benchmark::StaticDescriptor kDrawCallsPerSecondDescriptor(
    "Draw Calls Per Second", kSceneRatesGroup,
    "Draw calls per second.", "Kdraws/s");

static const Benchmark::StaticDescriptor kVerticesPerSecondDescriptor(
    "Vertices Per Second", kSceneRatesGroup,
    "Millions of vertices per second.", "Mvertices/s");

static const Benchmark::StaticDescriptor kPrimitivesPerSecondDescriptor(
    "Primitives Per Second", kSceneRatesGroup,
    "Millions of primitives per second.", "Mprimitives/s");

static const Benchmark::StaticDescriptor kPixelsPerSecondDescriptor(
    "Pixels Per Second", kSceneRatesGroup,
    "Millions of pixels per second.", "Mpixels/s");

static const char kTimingsGroup[] = "Timings";

static const Benchmark::StaticDescriptor kRenderTimeDescriptor(
    "Render Time", kTimingsGroup,
    "Time to render unmodified scene.", "ms/frame");

static const Benchmark::StaticDescriptor kResourceCreationDescriptor(
    "Resource Creation", kTimingsGroup,
    "Time creating GL resources; CPU-GPU Bandwidth.", "ms/frame");

static const Benchmark::StaticDescriptor kNoDrawCallsDescriptor(
    "No Draw Calls", kTimingsGroup,
    "Ion & OpenGL state change time; draw calls ignored.", "ms/frame");

static const Benchmark::StaticDescriptor kMinViewportDescriptor(
    "Min Viewport", kTimingsGroup,
    "Render time with no fill; vertex transform only.", "ms/frame");

static const char kRatesBreakdownGroup[] = "Rates Breakdown";

static const Benchmark::StaticDescriptor kTransformRateDescriptor(
    "Transform Rate", kRatesBreakdownGroup,
    "Approximate Vertex Program performance.", "Mtriangles/s");

static const Benchmark::StaticDescriptor kFillRateDescriptor(
    "Fill Rate", kRatesBreakdownGroup,
    "Approximate Fragment Program performance.", "Mpixels/s");

const char kPercentBreakdownGroup[] = "Percent Breakdown";

static const Benchmark::StaticDescriptor kTraversalPercentDescriptor(
    "Traversal Percent", kPercentBreakdownGroup,
    "Approximate Ion and OpenGL API overhead.", "%");

static const Benchmark::StaticDescriptor kTransformPercentDescriptor(
    "Transform Percent", kPercentBreakdownGroup,
    "Approximate Vertex Program utilization.", "%");

static const Benchmark::StaticDescriptor kFillPercentDescriptor(
    "Fill Percent", kPercentBreakdownGroup,
    "Approximate Fragment Program utilization.", "%");

// ApplyToTree calls Functor f on each node in a scene graph.
template <typename FUNCTOR>
void ApplyToTree(const NodePtr& node, FUNCTOR& f) {  // NOLINT
  if (node.Get()) {
    f(node);
    const size_t size = node->GetChildren().size();
    for (size_t i = 0; i < size; ++i) {
      ApplyToTree(node->GetChildren()[i], f);
    }
  }
}

// NullifyViewport is a functor that insures all viewport states are "Null",
// i.e., minimal size, which is actualy (0,0)--(1,1) (a 1 pixel viewport).
struct MinifyViewport {
  void operator()(const NodePtr& node) {
    DCHECK(node.Get());
    if (StateTable* state_table = node->GetStateTable().Get()) {
      static const Range2i kZeroRange(Point2i(0, 0), Point2i(0, 0));
      static const Range2i kPixelRange(Point2i(0, 0), Point2i(1, 0));
      if (!state_table->GetViewport().IsEmpty()) {
        state_table->SetViewport(kPixelRange);
      }
    }
  }
};

// Disable depth test, turns off depth test for nodes that have a state table.
struct DisableDepthTest {
  void operator()(const NodePtr& node) {
    DCHECK(node.Get());
    if (StateTable* state_table = node->GetStateTable().Get()) {
      state_table->ResetCapability(StateTable::kDepthTest);
    }
  }
};

// Functor that clears all shapes from a scene graph.
struct RemoveGeometry {
  void operator()(const NodePtr& node) {
    DCHECK(node.Get());
    node->ClearShapes();
  }
};

// Functor that counts the number of nodes and total primitives rendered.
struct CountPrimitives {
  void operator()(const NodePtr& node) {
    DCHECK(node.Get());
    ++node_count;
    // Count geometry
    const size_t size = node->GetShapes().size();
    shape_count += size;
    for (size_t i = 0; i < size; ++i) {
      const ShapePtr& shape = node->GetShapes()[i];
      GLint patch_vertices = shape->GetPatchVertices();
      size_t count = 0;
      // If the shape has ranges, sum them.
      if (const size_t range_count = shape->GetVertexRangeCount()) {
        for (size_t r = 0; r < range_count; ++r) {
          if (shape->IsVertexRangeEnabled(r)) {
            count += shape->GetVertexRange(r).GetSize();
            draw_count += 1;
          }
        }
      } else {
        // No ranges, so use all indices or vertices.
        if (IndexBuffer* index_buffer = shape->GetIndexBuffer().Get()) {
          // Indexed shape; look at the index buffer.
          count += index_buffer->GetCount();
        } else {
          // Nonindexed shape; need to dig into attribute array.
          if (shape->GetAttributeArray().Get() &&
              shape->GetAttributeArray()->GetBufferAttributeCount()) {
            const Attribute& attrib =
              shape->GetAttributeArray()->GetBufferAttribute(0);
            const BufferObjectElement& buffer_element =
              attrib.GetValue<BufferObjectElement>();
            count = buffer_element.buffer_object->GetCount();
          }
        }
        draw_count += 1;
      }
      vertex_count += count;
      // Correct the count based on the primitive being rendered.
      switch (shape->GetPrimitiveType()) {
        case Shape::kLines:
          count /= 2;
          line_count += count;
          break;
        case Shape::kLineLoop:
          line_count += count;
          break;
        case Shape::kLineStrip:
          count -= 1;
          line_count += count;
          break;
        case Shape::kPoints:
          point_count += count;
          break;
        case Shape::kTriangles:
          count /= 3;
          triangle_count += count;
          break;
        case Shape::kTriangleFan:
        case Shape::kTriangleStrip:
          count -= 2;
          triangle_count += count;
          break;
        case Shape::kPatches:
          count /= patch_vertices;
          patch_count += count;
          break;
      }
    }  // for each shape
  }
  size_t node_count = 0;
  size_t shape_count = 0;
  size_t draw_count = 0;
  size_t triangle_count = 0;
  size_t line_count = 0;
  size_t point_count = 0;
  size_t patch_count = 0;
  size_t vertex_count = 0;
};

static NodePtr GetClearNode(uint32 width, uint32 height,
                            const AllocatorPtr& allocator) {
  NodePtr clear_node(new(allocator) Node);
  StateTablePtr clear_state_table(new(allocator) StateTable(width, height));
  clear_state_table->SetViewport(Range2i(Point2i(0, 0),
                                         Point2i(width, height)));
  clear_state_table->SetClearColor(Vector4f(1.f, 0.f, 0.f, 1.0f));
  clear_state_table->SetClearDepthValue(1.f);
  clear_node->SetStateTable(clear_state_table);
  return clear_node;
}

static void ResetVariable(Benchmark::AccumulatedVariable* variable) {
  DCHECK(variable);
  variable->samples = 0;
  variable->minimum = kMaxValue;
  variable->maximum = kMinValue;
  variable->mean = 0;
  variable->standard_deviation = 0;
}

static void ClearVariable(Benchmark::AccumulatedVariable* variable) {
  DCHECK(variable);
  variable->samples = 0;
  variable->minimum = 0;
  variable->maximum = 0;
  variable->mean = 0;
  variable->standard_deviation = 0;
}

static void AccumulateVariable(Benchmark::AccumulatedVariable* variable,
                               const GpuPerformanceTester::Measurement& m) {
  DCHECK(variable);
  // Accumulate mean via summation, this works because the number of samples
  // match, i.e:
  // Sum(x_i + y_i) / samples = Sum(x_i) / samples + Sum(y_i) / samples
  variable->mean += m.mean;
  // When adding or subtracting random variables, the standard deviations sum.
  variable->standard_deviation += m.deviation;
  DCHECK_NE(m.maximum, kMinValue);
  DCHECK_NE(m.minimum, kMaxValue);
  if (variable->maximum != kMinValue) {
    variable->maximum += m.maximum;
  } else {
    variable->maximum = m.maximum;
  }
  if (variable->minimum != kMaxValue) {
    variable->minimum += m.minimum;
  } else {
    variable->minimum = m.minimum;
  }
}

static void AccumulateInverseVariable(
    Benchmark::AccumulatedVariable* inverse_variable,
    const GpuPerformanceTester::Measurement& m) {
  DCHECK(inverse_variable);
  if (inverse_variable->mean != 0.0) {
    inverse_variable->mean = 1.0 / (1.0 / inverse_variable->mean +
                                    1.0 / m.inverse_mean);
  } else {
    inverse_variable->mean = m.inverse_mean;
  }
  if (inverse_variable->standard_deviation != 0.0) {
    inverse_variable->standard_deviation =
      1.0 / (1.0 / inverse_variable->standard_deviation +
             1.0 / m.inverse_deviation);
  } else {
    inverse_variable->standard_deviation = m.inverse_deviation;
  }
  DCHECK_NE(m.maximum, kMinValue);
  DCHECK_NE(m.minimum, kMaxValue);
  if (inverse_variable->maximum != kMinValue) {
    inverse_variable->maximum = 1.0 / (1.0 / inverse_variable->maximum +
                                       m.minimum);
  } else {
    inverse_variable->maximum = 1.0 / m.minimum;
  }
  if (inverse_variable->minimum != kMaxValue) {
    inverse_variable->minimum = 1.0 / (1.0 / inverse_variable->minimum +
                                       m.maximum);
  } else {
    inverse_variable->minimum = 1.0 / m.maximum;
  }
}

}  // namespace

// InstanceCopy creates a copy of a scene graph with shared geometry,
// copied state table, and copied uniforms.
NodePtr GpuPerformanceTester::InstanceCopy(const NodePtr& scene) {
  DCHECK(scene.Get());
  NodePtr instance(new(scene->GetAllocator()) Node);
  // State Table deep copy.
  if (StateTable* scene_state_table = scene->GetStateTable().Get()) {
    StateTablePtr state_table(
        new(scene_state_table->GetAllocator()) StateTable);
    state_table->CopyFrom(*scene_state_table);
    instance->SetStateTable(state_table);
  }
  // Shader shallow copy.
  instance->SetShaderProgram(scene->GetShaderProgram());
  // Uniform deep copy.
  const size_t uniform_size = scene->GetUniforms().size();
  for (size_t i = 0; i < uniform_size; ++i) {
    instance->AddUniform(scene->GetUniforms()[i]);
  }
  // Uniform blocks deep copy.
  const size_t uniform_blocks_size = scene->GetUniformBlocks().size();
  for (size_t i = 0; i < uniform_blocks_size; ++i) {
    instance->AddUniformBlock(scene->GetUniformBlocks()[i]);
  }
  // Shapes shallow copy.
  const size_t shape_size = scene->GetShapes().size();
  for (size_t i = 0; i < shape_size; ++i) {
    instance->AddShape(scene->GetShapes()[i]);
  }
  // Children recursive instance copy.
  const size_t children_size = scene->GetChildren().size();
  for (size_t i = 0; i < children_size; ++i) {
    instance->AddChild(InstanceCopy(scene->GetChildren()[i]));
  }
  // Enable state copy.
  instance->Enable(scene->IsEnabled());
  return instance;
}

GpuPerformanceTester::GpuPerformanceTester(uint32 width, uint32 height)
    : width_(width),
      height_(height),
      baseline_(kRenderTimeDescriptor, 0, kMaxValue, kMinValue, 0.0, 0.0),
      baseline_inverse_(kRenderTimeDescriptor, 0, kMaxValue, kMinValue,
                        0.0, 0.0),
      resource_(kResourceCreationDescriptor, 0, kMaxValue, kMinValue, 0.0, 0.0),
      no_draw_calls_(kNoDrawCallsDescriptor, 0, kMaxValue, kMinValue, 0.0, 0.0),
      min_viewport_(kMinViewportDescriptor, 0, kMaxValue, kMinValue, 0.0, 0.0),
      min_viewport_inverse_(kMinViewportDescriptor, 0, kMaxValue, kMinValue,
                            0.0, 0.0) {}

GpuPerformanceTester::~GpuPerformanceTester() {}

void GpuPerformanceTester::AccumulateMeasurements(
    const NodePtr& scene,
    const GraphicsManagerPtr& graphics_manager,
    const RendererPtr& renderer) {

  // Count scene constant values
  if (AreModesEnabled(kConstants)) {
    CountPrimitives primitive_count;
    ApplyToTree(scene, primitive_count);
    num_nodes_      += static_cast<int>(primitive_count.node_count);
    num_shapes_     += static_cast<int>(primitive_count.shape_count);
    num_draws_      += static_cast<int>(primitive_count.draw_count);
    num_triangles_  += static_cast<int>(primitive_count.triangle_count);
    num_lines_      += static_cast<int>(primitive_count.line_count);
    num_points_     += static_cast<int>(primitive_count.point_count);
    num_patches_    += static_cast<int>(primitive_count.patch_count);
    num_vertices_   += static_cast<int>(primitive_count.vertex_count);
  }

  // GPU memory usage.
  // 
  // the first pass of a frame if we are going to measure this, just to be sure
  // we are not carrying a bunch of data needed for previous frames but not this
  // frame.
  if (AreModesEnabled(kGpuMemory)) {
    const size_t current_buffer_memory =
        renderer->GetGpuMemoryUsage(Renderer::kBufferObject);
    buffer_memory_ = std::max(buffer_memory_, current_buffer_memory);

    const size_t current_fbo_memory =
        renderer->GetGpuMemoryUsage(Renderer::kFramebufferObject);
    fbo_memory_ = std::max(fbo_memory_, current_fbo_memory);

    const size_t current_texture_memory =
        renderer->GetGpuMemoryUsage(Renderer::kTexture);
    texture_memory_ = std::max(texture_memory_, current_texture_memory);

    // Query all framebuffer bits per pixel
    GLint red_bits, green_bits, blue_bits, alpha_bits, depth_bits, stencil_bits;
    graphics_manager->GetIntegerv(GL_RED_BITS, &red_bits);
    graphics_manager->GetIntegerv(GL_GREEN_BITS, &green_bits);
    graphics_manager->GetIntegerv(GL_BLUE_BITS, &blue_bits);
    graphics_manager->GetIntegerv(GL_ALPHA_BITS, &alpha_bits);
    graphics_manager->GetIntegerv(GL_DEPTH_BITS, &depth_bits);
    graphics_manager->GetIntegerv(GL_STENCIL_BITS, &stencil_bits);
    // Double-buffering is the default in OpenGL ES.
    static const int kBufferCount = 2;
    const int bits = depth_bits + stencil_bits +
        (red_bits + green_bits + blue_bits + alpha_bits) * kBufferCount;
    const size_t current_framebuffer_memory = bits/8 * width_ * height_;
    framebuffer_memory_ = std::max(framebuffer_memory_,
                                   current_framebuffer_memory);
  }

  // Constant metrics pulled from Ion GL Trace
  if (AreModesEnabled(kGlTrace)) {
    // 
    // replicating the logic here ion::gfx::testing::TraceVerifier
    // trace_verifier(&(*graphics_manager));

    // Acquire tracing stream
    gfx::TracingStream& stream = graphics_manager->GetTracingStream();
    stream.Clear();
    stream.StartTracing();
    renderer->gfx::Renderer::DrawScene(scene);
    stream.StopTracing();

    // Parse tracing stream for counts
    gfx::TraceCallExtractor extractor(stream.String());
    num_bind_shader_ += extractor.GetCountOf(kUseProgramString);
    num_bind_texture_ += extractor.GetCountOf(kBindTextureString);
    num_set_uniform_ += extractor.GetCountOf(kUniformString);
  }

  // Base-line Performance. Performance of given scene.  Add clear node to more
  // accurately simulate a real draw pass. Clear for each run is only used for
  // baseline, resulting in clear cost being accounted for in the fill
  // rate.
  if (AreModesEnabled(kBaseline)) {
    MeasureBaseline(scene, graphics_manager, renderer);
  } else {
    ClearVariable(&baseline_);
    ClearVariable(&baseline_inverse_);
  }
  // No draw calls.
  if (AreModesEnabled(kNoDraw)) {
    MeasureStateChanges(scene, graphics_manager, renderer);
  } else {
    ClearVariable(&no_draw_calls_);
  }
  // Minimum viewport.
  if (AreModesEnabled(kMinimumViewport)) {
    MeasureMinViewportSpeed(scene, graphics_manager, renderer);
  } else {
    ClearVariable(&min_viewport_);
    ClearVariable(&min_viewport_inverse_);
  }
  // Resource creation time.
  if (AreModesEnabled(kResource)) {
    MeasureResourceCreation(scene, graphics_manager);
    // Clear Bindings on renderer because MeasureResourceCreation may have
    // modified the opengl state, requiring the current renderer's cached state
    // to be invalidated.
    renderer->ClearCachedBindings();
  } else {
    ClearVariable(&resource_);
  }
}

const Benchmark GpuPerformanceTester::GetResults() {
  Benchmark benchmark;

  int num_primitives = 0;
  double percent_triangles = 0;
  double percent_lines = 0;
  double percent_points = 0;
  double percent_patches = 0;
  double vertices_per_shape = 0;
  double primitives_per_shape = 0;

  // The order of adding constants and variables here matters. It must
  // match the order in ConstantIndices and VariableIndices. Thus we
  // must add all the supported constants and variables, even if they
  // are disabled and have not been measured. Reporting the default
  // values as well as derived values appears to be safe enough (e.g.,
  // no divide by zero or other NaN)

  // Number of nodes, shapes, primitives, vertices, texels, and
  // texture bytes in scene graph.
  if (AreModesEnabled(kConstants)) {
    num_primitives = num_triangles_ + num_lines_ + num_points_;
    if (num_primitives > 0) {
      percent_triangles =
          static_cast<double>(num_triangles_) /
          static_cast<double>(num_primitives) * kToPercent;
      percent_lines =
          static_cast<double>(num_lines_) /
          static_cast<double>(num_primitives) * kToPercent;
      percent_points =
          static_cast<double>(num_points_) /
          static_cast<double>(num_primitives) * kToPercent;
      percent_patches =
          static_cast<double>(num_patches_) /
          static_cast<double>(num_primitives) * kToPercent;
    }
    if (num_shapes_ > 0) {
      vertices_per_shape = static_cast<double>(num_vertices_) /
          static_cast<double>(num_shapes_);
      primitives_per_shape = static_cast<double>(num_primitives) /
          static_cast<double>(num_shapes_);
    }
  }

  benchmark.AddConstant(Benchmark::Constant(kNodeCountDescriptor,
                                            num_nodes_));
  benchmark.AddConstant(Benchmark::Constant(kShapeCountDescriptor,
                                            num_shapes_));
  benchmark.AddConstant(Benchmark::Constant(kDrawCountDescriptor,
                                            num_draws_));
  benchmark.AddConstant(Benchmark::Constant(kVertexCountDescriptor,
                                            num_vertices_));
  benchmark.AddConstant(Benchmark::Constant(kPrimitiveCountDescriptor,
                                            num_primitives));
  benchmark.AddConstant(Benchmark::Constant(kTriangleCountDescriptor,
                                            num_triangles_));
  benchmark.AddConstant(Benchmark::Constant(kLineCountDescriptor,
                                            num_lines_));
  benchmark.AddConstant(Benchmark::Constant(kPointCountDescriptor,
                                            num_points_));
  benchmark.AddConstant(Benchmark::Constant(kPatchCountDescriptor,
                                            num_patches_));
  benchmark.AddConstant(Benchmark::Constant(kTrianglePercentDescriptor,
                                            percent_triangles));
  benchmark.AddConstant(Benchmark::Constant(kLinePercentDescriptor,
                                            percent_lines));
  benchmark.AddConstant(Benchmark::Constant(kPointPercentDescriptor,
                                            percent_points));
  benchmark.AddConstant(Benchmark::Constant(kPatchPercentDescriptor,
                                            percent_patches));
  benchmark.AddConstant(Benchmark::Constant(kVerticesPerShapeDescriptor,
                                            vertices_per_shape));
  benchmark.AddConstant(Benchmark::Constant(kPrimitivesPerShapeDescriptor,
                                            primitives_per_shape));

  // Number of trials used to compute averages.
  const int trial_count = GetTrialCount();
  benchmark.AddConstant(Benchmark::Constant(kTrialCountDescriptor,
                                            trial_count));

  // GL trace stream constants
  benchmark.AddConstant(Benchmark::Constant(
      kBindShaderCountDescriptor, static_cast<double>(num_bind_shader_)));
  benchmark.AddConstant(Benchmark::Constant(
      kBindTextureCountDescriptor, static_cast<double>(num_bind_texture_)));
  benchmark.AddConstant(Benchmark::Constant(
      kSetUniformCountDescriptor, static_cast<double>(num_set_uniform_)));

  // Gpu memory constants
  const double kBytesToMegabytes = 1.0 / (1024 * 1024);
  const size_t total_gpu_memory =
      buffer_memory_ + fbo_memory_ + texture_memory_ + framebuffer_memory_;
  benchmark.AddConstant(Benchmark::Constant(
      kBufferMemoryDescriptor,
      static_cast<double>(buffer_memory_) * kBytesToMegabytes));
  benchmark.AddConstant(Benchmark::Constant(
      kFboMemoryDescriptor,
      static_cast<double>(fbo_memory_) * kBytesToMegabytes));
  benchmark.AddConstant(Benchmark::Constant(
      kTextureMemoryDescriptor,
      static_cast<double>(texture_memory_) * kBytesToMegabytes));
  benchmark.AddConstant(Benchmark::Constant(
      kFramebufferMemoryDescriptor,
      static_cast<double>(framebuffer_memory_) * kBytesToMegabytes));
  benchmark.AddConstant(Benchmark::Constant(
      kTotalGpuMemoryDescriptor,
      static_cast<double>(total_gpu_memory) * kBytesToMegabytes));

  // Base-line Performance.
  const double fps_baseline = baseline_inverse_.mean;
  const double fps_deviation = baseline_inverse_.standard_deviation;
  const double fps_min = baseline_inverse_.minimum;
  const double fps_max = baseline_inverse_.maximum;
  benchmark.AddAccumulatedVariable(
      Benchmark::AccumulatedVariable(kFramesPerSecondDescriptor,
                                     trial_count,
                                     fps_min,
                                     fps_max,
                                     fps_baseline,
                                     fps_deviation));

  double baseline, deviation, min, max;

  // Nodes per second
  baseline = fps_baseline * num_nodes_;
  deviation = fps_deviation * num_nodes_;
  min = fps_min * num_nodes_;
  max = fps_max * num_nodes_;
  benchmark.AddAccumulatedVariable(
      Benchmark::AccumulatedVariable(kNodesPerSecondDescriptor,
                                     trial_count,
                                     min * kToKilo,
                                     max * kToKilo,
                                     baseline * kToKilo,
                                     deviation * kToKilo));

  // Shapes per second
  baseline = fps_baseline * num_shapes_;
  deviation = fps_deviation * num_shapes_;
  min = fps_min * num_shapes_;
  max = fps_max * num_shapes_;
  benchmark.AddAccumulatedVariable(
      Benchmark::AccumulatedVariable(kShapesPerSecondDescriptor,
                                     trial_count,
                                     min * kToKilo,
                                     max * kToKilo,
                                     baseline * kToKilo,
                                     deviation * kToKilo));

  // Draw calls per second
  baseline = fps_baseline * num_draws_;
  deviation = fps_deviation * num_draws_;
  min = fps_min * num_draws_;
  max = fps_max * num_draws_;
  benchmark.AddAccumulatedVariable(
      Benchmark::AccumulatedVariable(kDrawCallsPerSecondDescriptor,
                                     trial_count,
                                     min * kToKilo,
                                     max * kToKilo,
                                     baseline * kToKilo,
                                     deviation * kToKilo));

  // Vertices per second
  baseline = fps_baseline * num_vertices_;
  deviation = fps_deviation * num_vertices_;
  min = fps_min * num_vertices_;
  max = fps_max * num_vertices_;
  benchmark.AddAccumulatedVariable(
      Benchmark::AccumulatedVariable(kVerticesPerSecondDescriptor,
                                     trial_count,
                                     min * kToMega,
                                     max * kToMega,
                                     baseline * kToMega,
                                     deviation * kToMega));

  // Primitives per second
  baseline = fps_baseline * num_primitives;
  deviation = fps_deviation * num_primitives;
  min = fps_min * num_primitives;
  max = fps_max * num_primitives;
  benchmark.AddAccumulatedVariable(
      Benchmark::AccumulatedVariable(kPrimitivesPerSecondDescriptor,
                                     trial_count,
                                     min * kToMega,
                                     max * kToMega,
                                     baseline * kToMega,
                                     deviation * kToMega));

  // Pixels per second (pps)
  const int num_pixels = width_ * height_;
  baseline = fps_baseline * num_pixels;
  deviation = fps_deviation * num_pixels;
  min = fps_min * num_pixels;
  max = fps_max * num_pixels;
  benchmark.AddAccumulatedVariable(
      Benchmark::AccumulatedVariable(kPixelsPerSecondDescriptor,
                                     trial_count,
                                     min * kToMega,
                                     max * kToMega,
                                     baseline * kToMega,
                                     deviation * kToMega));

  // Base-line Performance time.  All inclusive time to render scene.
  benchmark.AddAccumulatedVariable(
      Benchmark::AccumulatedVariable(kRenderTimeDescriptor,
                                     trial_count,
                                     baseline_.minimum * kToMilli,
                                     baseline_.maximum * kToMilli,
                                     baseline_.mean * kToMilli,
                                     baseline_.standard_deviation *
                                     kToMilli));

  // Resource creation time.
  // 
  benchmark.AddAccumulatedVariable(
      Benchmark::AccumulatedVariable(kResourceCreationDescriptor,
                                     trial_count,
                                     resource_.minimum * kToMilli,
                                     resource_.maximum * kToMilli,
                                     resource_.mean * kToMilli,
                                     resource_.standard_deviation *
                                     kToMilli));

  // State-change Performance. Includes scene-graph traversal time.
  benchmark.AddAccumulatedVariable(
      Benchmark::AccumulatedVariable(kNoDrawCallsDescriptor,
                                     trial_count,
                                     no_draw_calls_.minimum * kToMilli,
                                     no_draw_calls_.maximum * kToMilli,
                                     no_draw_calls_.mean * kToMilli,
                                     no_draw_calls_.standard_deviation *
                                     kToMilli));

  // Null Viewport Performance. Performance of scene without fill.
  benchmark.AddAccumulatedVariable(
      Benchmark::AccumulatedVariable(kMinViewportDescriptor,
                                     trial_count,
                                     min_viewport_.minimum * kToMilli,
                                     min_viewport_.maximum * kToMilli,
                                     min_viewport_.mean * kToMilli,
                                     min_viewport_.standard_deviation *
                                     kToMilli));

  // Transform-rate = Null-viewport - State-changes.
  // Units of triangles per second.
  // 
  const double transform_time = min_viewport_.mean;
  const double transform_deviation = min_viewport_.standard_deviation;
  const double transform_min = min_viewport_.minimum;
  const double transform_max = min_viewport_.maximum;
  const double inv_transform_time = min_viewport_inverse_.mean;
  const double inv_transform_min = min_viewport_inverse_.minimum;
  const double inv_transform_max = min_viewport_inverse_.maximum;
  const double inv_transform_deviation =
      min_viewport_inverse_.standard_deviation;
  const double num_mega_primitives = num_primitives * GetTrialCount() * kToMega;
  const double transform_tps = num_mega_primitives * inv_transform_time;
  const double transform_tps_deviation =
      num_mega_primitives * inv_transform_deviation;
  const double transform_tps_min = inv_transform_min * num_mega_primitives;
  const double transform_tps_max = inv_transform_max * num_mega_primitives;
  benchmark.AddAccumulatedVariable(
      Benchmark::AccumulatedVariable(kTransformRateDescriptor,
                                     trial_count,
                                     transform_tps_min,
                                     transform_tps_max,
                                     transform_tps,
                                     transform_tps_deviation));

  // Fill-rate = Base-line - Null-viewport.
  // Units of pixels per second.
  const double fill_time =
      std::max(kMinValue, baseline_.mean - min_viewport_.mean);
  const double fill_deviation =
      baseline_.standard_deviation + min_viewport_.standard_deviation;
  const double fill_min =
      std::max(kMinValue, baseline_.minimum - transform_max);
  const double fill_max =
      std::max(kMinValue, baseline_.maximum - transform_min);
  const double inv_fill_time =
      std::max(kMinValue, 1.0/ (baseline_.mean - min_viewport_.mean));
  const double inv_fill_deviation = baseline_inverse_.standard_deviation +
      min_viewport_inverse_.standard_deviation;
  const double pixel_count = GetTrialCount() * width_ * height_;
  const double fill_pps =  pixel_count * inv_fill_time;
  const double fill_pps_deviation = pixel_count * inv_fill_deviation;
  const double fill_pps_min =
      fill_max == kMinValue ? kMaxValue : pixel_count / fill_max;
  const double fill_pps_max =
      fill_min == kMinValue ? kMaxValue : pixel_count / fill_min;
  benchmark.AddAccumulatedVariable(
      Benchmark::AccumulatedVariable(kFillRateDescriptor,
                                     trial_count,
                                     fill_pps_min * kToMega,
                                     fill_pps_max * kToMega,
                                     fill_pps * kToMega,
                                     fill_pps_deviation * kToMega));

  // Rendertime breakdown: %statechange, %transform, %fill
  const double inv_total_time =
      1.0 / (no_draw_calls_.mean + transform_time + fill_time);
  const double inv_time_min_traversal =
      1.0 / (no_draw_calls_.minimum + transform_max + fill_max);
  const double inv_time_max_traversal =
      1.0 / (no_draw_calls_.maximum + transform_min + fill_min);
  const double traversal_frac = no_draw_calls_.mean * inv_total_time;
  const double traversal_min_frac =
      no_draw_calls_.minimum * inv_time_min_traversal;
  const double traversal_max_frac =
      no_draw_calls_.maximum * inv_time_max_traversal;
  const double traversal_frac_deviation =
      no_draw_calls_.standard_deviation * inv_total_time;
  benchmark.AddAccumulatedVariable(
      Benchmark::AccumulatedVariable(kTraversalPercentDescriptor,
                                     trial_count,
                                     traversal_min_frac * kToPercent,
                                     traversal_max_frac * kToPercent,
                                     traversal_frac * kToPercent,
                                     traversal_frac_deviation * kToPercent));

  const double transform_frac = transform_time * inv_total_time;
  const double inv_time_min_transform =
      1.0 / (no_draw_calls_.maximum + transform_min + fill_max);
  const double inv_time_max_transform =
      1.0 / (no_draw_calls_.minimum + transform_max + fill_min);
  const double transform_min_frac = transform_min * inv_time_min_transform;
  const double transform_max_frac = transform_max * inv_time_max_transform;
  const double transform_frac_deviation =
      transform_deviation * inv_total_time;
  benchmark.AddAccumulatedVariable(
      Benchmark::AccumulatedVariable(kTransformPercentDescriptor,
                                     trial_count,
                                     transform_min_frac * kToPercent,
                                     transform_max_frac * kToPercent,
                                     transform_frac * kToPercent,
                                     transform_frac_deviation * kToPercent));

  const double fill_frac = fill_time * inv_total_time;
  const double inv_time_min_fill =
      1.0 / (no_draw_calls_.maximum + transform_max + fill_min);
  const double inv_time_max_fill =
      1.0 / (no_draw_calls_.minimum + transform_min + fill_max);
  const double fill_frac_min = fill_min * inv_time_min_fill;
  const double fill_frac_max = fill_max * inv_time_max_fill;
  const double fill_frac_deviation = fill_deviation * inv_total_time;
  benchmark.AddAccumulatedVariable(
      Benchmark::AccumulatedVariable(kFillPercentDescriptor,
                                     trial_count,
                                     fill_frac_min * kToPercent,
                                     fill_frac_max * kToPercent,
                                     fill_frac * kToPercent,
                                     fill_frac_deviation * kToPercent));

  // Reset min, max, mean, standard deviation, and counts for next run.
  ResetVariable(&baseline_);
  ResetVariable(&baseline_inverse_);
  ResetVariable(&resource_);
  ResetVariable(&no_draw_calls_);
  ResetVariable(&min_viewport_);
  ResetVariable(&min_viewport_inverse_);
  num_nodes_ = 0;
  num_shapes_ = 0;
  num_draws_ = 0;
  num_vertices_ = 0;
  num_triangles_ = 0;
  num_lines_ = 0;
  num_points_ = 0;
  num_bind_shader_ = 0;
  num_bind_texture_ = 0;
  num_set_uniform_ = 0;
  buffer_memory_ = 0;
  fbo_memory_ = 0;
  texture_memory_ = 0;
  framebuffer_memory_ = 0;
  return benchmark;
}

const Benchmark GpuPerformanceTester::RunAllMeasurements(
    const NodePtr& scene,
    const GraphicsManagerPtr& graphics_manager,
    const RendererPtr& renderer) {
  AccumulateMeasurements(scene, graphics_manager, renderer);
  return GetResults();
}

Measurement GpuPerformanceTester::MeasureBaseline(
    const NodePtr& scene,
    const GraphicsManagerPtr& graphics_manager,
    const RendererPtr& renderer) {
  DCHECK(scene.Get());
  NodePtr clear_test_node(GetClearNode(width_, height_,
                                       scene->GetAllocator()));
  clear_test_node->AddChild(InstanceCopy(scene));
  Measurement m = MeasurePerformance(clear_test_node, graphics_manager,
                                     renderer);
  AccumulateVariable(&baseline_, m);
  AccumulateInverseVariable(&baseline_inverse_, m);
  return m;
}

Measurement GpuPerformanceTester::MeasureMinViewportSpeed(
    const NodePtr& scene,
    const GraphicsManagerPtr& graphics_manager,
    const RendererPtr& renderer) {
  DCHECK(scene.Get());
  NodePtr test_scene(InstanceCopy(scene));
  MinifyViewport minviewport_functor;
  ApplyToTree(test_scene, minviewport_functor);
  static const Range2i kPixelRange(Point2i(0, 0), Point2i(1, 1));
  if (!test_scene->GetStateTable().Get()) {
    test_scene->SetStateTable(
        StateTablePtr(new(scene->GetAllocator()) StateTable));
  }
  test_scene->GetStateTable()->SetViewport(kPixelRange);
  Measurement m = MeasurePerformance(test_scene, graphics_manager, renderer);
  AccumulateVariable(&min_viewport_, m);
  AccumulateInverseVariable(&min_viewport_inverse_, m);
  return m;
}

Measurement GpuPerformanceTester::MeasureStateChanges(
    const NodePtr& scene,
    const GraphicsManagerPtr& graphics_manager,
    const RendererPtr& renderer) {
  DCHECK(scene.Get());
  NodePtr test_scene(InstanceCopy(scene));
  RemoveGeometry remove_geometry_functor;
  ApplyToTree(test_scene, remove_geometry_functor);
  Measurement m = MeasurePerformance(test_scene, graphics_manager, renderer);
  AccumulateVariable(&no_draw_calls_, m);
  return m;
}

Measurement GpuPerformanceTester::MeasurePerformance(
    const NodePtr& scene,
    const GraphicsManagerPtr& graphics_manager,
    const RendererPtr& renderer) {
  DCHECK(scene.Get());

  // Disable depth test always.
  DisableDepthTest disable_depth_test_functor;
  ApplyToTree(scene, disable_depth_test_functor);

  NodePtr clear_node(GetClearNode(width_, height_, scene->GetAllocator()));
  // NOTE: We are forcing the call to DrawScene to be gfx::Renderer's,
  // this is because we now have a derived Renderer, BenchmarkRenderer, which
  // overrides DrawScene to run this benchmark.  Therefore, we get an infinite
  // recursion if we don't call the right version of DrawScene().
  renderer->gfx::Renderer::DrawScene(clear_node);

  // Finish any outstanding GPU work before starting timer.
  // Warm up gpu and create resources in advance by throwing out the first run.
  renderer->gfx::Renderer::DrawScene(scene);
  renderer->gfx::Renderer::DrawScene(clear_node);
  graphics_manager->Finish();

  double time_sum = 0.0;
  double inv_time_sum = 0.0;
  double time_squared = 0.0;
  double inv_time_squared = 0.0;
  double minimum = std::numeric_limits<double>::max();
  double maximum = std::numeric_limits<double>::min();
  for (uint32 frame = 0; frame < number_of_trials_; ++frame) {
    port::Timer timer;
    for (int i = 0; i < kInnerTrialCount; ++i)
      renderer->gfx::Renderer::DrawScene(scene);
    graphics_manager->Finish();
    double frame_time = timer.GetInS() / kInnerTrialCount;
    // Frame time must be strictly positive.
    DCHECK_GE(frame_time, 0.0);
    frame_time = frame_time > 0.0 ? frame_time : 0.0;
    time_sum += frame_time;
    time_squared += frame_time * frame_time;
    inv_time_sum += 1.0 / frame_time;
    inv_time_squared += 1.0 / frame_time * 1.0 / frame_time;
    minimum = std::min(frame_time, minimum);
    maximum = std::max(frame_time, maximum);
  }
  const double inv_num_trials = 1.0 / number_of_trials_;
  const double ave_time = time_sum * inv_num_trials;
  const double ave_time_squared = time_squared * inv_num_trials;
  const double ave_inv_time = inv_time_sum * inv_num_trials;
  const double ave_inv_time_squared = inv_time_squared * inv_num_trials;
  const double stddev_time = kInnerTrialCount *
      sqrt(fabs(ave_time_squared - ave_time * ave_time));
  const double stddev_inv_time = kInnerTrialCount *
      sqrt(fabs(ave_inv_time_squared - ave_inv_time * ave_inv_time));

  return Measurement(
      ave_time, stddev_time, ave_inv_time, stddev_inv_time, minimum, maximum);
}

Measurement GpuPerformanceTester::MeasureResourceCreation(
    const gfx::NodePtr& scene,
    const GraphicsManagerPtr& graphics_manager) {
  // Do two passes to compute something like a standard deviation.
  // Really, we are interested in getting any kind of bounds on these
  // statistics.
  // 
  NodePtr clear_node(GetClearNode(width_, height_, scene->GetAllocator()));

  RendererPtr resource_renderer_one(new Renderer(graphics_manager));
  resource_renderer_one->gfx::Renderer::DrawScene(clear_node);
  port::Timer timer;
  resource_renderer_one->gfx::Renderer::DrawScene(scene);
  graphics_manager->Finish();
  const double time_one = timer.GetInS();

  RendererPtr resource_renderer_two(new Renderer(graphics_manager));
  resource_renderer_two->gfx::Renderer::DrawScene(clear_node);
  timer.Reset();
  resource_renderer_two->gfx::Renderer::DrawScene(scene);
  graphics_manager->Finish();
  const double time_two = timer.GetInS();

  Measurement m((time_one + time_two) / 2.0,
                fabs(time_one - time_two) / 2.0,
                (1.0 / time_one + 1.0 / time_two) / 2.0,
                fabs(1.0 / time_one - 1.0 / time_two) / 2.0,
                time_one < time_two ? time_one : time_two,
                time_one > time_two ? time_one : time_two);
  AccumulateVariable(&resource_, m);
  return m;
}

}  // namespace analytics
}  // namespace ion

#endif  // !ION_PRODUCTION
