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

#ifndef ION_ANALYTICS_GPUPERFORMANCE_H_
#define ION_ANALYTICS_GPUPERFORMANCE_H_

#if !ION_PRODUCTION

#include <functional>
#include <ostream>  // NOLINT
#include <utility>
#include <vector>

#include "base/integral_types.h"
#include "ion/analytics/benchmark.h"
#include "ion/external/gtest/gunit_prod.h"  // For FRIEND_TEST().
#include "ion/gfx/graphicsmanager.h"
#include "ion/gfx/node.h"
#include "ion/gfx/renderer.h"

namespace ion {
namespace analytics {

// GPUPerformanceTester measures basic GPU rendering characteristics. The member
// function RunAllMeasurements measures the following performance timings:
// A) Average render time for the unmodified scene
// B) Average render time with viewport set to 1 pixel (minviewport)
// C) Average render time with draw calls ignored (traversal & state change)
// D) Render time including resource creation; uses a new ion::gfx::Renderer
//
// From these measurements and their standard deviations the following
// quantities are reported:
// - Scene constants
// --- Nodes
// --- Shapes
// --- Draw calls
// --- Vertices
// --- Primitives
// --- Triangles
// --- Lines
// --- Points
// --- Patches
// --- Triangle percent (triangles/primitives)
// --- Line percent (lines/primitives)
// --- Point percent (points/primitives)
// --- Patch percent (patches/primitives)
// --- Vertices/shape
// --- Primitives/shape
// - Number of trials used to compute averages
// - Frames per second: 1 / A
// - Millions of triangles per second for unmodified scene: #triangles / A
// - Millions of pixels per second for umodified scene: #pixels / A
// - Average render time for unmodified scene (A)
// - Resource creation time (D - A)
// - Average render time with draw calls ignored (C)
// - Average render time with minviewport (B)
// - Fill rate: #pixels / (A - B)
// - Transform rate: #triangles / B
// - % of render time spent in traversal and state change: C / A * 100
// - % of render time spent transforming geometry: B / A * 100
// - % of render time spent on fill: (A - B) / A * 100
class ION_API GpuPerformanceTester {
 public:
  // This is used for values that with no meaning & should not be printed.
  // For example, when there is no standard deviation for a value and/or
  // the min and max values are the same as the value itself.
  static const double kInvalidValue;

  // Enable/disable five modes of GPU performance measurement:
  // - Scene constants (listed above).
  // - Baseline framerate. This is the time to render the scene as
  //   originally provided, measured between glFinish calls, with
  //   depth buffering disabled.
  // - No draw framerate. The scene is rendered with all primitives
  //   removed, which results in a timing of the traversal and state
  //   change overhead.
  // - Minimum viewport framerate. The scene is rendered as provided,
  //   but in a 1 pixel viewport. This minimizes the fill time,
  //   resulting in a timing of vertex processing (and traversal/state
  //   change overhead).
  // - Resource creation time. This renderers the scene in a new
  //   Renderer instance, incorporating the time to create (duplicate)
  //   all the Renderer-related resources.

  enum Enables {
    kNoEnables       = 0x00,
    kConstants       = 0x01,
    kBaseline        = 0x02,
    kNoDraw          = 0x04,
    kMinimumViewport = 0x08,
    kResource        = 0x10,
    kGpuMemory       = 0x20,
    kGlTrace         = 0x40,
    kAllEnables      = 0x7F
  };

  struct Measurement {
    Measurement(double mean_value, double standard_deviation,
                double reciprocal_mean, double reciprocal_deviation,
                double minimum_value, double maximum_value)
      : mean(mean_value),
        deviation(standard_deviation),
        inverse_mean(reciprocal_mean),
        inverse_deviation(reciprocal_deviation),
        minimum(minimum_value), maximum(maximum_value) {}
    double mean;
    double deviation;
    double inverse_mean;
    double inverse_deviation;
    double minimum;
    double maximum;
  };

  // 
  // enums alphabetically for improved readability. Requires
  // additional changes to Benchmark interface

  // Named indices for each entries in the Benchmark returned by
  // RunAllMeasurements() and GetResults(). These names (minus 'k', plus
  // spaces between words) should correspond to the id string in the
  // Constant/Variable descriptor.
  // Example: (enum) kPrimitiveCount -> (token) Primitive Count.
  enum ConstantIndices {
    kNodeCount,
    kShapeCount,
    kDrawCount,
    kVertexCount,
    kPrimitiveCount,
    kTriangleCount,
    kLineCount,
    kPointCount,
    kPatchesCount,
    kTrianglePercent,
    kLinePercent,
    kPointPercent,
    kPatchesPercent,
    kVerticesPerShape,
    kPrimitivesPerShape,
    kTrialCount,
    kBindShaderCount,
    kBindTextureCount,
    kSetUniformCount,
    kBufferMemory,
    kFboMemory,
    kTextureMemory,
    kFrameBufferMemory,
    kTotalGpuMemory
  };

  // 

  enum VariableIndices {
    kFramesPerSecond,
    kNodesPerSecond,
    kShapesPerSecond,
    kDrawCallsPerSecond,
    kVerticesPerSecond,
    kPrimitivesPerSecond,
    kPixelsPerSecond,
    kRenderTime,
    kResourceCreation,
    kNoDrawCalls,
    kMinViewport,
    kTransformRate,
    kFillRate,
    kTraversalPercent,
    kTransformPercent,
    kFillPercent
  };

  // Width and height should be the OpenGL render window dimensions.
  GpuPerformanceTester(uint32 width, uint32 height);
  virtual ~GpuPerformanceTester();

