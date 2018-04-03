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

// These tests rely on trace streams, which are disabled in production builds.
#if !ION_PRODUCTION

#include "ion/gfx/tests/renderer_common.h"
#include "absl/base/macros.h"

namespace ion {
namespace gfx {

using math::Point2i;
using math::Range1i;
using math::Range1ui;
using math::Range2i;
using math::Vector2i;
using portgfx::GlContextPtr;
using testing::FakeGlContext;

TEST_F(RendererTest, TextureWithZeroDimensionsAreNotAllocated) {
  base::LogChecker log_checker;

  // A default scene should render fine.
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  {
    RendererPtr renderer(new Renderer(gm_));
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(7U, trace_verifier_->GetCountOf("TexImage2D"));
  }

  data_->image->Set(options_->image_format, 0, 0, base::DataContainerPtr());
  SetImages(data_);

  Reset();
  {
    RendererPtr renderer(new Renderer(gm_));
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  }

  EXPECT_FALSE(log_checker.HasAnyMessages());
  BuildImage(data_, options_);
}

TEST_F(RendererTest, Texture3dWarningsWhenDisabled) {
  RendererPtr renderer(new Renderer(gm_));
  base::LogChecker log_checker;

  // A default scene should render fine.
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  Reset();
  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // Using a 3D image should be no problem if the function group is available.
  options_->image_type = Image::kDense;
  options_->image_dimensions = Image::k3d;
  BuildImage(data_, options_);
  Reset();
  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_LT(0U, trace_verifier_->GetCountOf("TexImage3D"));

  // Without the function group we get an error.
  gm_->EnableFeature(GraphicsManager::kTexture3d, false);
  BuildImage(data_, options_);
  Reset();
  renderer->DrawScene(root);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "3D texturing is not supported"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage3D"));

  options_->image_format = Image::kDxt5;
  gm_->EnableFeature(GraphicsManager::kTexture3d, true);
  BuildImage(data_, options_);
  Reset();
  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_LT(0U, trace_verifier_->GetCountOf("CompressedTexImage3D"));

  gm_->EnableFeature(GraphicsManager::kTexture3d, false);
  BuildImage(data_, options_);
  Reset();
  renderer->DrawScene(root);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "3D texturing is not supported"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("CompressedTexImage3D"));

  gm_->EnableFeature(GraphicsManager::kTexture3d, true);
}

TEST_F(RendererTest, TextureTargets) {
  RendererPtr renderer(new Renderer(gm_));

  // Test usage of TexImage2D.
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  Reset();
  renderer->DrawScene(root);

  options_->SetImageType(Image::kArray, Image::k2d);
  BuildImage(data_, options_);
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexImage2D(GL_TEXTURE_1D_ARRAY"));
  EXPECT_EQ(6U, trace_verifier_->GetCountOf("TexImage2D(GL_TEXTURE_CUBE"));

  options_->SetImageType(Image::kDense, Image::k2d);
  BuildImage(data_, options_);
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexImage2D(GL_TEXTURE_2D, "));
  EXPECT_EQ(6U, trace_verifier_->GetCountOf("TexImage2D(GL_TEXTURE_CUBE"));

  options_->SetImageType(Image::kArray, Image::k3d);
  BuildImage(data_, options_);
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexImage3D(GL_TEXTURE_2D_ARRAY"));
  EXPECT_EQ(6U, trace_verifier_->GetCountOf("TexImage3D(GL_TEXTURE_CUBE"));

  options_->SetImageType(Image::kDense, Image::k3d);
  BuildImage(data_, options_);
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexImage3D(GL_TEXTURE_3D, "));
  EXPECT_EQ(6U, trace_verifier_->GetCountOf("TexImage3D(GL_TEXTURE_CUBE"));

  {
    options_->SetImageType(Image::kExternalEgl, Image::k2d);
    base::LogChecker log_checker;
    BuildImage(data_, options_);
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(7U, trace_verifier_->GetCountOf(
                      "EGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, "));
    EXPECT_TRUE(log_checker.HasMessage("WARNING", "number of components"));
  }

  options_->SetImageType(Image::kEgl, Image::k2d);
  BuildImage(data_, options_);
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(7U, trace_verifier_->GetCountOf(
                    "EGLImageTargetTexture2DOES(GL_TEXTURE_2D, "));
  gm_->EnableFeature(GraphicsManager::kTexture3d, true);

  // Ensure that kClearTextures is unbinding all supported texture targets.
  const uint32 nunits = GetGraphicsManager()->GetConstant<int>(
      GraphicsManager::kMaxTextureImageUnits);
  renderer->SetFlag(Renderer::kClearTextures);
  renderer->DrawScene(root);
  EXPECT_EQ(nunits, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D, "));
  EXPECT_EQ(nunits,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D_ARRAY, "));
  EXPECT_EQ(nunits, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_3D, "));
  EXPECT_EQ(nunits, trace_verifier_->GetCountOf(
                        "BindTexture(GL_TEXTURE_EXTERNAL_OES, "));
  EXPECT_EQ(nunits,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_1D_ARRAY, "));

  // Ensure that disabled targets are not cleared.
  options_->SetImageType(Image::kDense, Image::k2d);
  root = BuildGraph(data_, options_, kWidth, kHeight);
  Reset();
  gm_->EnableFeature(GraphicsManager::kTextureArray1d, false);
  renderer->DrawScene(root);
  EXPECT_EQ(0U,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_1D_ARRAY, "));
}

TEST_F(RendererTest, ImageFormat) {
  // Test image format usage.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  TracingHelper helper;
  base::LogChecker log_checker;

  // Save image dimensions.
  const uint32 width_2d = data_->image->GetWidth();
  const uint32 height_2d = data_->image->GetHeight();

  options_->SetImageType(Image::kDense, Image::k3d);
  BuildImage(data_, options_);
  const uint32 width_3d = data_->image->GetWidth();
  const uint32 height_3d = data_->image->GetHeight();
  const uint32 depth_3d = data_->image->GetDepth();

  // Test usage of TexImage2D.
  Options options_2d;  // Default options describe a 2D dense image.
  VerifyRenderData<Image::Format> verify_2d_data;
  verify_2d_data.update_func = std::bind(BuildImage, data_, &options_2d);
  verify_2d_data.option = &options_2d.image_format;
  verify_2d_data.call_name = "TexImage2D";
  verify_2d_data.static_args.push_back(StaticArg(1, "GL_TEXTURE_2D"));
  verify_2d_data.static_args.push_back(StaticArg(2, "0"));
  verify_2d_data.static_args.push_back(
      StaticArg(4, helper.ToString("GLsizei", static_cast<GLsizei>(width_2d))));
  verify_2d_data.static_args.push_back(StaticArg(
      5, helper.ToString("GLsizei", static_cast<GLsizei>(height_2d))));
  verify_2d_data.static_args.push_back(StaticArg(7, "0"));  // Format.
  verify_2d_data.static_args.push_back(StaticArg(8, "0"));  // Type.
  verify_2d_data.static_args.push_back(
      StaticArg(9, helper.ToString(
          "void*", data_->image_container->GetData())));
  verify_2d_data.varying_arg_index = 3U;

  // Test usage of TexImage3D.
  Options options_3d;
  options_3d.SetImageType(Image::kDense, Image::k3d);
  VerifyRenderData<Image::Format> verify_3d_data;
  verify_3d_data.update_func = std::bind(BuildImage, data_, &options_3d);
  verify_3d_data.option = &options_3d.image_format;
  verify_3d_data.call_name = "TexImage3D";
  verify_3d_data.static_args.push_back(StaticArg(1, "GL_TEXTURE_3D"));
  verify_3d_data.static_args.push_back(StaticArg(2, "0"));
  verify_3d_data.static_args.push_back(
      StaticArg(4, helper.ToString("GLsizei", static_cast<GLsizei>(width_3d))));
  verify_3d_data.static_args.push_back(StaticArg(
      5, helper.ToString("GLsizei", static_cast<GLsizei>(height_3d))));
  verify_3d_data.static_args.push_back(
      StaticArg(6, helper.ToString("GLsizei", static_cast<GLsizei>(depth_3d))));
  verify_3d_data.static_args.push_back(StaticArg(8, "0"));  // Format.
  verify_3d_data.static_args.push_back(StaticArg(9, "0"));  // Type.
  verify_3d_data.static_args.push_back(
      StaticArg(10, helper.ToString(
          "void*", data_->image_container->GetData())));
  verify_3d_data.varying_arg_index = 3U;

  int last_component_count = 0;
  for (uint32 i = 0; i < Image::kNumFormats - 1U; ++i) {
    const Image::Format format = static_cast<Image::Format>(i);
    if (!Image::IsCompressedFormat(format)) {
      const Image::PixelFormat& pf = Image::GetPixelFormat(format);
      if (pf.internal_format != GL_STENCIL_INDEX8) {
        verify_2d_data.static_args[4] =
            StaticArg(7, helper.ToString("GLenum", pf.format));
        verify_2d_data.static_args[5] =
            StaticArg(8, helper.ToString("GLenum", pf.type));
        verify_3d_data.static_args[5] =
            StaticArg(8, helper.ToString("GLenum", pf.format));
        verify_3d_data.static_args[6] =
            StaticArg(9, helper.ToString("GLenum", pf.type));
        verify_2d_data.arg_tests.clear();
        verify_3d_data.arg_tests.clear();
        VaryingArg<Image::Format> arg = VaryingArg<Image::Format>(
            0, format, helper.ToString("GLenum", pf.internal_format));
        verify_2d_data.arg_tests.push_back(arg);
        verify_3d_data.arg_tests.push_back(arg);
        EXPECT_TRUE(
            VerifyRenderCalls(verify_2d_data, trace_verifier_, renderer, root));
        const int component_count = Image::GetNumComponentsForFormat(format);
        if (last_component_count && component_count < last_component_count) {
          EXPECT_TRUE(log_checker.HasMessage(
              "WARNING", "the number of components for this upload is"));
        } else {
          EXPECT_FALSE(log_checker.HasAnyMessages());
        }
        EXPECT_TRUE(
            VerifyRenderCalls(verify_3d_data, trace_verifier_, renderer, root));
        EXPECT_FALSE(log_checker.HasAnyMessages());
        last_component_count = component_count;
      }
    }
  }

  // Test deprecation of luminance and luminance-alpha textures on newer desktop
  // GL. In the following paragraph, static_args[4] corresponds to the pixel
  // format (the 7th arg to glTexImage2D), and arg_tests[0] corresponds to the
  // internal format (the 3rd arg to glTexImage2D).
  verify_2d_data.static_args[5] = StaticArg(8, "GL_UNSIGNED_BYTE");

  // Luminance remains luminance in OpenGL 2.9.
  gm_->SetVersionString("2.9 Ion OpenGL");
  verify_2d_data.arg_tests[0] =
      VaryingArg<Image::Format>(0, Image::kLuminance, "GL_LUMINANCE");
  verify_2d_data.static_args[4] = StaticArg(7, "GL_LUMINANCE");
  EXPECT_TRUE(
      VerifyRenderCalls(verify_2d_data, trace_verifier_, renderer, root));

  // R8 becomes luminance in OpenGL 2.9.
  verify_2d_data.arg_tests[0] =
      VaryingArg<Image::Format>(0, Image::kR8, "GL_LUMINANCE");
  verify_2d_data.static_args[4] = StaticArg(7, "GL_LUMINANCE");
  EXPECT_TRUE(
      VerifyRenderCalls(verify_2d_data, trace_verifier_, renderer, root));

  // Luminance becomes R8 in OpenGL 3.0.
  gm_->SetVersionString("3.0 Ion OpenGL");
  verify_2d_data.arg_tests[0] =
      VaryingArg<Image::Format>(0, Image::kLuminance, "GL_R8");
  verify_2d_data.static_args[4] = StaticArg(7, "GL_RED");
  EXPECT_TRUE(
      VerifyRenderCalls(verify_2d_data, trace_verifier_, renderer, root));

  // R8 remains R8 in OpenGL 3.0.
  verify_2d_data.arg_tests[0] =
      VaryingArg<Image::Format>(0, Image::kR8, "GL_R8");
  verify_2d_data.static_args[4] = StaticArg(7, "GL_RED");
  EXPECT_TRUE(
      VerifyRenderCalls(verify_2d_data, trace_verifier_, renderer, root));

  // Luminance/alpha remains luminance/alpha in OpenGL 2.9.
  gm_->SetVersionString("2.9 Ion OpenGL");
  verify_2d_data.arg_tests[0] =
      VaryingArg<Image::Format>(
          0, Image::kLuminanceAlpha, "GL_LUMINANCE_ALPHA");
  verify_2d_data.static_args[4] = StaticArg(7, "GL_LUMINANCE_ALPHA");
  EXPECT_TRUE(
      VerifyRenderCalls(verify_2d_data, trace_verifier_, renderer, root));

  // RG8 becomes luminance/alpha in OpenGL 2.9.
  verify_2d_data.arg_tests[0] =
      VaryingArg<Image::Format>(0, Image::kRg8, "GL_LUMINANCE_ALPHA");
  verify_2d_data.static_args[4] = StaticArg(7, "GL_LUMINANCE_ALPHA");
  EXPECT_TRUE(
      VerifyRenderCalls(verify_2d_data, trace_verifier_, renderer, root));

  // Luminance/alpha becomes RG8 in OpenGL 3.0.
  gm_->SetVersionString("3.0 Ion OpenGL");
  verify_2d_data.arg_tests[0] =
      VaryingArg<Image::Format>(0, Image::kLuminanceAlpha, "GL_RG8");
  verify_2d_data.static_args[4] = StaticArg(7, "GL_RG");
  EXPECT_TRUE(
      VerifyRenderCalls(verify_2d_data, trace_verifier_, renderer, root));

  // RG8 remains RG8 in OpenGL 3.0.
  verify_2d_data.arg_tests[0] =
      VaryingArg<Image::Format>(0, Image::kRg8, "GL_RG8");
  verify_2d_data.static_args[4] = StaticArg(7, "GL_RG");
  EXPECT_TRUE(
      VerifyRenderCalls(verify_2d_data, trace_verifier_, renderer, root));

  gm_->SetVersionString("3.3 Ion OpenGL / ES");

  // Test compressed formats.
  Image::Format compressed_formats[] = {
      Image::kDxt1,
      Image::kEtc1,
      Image::kEtc2Rgb,
      Image::kEtc2Rgba,
      Image::kEtc2Rgba1,
      Image::kPvrtc1Rgb4,
      Image::kDxt5,
      Image::kPvrtc1Rgba2,
      Image::kPvrtc1Rgba4};
  const int num_compressed_formats =
      static_cast<int>(ABSL_ARRAYSIZE(compressed_formats));

  verify_3d_data.arg_tests.clear();
  verify_3d_data.static_args.clear();
  verify_3d_data.update_func = std::bind(BuildImage, data_, &options_3d);
  verify_3d_data.call_name = "CompressedTexImage3D";
  verify_3d_data.option = &options_3d.image_format;
  verify_3d_data.static_args.push_back(StaticArg(2, "0"));
  verify_3d_data.static_args.push_back(
      StaticArg(4, helper.ToString("GLsizei", static_cast<GLsizei>(width_3d))));
  verify_3d_data.static_args.push_back(StaticArg(
      5, helper.ToString("GLsizei", static_cast<GLsizei>(height_3d))));
  verify_3d_data.static_args.push_back(
      StaticArg(6, helper.ToString("GLsizei", static_cast<GLsizei>(depth_3d))));
  verify_3d_data.static_args.push_back(StaticArg(7, "0"));
  verify_3d_data.static_args.push_back(
      StaticArg(9, helper.ToString(
          "void*", data_->image_container->GetData())));
  verify_3d_data.varying_arg_index = 3U;

  verify_2d_data.update_func = std::bind(BuildImage, data_, &options_2d);
  verify_2d_data.arg_tests.clear();
  verify_2d_data.static_args.clear();
  verify_2d_data.call_name = "CompressedTexImage2D";
  verify_2d_data.option = &options_2d.image_format;
  verify_2d_data.static_args.push_back(StaticArg(2, "0"));
  verify_2d_data.static_args.push_back(
      StaticArg(4, helper.ToString("GLsizei", static_cast<GLsizei>(width_2d))));
  verify_2d_data.static_args.push_back(StaticArg(
      5, helper.ToString("GLsizei", static_cast<GLsizei>(height_2d))));
  verify_2d_data.static_args.push_back(StaticArg(6, "0"));
  verify_2d_data.static_args.push_back(
      StaticArg(8, helper.ToString(
          "void*", data_->image_container->GetData())));
  verify_2d_data.varying_arg_index = 3U;

  verify_2d_data.arg_tests.push_back(
      VaryingArg<Image::Format>(
          0, Image::kDxt1, "GL_COMPRESSED_RGB_S3TC_DXT1_EXT"));
  verify_2d_data.static_args.push_back(StaticArg(
      7, helper.ToString("GLsizei", static_cast<GLsizei>(Image::ComputeDataSize(
                                        Image::kDxt1, width_2d, height_2d)))));
  verify_3d_data.arg_tests.push_back(verify_2d_data.arg_tests[0]);
  verify_3d_data.static_args.push_back(StaticArg(
      8, helper.ToString("GLsizei",
                         static_cast<GLsizei>(Image::ComputeDataSize(
                             Image::kDxt1, width_3d, height_3d, depth_3d)))));

  for (int i = 0; i < num_compressed_formats; ++i) {
    const Image::Format format = compressed_formats[i];
    SCOPED_TRACE(Image::GetFormatString(format));
    verify_3d_data.arg_tests[0] = verify_2d_data.arg_tests[0] =
        VaryingArg<Image::Format>(
            0, format,
            helper.ToString("GLenum",
                            Image::GetPixelFormat(format).internal_format));
    verify_2d_data.static_args[5] = StaticArg(
        7,
        helper.ToString("GLsizei", static_cast<GLsizei>(Image::ComputeDataSize(
                                       format, width_2d, height_2d))));
    verify_3d_data.static_args[6] = StaticArg(
        8, helper.ToString("GLsizei",
                           static_cast<GLsizei>(Image::ComputeDataSize(
                               format, width_3d, height_3d, depth_3d))));
    EXPECT_TRUE(
        VerifyRenderCalls(verify_2d_data, trace_verifier_, renderer, root));
    // Clear away warnings that occur when changing a texture format to another
    // format that has a different number of components.
    log_checker.ClearLog();
    EXPECT_TRUE(
        VerifyRenderCalls(verify_3d_data, trace_verifier_, renderer, root));
    EXPECT_FALSE(log_checker.HasAnyMessages());
  }

  Reset();
  renderer = nullptr;
  EXPECT_TRUE(VerifyReleases(1));
}

TEST_F(RendererTest, FramebufferObject) {
  gm_->EnableErrorChecking(true);
  // Disable implicit multisampling for this test. This functionality is tested
  // separately.
  gm_->EnableFeature(GraphicsManager::kImplicitMultisample, false);
  options_->image_format = Image::kRgba8888;
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  base::LogChecker log_checker;
  TracingHelper helper;

  // Several images of different sizes.
  static const uint32 kMaxImageSize = 32;
  static const uint32 kNumMipmaps = 6U;  // log2(kMaxImageSize) + 1U;
  ImagePtr mipmaps[kNumMipmaps];
  for (uint32 i = 0; i < kNumMipmaps; ++i)
    mipmaps[i] = CreateNullImage(kMaxImageSize >> i, kMaxImageSize >> i,
                                 Image::kRgb888);

  uint32 texture_width = data_->texture->GetImage(0U)->GetWidth();
  uint32 texture_height = data_->texture->GetImage(0U)->GetHeight();
  data_->fbo = new FramebufferObject(0, texture_height);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "zero width or height"));

  data_->fbo = new FramebufferObject(texture_width, 0);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "zero width or height"));

  data_->fbo = new FramebufferObject(texture_width, texture_height);

  {
    RendererPtr renderer(new Renderer(gm_));
    // Test an incomplete framebuffer.
    Reset();
    EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == nullptr);
    renderer->BindFramebuffer(data_->fbo);
    EXPECT_EQ(data_->fbo.Get(), renderer->GetCurrentFramebuffer().Get());
    renderer->DrawScene(root);
    EXPECT_TRUE(log_checker.HasMessage("ERROR", "Framebuffer is not complete"));

    // Check no calls are made if there is no node.
    Reset();
    renderer->DrawScene(NodePtr());
    EXPECT_EQ(0U, trace_verifier_->GetCallCount());
  }

  {
    // Check a texture color attachment.
    RendererPtr renderer(new Renderer(gm_));
    data_->fbo->SetColorAttachment(
        0U, FramebufferObject::Attachment(data_->texture));
    Reset();
    EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == nullptr);
    renderer->BindFramebuffer(data_->fbo);
    EXPECT_EQ(data_->fbo.Get(), renderer->GetCurrentFramebuffer().Get());
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindFramebuffer"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexImage2D"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("FramebufferTexture2D"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawBuffers"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("ReadBuffer"));

    // Should not be multisampled.
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2DMultisample"));

    // Check that the texture was generated and bound before being bound as
    // the framebuffer's attachment.
    EXPECT_LT(trace_verifier_->GetNthIndexOf(0U, "TexImage2D"),
              trace_verifier_->GetNthIndexOf(
                  0U, "FramebufferTexture2D(GL_RENDERBUFFER"));

    // Check args to FramebufferTexture2D.
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
          trace_verifier_->GetNthIndexOf(0U, "FramebufferTexture2D"))
              .HasArg(3, "GL_TEXTURE_2D"));

    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());
  }

  {
    // Check that DrawBuffers and ReadBuffer are not used if not supported.
    gm_->EnableFeature(GraphicsManager::kDrawBuffers, false);
    gm_->EnableFeature(GraphicsManager::kReadBuffer, false);

    RendererPtr renderer(new Renderer(gm_));
    data_->fbo->SetColorAttachment(
        0U, FramebufferObject::Attachment(data_->texture));
    Reset();
    EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == nullptr);
    renderer->BindFramebuffer(data_->fbo);
    EXPECT_EQ(data_->fbo.Get(), renderer->GetCurrentFramebuffer().Get());
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindFramebuffer"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexImage2D"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("FramebufferTexture2D"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawBuffers"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("ReadBuffer"));

    // Should not be multisampled.
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2DMultisample"));

    // Check that the texture was generated and bound before being bound as
    // the framebuffer's attachment.
    EXPECT_LT(trace_verifier_->GetNthIndexOf(0U, "TexImage2D"),
              trace_verifier_->GetNthIndexOf(
                  0U, "FramebufferTexture2D(GL_RENDERBUFFER"));

    // Check args to FramebufferTexture2D.
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
          trace_verifier_->GetNthIndexOf(0U, "FramebufferTexture2D"))
              .HasArg(3, "GL_TEXTURE_2D"));

    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());

