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

#ifndef ION_GFX_TESTS_TESTSCENE_H_
#define ION_GFX_TESTS_TESTSCENE_H_

#include "ion/gfx/cubemaptexture.h"
#include "ion/gfx/node.h"
#include "ion/gfx/texture.h"
#include "ion/math/vector.h"

namespace ion {
namespace gfx {
namespace testing {

// A TestScene creates a simple scene graph suitable for testing.
class TestScene {
 public:
  // Vertex struct that contains one field of each attribute type.
  struct Vertex {
    Vertex() : f(0.f) {}
    // Convenience constructor that sets all fields to deterministic values.
    explicit Vertex(int i) :
        f(static_cast<float>(i) + 1.0f),
        fv2(f, f + 1.0f) {}
    float f;
    math::Vector2f fv2;
  };

  explicit TestScene(bool capture_varyings = false);
  ~TestScene() {}

  // Returns the root of the graph.
  const NodePtr GetScene() const { return scene_; }
  // Returns a new CubeMapTexture.
  const CubeMapTexturePtr CreateCubeMapTexture() const;
  // Returns a new Texture.
  const TexturePtr CreateTexture() const;
  // Gets the number of indices in the scene's IndexBuffer.
  size_t GetIndexCount() const;
  // Gets the size in bytes of the buffer object in the scene.
  size_t GetBufferSize() const;
  // Gets the stride in bytes of the buffer object in the scene.
  size_t GetBufferStride() const;
  // Returns the offset of the second BufferObjectElement attribute in the
  // scene.
  static size_t GetSecondBoeAttributeOffset();

  // Returns the source code of the scene's vertex shader.
  const std::string GetVertexShaderSource() const;
  // Returns the source code of the scene's geometry shader.
  const std::string GetGeometryShaderSource() const;
  // Returns the source code of the scene's fragment shader.
  const std::string GetFragmentShaderSource() const;

 private:
  NodePtr scene_;
};

}  // namespace testing
}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_TESTS_TESTSCENE_H_
