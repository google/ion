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

#include "absl/memory/memory.h"
#if !ION_PRODUCTION

#include "ion/remote/shaderhandler.h"

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>  // NOLINT(build/c++11)
#include <vector>

#include "ion/base/invalid.h"
#include "ion/base/zipassetmanager.h"
#include "ion/gfx/graphicsmanager.h"
#include "ion/gfx/renderer.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfx/shaderprogram.h"
#include "ion/gfx/tests/fakegraphicsmanager.h"
#include "ion/gfxutils/shadermanager.h"
#include "ion/gfxutils/shadersourcecomposer.h"
#include "ion/port/semaphore.h"
#include "ion/portgfx/glcontext.h"
#include "ion/remote/tests/httpservertest.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace remote {

// Save some typing by typedefing the handler pointer.
typedef HttpServer::RequestHandlerPtr RequestHandlerPtr;

using gfx::GraphicsManagerPtr;
using gfx::Renderer;
using gfx::RendererPtr;
using gfx::ShaderInputRegistry;
using gfx::ShaderInputRegistryPtr;
using gfx::ShaderProgramPtr;
using gfx::testing::FakeGraphicsManager;
using gfx::testing::FakeGlContext;
using gfxutils::ShaderManager;
using gfxutils::ShaderManagerPtr;
using gfxutils::ShaderSourceComposer;
using gfxutils::ShaderSourceComposerPtr;
using gfxutils::StringComposer;
using port::Semaphore;
using portgfx::GlContext;
using portgfx::GlContextPtr;

namespace {

class ShaderHandlerTest : public RemoteServerTest {
 protected:
  ShaderHandlerTest() : renderer_thread_quit_flag_(false) {}

  ~ShaderHandlerTest() override {}

  void SetUp() override {
    RemoteServerTest::SetUp();
    server_->SetHeaderHtml("");
    server_->SetFooterHtml("");

    shader_manager_.Reset(new ShaderManager());
  }

  void TearDown() override {
    ASSERT_EQ(std::thread::id(), renderer_thread_.get_id());
    RemoteServerTest::TearDown();
  }

  // The Renderer must continue to process info request even as the main test
  // thread blocks on the completion of the request.  Thus we run the Renderer
  // in a separate thread here.
  void StartRenderer() {
    ASSERT_EQ(std::thread::id(), renderer_thread_.get_id());
    renderer_thread_quit_flag_.store(false, std::memory_order_relaxed);
    render_thread_start_ = absl::make_unique<Semaphore>();
    renderer_thread_ = std::thread(&ShaderHandlerTest::RendererFunc, this);
    render_thread_start_->Wait();
    render_thread_start_.reset();
  }

  void StopRenderer() {
    ASSERT_NE(std::thread::id(), renderer_thread_.get_id());
    renderer_thread_quit_flag_.store(true, std::memory_order_relaxed);
    renderer_thread_.join();
    renderer_thread_ = std::thread();
  }

  bool RendererFunc() {
    GlContextPtr gl_context = FakeGlContext::Create(800, 800);
    GlContext::MakeCurrent(gl_context);
    GraphicsManagerPtr graphics_manager(new FakeGraphicsManager());
    RendererPtr renderer(new Renderer(graphics_manager));
    RequestHandlerPtr sh(new ShaderHandler(shader_manager_, renderer));
    server_->RegisterHandler(sh);

    // Notify the thread calling StartRender() that |renderer| is set up.
    render_thread_start_->Post();

    // Now service info requests on this thread.
    while (!renderer_thread_quit_flag_.load(std::memory_order_relaxed)) {
      renderer->ProcessResourceInfoRequests();
    }

    // Clean up renderer state.
    server_->UnregisterHandler(sh->GetBasePath());
    return true;
  }

  void VerifyHtmlTitle(int line, const std::string& title) {
    SCOPED_TRACE(::testing::Message() << "Verifying from line " << line
                                      << " that response has title: " << title);
    EXPECT_EQ(200, response_.status);
    EXPECT_FALSE(response_.data.empty());
    EXPECT_NE(std::string::npos,
              response_.data.find("<title>" + title + "</title"));
  }

  void VerifyListElements(int line, const std::vector<std::string>& elements) {
    SCOPED_TRACE(::testing::Message() << "Verifying list from line " << line);
    const size_t count = elements.size();
    for (size_t i = 0; i < count; ++i) {
      SCOPED_TRACE(::testing::Message() << "Verifying element " << i << " ("
                                        << elements[i] << ") is present.");
      EXPECT_NE(std::string::npos, response_.data.find(elements[i]));
    }
  }

  // State supporting the renderer thread.
  std::thread renderer_thread_;
  std::unique_ptr<Semaphore> render_thread_start_;
  std::atomic<bool> renderer_thread_quit_flag_;