    gm_->EnableFeature(GraphicsManager::kDrawBuffers, true);
    gm_->EnableFeature(GraphicsManager::kReadBuffer, true);
  }

  {
    // Check that the current fbo follows the current GL context.
    RendererPtr renderer(new Renderer(gm_));
    data_->fbo->SetColorAttachment(
        0U, FramebufferObject::Attachment(data_->texture));
    FramebufferObjectPtr fbo2(
        new FramebufferObject(texture_width, texture_height));
    fbo2->SetColorAttachment(0U, FramebufferObject::Attachment(data_->texture));

    EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == nullptr);
    renderer->BindFramebuffer(data_->fbo);
    EXPECT_EQ(data_->fbo.Get(), renderer->GetCurrentFramebuffer().Get());

    GlContextPtr share_context = FakeGlContext::CreateShared(*gl_context_);
    EXPECT_EQ(data_->fbo.Get(), renderer->GetCurrentFramebuffer().Get());
    portgfx::GlContext::MakeCurrent(share_context);
    EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == nullptr);
    renderer->BindFramebuffer(fbo2);
    EXPECT_EQ(fbo2.Get(), renderer->GetCurrentFramebuffer().Get());

    portgfx::GlContext::MakeCurrent(gl_context_);
    EXPECT_EQ(data_->fbo.Get(), renderer->GetCurrentFramebuffer().Get());

    portgfx::GlContext::MakeCurrent(share_context);
    EXPECT_EQ(fbo2.Get(), renderer->GetCurrentFramebuffer().Get());
    // Destroy the shared resource binder.
    Renderer::DestroyCurrentStateCache();
    portgfx::GlContext::MakeCurrent(gl_context_);
  }

  {
    // Check a texture color attachment that uses mipmaps.
    // Set a full image pyramid.
    for (uint32 i = 0; i < kNumMipmaps; ++i)
      data_->texture->SetImage(i, mipmaps[i]);

    RendererPtr renderer(new Renderer(gm_));
    data_->fbo->SetColorAttachment(
        0U, FramebufferObject::Attachment(data_->texture));
    Reset();
    EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == nullptr);
    renderer->BindFramebuffer(data_->fbo);
    EXPECT_EQ(data_->fbo.Get(), renderer->GetCurrentFramebuffer().Get());
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());

    // Set up fbo to draw into first mip level.
    FramebufferObjectPtr mip_fbo(new FramebufferObject(
        texture_width >> 1, texture_height >> 1));
    mip_fbo->SetColorAttachment(0U,
        FramebufferObject::Attachment(data_->texture, 1));
    Reset();
    renderer->BindFramebuffer(mip_fbo);
    EXPECT_EQ(mip_fbo.Get(), renderer->GetCurrentFramebuffer().Get());
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());

    // Expect mismatched dimensions error.
    mip_fbo->SetColorAttachment(0U,
        FramebufferObject::Attachment(data_->texture, 0));
    Reset();
    renderer->BindFramebuffer(mip_fbo);
    EXPECT_EQ(mip_fbo.Get(), renderer->GetCurrentFramebuffer().Get());
    renderer->DrawScene(root);
    msg_stream_ << "Mismatched texture and FBO dimensions: 32 x 32 "
        "vs. 16 x 16";
    EXPECT_TRUE(log_checker.HasMessage("ERROR", msg_stream_.str()));
  }

  {
    // Check a cubemap color attachment.
    RendererPtr renderer(new Renderer(gm_));
    data_->fbo->SetColorAttachment(
        0U, FramebufferObject::Attachment(data_->cubemap,
                                          CubeMapTexture::kPositiveX));
    Reset();
    EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == nullptr);
    renderer->BindFramebuffer(data_->fbo);
    EXPECT_EQ(data_->fbo.Get(), renderer->GetCurrentFramebuffer().Get());
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());
  }

  {
    // Check a cubemap color attachment that uses mipmaps.
    // Set a full image pyramid for each face.
    for (int j = 0; j < 6; ++j) {
      for (uint32 i = 0; i < kNumMipmaps; ++i)
        data_->cubemap->SetImage(
            static_cast<CubeMapTexture::CubeFace>(j), i, mipmaps[i]);
    }

    RendererPtr renderer(new Renderer(gm_));
    data_->fbo->SetColorAttachment(
        0U, FramebufferObject::Attachment(data_->cubemap,
                                          CubeMapTexture::kPositiveZ));
    Reset();
    EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == nullptr);
    renderer->BindFramebuffer(data_->fbo);
    EXPECT_EQ(data_->fbo.Get(), renderer->GetCurrentFramebuffer().Get());
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());

    // Set up fbo to draw into first mip level.
    FramebufferObjectPtr mip_fbo(
        new FramebufferObject(texture_width >> 1, texture_height >> 1));
    mip_fbo->SetColorAttachment(
        0U, FramebufferObject::Attachment(data_->cubemap,
                                          CubeMapTexture::kPositiveZ, 1));
    Reset();
    renderer->BindFramebuffer(mip_fbo);
    EXPECT_EQ(mip_fbo.Get(), renderer->GetCurrentFramebuffer().Get());
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());

    // Expect mismatched dimensions error.
    mip_fbo->SetColorAttachment(
        0U, FramebufferObject::Attachment(data_->cubemap,
                                          CubeMapTexture::kPositiveZ, 0));
    Reset();
    renderer->BindFramebuffer(mip_fbo);
    EXPECT_EQ(mip_fbo.Get(), renderer->GetCurrentFramebuffer().Get());
    renderer->DrawScene(root);
    msg_stream_ << "Mismatched texture and FBO dimensions: 32 x 32 "
        "vs. 16 x 16";
    EXPECT_TRUE(log_checker.HasMessage("ERROR", msg_stream_.str()));
  }

  {
    // Check renderbuffer types.
    RendererPtr renderer(new Renderer(gm_));
    data_->fbo->SetColorAttachment(
        0U, FramebufferObject::Attachment(Image::kRgba4Byte));
    Reset();
    EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == nullptr);
    renderer->BindFramebuffer(data_->fbo);
    EXPECT_EQ(data_->fbo.Get(), renderer->GetCurrentFramebuffer().Get());
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(
            0U, "FramebufferRenderbuffer(GL_FRAMEBUFFER"))
                .HasArg(4, "0x1"));
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(
            0U, "RenderbufferStorage(GL_RENDERBUFFER"))
                .HasArg(2, helper.ToString(
                    "GLenum", static_cast<GLenum>(GL_RGBA4))));

    data_->fbo->SetColorAttachment(
        0U, FramebufferObject::Attachment(Image::kRgb565Byte));
    Reset();
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(
            0U, "RenderbufferStorage(GL_RENDERBUFFER"))
                .HasArg(2, helper.ToString(
                    "GLenum", static_cast<GLenum>(GL_RGB565))));

    static uint8 kData[12] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6,
                              0x7, 0x8, 0x9, 0xa, 0xb, 0xc};
    ImagePtr egl_image(new Image);
    egl_image->SetEglImage(base::DataContainer::Create<void>(
        kData, kNullFunction, false, egl_image->GetAllocator()));
    data_->fbo->SetColorAttachment(
        0U, FramebufferObject::Attachment::CreateFromEglImage(egl_image));
    Reset();
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(
                      "EGLImageTargetRenderbufferStorageOES"));
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(
            0U, "FramebufferRenderbuffer(GL_FRAMEBUFFER"))
                .HasArg(4, "0x1"));

    egl_image->SetEglImage(base::DataContainer::Create<void>(
        nullptr, kNullFunction, false, egl_image->GetAllocator()));
    Reset();
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());
    // Since the buffer is NULL nothing will be set.  This isn't an error
    // since the caller could just set it manually through OpenGL directly.
    EXPECT_EQ(0U, trace_verifier_->GetCountOf(
                      "EGLImageTargetRenderbufferStorageOES"));

    data_->fbo->SetColorAttachment(
        0U, FramebufferObject::Attachment(Image::kRgb5a1Byte));
    Reset();
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(
            0U, "RenderbufferStorage(GL_RENDERBUFFER"))
                .HasArg(2, helper.ToString(
                    "GLenum", static_cast<GLenum>(GL_RGB5_A1))));

    data_->fbo->SetDepthAttachment(
        FramebufferObject::Attachment(Image::kRenderbufferDepth16));
    Reset();
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(
            0U, "RenderbufferStorage(GL_RENDERBUFFER"))
                .HasArg(2, helper.ToString(
                    "GLenum", static_cast<GLenum>(GL_DEPTH_COMPONENT16))));

    data_->fbo->SetStencilAttachment(
        FramebufferObject::Attachment(Image::kStencil8));
    Reset();
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(
            0U, "RenderbufferStorage(GL_RENDERBUFFER"))
                .HasArg(2, helper.ToString(
                    "GLenum", static_cast<GLenum>(GL_STENCIL_INDEX8))));

    // Verify that packed depth stencil renderbuffers get one ID for both
    // attachments.
    data_->fbo->SetDepthAttachment(FramebufferObject::Attachment(
        Image::kRenderbufferDepth32fStencil8));
    data_->fbo->SetStencilAttachment(FramebufferObject::Attachment(
        Image::kRenderbufferDepth32fStencil8));
    Reset();
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("RenderbufferStorage("));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("DeleteRenderbuffers"));
    // There are 2 calls: first the old stencil buffer is unbound, then
    // the packed depth stencil renderbuffer is bound.
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("FramebufferRenderbuffer("));
    EXPECT_EQ(
        trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
            1U, "FramebufferRenderbuffer(")).GetArg(2),
        "GL_DEPTH_STENCIL_ATTACHMENT");

    // Verify that packed depth stencil textures have only one attachment call.
    ImagePtr depth_image(new Image);
    depth_image->Set(Image::Format::kRenderbufferDepth24Stencil8,
                     data_->image->GetWidth(), data_->image->GetHeight(),
                     ion::base::DataContainerPtr());
    ion::gfx::TexturePtr depth_texture(new Texture);
    depth_texture->SetImage(0U, depth_image);
    depth_texture->SetSampler(data_->sampler);
    renderer->CreateOrUpdateResource(depth_texture.Get());
    FramebufferObject::Attachment attachment(depth_texture);
    data_->fbo->SetDepthAttachment(attachment);
    data_->fbo->SetStencilAttachment(attachment);
    Reset();
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("FramebufferTexture2D("));
    EXPECT_EQ(
        trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
            0U, "FramebufferTexture2D(")).GetArg(2),
        "GL_DEPTH_STENCIL_ATTACHMENT");
  }

  {
    // Check color render buffer for multisamping.
    RendererPtr renderer(new Renderer(gm_));
    data_->fbo->SetColorAttachment(0U,
        FramebufferObject::Attachment::CreateMultisampled(Image::kRgba8, 4));
    data_->fbo->SetDepthAttachment(
        FramebufferObject::Attachment::CreateMultisampled(kDepthFormat, 4));
    data_->fbo->SetStencilAttachment(FramebufferObject::Attachment());
    Reset();
    EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == nullptr);
    renderer->BindFramebuffer(data_->fbo);
    EXPECT_EQ(data_->fbo.Get(), renderer->GetCurrentFramebuffer().Get());
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(
            0U, "RenderbufferStorageMultisample(GL_RENDERBUFFER"))
                .HasArg(2, helper.ToString(
                    "GLsizei", static_cast<GLsizei>(4))));

    // Try to set an incompatible attachment.
    data_->fbo->SetColorAttachment(0U,
        FramebufferObject::Attachment::CreateMultisampled(Image::kRgba8, 2));
    Reset();
    renderer->DrawScene(root);
    EXPECT_TRUE(log_checker.HasMessage(
        "ERROR", "Multisampled framebuffer is not complete"));
    data_->fbo->SetDepthAttachment(FramebufferObject::Attachment(kDepthFormat));
    renderer->DrawScene(root);
    EXPECT_TRUE(log_checker.HasMessage(
        "ERROR", "Multisampled framebuffer is not complete"));
    data_->fbo->SetDepthAttachment(
        FramebufferObject::Attachment::CreateMultisampled(kDepthFormat, 2));
    Reset();
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(
            0U, "RenderbufferStorageMultisample(GL_RENDERBUFFER"))
                .HasArg(2, helper.ToString(
                    "GLsizei", static_cast<GLsizei>(2))));
  }

  {
    // Use a new fbo instead of the old one since we don't care about
    // color buffer.
    data_->fbo = new FramebufferObject(texture_width, texture_height);
    // Check depth render buffer for multisamping.
    RendererPtr renderer(new Renderer(gm_));
    data_->fbo->SetDepthAttachment(
        FramebufferObject::Attachment::CreateMultisampled(kDepthFormat, 4));
    Reset();
    EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == nullptr);
    renderer->BindFramebuffer(data_->fbo);
    EXPECT_EQ(data_->fbo.Get(), renderer->GetCurrentFramebuffer().Get());
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(
            0U, "RenderbufferStorageMultisample(GL_RENDERBUFFER"))
                .HasArg(2, helper.ToString(
                    "GLsizei", static_cast<GLsizei>(4))));

    data_->fbo->SetDepthAttachment(
        FramebufferObject::Attachment::CreateMultisampled(kDepthFormat, 2));
    Reset();
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(
            0U, "RenderbufferStorageMultisample(GL_RENDERBUFFER"))
                .HasArg(2, helper.ToString(
                    "GLsizei", static_cast<GLsizei>(2))));
  }
}