  // Enable only modes indicated by enables bitmask (effectively
  // clearing the bitmask and setting the indicated enable bits).
  // Enable(kNoEnables) disables all.
  // Enable(kConstants | kBaseline) enables first two modes.
  // By default, all phases are enabled.
  void SetEnables(Enables enables);
  Enables GetEnables() const { return enables_; }
  // Returns true if all of the specified modes are currently enabled
  bool AreModesEnabled(Enables enables) const {
    return (enables_ & enables) == enables;
  }

  // Runs full set of basic measurements, returns Benchmark immediately.
  virtual const Benchmark RunAllMeasurements(
      const gfx::NodePtr& scene,
      const gfx::GraphicsManagerPtr& graphics_manager,
      const gfx::RendererPtr& renderer);

  // Keeps running totals for the full set of basic measurements for all
  // scene nodes passed to it until GetResults() is called.
  virtual void AccumulateMeasurements(
      const gfx::NodePtr& scene,
      const gfx::GraphicsManagerPtr& graphics_manager,
      const gfx::RendererPtr& renderer);

  // Finalizes accumulated measurements, returns benchmark data, and resets
  // accumulation totals.
  virtual const Benchmark GetResults();

  // Returns/sets the number of trials used to measure performance.
  int GetTrialCount() const { return number_of_trials_; }
  void SetTrialCount(int number_of_trials) {
    number_of_trials_ = number_of_trials;
  }

 protected:
  // Returns the average (avg) time and standard deviation (stddev) for
  // rendering a scene some number of trials.
  virtual Measurement MeasurePerformance(
      const gfx::NodePtr& scene,
      const gfx::GraphicsManagerPtr& graphics_manager,
      const gfx::RendererPtr& renderer);

  // Returns the avg and stddev time to render unmodified scene.
  virtual Measurement MeasureBaseline(
      const gfx::NodePtr& scene,
      const gfx::GraphicsManagerPtr& graphics_manager,
      const gfx::RendererPtr& renderer);

  // Returns the avg and stddev time to render scene with a minimal viewport,
  // i.e. render time no fill, just measure traversal and
  // transform/vertex-program performance.
  virtual Measurement MeasureMinViewportSpeed(
      const gfx::NodePtr& scene,
      const gfx::GraphicsManagerPtr& graphics_manager,
      const gfx::RendererPtr& renderer);

  // Measures performance of state changes, i.e. traverse scene without
  // actually rendering any geometry.
  // 
  // platforms, consider adding trivial geometry draw to insure a meaningful
  // measurement.
  virtual Measurement MeasureStateChanges(
      const gfx::NodePtr& scene,
      const gfx::GraphicsManagerPtr& graphics_manager,
      const gfx::RendererPtr& renderer);

  // Measures render performance WITH resource creation time by creating a
  // new Renderer.  This should force the reallocation and update of all
  // OpenGL resources (attribute buffers, shaders, textures, etc...)
  virtual Measurement MeasureResourceCreation(
      const gfx::NodePtr& scene,
      const gfx::GraphicsManagerPtr& graphics_manager);

  // Copy scene in preparation for modifying the nodes for various
  // measurements.
  static gfx::NodePtr InstanceCopy(const gfx::NodePtr& scene);

  uint32 number_of_trials_ = 5;
  uint32 width_;
  uint32 height_;

  // Enable and disable measurement phases
  Enables enables_ = static_cast<Enables>(kConstants | kBaseline | kNoDraw |
                                          kMinimumViewport |kGpuMemory |
                                          kGlTrace);

  // The finalized benchmark data.
  Benchmark benchmark_;

  // Benchmark data in progress.
  int num_nodes_ = 0;
  int num_shapes_ = 0;
  int num_draws_ = 0;
  int num_vertices_ = 0;
  int num_triangles_ = 0;
  int num_lines_ = 0;
  int num_points_ = 0;
  int num_patches_ = 0;
  size_t num_bind_shader_ = 0;
  size_t num_bind_texture_ = 0;
  size_t num_set_uniform_ = 0;
  size_t buffer_memory_ = 0;
  size_t fbo_memory_ = 0;
  size_t texture_memory_ = 0;
  size_t framebuffer_memory_ = 0;
  Benchmark::AccumulatedVariable baseline_;
  Benchmark::AccumulatedVariable baseline_inverse_;
  Benchmark::AccumulatedVariable resource_;
  Benchmark::AccumulatedVariable no_draw_calls_;
  Benchmark::AccumulatedVariable min_viewport_;
  Benchmark::AccumulatedVariable min_viewport_inverse_;

  // Allow test to access private function.
  FRIEND_TEST(GPUPerformanceTest, InstanceCopy);
};

inline GpuPerformanceTester::Enables operator|(
    GpuPerformanceTester::Enables left,
    GpuPerformanceTester::Enables right) {
  return static_cast<GpuPerformanceTester::Enables>(
        static_cast<int>(left) | static_cast<int>(right));
}

inline void GpuPerformanceTester::SetEnables(
    GpuPerformanceTester::Enables enables) {
  if ((enables & kResource) && (enables & kGpuMemory)) {
    enables = static_cast<GpuPerformanceTester::Enables>(
        static_cast<int>(enables) & ~static_cast<int>(kResource));
    LOG(WARNING) <<
        "GpuPerformanceTester: kResource and kGpuMemory are incompatible" <<
        std::endl << "Disabling kResource" << std::endl;
  }
  enables_ = enables;
}

}  // namespace analytics
}  // namespace ion

#endif  // ION_PRODUCTION

#endif  // ION_ANALYTICS_GPUPERFORMANCE_H_
