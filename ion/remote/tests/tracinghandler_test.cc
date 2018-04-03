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

#if !ION_PRODUCTION

#include "ion/remote/tracinghandler.h"

#include <iomanip>
#include <string>
#include <vector>

#include "ion/base/invalid.h"
#include "ion/base/logchecker.h"
#include "ion/base/tests/multilinestringsequal.h"
#include "ion/base/zipassetmanager.h"
#include "ion/gfx/renderer.h"
#include "ion/gfx/tests/fakegraphicsmanager.h"
#include "ion/gfx/tests/testscene.h"
#include "ion/gfx/tests/traceverifier.h"
#include "ion/gfxutils/frame.h"
#include "ion/remote/tests/httpservertest.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace remote {

class TracingHandlerTest : public RemoteServerTest {
 protected:
  void SetUp() override {
    RemoteServerTest::SetUp();
    make_opengl_calls_ = false;
    server_->SetHeaderHtml("");
    server_->SetFooterHtml("");

    // Create a Frame, FakeGraphicsManager, and Renderer to handle tracing.
    frame_ = new gfxutils::Frame();
    fg_fake_context_ = gfx::testing::FakeGlContext::Create(500, 400);
    portgfx::GlContext::MakeCurrent(fg_fake_context_);
    fake_gm_ = new gfx::testing::FakeGraphicsManager();
    renderer_ = new gfx::Renderer(fake_gm_);

    // Create and register a TracingHandler.
    TracingHandler* th = new TracingHandler(frame_, renderer_);
    HttpServerTestRequestHandler* test_handler =
        new HttpServerTestRequestHandler(HttpServer::RequestHandlerPtr(th));
    test_handler->SetPreHandler(
        std::bind(&TracingHandlerTest::FakeGlContextSetup, this));
    test_handler->SetPostHandler(
        std::bind(&TracingHandlerTest::FakeGlContextTearDown, this));
    server_->RegisterHandler(HttpServer::RequestHandlerPtr(test_handler));

    // Add a pre-frame callback that will get invoked after the
    // TracingHandler's. This allows the test to make calls to the
    // FakeGraphicsManager between the Begin()/End() calls.
    // Note: Relies on the fact that handlers are called in alphabetical order.
    frame_->AddPreFrameCallback("zTracingHandlerTest",
                                std::bind(&TracingHandlerTest::MakeOpenGLCalls,
                                          this, std::placeholders::_1));
  }

  void TearDown() override {
    RemoteServerTest::TearDown();
    // Make sure objects are destroyed properly.
    renderer_ = nullptr;
    fake_gm_ = nullptr;
    fg_fake_context_.Reset();
    bg_fake_context_.Reset();
    ion::portgfx::GlContext::MakeCurrent(ion::portgfx::GlContextPtr());
    make_opengl_calls_ = false;
  }

  void FakeGlContextSetup() {
    // Each test creates two GL contexts: one for the foreground thread, and one
    // for the background thread.  The same background GL context is used for
    // all http requests in a single test, which makes it easy to verify the
    // correct context id in the generated trace.
    if (!bg_fake_context_) {
      bg_fake_context_ =
          gfx::testing::FakeGlContext::CreateShared(*fg_fake_context_);
    }
    ion::portgfx::GlContext::MakeCurrent(bg_fake_context_);
  }
  void FakeGlContextTearDown() {
    // TestScene includes some invalid index buffer types.
    if (gfx::testing::FakeGlContext::GetCurrent()) {
      fake_gm_->SetErrorCode(GL_NO_ERROR);
    }
  }

  void FakeGlContextRestore() {
    ion::portgfx::GlContext::MakeCurrent(fg_fake_context_);
  }

  void MakeOpenGLCalls(const gfxutils::Frame&) {
    // Make the calls only if requested and the TracingHandler's stream is
    // active.
    auto& tracing_stream = fake_gm_->GetTracingStream();
    if (make_opengl_calls_ && tracing_stream.IsTracing()) {
      base::LogChecker log_checker;
      fake_gm_->EnableErrorChecking(true);
      // Simulate labels and indentation.
      tracing_stream << ">Top level label:\n";
      fake_gm_->Clear(GL_COLOR_BUFFER_BIT);
      tracing_stream.EnterScope(bg_fake_context_->GetId(), "Nested label");
      uniform_storage[0] = 3.0f;
      uniform_storage[1] = 4.0f;
      uniform_storage[2] = 5.0f;
      uniform_storage[3] = 6.0f;
      EXPECT_FALSE(log_checker.HasAnyMessages());
      // This should result in an error.
      fake_gm_->Uniform4fv(2, 1, uniform_storage);
      fake_gm_->EnableErrorChecking(false);
      EXPECT_TRUE(log_checker.HasMessage(
          "ERROR", "GL error after call to Uniform4fv"));
    }
  }

  const std::string GetUniformStorageAddressAsString() {
    std::ostringstream s;
    s << "0x" << std::hex << reinterpret_cast<size_t>(uniform_storage);
    return s.str();
  }

  gfx::testing::FakeGraphicsManagerPtr fake_gm_;
  gfx::RendererPtr renderer_;
  gfxutils::FramePtr frame_;
  // When true, this actually makes some OpenGL calls in MakeOpenGLCalls().
  bool make_opengl_calls_;

  // This is passed to Uniform4fv(); its address appears the trace.
  GLfloat uniform_storage[4];
  base::SharedPtr<gfx::testing::FakeGlContext> fg_fake_context_;
  base::SharedPtr<gfx::testing::FakeGlContext> bg_fake_context_;
};