TEST_F(RendererTest, FramebufferObjectMultisampleTextureAttachment) {
  options_->image_format = Image::kRgba8888;
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  base::LogChecker log_checker;
  TracingHelper helper;

  uint32 texture_width = data_->texture->GetImage(0U)->GetWidth();
  uint32 texture_height = data_->texture->GetImage(0U)->GetHeight();
  data_->fbo = new FramebufferObject(texture_width, texture_height);

  // Enable multisampling.
  data_->texture->SetMultisampling(8, true);

  // Check a texture color attachment.
  RendererPtr renderer(new Renderer(gm_));
  data_->fbo->SetColorAttachment(
      0U, FramebufferObject::Attachment(data_->texture));
  Reset();
  EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == nullptr);
  renderer->BindFramebuffer(data_->fbo);
  EXPECT_EQ(data_->fbo.Get(), renderer->GetCurrentFramebuffer().Get());
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindFramebuffer"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexImage2DMultisample"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("FramebufferTexture2D"));

  // Check args to TexImage2DMultisample.
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(0U, "TexImage2DMultisample"))
            .HasArg(1, "GL_TEXTURE_2D_MULTISAMPLE")
            .HasArg(2, "8"));

  // Check texture target arg to FramebufferTexture2D.
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(0U, "FramebufferTexture2D"))
            .HasArg(3, "GL_TEXTURE_2D_MULTISAMPLE"));

  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST_F(RendererTest, FramebufferObjectTextureLayerAttachment) {
  options_->image_format = Image::kRgba8888;
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  base::LogChecker log_checker;
  TracingHelper helper;

  ImagePtr image(new Image);
  image->SetArray(Image::kRgba8888, 512, 512, 4, base::DataContainerPtr());
  data_->texture->SetImage(0U, image);
  data_->texture->SetSampler(data_->sampler);

  RendererPtr renderer(new Renderer(gm_));
  data_->fbo = new FramebufferObject(512, 512);
  data_->fbo->SetColorAttachment(
      0U, FramebufferObject::Attachment::CreateFromLayer(data_->texture, 2));
  Reset();
  EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == nullptr);
  renderer->BindFramebuffer(data_->fbo);
  EXPECT_EQ(data_->fbo.Get(), renderer->GetCurrentFramebuffer().Get());
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindFramebuffer"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexImage3D"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("FramebufferTextureLayer"));

  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "TexImage3D"))
          .HasArg(1, "GL_TEXTURE_2D_ARRAY")
          .HasArg(6, "4"));
  std::string texture_id = helper.ToString("GLuint", renderer->GetResourceGlId(
      data_->texture.Get()));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "FramebufferTextureLayer"))
          .HasArg(3, texture_id)
          .HasArg(5, "2"));

  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());

  image->SetArray(Image::kRgba8888, 512, 512, 6, base::DataContainerPtr());
  data_->fbo->SetColorAttachment(
      0U, FramebufferObject::Attachment::CreateFromLayer(data_->texture, 4));
  image->SetArray(Image::kRgba8888, 512, 512, 4, base::DataContainerPtr());
  Reset();
  renderer->BindFramebuffer(data_->fbo);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Invalid texture layer index"));

  data_->fbo->SetColorAttachment(
      0U, FramebufferObject::Attachment::CreateFromLayer(data_->texture, 2));
  Reset();
  gm_->EnableFeature(GraphicsManager::kFramebufferTextureLayer, false);
  renderer->BindFramebuffer(data_->fbo);
  EXPECT_TRUE(log_checker.HasMessage(
      "ERROR", "glFramebufferTextureLayer is not supported"));
}

TEST_F(RendererTest, FramebufferObjectImplicitMultisampling) {
  options_->image_format = Image::kRgba8888;
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  base::LogChecker log_checker;
  TracingHelper helper;
  gm_->EnableErrorChecking(true);

  const uint32 texture_width = data_->texture->GetImage(0U)->GetWidth();
  const uint32 texture_height = data_->texture->GetImage(0U)->GetHeight();

  RendererPtr renderer(new Renderer(gm_));
  data_->fbo = new FramebufferObject(texture_width, texture_height);
  data_->fbo->SetColorAttachment(
      0U, FramebufferObject::Attachment::CreateImplicitlyMultisampled(
          data_->texture, 8));
  data_->fbo->SetDepthAttachment(
      FramebufferObject::Attachment::CreateMultisampled(
          Image::kRenderbufferDepth16, 8));
  Reset();
  renderer->BindFramebuffer(data_->fbo);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "FramebufferTexture2DMultisampleEXT("))
          .HasArg(6, "8"));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "RenderbufferStorageMultisampleEXT("))
          .HasArg(2, "8"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("FramebufferTexture2D("));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("RenderbufferStorageMultisample("));

  data_->fbo->SetColorAttachment(
      1U, FramebufferObject::Attachment::CreateImplicitlyMultisampled(
          data_->cubemap, CubeMapTexture::kPositiveY, 8));
  Reset();
  renderer->BindFramebuffer(data_->fbo);
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "FramebufferTexture2DMultisampleEXT("))
          .HasArg(6, "8"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("FramebufferTexture2D("));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("RenderbufferStorageMultisample("));

  gm_->EnableFeature(GraphicsManager::kImplicitMultisample, false);
  renderer.Reset(new Renderer(gm_));
  Reset();
  renderer->BindFramebuffer(data_->fbo);
  EXPECT_TRUE(log_checker.HasMessage(
      "ERROR", "Multisampled framebuffer is not complete"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("FramebufferTexture2D("));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "RenderbufferStorageMultisample("))
          .HasArg(2, "8"));
  EXPECT_EQ(0U,
            trace_verifier_->GetCountOf("FramebufferTexture2DMultisampleEXT("));
  EXPECT_EQ(0U,
            trace_verifier_->GetCountOf("RenderbufferStorageMultisampleEXT("));

  gm_->EnableFeature(GraphicsManager::kImplicitMultisample, true);
  data_->texture->SetMultisampling(8, true);
  data_->fbo->SetColorAttachment(
    0U, FramebufferObject::Attachment(data_->texture));
  data_->fbo->SetColorAttachment(1U, FramebufferObject::Attachment());
  renderer->ClearResources(data_->texture.Get());
  Reset();
  renderer->BindFramebuffer(data_->fbo);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("FramebufferTexture2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("RenderbufferStorageMultisample"));
  EXPECT_EQ(0U,
            trace_verifier_->GetCountOf("FramebufferTexture2DMultisampleEXT"));
  EXPECT_EQ(0U,
            trace_verifier_->GetCountOf("RenderbufferStorageMultisampleEXT"));
}