  gfxutils::ShaderManagerPtr shader_manager_;
};

// Simple composer that fakes a dependency and changes to it.
class Composer : public ShaderSourceComposer {
 public:
  Composer(const std::string& dependency, const std::string& source)
      : source_(source),
        dependency_(dependency),
        changed_(false) {}
  ~Composer() override {}
  const std::string GetSource() override { return source_; }
  const std::string GetDependencySource(
      const std::string& dependency) const override {
    return std::string();
  }
  bool SetDependencySource(const std::string& dependency,
                           const std::string& source) override {
    changed_ = true;
    source_ = source;
    return true;
  }
  void SetSource(const std::string& source) { source_ = source; }
  bool DependsOn(const std::string& resource) const override {
    return resource == dependency_;
  }
  const std::string GetDependencyName(unsigned int id) const override {
    if (id == 0)
      return dependency_;
    else
      return std::string();
  }
  const std::vector<std::string> GetDependencyNames() const override {
    return std::vector<std::string>(1, dependency_);
  }
  const std::vector<std::string> GetChangedDependencies() override {
    if (changed_) {
      changed_ = false;
      std::vector<std::string> vec;
      vec.push_back(dependency_);
      return vec;
    } else {
      return std::vector<std::string>();
    }
  }

 private:
  std::string source_;
  std::string dependency_;
  bool changed_;
};

}  // anonymous namespace

TEST_F(ShaderHandlerTest, ServeShaders) {
  StartRenderer();
  std::vector<std::string> elements;

  GetUri("/ion/shaders/does/not/exist");
  Verify404(__LINE__);

  GetUri("/ion/shaders/index.html");
  Verify404(__LINE__);

  GetUri("/ion/shaders/shader_status");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("\n", response_.data);

  GetUri("/ion/shaders");
  EXPECT_EQ(200, response_.status);
  VerifyHtmlTitle(__LINE__, "Registered shader programs");
  elements.push_back("shader_editor");
  VerifyListElements(__LINE__, elements);
  GetUri("/ion/shaders?raw");
  EXPECT_EQ(200, response_.status);

  // Create a couple of shaders.
  ShaderInputRegistryPtr registry(new ShaderInputRegistry());
  ShaderSourceComposerPtr vertex_composer1(
      new StringComposer("vertex1dep", "vertex1"));
  ShaderSourceComposerPtr vertex_composer2(
      new StringComposer("vertex2dep", "vertex2"));
  ShaderSourceComposerPtr geometry_composer2(
      new StringComposer("geometry2dep", "geometry2"));
  ShaderSourceComposerPtr fragment_composer1(
      new StringComposer("fragment1dep", "fragment1"));
  ShaderSourceComposerPtr fragment_composer2(
      new StringComposer("fragment2dep", "fragment2"));
  ShaderProgramPtr shader1(shader_manager_->CreateShaderProgram(
      "shader1", registry, vertex_composer1, fragment_composer1));
  ShaderProgramPtr shader2(shader_manager_->CreateShaderProgram(
      "shader2", registry, vertex_composer2, fragment_composer2));
  ShaderProgramPtr shader3(shader_manager_->CreateShaderProgram(
      "shader3", registry, vertex_composer2, fragment_composer2,
      geometry_composer2));

  GetUri("/ion/shaders/shader_status");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("shader1,OK,OK,OK\nshader2,OK,OK,OK\nshader3,OK,OK,OK",
            response_.data);

  GetUri("/ion/shaders");
  EXPECT_EQ(200, response_.status);
  VerifyHtmlTitle(__LINE__, "Registered shader programs");
  elements.clear();
  elements.push_back("shader_editor");
  elements.push_back("shader1");
  elements.push_back("shader2");
  elements.push_back("shader3");
  VerifyListElements(__LINE__, elements);
  // Check the raw list.
  GetUri("/ion/shaders?raw");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("shader1\nshader2\nshader3", response_.data);

  // Get info on shader1.
  GetUri("/ion/shaders/shader1");
  EXPECT_EQ(200, response_.status);
  VerifyHtmlTitle(__LINE__, "Info log and shader stages for shader1");
  elements.clear();
  elements.push_back("|info log|");
  elements.push_back("vertex");
  elements.push_back("fragment");
  VerifyListElements(__LINE__, elements);
  // Check the raw list.
  GetUri("/ion/shaders/shader1?raw");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("|info log|\nvertex\nfragment", response_.data);
  GetUri("/ion/shaders/shader2/%7Cinfo%20log%7C");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("OK", response_.data);

  // Get shader2's vertex dependencies.
  GetUri("/ion/shaders/shader2/vertex");
  EXPECT_EQ(200, response_.status);
  VerifyHtmlTitle(__LINE__,
                  "List of dependencies for the vertex stage of shader2");
  elements.clear();
  elements.push_back("|info log|");
  elements.push_back("vertex2dep");
  VerifyListElements(__LINE__, elements);
  // Check the raw list.
  GetUri("/ion/shaders/shader2/vertex?raw");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("|info log|\nvertex2dep", response_.data);
  GetUri("/ion/shaders/shader2/vertex/vertex2dep");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("vertex2", response_.data);
  GetUri("/ion/shaders/shader2/vertex/%7Cinfo%20log%7C");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("OK", response_.data);
  GetUri("/ion/shaders/shader2/vertex/vertex1dep");
  Verify404(__LINE__);

  // Get shader1's fragment dependencies.
  GetUri("/ion/shaders/shader1/fragment");
  EXPECT_EQ(200, response_.status);
  VerifyHtmlTitle(__LINE__,
                  "List of dependencies for the fragment stage of shader1");
  elements.clear();
  elements.push_back("|info log|");
  elements.push_back("fragment1dep");
  VerifyListElements(__LINE__, elements);
  // Check the raw list.
  GetUri("/ion/shaders/shader1/fragment?raw");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("|info log|\nfragment1dep", response_.data);
  GetUri("/ion/shaders/shader1/fragment/fragment1dep");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("fragment1", response_.data);
  GetUri("/ion/shaders/shader1/fragment/%7Cinfo%20log%7C");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("OK", response_.data);
  GetUri("/ion/shaders/shader1/fragment/fragment2dep");
  Verify404(__LINE__);

  // Set a dependency source.
  GetUri(
      "/ion/shaders/shader1/fragment/"
      "fragment1dep?set_source=some%20new%20source");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("Shader source changed.", response_.data);
  // Check that the source has changed.
  GetUri("/ion/shaders/shader1/fragment/fragment1dep");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("some new source", response_.data);

  // Try getting an invalid info log.
  GetUri("/ion/shaders/shader1/geometry/%7Cinfo%20log%7C");
  Verify404(__LINE__);

  StopRenderer();
}