TEST_F(TracingHandlerTest, ServeTracing) {
#if !ION_PRODUCTION  // OpenGL tracing is disabled in prod builds.
  GetUri("/ion/tracing/does/not/exist");
  Verify404(__LINE__);

  GetUri("/ion/tracing/index.html");
  const std::string& index =
      base::ZipAssetManager::GetFileData("ion/tracing/index.html");
  EXPECT_FALSE(base::IsInvalidReference(index));
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ(index, response_.data);

  GetUri("/ion/tracing/");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ(index, response_.data);

  GetUri("/ion/tracing");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ(index, response_.data);

  // Skip to frame 2.
  frame_->Begin();
  frame_->End();
  frame_->Begin();
  frame_->End();
  EXPECT_EQ(2U, frame_->GetCounter());

  // Trace the next frame. Mark this as nonblocking so the handler does not
  // block until a frame is rendered.  The response should be an empty trace.
  GetUri("/ion/tracing/trace_next_frame?nonblocking");
  EXPECT_EQ(200, response_.status);
  const std::string expected_start1 = "<span class=\"trace_header\">Frame ";
  const std::string expected_start2 =
      "</span><br><br>\n<div class=\"tree\">\n<ul>\n";
  const std::string expected_end = "</ul>\n</div>\n";
  const std::string expected1 =
      expected_start1 + "2" + expected_start2 + expected_end;
  EXPECT_TRUE(base::testing::MultiLineStringsEqual(expected1, response_.data));

  // Skip a frame and trace again, this time with labels and OpenGL
  // calls. Use calls that will cover all HTML generation code.
  frame_->Begin();
  frame_->End();
  make_opengl_calls_ = true;
  frame_->Begin();
  frame_->End();
  GetUri("/ion/tracing/trace_next_frame?nonblocking");
  EXPECT_EQ(200, response_.status);
  std::string bg_vid = ion::base::ValueToString(bg_fake_context_->GetId());
  const std::string uniform_address_string = GetUniformStorageAddressAsString();
  EXPECT_TRUE(base::testing::MultiLineStringsEqual(
      expected1 + "<hr>\n" + expected_start1 + "5, GlContext " + bg_vid +
          expected_start2 +
          R"(<li><input type ="checkbox" checked="checked" id="list-0"/>)"
          "<label for=\"list-0\">Top level label</label>\n"
          "<ul>\n</ul>\n</li>\n"
          "<li><span class=\"trace_function\">Clear</span>("
          "<span class=\"trace_arg_name\">mask</span> = "
          "<span class=\"trace_arg_value\">GL_COLOR_BUFFER_BIT</span>)</li>\n"
          R"(<li><input type ="checkbox" checked="checked" id="list-1"/>)"
          "<label for=\"list-1\">Nested label</label>\n"
          "<ul>\n"
          "<li><span class=\"trace_function\">Uniform4fv</span>("
          "<span class=\"trace_arg_name\">location</span> ="
          " <span class=\"trace_arg_value\">2</span>,"
          " <span class=\"trace_arg_name\">count</span> ="
          " <span class=\"trace_arg_value\">1</span>,"
          " <span class=\"trace_arg_name\">value</span> ="
          " <span class=\"trace_arg_value\">" +
          uniform_address_string +
          " -> [3; 4; 5; 6]</span>)"
          "</li>\n"
          "<br><span class=\"trace_error\">***OpenGL Error: "
          "GL_INVALID_OPERATION</span><br><br>\n"
          "</ul>\n</li>\n" +
          expected_end,
      response_.data));
  make_opengl_calls_ = false;

  // Test clearing.
  GetUri("/ion/tracing/clear");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("clear", response_.data);

  // Tracing the next frame should result in an empty trace again.
  GetUri("/ion/tracing/trace_next_frame?nonblocking");
  EXPECT_EQ(200, response_.status);
  EXPECT_TRUE(base::testing::MultiLineStringsEqual(
                  expected_start1 + "6" + expected_start2 + expected_end,
                  response_.data));

  FakeGlContextRestore();
#endif
}

TEST_F(TracingHandlerTest, DeleteResources) {
  // This test verifies that resources are cleared properly by the Renderer if
  // the URI contains resources_to_delete.

#if !ION_PRODUCTION  // OpenGL tracing is disabled in prod builds.
  gfx::testing::TestScene test_scene;
  gfx::testing::TraceVerifier trace_verifier(fake_gm_.Get());
  // Render one frame to create resources.
  gfx::NodePtr root = test_scene.GetScene()->GetChildren()[0];
  gfx::ShaderProgramPtr shader_program = root->GetShaderProgram();
  gfx::ShaderPtr fragment = shader_program->GetFragmentShader();
  fragment->SetSource(std::string("    \n") + fragment->GetSource() +
                      std::string("     \n"));
  frame_->Begin();
  renderer_->DrawScene(test_scene.GetScene());
  frame_->End();

  // Delete the resources when rendering the next frame.
  GetUri(
      "/ion/tracing/"
      "trace_next_frame?nonblocking&resources_to_delete=Samplers%2cShader+"
      "Programs");
  EXPECT_EQ(200, response_.status);

  // Verify that resources were deleted.
  std::vector<std::string> calls;
  calls.push_back("DeleteSamplers");
  calls.push_back("DeleteProgram");
  EXPECT_TRUE(trace_verifier.VerifySomeCalls(calls));

  // TestScene includes some invalid index buffer types.
  fake_gm_->SetErrorCode(GL_NO_ERROR);
#endif
}

}  // namespace remote
}  // namespace ion

#endif