TEST_F(RendererTest, FramebufferObjectMultiviewAttachments) {
  options_->image_format = Image::kRgba8888;
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  base::LogChecker log_checker;
  TracingHelper helper;

  ImagePtr image(new Image);
  image->SetArray(Image::kRgba8888, 512, 512, 8, base::DataContainerPtr());
  data_->texture->SetImage(0U, image);
  data_->texture->SetSampler(data_->sampler);

  RendererPtr renderer(new Renderer(gm_));
  data_->fbo = new FramebufferObject(512, 512);
  data_->fbo->SetColorAttachment(
      0U, FramebufferObject::Attachment::CreateMultiview(
          data_->texture, 0, 8));
  Reset();
  renderer->BindFramebuffer(data_->fbo);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Too many views"));
  image->SetArray(Image::kRgba8888, 512, 512, 16, base::DataContainerPtr());
  data_->fbo->SetColorAttachment(
      0U, FramebufferObject::Attachment::CreateMultiview(
          data_->texture, 12, 2));
  image->SetArray(Image::kRgba8888, 512, 512, 8, base::DataContainerPtr());
  Reset();
  renderer->BindFramebuffer(data_->fbo);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Invalid multiview parameters"));
  data_->fbo->SetColorAttachment(
      0U, FramebufferObject::Attachment::CreateMultiview(
          data_->texture, 1, 4));
  Reset();
  renderer->BindFramebuffer(data_->fbo);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "FramebufferTextureMultiviewOVR("))
          .HasArg(5, "1").HasArg(6, "4"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("FramebufferTexture2D("));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("RenderbufferStorage"));

  gm_->EnableFeature(GraphicsManager::kMultiview, false);
  renderer.Reset(new Renderer(gm_));
  Reset();
  renderer->BindFramebuffer(data_->fbo);
  EXPECT_TRUE(log_checker.HasMessage("ERROR",
      "GL_OVR_multiview2 extension is not supported"));
  EXPECT_EQ(0U,
            trace_verifier_->GetCountOf("FramebufferTextureMultiviewOVR("));

  gm_->EnableFeature(GraphicsManager::kMultiview, true);
  data_->fbo->SetColorAttachment(
      0U, FramebufferObject::Attachment::CreateImplicitlyMultisampledMultiview(
          data_->texture, 1, 4, 64));
  Reset();
  renderer->BindFramebuffer(data_->fbo);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Too many samples"));
  data_->fbo->SetColorAttachment(
      0U, FramebufferObject::Attachment::CreateImplicitlyMultisampledMultiview(
          data_->texture, 1, 4, 8));
  Reset();
  renderer->BindFramebuffer(data_->fbo);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U,
          "FramebufferTextureMultisampleMultiviewOVR("))
              .HasArg(5, "8").HasArg(6, "1").HasArg(7, "4"));

  gm_->EnableFeature(GraphicsManager::kMultiviewImplicitMultisample, false);
  renderer.Reset(new Renderer(gm_));
  Reset();
  renderer->BindFramebuffer(data_->fbo);
  EXPECT_TRUE(log_checker.HasMessage("ERROR",
      "GL_OVR_multiview_multisampled_render_to_texture extension is not "
      "supported"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf(
      "FramebufferTextureMultisampleMultiviewOVR("));

  // Test for proper DEPTH_STENCIL behavior.
  renderer.Reset(new Renderer(gm_));
  Reset();
  ImagePtr depth_image(new Image);
  depth_image->SetArray(Image::kRenderbufferDepth24Stencil8, 512, 512, 2,
                        base::DataContainerPtr());
  ion::gfx::TexturePtr depth_texture(new Texture);
  depth_texture->SetImage(0U, depth_image);
  depth_texture->SetSampler(data_->sampler);
  renderer->CreateOrUpdateResource(depth_texture.Get());
  FramebufferObject::Attachment depth_attachment =
      FramebufferObject::Attachment::CreateMultiview(depth_texture, 0, 2);
  data_->fbo->SetDepthAttachment(depth_attachment);
  data_->fbo->SetStencilAttachment(depth_attachment);
  data_->fbo->SetColorAttachment(
      0U, FramebufferObject::Attachment::CreateMultiview(
          data_->texture, 0, 2));
  renderer->BindFramebuffer(data_->fbo);
  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "FramebufferTextureMultiviewOVR("))
          .HasArg(2, "GL_COLOR_ATTACHMENT"));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(1U, "FramebufferTextureMultiviewOVR("))
          .HasArg(2, "GL_DEPTH_ATTACHMENT"));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(2U, "FramebufferTextureMultiviewOVR("))
          .HasArg(2, "GL_STENCIL_ATTACHMENT"));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(3U, "FramebufferTextureMultiviewOVR("))
          .HasArg(2, "GL_DEPTH_STENCIL_ATTACHMENT"));
  // Now, create a proper array texture that's backed by an EGLImage.
  // This should succeed.
  Reset();
  static uint8 kTexelData[12] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6,
                                 0x7, 0x8, 0x9, 0xa, 0xb, 0xc};
  image->SetEglImageArray(base::DataContainer::Create<void>(
      kTexelData, kNullFunction, false, base::AllocatorPtr()));
  TexturePtr texture(new Texture);
  texture->SetImage(0U, image);
  texture->SetSampler(data_->sampler);
  data_->fbo->SetDepthAttachment(FramebufferObject::Attachment());
  data_->fbo->SetStencilAttachment(FramebufferObject::Attachment());
  data_->fbo->SetColorAttachment(
      0U, FramebufferObject::Attachment::CreateMultiview(texture, 0, 4));
  renderer->BindFramebuffer(data_->fbo);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("EGLImageTargetTexture2DOES("));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("FramebufferTextureMultiviewOVR("));
}

TEST_F(RendererTest, NonArrayMultiviewAttachmentDeathTest) {
  Reset();
  options_->image_format = Image::kRgba8888;
  BuildGraph(data_, options_, kWidth, kHeight);

  // Verify failure when attempting to create a multiview attachment from a
  // non-array image.
  ImagePtr image(new Image);
  data_->texture->SetImage(0U, image);
  data_->texture->SetSampler(data_->sampler);
  static uint8 kTexelData[12] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6,
                                 0x7, 0x8, 0x9, 0xa, 0xb, 0xc};
  image->SetEglImage(base::DataContainer::Create<void>(
      kTexelData, kNullFunction, false, base::AllocatorPtr()));
#if !ION_PRODUCTION
  EXPECT_DEATH_IF_SUPPORTED(
      FramebufferObject::Attachment::CreateMultiview(data_->texture, 0, 4),
      "Multiview image must be an array");
#endif
  Reset();
}

TEST_F(RendererTest, FramebufferObjectAttachmentsImplicitlyChangedByDraw) {
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  base::LogChecker log_checker;
  TracingHelper helper;

  uint32 texture_width = data_->texture->GetImage(0U)->GetWidth();
  uint32 texture_height = data_->texture->GetImage(0U)->GetHeight();
  data_->fbo = new FramebufferObject(texture_width, texture_height);

  {
    RendererPtr renderer(new Renderer(gm_));
    data_->fbo->SetColorAttachment(
        0U, FramebufferObject::Attachment(data_->texture));
    data_->sampler->SetAutogenerateMipmapsEnabled(true);
    renderer->BindFramebuffer(data_->fbo);
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenerateMipmap"));
    Reset();
    // Since the contents have changed, we should regenerate mipmaps.
    renderer->DrawScene(root);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenerateMipmap"));
    Reset();
    // Same thing again.
    renderer->DrawScene(root);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenerateMipmap"));

    // Nothing should happen if mipmaps are disabled.
    data_->sampler->SetAutogenerateMipmapsEnabled(false);
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenerateMipmap"));

    EXPECT_FALSE(log_checker.HasAnyMessages());
  }

  // Same thing with a cubemap face attachment.
  {
    RendererPtr renderer(new Renderer(gm_));
    data_->fbo->SetColorAttachment(
        0U, FramebufferObject::Attachment(data_->cubemap,
                                          CubeMapTexture::kPositiveX));
    data_->sampler->SetAutogenerateMipmapsEnabled(true);
    renderer->BindFramebuffer(data_->fbo);
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenerateMipmap"));
    Reset();
    // Since the contents have changed, we should regenerate mipmaps.
    renderer->DrawScene(root);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenerateMipmap"));
    Reset();
    // Same thing again.
    renderer->DrawScene(root);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenerateMipmap"));

    // Nothing should happen if mipmaps are disabled.
    data_->sampler->SetAutogenerateMipmapsEnabled(false);
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenerateMipmap"));

    EXPECT_FALSE(log_checker.HasAnyMessages());
  }
}

TEST_F(RendererTest, CubeMapTextureMipmaps) {
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  base::LogChecker log_checker;
  TracingHelper helper;

  // Check that each face of a CubeMapTexture with an image is sent as mipmap
  // level 0.
  Reset();
  renderer->DrawScene(root);
  EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U + kVboSize, 0U, 28672U));
  EXPECT_EQ(7U, trace_verifier_->GetCountOf("TexImage2D"));
  for (int i = 0; i < 6; ++i) {
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(i + 1U, "TexImage2D"))
            .HasArg(2, "0"));
  }

  // Several images of different sizes.
  static const uint32 kMaxImageSize = 32;
  static const uint32 kNumMipmaps = 6U;  // log2(kMaxImageSize) + 1U;
  ImagePtr mipmaps[kNumMipmaps];
  for (uint32 i = 0; i < kNumMipmaps; ++i)
    mipmaps[i] = CreateNullImage(kMaxImageSize >> i, kMaxImageSize >> i,
                                 Image::kRgba8888);

  // Set a full image pyramid for each face.
  for (int j = 0; j < 6; ++j) {
    for (uint32 i = 0; i < kNumMipmaps; ++i)
      data_->cubemap->SetImage(
          static_cast<CubeMapTexture::CubeFace>(j), i, mipmaps[i]);
  }

  // Check consistent dimensions.
  EXPECT_FALSE(log_checker.HasAnyMessages());
  Reset();
  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(kNumMipmaps * 6U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenerateMipmap"));
  // The cubemap now has mipmaps, so it's usage has increased by 4/3.
  EXPECT_EQ(36864U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(32768U, data_->cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, data_->texture->GetGpuMemoryUsed());

  // Check the right calls were made. First check the level 0 mipmaps.
  for (uint32 j = 0; j < 6; ++j) {
    SCOPED_TRACE(::testing::Message() << "Testing face " << j);
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(j, "TexImage2D"))
            .HasArg(1, helper.ToString(
                "GLenum", base::EnumHelper::GetConstant(
                    static_cast<CubeMapTexture::CubeFace>(j))))
            .HasArg(2, "0")
            .HasArg(4, helper.ToString(
                "GLsizei", static_cast<GLint>(mipmaps[0]->GetWidth())))
            .HasArg(5, helper.ToString(
                "GLsizei", static_cast<GLint>(mipmaps[0]->GetHeight()))));
  }
  // Now check the level 1+ mipmaps.
  for (uint32 j = 0; j < 6; ++j) {
  SCOPED_TRACE(::testing::Message() << "Testing face " << j);
    for (uint32 i = 1; i < kNumMipmaps; ++i) {
      SCOPED_TRACE(::testing::Message() << "Testing mipmap level " << i);
      EXPECT_TRUE(trace_verifier_->VerifyCallAt(
          trace_verifier_->GetNthIndexOf(6U + j * (kNumMipmaps - 1U) + i - 1U,
                                         "TexImage2D"))
              .HasArg(1, helper.ToString(
                  "GLenum", base::EnumHelper::GetConstant(
                      static_cast<CubeMapTexture::CubeFace>(j))))
              .HasArg(2, helper.ToString("GLint", static_cast<GLint>(i)))
              .HasArg(4, helper.ToString(
                  "GLsizei", static_cast<GLint>(mipmaps[i]->GetWidth())))
              .HasArg(5, helper.ToString(
                  "GLsizei", static_cast<GLint>(mipmaps[i]->GetHeight()))));
    }
  }

  // Remove an image from a few faces and verify that GenerateMipmap is called
  // only once for the entire texture.
  data_->cubemap->SetImage(CubeMapTexture::kNegativeZ, 1, ImagePtr(nullptr));
  data_->cubemap->SetImage(CubeMapTexture::kPositiveY, 3, ImagePtr(nullptr));
  data_->cubemap->SetImage(CubeMapTexture::kPositiveZ, 2, ImagePtr(nullptr));
  Reset();
  renderer->DrawScene(root);
  // Overall memory usage should be unchanged.
  EXPECT_EQ(36864U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(32768U, data_->cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, data_->texture->GetGpuMemoryUsed());
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenerateMipmap"));
  // Since GenerateMipmap was called the override mipmaps were sent, but the
  // 0th level mipmap doesn't have to be (GenerateMipmap won't override it).
  // Only the mipmaps of the 3 modified faces are sent.
  EXPECT_EQ((kNumMipmaps - 1U) * 3U - 3U,
            trace_verifier_->GetCountOf("TexImage2D"));

  // Set an invalid image dimension.
  data_->cubemap->SetImage(
      CubeMapTexture::kPositiveY, 1,
      CreateNullImage(mipmaps[1]->GetWidth() - 1, mipmaps[1]->GetHeight(),
                      Image::kRgba8888));
  Reset();
  renderer->DrawScene(root);
  // Generate mipmap will be called since a mipmap has changed, but no
  // overriding mipmaps will be set since one is invalid.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenerateMipmap"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  msg_stream_ << "Mipmap width: " << (mipmaps[1]->GetWidth() - 1) <<
      " is not a power of 2.";
  EXPECT_TRUE(log_checker.HasMessage("ERROR", msg_stream_.str()));
  // Overall memory usage should be unchanged since there was an error
  EXPECT_EQ(36864U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(32768U, data_->cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, data_->texture->GetGpuMemoryUsed());

  data_->cubemap->SetImage(
      CubeMapTexture::kPositiveY, 1,
      CreateNullImage(mipmaps[1]->GetWidth(), mipmaps[1]->GetHeight() - 1,
                      Image::kRgba8888));
  Reset();
  renderer->DrawScene(root);
  // Generate mipmap will be called since a mipmap has changed, but no
  // overriding mipmaps will be set since one is invalid.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenerateMipmap"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  msg_stream_ << "Mipmap height: " << (mipmaps[1]->GetWidth() - 1) <<
      " is not a power of 2.";
  EXPECT_TRUE(log_checker.HasMessage("ERROR", msg_stream_.str()));
  // Overall memory usage should be unchanged since there was an error
  EXPECT_EQ(36864U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(32768U, data_->cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, data_->texture->GetGpuMemoryUsed());

  // Set an invalid image dimension.
  data_->cubemap->SetImage(
      CubeMapTexture::kPositiveY, 1, CreateNullImage(
          mipmaps[1]->GetWidth(), mipmaps[1]->GetHeight(), Image::kRgb888));
  Reset();
  renderer->DrawScene(root);
  // Generate mipmap will be called since a mipmap has changed, but no
  // overriding mipmaps will be set since one is invalid.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenerateMipmap"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_TRUE(
      log_checker.HasMessage("ERROR", "level 1 has different format"));
  // Overall memory usage should be unchanged since there was an error
  EXPECT_EQ(36864U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(32768U, data_->cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, data_->texture->GetGpuMemoryUsed());
}

TEST_F(RendererTest, CubeMapTextureSubImages) {
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  base::LogChecker log_checker;
  TracingHelper helper;

  // Check that a texture with an image is sent as mipmap level 0.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(7U, trace_verifier_->GetCountOf("TexImage2D"));

  // Now set a subimage on the CubeMapTexture.
  data_->cubemap->SetSubImage(
      CubeMapTexture::kNegativeZ, 0U, math::Point2ui(12, 20), CreateNullImage(
          4, 8, Image::kRgba8888));
  // Check that the subimage is applied.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexSubImage2D"));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "TexSubImage2D"))
          .HasArg(2, "0")
          .HasArg(3, "12")
          .HasArg(4, "20")
          .HasArg(5, "4")
          .HasArg(6, "8")
          .HasArg(7, "GL_RGB")
          .HasArg(8, "GL_UNSIGNED_BYTE"));

  // Several images of different sizes.
  static const uint32 kMaxImageSize = 32;
  static const uint32 kNumMipmaps = 6U;  // log2(kMaxImageSize) + 1U;
  ImagePtr mipmaps[kNumMipmaps];
  for (uint32 i = 0; i < kNumMipmaps; ++i)
    mipmaps[i] = CreateNullImage(kMaxImageSize >> i, kMaxImageSize >> i,
                                 Image::kRgba8888);

  // Set a full image pyramid for each face.
  for (int j = 0; j < 6; ++j) {
    for (uint32 i = 0; i < kNumMipmaps; ++i)
      data_->cubemap->SetImage(
          static_cast<CubeMapTexture::CubeFace>(j), i, mipmaps[i]);
  }

  // Check consistent dimensions.
  EXPECT_FALSE(log_checker.HasAnyMessages());
  Reset();
  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(kNumMipmaps * 6U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenerateMipmap"));
  EXPECT_EQ(36864U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(32768U, data_->cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, data_->texture->GetGpuMemoryUsed());

  // Set a submipmap at level 3. Setting a compressed image requires non-NULL
  // image data.
  ImagePtr compressed_image(new Image);
  compressed_image->Set(
      Image::kDxt5, 4, 8,
      base::DataContainerPtr(base::DataContainer::Create(
          reinterpret_cast<void*>(1), kNullFunction, false,
          compressed_image->GetAllocator())));
  data_->cubemap->SetSubImage(
      CubeMapTexture::kNegativeZ, 3U, math::Point2ui(12, 8), compressed_image);

  // Check the right call was made.
  Reset();
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), gm_->GetError());
  renderer->DrawScene(root);
  // Subimages do not resize textures.
  EXPECT_EQ(36864U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(32768U, data_->cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, data_->texture->GetGpuMemoryUsed());
  // Technically there is an errors since the cubemap is not compressed, but
  // this is just to test that the call is made.
  gm_->SetErrorCode(GL_NO_ERROR);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexSubImage2D"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("CompressedTexSubImage2D"));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "CompressedTexSubImage2D"))
          .HasArg(2, "3")
          .HasArg(3, "12")
          .HasArg(4, "8")
          .HasArg(5, "4")
          .HasArg(6, "8")
          .HasArg(7, "GL_COMPRESSED_RGBA_S3TC_DXT5_EXT"));
}

TEST_F(RendererTest, CubeMapTextureMisc) {
  // Test various texture corner cases.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  base::LogChecker log_checker;

  // Check that a texture with no image does not get sent.
  for (int j = 0; j < 6; ++j)
    data_->cubemap->SetImage(static_cast<CubeMapTexture::CubeFace>(j), 0U,
                             ImagePtr(nullptr));
  Reset();
  renderer->DrawScene(root);
  // The regular texture is still sent the first time.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_TRUE(log_checker.HasMessage("WARNING",
                                     "texture face Negative X has no level"));
  Reset();
  data_->cubemap->SetImage(CubeMapTexture::kNegativeX, 0U, data_->image);
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_TRUE(log_checker.HasMessage("WARNING",
                                     "texture face Negative Y has no level"));
  Reset();
  data_->cubemap->SetImage(CubeMapTexture::kNegativeY, 0U, data_->image);
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_TRUE(log_checker.HasMessage("WARNING",
                                     "texture face Negative Z has no level"));

  // Make the texture valid.
  for (int j = 0; j < 6; ++j)
    data_->cubemap->SetImage(static_cast<CubeMapTexture::CubeFace>(j), 0U,
                             data_->image);

  // Check that a GenerateMipmap is called if requested.
  data_->sampler->SetAutogenerateMipmapsEnabled(true);
  Reset();
  renderer->DrawScene(root);
  // Both the cubemap and texture will generate mipmaps.
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("GenerateMipmap"));
  EXPECT_FALSE(log_checker.HasAnyMessages());
  // Check that when disabled GenerateMipmap is not called.
  data_->sampler->SetAutogenerateMipmapsEnabled(false);
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenerateMipmap"));
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // Check error cases for equal cubemap sizes.
  data_->image->Set(options_->image_format, 32, 33, data_->image_container);
  data_->sampler->SetMinFilter(Sampler::kLinearMipmapNearest);
  data_->sampler->SetWrapS(Sampler::kClampToEdge);
  Reset();
  renderer->DrawScene(root);
  EXPECT_TRUE(
      log_checker.HasMessage("ERROR", "does not have square dimensions"));

  // Check error cases for non-power-of-2 textures.
  data_->image->Set(options_->image_format, 30, 30, data_->image_container);
  data_->sampler->SetMinFilter(Sampler::kLinearMipmapNearest);
  data_->sampler->SetWrapS(Sampler::kClampToEdge);
  Reset();
  renderer->DrawScene(root);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Non-power-of-two textures"));

  data_->image->Set(options_->image_format, 30, 30, data_->image_container);
  Reset();
  renderer->DrawScene(root);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Non-power-of-two textures"));

  data_->image->Set(options_->image_format, 30, 30, data_->image_container);
  data_->sampler->SetWrapS(Sampler::kRepeat);
  data_->sampler->SetWrapT(Sampler::kClampToEdge);
  Reset();
  renderer->DrawScene(root);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Non-power-of-two textures"));

  data_->sampler->SetWrapT(Sampler::kRepeat);
  Reset();
  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // Reset data.
  BuildRectangle(data_, options_);
}