TEST_F(ShaderHandlerTest, ShaderEditor) {
  StartRenderer();

  // Check that the shader editor HTML file is served.
  GetUri("/ion/shaders/shader_editor");
  EXPECT_EQ(200, response_.status);
  const std::string& editor_source = base::ZipAssetManager::GetFileData(
      "ion/shaders/shader_editor/index.html");
  EXPECT_FALSE(base::IsInvalidReference(editor_source));
  EXPECT_EQ(editor_source, response_.data);

  GetUri("/ion/shaders/shader_editor/index.html");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ(editor_source, response_.data);

  GetUri("/ion/shaders/shader_editor/notafile.html");
  Verify404(__LINE__);

  StopRenderer();
}

TEST_F(ShaderHandlerTest, FormatInfoLogs) {
  StartRenderer();

  // Create a couple of shaders.
  ShaderInputRegistryPtr registry(new ShaderInputRegistry());
  ShaderSourceComposerPtr vertex_composer(
      new StringComposer("vertex_dep", "vertex"));
  ShaderSourceComposerPtr fragment_composer(
      new StringComposer("fragment_dep", "fragment"));
  ShaderProgramPtr shader(shader_manager_->CreateShaderProgram(
      "shader", registry, vertex_composer, fragment_composer));

  // Set the shader info logs.
  const std::string non_apple_log(
      "1(11): error C1234: some error message\n"
      "1(42): note HHGTTG42: the meaning of everything");
  const std::string apple_log(
      "ERROR: 1:11: some error message\n"
      "NOTE: 1:42: the meaning of everything");

  shader->GetVertexShader()->SetInfoLog(non_apple_log);
  shader->GetFragmentShader()->SetInfoLog(apple_log);

  // Check that the logs are formatted correctly.
  GetUri("/ion/shaders/shader/vertex/%7Cinfo%20log%7C");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("vertex_dep:11: some error message<br>\n"
            "vertex_dep:42: the meaning of everything<br>\n", response_.data);
  GetUri("/ion/shaders/shader/fragment/%7Cinfo%20log%7C");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("fragment_dep:11: some error message<br>\n"
            "fragment_dep:42: the meaning of everything<br>\n", response_.data);
  GetUri("/ion/shaders/shader/geometry/%7Cinfo%20log%7C");
  Verify404(__LINE__);

  StopRenderer();
}

TEST_F(ShaderHandlerTest, UpdateAndServeChangedDependencies) {
  StartRenderer();

  // Nothing should have changed yet, since there's nothing to change.
  GetUri("/ion/shaders/update_changed_dependencies");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ(";", response_.data);

  // Create a couple of shaders.
  ShaderSourceComposerPtr vertex_composer(new Composer("vertex_dep", "vertex"));
  ShaderSourceComposerPtr fragment_composer(
      new Composer("fragment_dep", "fragment"));
  ShaderInputRegistryPtr registry(new ShaderInputRegistry());
  ShaderProgramPtr shader(shader_manager_->CreateShaderProgram(
      "shader", registry, vertex_composer, fragment_composer));

  // Now change a dependency.
  vertex_composer->SetDependencySource("vertex_dep", "vertex2");

  GetUri("/ion/shaders/update_changed_dependencies");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("vertex_dep", response_.data);

  // Change two dependencies.
  vertex_composer->SetDependencySource("vertex_dep", "vertex3");
  fragment_composer->SetDependencySource("fragment_dep", "fragment2");

  GetUri("/ion/shaders/update_changed_dependencies");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("fragment_dep;vertex_dep", response_.data);

  // Nothing since the last call has changed.
  GetUri("/ion/shaders/update_changed_dependencies");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ(";", response_.data);

  StopRenderer();
}

}  // namespace remote
}  // namespace ion

#endif