TEST_F(RendererTest, SamplersFollowTextures) {
  // Test that a sampler follows a texture's binding when the texture's bind
  // point changes. This can happen when a set of textures share a sampler and
  // then the bind point changes for all of those textures (e.g., they are bound
  // to a uniform and that uniform's bind point changes).
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  data_->rect->ClearChildren();
  data_->rect->ClearUniforms();
  data_->rect->ClearShapes();

  static const char* kFragmentShaderString = (
      "uniform sampler2D uTexture1;\n"
      "uniform sampler2D uTexture2;\n"
      "varying vec2 vTexCoords;\n"
      "\n"
      "void main(void) {\n"
      "  gl_FragColor = texture2D(uTexture, vTexCoords);\n"
      "}\n");
  static const char* kBaseShaderString = (
      "varying vec2 vTexCoords;\n"
      "\n"
      "void main(void) {\n"
      "  gl_FragColor = texture2D(uTexture, vTexCoords);\n"
      "}\n");

  // Create a shader that uses all of the image units.
  GLuint num_textures = -1;
  gm_->GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS,
                   reinterpret_cast<GLint*>(&num_textures));
  std::ostringstream shader_string;
  for (GLuint i = 0; i < num_textures; ++i) {
    shader_string << "uniform sampler2D uTexture" << i << ";\n";
  }
  shader_string << kBaseShaderString;
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  reg->IncludeGlobalRegistry();
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "aTestAttrib", kBufferObjectElementAttribute, "Testing attribute"));
  ShaderProgramPtr shader = ShaderProgram::BuildFromStrings(
      "BigShader", reg, kPlaneVertexShaderString, shader_string.str(),
      base::AllocatorPtr());

  // Add a node with the full compliment of textures.
  NodePtr node(new Node);
  node->SetShaderProgram(shader);
  node->AddUniform(data_->shader->GetRegistry()->Create<Uniform>(
      "uProjectionMatrix", math::Matrix4f::Identity()));
  node->AddUniform(data_->shader->GetRegistry()->Create<Uniform>(
      "uModelviewMatrix", math::Matrix4f::Identity()));

  SamplerPtr sampler(new Sampler());
  data_->sampler->SetLabel("Big Sampler");
  for (GLuint j = 0; j < num_textures; ++j) {
    // Make a unique name to create a new Uniform for each texture.
    std::ostringstream str;
    str << "uTexture" << j;

    TexturePtr texture(new Texture);
    texture->SetImage(0U, data_->image);
    texture->SetSampler(sampler);
    node->AddUniform(reg->Create<Uniform>(str.str(), texture));
  }
  node->AddShape(data_->shape);

  ShaderInputRegistryPtr reg1(new ShaderInputRegistry);
  reg1->IncludeGlobalRegistry();
  reg1->Add(ShaderInputRegistry::AttributeSpec(
      "aTestAttrib", kBufferObjectElementAttribute, "Testing attribute"));
  ShaderInputRegistryPtr reg2(new ShaderInputRegistry);
  reg2->IncludeGlobalRegistry();
  reg2->Add(ShaderInputRegistry::AttributeSpec(
      "aTestAttrib", kBufferObjectElementAttribute, "Testing attribute"));
  ShaderProgramPtr shader1 = ShaderProgram::BuildFromStrings(
      "Shader1", reg1, kPlaneVertexShaderString, kFragmentShaderString,
      base::AllocatorPtr());
  ShaderProgramPtr shader2 = ShaderProgram::BuildFromStrings(
      "Shader2", reg2, kPlaneVertexShaderString, kFragmentShaderString,
      base::AllocatorPtr());
  data_->rect->AddUniform(data_->shader->GetRegistry()->Create<Uniform>(
      "uProjectionMatrix", math::Matrix4f::Identity()));

  // Add two nodes that have both textures.
  TexturePtr texture1(new Texture);
  texture1->SetImage(0U, data_->image);
  texture1->SetSampler(data_->sampler);
  TexturePtr texture2(new Texture);
  texture2->SetImage(0U, data_->image);
  texture2->SetSampler(data_->sampler);
  NodePtr node1(new Node);
  node1->SetShaderProgram(shader1);
  node1->AddUniform(reg1->Create<Uniform>("uTexture1", texture1));
  node1->AddUniform(reg1->Create<Uniform>("uTexture2", texture2));
  node1->AddShape(data_->shape);
  data_->rect->AddChild(node1);

  NodePtr node2(new Node);
  node2->SetShaderProgram(shader2);
  node2->AddUniform(reg2->Create<Uniform>("uTexture1", texture1));
  node2->AddUniform(reg2->Create<Uniform>("uTexture2", texture2));
  node2->AddShape(data_->shape);
  data_->rect->AddChild(node2);

  // Each texture should be bound once, and the sampler bound to each unit.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(4U, trace_verifier_->GetCountOf("ActiveTexture"));
  // 4 image units will be used since there are 4 distinct uniforms.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE0)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE1)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE2)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE3)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE4)"));
  EXPECT_EQ(4U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  EXPECT_EQ(4U, trace_verifier_->GetCountOf("BindSampler"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindSampler(0x0"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindSampler(0x1"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindSampler(0x2"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindSampler(0x3"));
  // The image units should be sent.
  EXPECT_EQ(4U, trace_verifier_->GetCountOf("Uniform1i"));

  Reset();
  renderer->DrawScene(root);
  // Nothing new should be sent.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ActiveTexture"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindSampler"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Uniform1i"));

  // Uniform -> image unit associations do not change since they are determined
  // at shader program bind time.
  Reset();
  renderer->DrawScene(node);

  // Check that the samplers were bound correctly.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(4U, trace_verifier_->GetCountOf("ActiveTexture"));
  EXPECT_EQ(4U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  EXPECT_EQ(4U, trace_verifier_->GetCountOf("BindSampler"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindSampler(0x0"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindSampler(0x1"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindSampler(0x2"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindSampler(0x3"));
  // Do not expect image units to change.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Uniform1i"));
}

TEST_F(RendererTest, MissingSamplerCausesWarning) {
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  base::LogChecker log_checker;
  Reset();
  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());

  data_->texture->SetSampler(SamplerPtr());
  Reset();
  renderer->DrawScene(root);
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "has no Sampler!"));

  data_->texture->SetSampler(data_->sampler);
  Reset();
  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST_F(RendererTest, ImmutableTextures) {
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);

  gm_->EnableErrorChecking(true);
  // Create immutable textures and test the proper TexStorage calls are made.
  options_->SetImageType(Image::kArray, Image::k2d);
  BuildImage(data_, options_);
  Reset();
  EXPECT_TRUE(VerifyImmutableTexture<Texture>(data_,
      renderer, trace_verifier_, 3, "TexStorage2D(GL_TEXTURE_1D_ARRAY, "));
  Reset();
  // 1D cubemaps are illegal.

  options_->SetImageType(Image::kDense, Image::k2d);
  BuildImage(data_, options_);
  Reset();
  EXPECT_TRUE(VerifyImmutableTexture<Texture>(
      data_, renderer, trace_verifier_, 2, "TexStorage2D(GL_TEXTURE_2D, "));
  Reset();
  EXPECT_TRUE(VerifyImmutableCubemapTexture<CubeMapTexture>(
      data_, renderer, trace_verifier_, 4,
      "TexStorage2D(GL_TEXTURE_CUBE_MAP, "));

  options_->SetImageType(Image::kArray, Image::k3d);
  BuildImage(data_, options_);
  Reset();
  EXPECT_TRUE(
      VerifyImmutableTexture<Texture>(data_, renderer, trace_verifier_, 4,
                                      "TexStorage3D(GL_TEXTURE_2D_ARRAY, "));
  Reset();
  EXPECT_TRUE(VerifyImmutableCubemapTexture<CubeMapTexture>(
      data_, renderer, trace_verifier_, 3,
      "TexStorage3D(GL_TEXTURE_CUBE_MAP_ARRAY, "));

  options_->SetImageType(Image::kDense, Image::k3d);
  BuildImage(data_, options_);
  Reset();
  EXPECT_TRUE(VerifyImmutableTexture<Texture>(
      data_, renderer, trace_verifier_, 3, "TexStorage3D(GL_TEXTURE_3D, "));
  // 3D cubemaps are illegal.

  data_->image.Reset(nullptr);
}

TEST_F(RendererTest, ProtectedTextures) {
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);

  // Create protected textures and test the proper TexStorage calls are made.
  options_->SetImageType(Image::kArray, Image::k2d);
  BuildImage(data_, options_);
  Reset();
  EXPECT_TRUE(
      VerifyProtectedTexture<Texture>(data_, renderer, trace_verifier_, 3,
                                      "TexStorage2D(GL_TEXTURE_1D_ARRAY, "));
  Reset();
  // 1D cubemaps are illegal.

  options_->SetImageType(Image::kDense, Image::k2d);
  BuildImage(data_, options_);
  Reset();
  EXPECT_TRUE(VerifyProtectedTexture<Texture>(
      data_, renderer, trace_verifier_, 2, "TexStorage2D(GL_TEXTURE_2D, "));
  Reset();
  EXPECT_TRUE(VerifyProtectedTexture<CubeMapTexture>(
      data_, renderer, trace_verifier_, 4,
      "TexStorage2D(GL_TEXTURE_CUBE_MAP, "));

  {
    // Test the warning when protected textures are not supported.
    base::LogChecker log_checker;
    gm_->EnableFeature(GraphicsManager::kProtectedTextures, false);
    TexturePtr texture(new Texture);
    texture->SetProtectedImage(data_->image, 1);
    texture->SetSampler(data_->sampler);
    renderer->CreateOrUpdateResource(texture.Get());
    // There should be no call enabling protection on the texture.
    size_t index = trace_verifier_->GetNthIndexOf(0, "TexParameteri");
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(index).HasArg(
        2, "GL_TEXTURE_PROTECTED_EXT"));
    EXPECT_TRUE(log_checker.HasMessage(
        "WARNING", "the system does not support protected textures"));

    gm_->EnableFeature(GraphicsManager::kProtectedTextures, true);
  }

  options_->SetImageType(Image::kArray, Image::k3d);
  BuildImage(data_, options_);
  Reset();
  EXPECT_TRUE(VerifyProtectedTexture<Texture>(data_,
      renderer, trace_verifier_, 4, "TexStorage3D(GL_TEXTURE_2D_ARRAY, "));
  Reset();
  EXPECT_TRUE(VerifyProtectedTexture<CubeMapTexture>(data_,
      renderer, trace_verifier_, 3,
      "TexStorage3D(GL_TEXTURE_CUBE_MAP_ARRAY, "));

  options_->SetImageType(Image::kDense, Image::k3d);
  BuildImage(data_, options_);
  Reset();
  EXPECT_TRUE(VerifyProtectedTexture<Texture>(
      data_, renderer, trace_verifier_, 3, "TexStorage3D(GL_TEXTURE_3D, "));
  // 3D cubemaps are illegal.

  data_->image.Reset(nullptr);
}


TEST_F(RendererTest, ImmutableMultisampleTextures) {
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);

  BuildImage(data_, options_);
  Reset();
  EXPECT_TRUE(VerifyImmutableMultisampledTexture<Texture>(data_,
      renderer, trace_verifier_, 8,
      "TexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, "));

  options_->SetImageType(Image::kArray, Image::k3d);
  BuildImage(data_, options_);
  Reset();
  EXPECT_TRUE(VerifyImmutableMultisampledTexture<Texture>(data_,
      renderer, trace_verifier_, 8,
      "TexStorage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, "));

  data_->image.Reset(nullptr);
}

TEST_F(RendererTest, TextureEvictionCausesRebind) {
  // Test that when a texture is evicted from an image unit by a texture from a
  // different uniform that the original texture will be rebound when drawn
  // again.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  data_->rect->ClearChildren();
  data_->rect->ClearUniforms();
  data_->rect->ClearShapes();

  // Add many nodes with different textures bound to different uniforms; they
  // will eventually wrap image units.
  GLuint num_textures = -1;
  gm_->GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS,
                   reinterpret_cast<GLint*>(&num_textures));

  // Construct a shader with many textures.
  static const char* kBaseShaderString = (
      "varying vec2 vTexCoords;\n"
      "\n"
      "void main(void) {\n"
      "  gl_FragColor = texture2D(uTexture, vTexCoords);\n"
      "}\n");
  std::ostringstream shader_string;
  for (GLuint i = 0; i < num_textures; ++i) {
    shader_string << "uniform sampler2D uTexture" << i << ";\n";
  }
  shader_string << kBaseShaderString;
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  reg->IncludeGlobalRegistry();
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "aTestAttrib", kBufferObjectElementAttribute, "Testing attribute"));
  ShaderProgramPtr shader1 = ShaderProgram::BuildFromStrings(
      "Shader1", reg, kPlaneVertexShaderString, shader_string.str(),
      base::AllocatorPtr());
  ShaderProgramPtr shader2 = ShaderProgram::BuildFromStrings(
      "Shader2", reg, kPlaneVertexShaderString, shader_string.str(),
      base::AllocatorPtr());

  data_->rect->AddUniform(data_->shader->GetRegistry()->Create<Uniform>(
      "uProjectionMatrix", math::Matrix4f::Identity()));

  // Add a node with the full compliment of textures.
  NodePtr node(new Node);
  node->SetShaderProgram(shader1);
  for (GLuint j = 0; j < num_textures; ++j) {
    // Make a unique name to create a new Uniform for each texture.
    std::ostringstream str;
    str << "uTexture" << j;

    TexturePtr texture(new Texture);
    texture->SetImage(0U, data_->image);
    texture->SetSampler(data_->sampler);
    node->AddUniform(reg->Create<Uniform>(str.str(), texture));
  }
  node->AddShape(data_->shape);
  data_->rect->AddChild(node);

  // Each texture should be bound once.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(num_textures, trace_verifier_->GetCountOf("TexImage2D"));
  for (GLuint i = 1; i < num_textures; ++i) {
    std::ostringstream str;
    str << "ActiveTexture(GL_TEXTURE" << i << ")";
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(str.str()));
  }
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  EXPECT_EQ(num_textures, trace_verifier_->GetCountOf("BindSampler"));
  // The texture uniforms are sent the first draw.
  EXPECT_EQ(num_textures, trace_verifier_->GetCountOf("Uniform1i"));

  // Drawing again should do nothing, since everything is already bound.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  // The texture uniforms are sent the first draw.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Uniform1i"));

  // Drawing again with a framebuffer should rebind all textures, since they are
  // all evicted when cleared.
  FramebufferObjectPtr fbo(new FramebufferObject(128, 128));
  fbo->SetColorAttachment(0U, FramebufferObject::Attachment(Image::kRgba4Byte));
  Reset();
  EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == nullptr);
  renderer->BindFramebuffer(fbo);
  EXPECT_EQ(fbo.Get(), renderer->GetCurrentFramebuffer().Get());
  renderer->DrawScene(root);
  renderer->BindFramebuffer(FramebufferObjectPtr());
  EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == nullptr);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE"));
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  // Since all the sampler bindings were reset across the fbo bind, they will
  // be bound again.
  EXPECT_EQ(num_textures, trace_verifier_->GetCountOf("BindSampler"));
  // The texture uniforms are sent the first draw.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Uniform1i"));

  // Drawing again should rebind, since they are evicted when the framebuffer
  // was changed.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE"));
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  EXPECT_EQ(num_textures, trace_verifier_->GetCountOf("BindSampler"));
  // The texture uniforms are sent the first draw.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Uniform1i"));

  // Add another node with the full compliment of textures.
  node.Reset(new Node);
  node->SetShaderProgram(shader2);
  for (GLuint j = 0; j < num_textures; ++j) {
    // Make a unique name to create a new Uniform for each texture.
    std::ostringstream str;
    str << "uTexture" << j;

    TexturePtr texture(new Texture);
    texture->SetImage(0U, data_->image);
    texture->SetSampler(data_->sampler);
    node->AddUniform(reg->Create<Uniform>(str.str(), texture));
  }
  node->AddShape(data_->shape);
  data_->rect->AddChild(node);

  // Get the new resources created.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(num_textures, trace_verifier_->GetCountOf("TexImage2D"));
  for (GLuint i = 0; i < num_textures; ++i) {
    std::ostringstream str;
    str << "ActiveTexture(GL_TEXTURE" << i << ")";
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(str.str()));
  }
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  // The new texture uniforms should share the same units, but they have to be
  // sent once for the new shader. The samplers, however, are already in the
  // right place.
  EXPECT_EQ(num_textures, trace_verifier_->GetCountOf("Uniform1i"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindSampler"));

  // Drawing again should rebind everything, since all textures were evicted by
  // the second node. The units should be consistent, however.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(num_textures * 2U,
            trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE"));
  EXPECT_EQ(num_textures * 2U,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Uniform1i"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindSampler"));

  // Reset data.
  data_->rect = nullptr;
  BuildRectangle(data_, options_);
}

TEST_F(RendererTest, ArrayTextureEvictionCausesRebind) {
  // Similar to the above test but using array textures.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  data_->rect->ClearChildren();
  data_->rect->ClearUniforms();
  data_->rect->ClearShapes();

  // Add many nodes with different textures bound to different uniforms; they
  // will eventually wrap image units.
  GLuint num_textures = -1;
  gm_->GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS,
                   reinterpret_cast<GLint*>(&num_textures));

  // Construct a shader with many textures.
  static const char* kBaseShaderString = (
      "varying vec2 vTexCoords;\n"
      "\n"
      "void main(void) {\n"
      "  gl_FragColor = texture2D(uTextures[0], vTexCoords);\n"
      "}\n");
  std::ostringstream shader_string;
  shader_string << "uniform sampler2D uTextures[" << num_textures << "];\n";
  shader_string << kBaseShaderString;
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  reg->IncludeGlobalRegistry();
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "aTestAttrib", kBufferObjectElementAttribute, "Testing attribute"));
  ShaderProgramPtr shader1 = ShaderProgram::BuildFromStrings(
      "Shader1", reg, kPlaneVertexShaderString, shader_string.str(),
      base::AllocatorPtr());
  ShaderProgramPtr shader2 = ShaderProgram::BuildFromStrings(
      "Shader2", reg, kPlaneVertexShaderString, shader_string.str(),
      base::AllocatorPtr());

  data_->rect->AddUniform(data_->shader->GetRegistry()->Create<Uniform>(
      "uProjectionMatrix", math::Matrix4f::Identity()));

  // Add a node with the full complement of textures.
  NodePtr node(new Node);
  std::vector<TexturePtr> textures(num_textures);
  node->SetShaderProgram(shader1);
  for (GLuint i = 0; i < num_textures; ++i) {
    textures[i].Reset(new Texture);
    textures[i]->SetImage(0U, data_->image);
    textures[i]->SetSampler(data_->sampler);
  }
  node->AddUniform(CreateArrayUniform(reg, "uTextures", textures));
  node->AddShape(data_->shape);
  data_->rect->AddChild(node);

  // Each texture should be bound once.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(num_textures, trace_verifier_->GetCountOf("TexImage2D"));
  for (GLuint i = 0; i < num_textures; ++i) {
    SCOPED_TRACE(i);
    std::ostringstream str;
    str << "ActiveTexture(GL_TEXTURE" << i << ")";
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(str.str()));
  }
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  // The texture uniforms are sent the first draw.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Uniform1iv"));

  // Drawing again should do nothing, since everything is already bound.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  // The texture uniforms are sent the first draw.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Uniform1i"));

  // Drawing again with a framebuffer should rebind all textures, since they are
  // all evicted when cleared.
  FramebufferObjectPtr fbo(new FramebufferObject(128, 128));
  fbo->SetColorAttachment(0U, FramebufferObject::Attachment(Image::kRgba4Byte));
  Reset();
  EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == nullptr);
  renderer->BindFramebuffer(fbo);
  EXPECT_EQ(fbo.Get(), renderer->GetCurrentFramebuffer().Get());
  renderer->DrawScene(root);
  renderer->BindFramebuffer(FramebufferObjectPtr());
  EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == nullptr);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE"));
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  // The texture uniforms are sent the first draw.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Uniform1iv"));

  // Drawing again should rebind, since they are evicted when the framebuffer
  // was changed.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE"));
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  // The texture uniforms are sent the first draw.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Uniform1i"));

  // Add another node with the full compliment of textures.
  textures.clear();
  textures.resize(num_textures);
  node.Reset(new Node);
  node->SetShaderProgram(shader2);
  for (GLuint i = 0; i < num_textures; ++i) {
    textures[i].Reset(new Texture);
    textures[i]->SetImage(0U, data_->image);
    textures[i]->SetSampler(data_->sampler);
  }
  node->AddUniform(CreateArrayUniform(reg, "uTextures", textures));
  node->AddShape(data_->shape);
  data_->rect->AddChild(node);

  // Get the new resources created.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(num_textures, trace_verifier_->GetCountOf("TexImage2D"));
  for (GLuint i = 0; i < num_textures; ++i) {
    std::ostringstream str;
    str << "ActiveTexture(GL_TEXTURE" << i << ")";
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(str.str()));
  }
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  // The texture uniforms for the new shader get their bindings.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Uniform1iv"));

  // Drawing again should rebind everything, since all textures were evicted by
  // the second node.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(num_textures * 2U,
            trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE"));
  EXPECT_EQ(num_textures * 2U,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Uniform1iv"));

  // Reset data.
  data_->rect = nullptr;
  BuildRectangle(data_, options_);
}

TEST_F(RendererTest, ArrayCubemapEvictionCausesRebind) {
  // The same as the above test but using array cubemaps.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  data_->rect->ClearChildren();
  data_->rect->ClearUniforms();
  data_->rect->ClearShapes();

  // Add many nodes with different textures bound to different uniforms; they
  // will eventually wrap image units.
  GLuint num_textures = -1;
  gm_->GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS,
                   reinterpret_cast<GLint*>(&num_textures));

  // Construct a shader with many textures.
  static const char* kBaseShaderString = (
      "varying vec2 vTexCoords;\n"
      "\n"
      "void main(void) {\n"
      "  gl_FragColor = texture2D(uTextures[0], vTexCoords);\n"
      "}\n");
  std::ostringstream shader_string;
  shader_string << "uniform samplerCube uTextures[" << num_textures << "];\n";
  shader_string << kBaseShaderString;
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  reg->IncludeGlobalRegistry();
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "aTestAttrib", kBufferObjectElementAttribute, "Testing attribute"));
  ShaderProgramPtr shader1 = ShaderProgram::BuildFromStrings(
      "Shader1", reg, kPlaneVertexShaderString, shader_string.str(),
      base::AllocatorPtr());
  ShaderProgramPtr shader2 = ShaderProgram::BuildFromStrings(
      "Shader2", reg, kPlaneVertexShaderString, shader_string.str(),
      base::AllocatorPtr());

  data_->rect->AddUniform(data_->shader->GetRegistry()->Create<Uniform>(
      "uProjectionMatrix", math::Matrix4f::Identity()));

  // Add a node with the full compliment of textures.
  NodePtr node(new Node);
  std::vector<CubeMapTexturePtr> textures(num_textures);
  node->SetShaderProgram(shader1);
  for (GLuint i = 0; i < num_textures; ++i) {
    textures[i].Reset(new CubeMapTexture);
    for (int j = 0; j < 6; ++j)
      textures[i]->SetImage(static_cast<CubeMapTexture::CubeFace>(j), 0U,
                            data_->image);
    textures[i]->SetSampler(data_->sampler);
  }
  node->AddUniform(CreateArrayUniform(reg, "uTextures", textures));
  node->AddShape(data_->shape);
  data_->rect->AddChild(node);

  // Each texture should be bound once.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(num_textures * 6U, trace_verifier_->GetCountOf("TexImage2D"));
  for (GLuint i = 0; i < num_textures; ++i) {
    SCOPED_TRACE(i);
    std::ostringstream str;
    str << "ActiveTexture(GL_TEXTURE" << i << ")";
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(str.str()));
  }
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_CUBE_MAP"));
  // The texture uniforms are sent the first draw.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Uniform1iv"));

  // Drawing again should do nothing, since everything is already bound.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_CUBE_MAP"));
  // The texture uniforms are sent the first draw.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Uniform1i"));

  // Drawing again with a framebuffer should rebind all textures, since they are
  // all evicted when cleared.
  FramebufferObjectPtr fbo(new FramebufferObject(128, 128));
  fbo->SetColorAttachment(0U, FramebufferObject::Attachment(Image::kRgba4Byte));
  Reset();
  EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == nullptr);
  renderer->BindFramebuffer(fbo);
  EXPECT_EQ(fbo.Get(), renderer->GetCurrentFramebuffer().Get());
  renderer->DrawScene(root);
  renderer->BindFramebuffer(FramebufferObjectPtr());
  EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == nullptr);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE"));
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_CUBE_MAP"));
  // The texture uniforms are sent the first draw.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Uniform1iv"));

  // Drawing again should rebind, since they are evicted when the framebuffer
  // was changed.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE"));
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_CUBE_MAP"));
  // The texture uniforms are sent the first draw.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Uniform1i"));

  // Add another node with the full compliment of textures.
  textures.clear();
  textures.resize(num_textures);
  node.Reset(new Node);
  node->SetShaderProgram(shader2);
  for (GLuint i = 0; i < num_textures; ++i) {
    textures[i].Reset(new CubeMapTexture);
    for (int j = 0; j < 6; ++j)
      textures[i]->SetImage(static_cast<CubeMapTexture::CubeFace>(j), 0U,
                            data_->image);
    textures[i]->SetSampler(data_->sampler);
  }
  node->AddUniform(CreateArrayUniform(reg, "uTextures", textures));
  node->AddShape(data_->shape);
  data_->rect->AddChild(node);

  // Get the new resources created.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(num_textures * 6U, trace_verifier_->GetCountOf("TexImage2D"));
  for (GLuint i = 0; i < num_textures; ++i) {
    std::ostringstream str;
    str << "ActiveTexture(GL_TEXTURE" << i << ")";
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(str.str()));
  }
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_CUBE_MAP"));
  // The texture uniforms for the new shader get their bindings.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Uniform1iv"));

  // Drawing again should rebind everything, since all textures were evicted by
  // the second node.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(num_textures * 2U,
            trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE"));
  EXPECT_EQ(num_textures * 2U,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_CUBE_MAP"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Uniform1iv"));

  // Reset data.
  data_->rect = nullptr;
  BuildRectangle(data_, options_);
}

TEST_F(RendererTest, TextureMipmaps) {
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  base::LogChecker log_checker;
  TracingHelper helper;

  // Check that a texture with an image is sent as mipmap level 0.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(28672U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(24576U, data_->cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, data_->texture->GetGpuMemoryUsed());
  EXPECT_EQ(7U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "TexImage2D"))
          .HasArg(2, "0"));

  // Several images of different sizes.
  static const uint32 kMaxImageSize = 32;
  static const uint32 kNumMipmaps = 6U;  // log2(kMaxImageSize) + 1U;
  ImagePtr mipmaps[kNumMipmaps];
  for (uint32 i = 0; i < kNumMipmaps; ++i)
    mipmaps[i] = CreateNullImage(kMaxImageSize >> i, kMaxImageSize >> i,
                                 Image::kRgba8888);

  // Set a full image pyramid.
  for (uint32 i = 0; i < kNumMipmaps; ++i)
    data_->texture->SetImage(i, mipmaps[i]);

  // Check consistent dimensions.
  EXPECT_FALSE(log_checker.HasAnyMessages());
  Reset();
  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(kNumMipmaps, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenerateMipmap"));
  // Check that the texture memory increased properly.
  EXPECT_EQ(30037U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(24576U, data_->cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(5461U, data_->texture->GetGpuMemoryUsed());

  // Check the right calls were made.
  for (uint32 i = 0; i < kNumMipmaps; ++i) {
    SCOPED_TRACE(::testing::Message() << "Testing mipmap level " << i);
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(i, "TexImage2D"))
            .HasArg(2, helper.ToString("GLint", static_cast<GLint>(i)))
            .HasArg(4, helper.ToString(
                "GLsizei", static_cast<GLint>(mipmaps[i]->GetWidth())))
            .HasArg(5, helper.ToString(
                "GLsizei", static_cast<GLint>(mipmaps[i]->GetHeight()))));
  }

  // Remove a texture and verify that GenerateMipmap is called.
  data_->texture->SetImage(1U, ImagePtr(nullptr));
  Reset();
  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenerateMipmap"));
  // Since GenerateMipmap was called the override mipmaps were sent, but the
  // 0th level mipmap doesn't have to be (GenerateMipmap won't override it).
  EXPECT_EQ(kNumMipmaps - 2U, trace_verifier_->GetCountOf("TexImage2D"));
  // Memory usage should not change.
  EXPECT_EQ(30037U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(24576U, data_->cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(5461U, data_->texture->GetGpuMemoryUsed());

  // Set an invalid image dimension.
  data_->texture->SetImage(
      1U, CreateNullImage(mipmaps[1]->GetWidth() - 1, mipmaps[1]->GetHeight(),
                          Image::kRgba8888));

  Reset();
  renderer->DrawScene(root);
  // Nothing will be called since the texture has the right number of levels,
  // just incorrect dimensions.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenerateMipmap"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  msg_stream_ << "Mipmap width: " << (mipmaps[1]->GetWidth() - 1) <<
      " is not a power of 2.";
  EXPECT_TRUE(log_checker.HasMessage("ERROR", msg_stream_.str()));
  // Memory usage should not change.
  EXPECT_EQ(30037U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(24576U, data_->cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(5461U, data_->texture->GetGpuMemoryUsed());

  data_->texture->SetImage(
      1U, CreateNullImage(mipmaps[1]->GetWidth(), mipmaps[1]->GetHeight() - 1,
                          Image::kRgba8888));

  Reset();
  renderer->DrawScene(root);
  // Nothing will be called since the texture has the right number of levels,
  // just incorrect dimensions.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenerateMipmap"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  msg_stream_ << "Mipmap height: " << (mipmaps[1]->GetWidth() - 1) <<
      " is not a power of 2.";
  EXPECT_TRUE(log_checker.HasMessage("ERROR", msg_stream_.str()));
  // Memory usage should not change.
  EXPECT_EQ(30037U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(24576U, data_->cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(5461U, data_->texture->GetGpuMemoryUsed());

  // Set an invalid image dimension.
  data_->texture
      ->SetImage(1U, CreateNullImage(mipmaps[1]->GetWidth(),
                                     mipmaps[1]->GetHeight(), Image::kRgb888));
  Reset();
  renderer->DrawScene(root);
  // Nothing will be called since the texture has the right number of levels,
  // just incorrect dimensions.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenerateMipmap"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_TRUE(
      log_checker.HasMessage("ERROR", "level 1 has different format"));
  // Memory usage should not change.
  EXPECT_EQ(30037U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(24576U, data_->cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(5461U, data_->texture->GetGpuMemoryUsed());
}

TEST_F(RendererTest, TextureMultisamplingDisablesMipmapping) {
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  base::LogChecker log_checker;
  TracingHelper helper;

  Reset();

  EXPECT_TRUE(gm_->IsFeatureAvailable(GraphicsManager::kTextureMultisample));

  // Set multisampling.
  data_->texture->SetMultisampling(4, true);
  // Clear cubemap as we don't need it and it affects the number of times
  // TexImage2D is invoked.
  for (int i = 0; i < 6; ++i) {
    data_->cubemap->SetImage(static_cast<CubeMapTexture::CubeFace>(i), 0U,
                             ImagePtr());
  }

  renderer->DrawScene(root);
  EXPECT_EQ(4096U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(0U, data_->cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, data_->texture->GetGpuMemoryUsed());

  // Verify call to TexImage2DMultisample.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexImage2DMultisample"));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "TexImage2DMultisample"))
          .HasArg(1, "GL_TEXTURE_2D_MULTISAMPLE"));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "TexImage2DMultisample"))
          .HasArg(2, "4"));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "TexImage2DMultisample"))
          .HasArg(6, "GL_TRUE"));

  // Verify calls to TexImage2D and GenerateMipmap. "TexImage2D" is a prefix
  // of "TexImage2DMultisample" so it should appear once.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenerateMipmap"));

  // Several images of different sizes.
  static const uint32 kMaxImageSize = 32;
  static const uint32 kNumMipmaps = 6U;  // log2(kMaxImageSize) + 1U;
  ImagePtr mipmaps[kNumMipmaps];
  for (uint32 i = 0; i < kNumMipmaps; ++i)
    mipmaps[i] = CreateNullImage(kMaxImageSize >> i, kMaxImageSize >> i,
                                 Image::kRgba8888);

  // Set a full image pyramid.
  for (uint32 i = 0; i < kNumMipmaps; ++i)
    data_->texture->SetImage(i, mipmaps[i]);

  // Mipmaps will be counted against memory but should still not be used.
  // "TexImage2D" is a prefix of "TexImage2DMultisample" so it should appear
  // once.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexImage2DMultisample"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenerateMipmap"));
  // Check that the texture memory is as expected.
  EXPECT_EQ(5461U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(0U, data_->cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(5461U, data_->texture->GetGpuMemoryUsed());

  // Unset multisampling.
  data_->texture->SetMultisampling(0, false);

  // Mipmaps should now be used.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2DMultisample"));
  EXPECT_EQ(6U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenerateMipmap"));
  // Check that the texture memory stayed the same.
  EXPECT_EQ(5461U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(0U, data_->cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(5461U, data_->texture->GetGpuMemoryUsed());

  // Clear warning from clearing the cubemap textures above.
  EXPECT_TRUE(log_checker.HasMessage("WARNING",
      "***ION: Cubemap texture face Negative X has no level 0 mipmap."));
}

TEST_F(RendererTest, TextureMultisamplingDisablesSubImages) {
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  base::LogChecker log_checker;
  TracingHelper helper;

  Reset();

  EXPECT_TRUE(gm_->IsFeatureAvailable(GraphicsManager::kTextureMultisample));

  // Set multisampling.
  data_->texture->SetMultisampling(4, true);
  // Clear cubemap as we don't need it and it affects the number of times
  // TexImage2D is invoked.
  for (int i = 0; i < 6; ++i) {
    data_->cubemap->SetImage(static_cast<CubeMapTexture::CubeFace>(i), 0U,
                             ImagePtr());
  }

  renderer->DrawScene(root);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexImage2DMultisample"));

  // Now set a subimage on the texture.
  data_->texture->SetSubImage(0U, math::Point2ui(4, 8),
                              CreateNullImage(10, 12, Image::kRgba8888));

  // Check that the subimage is NOT applied.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2DMultisample"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexSubImage2D"));

  // Disable multisampling.
  data_->texture->SetMultisampling(0, false);

  // Check that the subimage is applied.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2DMultisample"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexSubImage2D"));

  // Clear warning from clearing the cubemap textures above.
  EXPECT_TRUE(log_checker.HasMessage("WARNING",
      "***ION: Cubemap texture face Negative X has no level 0 mipmap."));
}

TEST_F(RendererTest, TextureSubImages) {
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  base::LogChecker log_checker;
  TracingHelper helper;

  // Check that a texture with an image is sent as mipmap level 0.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(7U, trace_verifier_->GetCountOf("TexImage2D"));

  // Now set a subimage on the texture.
  data_->texture->SetSubImage(0U, math::Point2ui(4, 8),
                              CreateNullImage(10, 12, Image::kRgba8888));
  // Check that the subimage is applied.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexSubImage2D"));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "TexSubImage2D"))
          .HasArg(2, "0")
          .HasArg(3, "4")
          .HasArg(4, "8")
          .HasArg(5, "10")
          .HasArg(6, "12")
          .HasArg(7, "GL_RGB")
          .HasArg(8, "GL_UNSIGNED_BYTE"));

  // Several images of different sizes.
  static const uint32 kMaxImageSize = 32;
  static const uint32 kNumMipmaps = 6U;  // log2(kMaxImageSize) + 1U;
  ImagePtr mipmaps[kNumMipmaps];
  for (uint32 i = 0; i < kNumMipmaps; ++i)
    mipmaps[i] = CreateNullImage(kMaxImageSize >> i, kMaxImageSize >> i,
                                 Image::kRgba8888);

  // Set a full image pyramid.
  for (uint32 i = 0; i < kNumMipmaps; ++i)
    data_->texture->SetImage(i, mipmaps[i]);

  // Check consistent dimensions.
  EXPECT_FALSE(log_checker.HasAnyMessages());
  Reset();
  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(kNumMipmaps, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenerateMipmap"));
  EXPECT_EQ(30037U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(24576U, data_->cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(5461U, data_->texture->GetGpuMemoryUsed());

  // Set a submipmap at level 3. Setting a compressed image requires non-NULL
  // image data.
  ImagePtr compressed_image(new Image);
  compressed_image->Set(
      Image::kDxt5, 4, 8,
      base::DataContainerPtr(base::DataContainer::Create(
          reinterpret_cast<void*>(1), kNullFunction, false,
          compressed_image->GetAllocator())));
  data_->texture->SetSubImage(3U, math::Point2ui(2, 6), compressed_image);

  // Check the right call was made.
  Reset();
  renderer->DrawScene(root);
  // Technically there is an errors since the cubemap is not compressed, but
  // this is just to test that the call is made.
  gm_->SetErrorCode(GL_NO_ERROR);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexSubImage2D"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("CompressedTexSubImage2D"));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "CompressedTexSubImage2D"))
          .HasArg(2, "3")
          .HasArg(3, "2")
          .HasArg(4, "6")
          .HasArg(5, "4")
          .HasArg(6, "8")
          .HasArg(7, "GL_COMPRESSED_RGBA_S3TC_DXT5_EXT"));
  // Memory usage is not affected by sub images.
  EXPECT_EQ(30037U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(24576U, data_->cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(5461U, data_->texture->GetGpuMemoryUsed());
}

TEST_F(RendererTest, TextureMisc) {
  // Test various texture corner cases.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(data_, options_, kWidth, kHeight);
  base::LogChecker log_checker;

  // Check that a texture with no image does not get sent.
  data_->texture->SetImage(0U, ImagePtr(nullptr));
  data_->texture->SetLabel("texture");
  Reset();
  renderer->DrawScene(root);
  // The cubemap is still sent.
  EXPECT_EQ(6U, trace_verifier_->GetCountOf("TexImage2D"));
  data_->texture->SetImage(0U, data_->image);
  EXPECT_TRUE(
      log_checker.HasMessage("WARNING", "Texture \"texture\" has no level 0"));

  // Check that a GenerateMipmap is called if requested.
  data_->sampler->SetAutogenerateMipmapsEnabled(true);
  Reset();
  renderer->DrawScene(root);
  // Both the cubemap and texture will generate mipmaps.
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("GenerateMipmap"));
  // Check that when disabled GenerateMipmap is not called.
  data_->sampler->SetAutogenerateMipmapsEnabled(false);
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenerateMipmap"));

  // Check error cases for non-power-of-2 textures.
  data_->image->Set(options_->image_format, 32, 33, data_->image_container);
  data_->sampler->SetMinFilter(Sampler::kLinearMipmapNearest);
  data_->sampler->SetWrapS(Sampler::kClampToEdge);
  Reset();
  renderer->DrawScene(root);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Non-power-of-two textures"));

  data_->image->Set(options_->image_format, 33, 32, data_->image_container);
  Reset();
  renderer->DrawScene(root);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Non-power-of-two textures"));

  data_->image->Set(options_->image_format, 32, 33, data_->image_container);
  data_->sampler->SetWrapS(Sampler::kRepeat);
  data_->sampler->SetWrapT(Sampler::kClampToEdge);
  Reset();
  renderer->DrawScene(root);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Non-power-of-two textures"));

  data_->sampler->SetWrapT(Sampler::kRepeat);
  Reset();
  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // Reset data.
  BuildRectangle(data_, options_);
}

TEST_F(RendererTest, TextureCompareFunction) {
  VerifyRenderData<Sampler::CompareFunction> verify_data;
  verify_data.option = &options_->compare_func;
  verify_data.static_args = {{2, "GL_TEXTURE_COMPARE_FUNC"}};
  verify_data.arg_tests = {
      {0, Sampler::kAlways, "GL_ALWAYS"},
      {0, Sampler::kEqual, "GL_EQUAL"},
      {0, Sampler::kGreater, "GL_GREATER"},
      {0, Sampler::kGreaterOrEqual, "GL_GEQUAL"},
      {0, Sampler::kLess, "GL_LESS"},
      {0, Sampler::kLessOrEqual, "GL_LEQUAL"},
      {0, Sampler::kNever, "GL_NEVER"},
      {0, Sampler::kNotEqual, "GL_NOTEQUAL"}
  };
  EXPECT_TRUE(VerifySamplerAndTextureCalls(this, &verify_data));
}

TEST_F(RendererTest, TextureCompareMode) {
  VerifyRenderData<Sampler::CompareMode> verify_data;
  verify_data.option = &options_->compare_mode;
  verify_data.static_args = {{2, "GL_TEXTURE_COMPARE_MODE"}};
  verify_data.arg_tests = {
      {0, Sampler::kCompareToTexture, "GL_COMPARE_REF_TO_TEXTURE"},
      {0, Sampler::kNone, "GL_NONE"}
  };
  EXPECT_TRUE(VerifySamplerAndTextureCalls(this, &verify_data));
}

TEST_F(RendererTest, TextureMaxAnisotropy) {
  VerifyRenderData<float> verify_data;
  verify_data.option = &options_->max_anisotropy;
  verify_data.static_args.push_back(
      StaticArg(2, "GL_TEXTURE_MAX_ANISOTROPY_EXT"));
  verify_data.arg_tests.push_back(VaryingArg<float>(0, 10.f, "10"));
  verify_data.arg_tests.push_back(VaryingArg<float>(0, 4.f, "4"));
  verify_data.arg_tests.push_back(VaryingArg<float>(0, 1.f, "1"));
  EXPECT_TRUE(VerifySamplerAndTextureCalls(this, &verify_data));

  // Check that max anisotropy is bounded.
  Reset();
  options_->max_anisotropy = 32.f;
  NodePtr root = BuildGraph(data_, options_, 800, 800);
  RendererPtr renderer(new Renderer(gm_));
  renderer->DrawScene(root);
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "SamplerParameterf"))
          .HasArg(2, "GL_TEXTURE_MAX_ANISOTROPY_EXT")
          .HasArg(3, "16"));

  // Disable anisotropy and make no anisotropy call is made.
  gm_->SetExtensionsString("");
  Reset();
  root = BuildGraph(data_, options_, 800, 800);
  renderer->DrawScene(root);
  EXPECT_EQ(std::string::npos, trace_verifier_->GetTraceString().find(
                                   "GL_TEXTURE_MAX_ANISOTROPY_EXT"));
  options_->max_anisotropy = 1.f;
}

TEST_F(RendererTest, TextureMagFilter) {
  VerifyRenderData<Sampler::FilterMode> verify_data;
  verify_data.option = &options_->mag_filter;
  verify_data.static_args = {{2, "GL_TEXTURE_MAG_FILTER"}};
  verify_data.arg_tests = {
      {0, Sampler::kLinear, "GL_LINEAR"},
      {0, Sampler::kNearest, "GL_NEAREST"}
  };
  EXPECT_TRUE(VerifySamplerAndTextureCalls(this, &verify_data));
}

TEST_F(RendererTest, TextureMaxLod) {
  VerifyRenderData<float> verify_data;
  verify_data.option = &options_->max_lod;
  verify_data.static_args = {{2, "GL_TEXTURE_MAX_LOD"}};
  verify_data.arg_tests = {
      {0, 100.f, "100"},
      {0, -2.1f, "-2.1"},
      {0, 23.45f, "23.45"}
  };
  EXPECT_TRUE(VerifySamplerAndTextureCalls(this, &verify_data));
}

TEST_F(RendererTest, TextureMinFilter) {
  VerifyRenderData<Sampler::FilterMode> verify_data;
  verify_data.option = &options_->min_filter;
  verify_data.static_args = {{2, "GL_TEXTURE_MIN_FILTER"}};
  verify_data.arg_tests = {
      {0, Sampler::kLinear, "GL_LINEAR"},
      {0, Sampler::kNearest, "GL_NEAREST"},
      {0, Sampler::kNearestMipmapNearest, "GL_NEAREST_MIPMAP_NEAREST"},
      {0, Sampler::kNearestMipmapLinear, "GL_NEAREST_MIPMAP_LINEAR"},
      {0, Sampler::kLinearMipmapNearest, "GL_LINEAR_MIPMAP_NEAREST"},
      {0, Sampler::kLinearMipmapLinear, "GL_LINEAR_MIPMAP_LINEAR"}
  };
  EXPECT_TRUE(VerifySamplerAndTextureCalls(this, &verify_data));
}

TEST_F(RendererTest, TextureMinLod) {
  VerifyRenderData<float> verify_data;
  verify_data.option = &options_->min_lod;
  verify_data.static_args = {{2, "GL_TEXTURE_MIN_LOD"}};
  verify_data.arg_tests = {
      {0, 10.f, "10"},
      {0, -3.1f, "-3.1"},
      {0, 12.34f, "12.34"}
  };
  EXPECT_TRUE(VerifySamplerAndTextureCalls(this, &verify_data));
}

TEST_F(RendererTest, TextureWrapR) {
  options_->SetImageType(Image::kDense, Image::k3d);
  VerifyRenderData<Sampler::WrapMode> verify_data;
  verify_data.option = &options_->wrap_r;
  verify_data.static_args = {{2, "GL_TEXTURE_WRAP_R"}};
  verify_data.arg_tests = {
      {0, Sampler::kClampToEdge, "GL_CLAMP_TO_EDGE"},
      {0, Sampler::kRepeat, "GL_REPEAT"},
      {0, Sampler::kMirroredRepeat, "GL_MIRRORED_REPEAT"}
  };
  EXPECT_TRUE(VerifySamplerAndTextureCalls(this, &verify_data));
}

TEST_F(RendererTest, TextureWrapS) {
  VerifyRenderData<Sampler::WrapMode> verify_data;
  verify_data.option = &options_->wrap_s;
  verify_data.static_args = {{2, "GL_TEXTURE_WRAP_S"}};
  verify_data.arg_tests = {
      {0, Sampler::kClampToEdge, "GL_CLAMP_TO_EDGE"},
      {0, Sampler::kRepeat, "GL_REPEAT"},
      {0, Sampler::kMirroredRepeat, "GL_MIRRORED_REPEAT"}
  };
  EXPECT_TRUE(VerifySamplerAndTextureCalls(this, &verify_data));
}

TEST_F(RendererTest, TextureWrapT) {
  VerifyRenderData<Sampler::WrapMode> verify_data;
  verify_data.option = &options_->wrap_t;
  verify_data.static_args = {{2, "GL_TEXTURE_WRAP_T"}};
  verify_data.arg_tests = {
      {0, Sampler::kClampToEdge, "GL_CLAMP_TO_EDGE"},
      {0, Sampler::kRepeat, "GL_REPEAT"},
      {0, Sampler::kMirroredRepeat, "GL_MIRRORED_REPEAT"}
  };
  EXPECT_TRUE(VerifySamplerAndTextureCalls(this, &verify_data));
}

TEST_F(RendererTest, TextureBaseLevel) {
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root;
  base::LogChecker log_checker;

  root = BuildGraph(data_, options_, kWidth, kHeight);
  VerifyRenderData<int> verify_data;
  verify_data.update_func =
      std::bind(static_cast<void (*)(Data*, Options*)>(BuildRectangle), data_,
                options_);
  verify_data.call_name = "TexParameteri";
  verify_data.option = &options_->base_level;
  verify_data.varying_arg_index = 3U;
  verify_data.static_args =
      {{1, "GL_TEXTURE_2D"}, {2, "GL_TEXTURE_BASE_LEVEL"}};
  verify_data.arg_tests = {{0, 10, "10"}, {0, 3, "3"}, {0, 123, "123"}};
  EXPECT_TRUE(VerifyRenderCalls(verify_data, trace_verifier_, renderer, root,
                                GraphicsManager::kTextureMipmapRange));
  // Emitted when the feature is disabled at the end of the test.
  EXPECT_TRUE(log_checker.HasMessage("WARNING",
      "OpenGL implementation does not support setting texture mipmap ranges"));
  Reset();
  renderer = nullptr;
  EXPECT_TRUE(VerifyReleases(1));
}

TEST_F(RendererTest, TextureMaxLevel) {
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root;
  base::LogChecker log_checker;

  root = BuildGraph(data_, options_, kWidth, kHeight);
  VerifyRenderData<int> verify_data;
  verify_data.update_func =
    std::bind(static_cast<void (*)(Data*, Options*)>(BuildRectangle), data_,
              options_);
  verify_data.call_name = "TexParameteri";
  verify_data.option = &options_->max_level;
  verify_data.varying_arg_index = 3U;
  verify_data.static_args = {{1, "GL_TEXTURE_2D"}, {2, "GL_TEXTURE_MAX_LEVEL"}};
  verify_data.arg_tests = {{0, 100, "100"}, {0, 33, "33"}, {0, 1234, "1234"}};
  EXPECT_TRUE(VerifyRenderCalls(verify_data, trace_verifier_, renderer, root,
                                GraphicsManager::kTextureMipmapRange));
  // Emitted when the feature is disabled at the end of the test.
  EXPECT_TRUE(log_checker.HasMessage("WARNING",
      "OpenGL implementation does not support setting texture mipmap ranges"));
  Reset();
  renderer = nullptr;
  EXPECT_TRUE(VerifyReleases(1));
}

TEST_F(RendererTest, TextureSwizzleRed) {
  RendererPtr renderer(new Renderer(gm_));
  TracingHelper helper;
  NodePtr root;
  base::LogChecker log_checker;

  root = BuildGraph(data_, options_, kWidth, kHeight);
  VerifyRenderData<Texture::Swizzle> verify_data;
  verify_data.update_func =
    std::bind(static_cast<void (*)(Data*, Options*)>(BuildRectangle), data_,
              options_);
  verify_data.call_name = "TexParameteri";
  verify_data.option = &options_->swizzle_r;
  verify_data.varying_arg_index = 3U;
  // helper.ToString("GLtextureenum", GL_TEXTURE_SWIZZLE_R)
  verify_data.static_args = {{1, "GL_TEXTURE_2D"}, {2, "GL_TEXTURE_SWIZZLE_R"}};
  verify_data.arg_tests = {
      {0, Texture::kGreen, "GL_GREEN"},
      {0, Texture::kBlue, "GL_BLUE"},
      {0, Texture::kAlpha, "GL_ALPHA"},
      {0, Texture::kRed, "GL_RED"}
  };
  EXPECT_TRUE(VerifyRenderCalls(verify_data, trace_verifier_, renderer, root,
                                GraphicsManager::kTextureSwizzle));
  // Emitted when the feature is disabled at the end of the test.
  EXPECT_TRUE(log_checker.HasMessage("ERROR",
      "OpenGL implementation does not support texture swizzles"));
  Reset();
  renderer = nullptr;
  EXPECT_TRUE(VerifyReleases(1));
}

TEST_F(RendererTest, TextureSwizzleGreen) {
  RendererPtr renderer(new Renderer(gm_));
  TracingHelper helper;
  NodePtr root;
  base::LogChecker log_checker;

  root = BuildGraph(data_, options_, kWidth, kHeight);
  VerifyRenderData<Texture::Swizzle> verify_data;
  verify_data.update_func =
    std::bind(static_cast<void (*)(Data*, Options*)>(BuildRectangle), data_,
              options_);
  verify_data.call_name = "TexParameteri";
  verify_data.option = &options_->swizzle_g;
  verify_data.varying_arg_index = 3U;
  verify_data.static_args = {{1, "GL_TEXTURE_2D"}, {2, "GL_TEXTURE_SWIZZLE_G"}};
  verify_data.arg_tests = {
      {0, Texture::kBlue, "GL_BLUE"},
      {0, Texture::kAlpha, "GL_ALPHA"},
      {0, Texture::kRed, "GL_RED"},
      {0, Texture::kGreen, "GL_GREEN"}
  };
  EXPECT_TRUE(VerifyRenderCalls(verify_data, trace_verifier_, renderer, root,
                                GraphicsManager::kTextureSwizzle));
  // Emitted when the feature is disabled at the end of the test.
  EXPECT_TRUE(log_checker.HasMessage("ERROR",
      "OpenGL implementation does not support texture swizzles"));
  Reset();
  renderer = nullptr;
  EXPECT_TRUE(VerifyReleases(1));
}

TEST_F(RendererTest, TextureSwizzleBlue) {
  RendererPtr renderer(new Renderer(gm_));
  TracingHelper helper;
  NodePtr root;
  base::LogChecker log_checker;

  root = BuildGraph(data_, options_, kWidth, kHeight);
  VerifyRenderData<Texture::Swizzle> verify_data;
  verify_data.update_func =
    std::bind(static_cast<void (*)(Data*, Options*)>(BuildRectangle), data_,
              options_);
  verify_data.call_name = "TexParameteri";
  verify_data.option = &options_->swizzle_b;
  verify_data.varying_arg_index = 3U;
  verify_data.static_args = {{1, "GL_TEXTURE_2D"}, {2, "GL_TEXTURE_SWIZZLE_B"}};
  verify_data.arg_tests = {
      {0, Texture::kAlpha, "GL_ALPHA"},
      {0, Texture::kRed, "GL_RED"},
      {0, Texture::kGreen, "GL_GREEN"},
      {0, Texture::kBlue, "GL_BLUE"}
  };
  EXPECT_TRUE(VerifyRenderCalls(verify_data, trace_verifier_, renderer, root,
                                GraphicsManager::kTextureSwizzle));
  // Emitted when the feature is disabled at the end of the test.
  EXPECT_TRUE(log_checker.HasMessage("ERROR",
      "OpenGL implementation does not support texture swizzles"));
  Reset();
  renderer = nullptr;
  EXPECT_TRUE(VerifyReleases(1));
}

TEST_F(RendererTest, TextureSwizzleAlpha) {
  RendererPtr renderer(new Renderer(gm_));
  TracingHelper helper;
  NodePtr root;
  base::LogChecker log_checker;

  root = BuildGraph(data_, options_, kWidth, kHeight);
  VerifyRenderData<Texture::Swizzle> verify_data;
  verify_data.update_func =
    std::bind(static_cast<void (*)(Data*, Options*)>(BuildRectangle), data_,
              options_);
  verify_data.call_name = "TexParameteri";
  verify_data.option = &options_->swizzle_a;
  verify_data.varying_arg_index = 3U;
  verify_data.static_args = {{1, "GL_TEXTURE_2D"}, {2, "GL_TEXTURE_SWIZZLE_A"}};
  verify_data.arg_tests = {
      {0, Texture::kRed, "GL_RED"},
      {0, Texture::kGreen, "GL_GREEN"},
      {0, Texture::kBlue, "GL_BLUE"},
      {0, Texture::kAlpha, "GL_ALPHA"}
  };
  EXPECT_TRUE(VerifyRenderCalls(verify_data, trace_verifier_, renderer, root,
                                GraphicsManager::kTextureSwizzle));
  // Emitted when the feature is disabled at the end of the test.
  EXPECT_TRUE(log_checker.HasMessage("ERROR",
      "OpenGL implementation does not support texture swizzles"));
  Reset();
  renderer = nullptr;
  EXPECT_TRUE(VerifyReleases(1));
}

}  // namespace gfx
}  // namespace ion

#endif  // ION_PRODUCTION
