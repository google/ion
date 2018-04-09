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

#include "ion/demos/demobase.h"

#include <GLES2/gl2.h>
#include <stdarg.h>
#include <stdio.h>

#include "ion/base/logging.h"
#include "ion/base/stringutils.h"
#include "ion/math/utils.h"

#include "ppapi/cpp/graphics_3d.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/lib/gl/gles2/gl2ext_ppapi.h"
#include "ppapi/utility/completion_callback_factory.h"

namespace {

class IonDemoModule;

// An instance of the Native Client IonDemo "application".  One of these will be
// created for every embed tag that loads the NMF file.
class IonDemoInstance : public pp::Instance {
 public:
  explicit IonDemoInstance(PP_Instance instance, IonDemoModule* module);
  virtual ~IonDemoInstance();

  // Silence virtual override warning.
  using pp::Instance::DidChangeView;

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]);
  virtual void HandleMessage(const pp::Var& var_messsage);
  virtual bool HandleInputEvent(const pp::InputEvent& event);
  virtual void DidChangeView(const pp::View& view);

 private:
  // Initializes the PPAPI OpenGL implementation and creates a context.
  void InitGraphics(int width, int height);

  // Invokes IonDemo::Update and IonDemo::Render, then swaps buffers.  The
  // argument is needed because this is used as a callback function from
  // callback_factory_.
  void DrawFrame(int32_t);

  IonDemoModule* module_;
  std::unique_ptr<DemoBase> demo_;
  pp::Graphics3D context_;
  pp::CompletionCallbackFactory<IonDemoInstance> callback_factory_;
};

// The PPAPI module for IonDemo.  Only one of these will ever exist.  Its sole
// responsibility is to create instances on request.
class IonDemoModule : public pp::Module {
 public:
  IonDemoModule() : pp::Module() {}
  virtual ~IonDemoModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new IonDemoInstance(instance, this);
  }
};

IonDemoInstance::IonDemoInstance(PP_Instance instance, IonDemoModule* module)
    : pp::Instance(instance),
      module_(module),
      callback_factory_(this) {
}
IonDemoInstance::~IonDemoInstance() {}

bool IonDemoInstance::Init(
    uint32_t argc, const char* argn[], const char* argv[]) {
  static const int kWidth = 800;
  static const int kHeight = 800;

  InitGraphics(kWidth, kHeight);
  demo_ = CreateDemo(kWidth, kHeight);

  RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE);
  return true;
}

void IonDemoInstance::HandleMessage(const pp::Var& message) {}

bool IonDemoInstance::HandleInputEvent(const pp::InputEvent& event) {
  switch (event.GetType()) {
    case PP_INPUTEVENT_TYPE_MOUSEDOWN: {
      pp::MouseInputEvent mouse_event(event);
      demo_->ProcessMotion(static_cast<float>(mouse_event.GetPosition().x()),
                           static_cast<float>(mouse_event.GetPosition().y()),
                           true);
      return true;
    }
    case PP_INPUTEVENT_TYPE_MOUSEMOVE: {
      pp::MouseInputEvent mouse_event(event);
      if (mouse_event.GetButton() == PP_INPUTEVENT_MOUSEBUTTON_LEFT) {
        demo_->ProcessMotion(static_cast<float>(mouse_event.GetPosition().x()),
                             static_cast<float>(mouse_event.GetPosition().y()),
                             false);
      } else if (mouse_event.GetButton() == PP_INPUTEVENT_MOUSEBUTTON_RIGHT) {
        static const float kScaleFactor = 0.005f;
        static float scale = 1.0f;
        pp::Point delta = mouse_event.GetMovement();
        scale += kScaleFactor * static_cast<float>(delta.y() - delta.x());
        demo_->ProcessScale(scale);
      }
      return true;
    }
    default:
      return false;
  }
}

void IonDemoInstance::DidChangeView(const pp::View& view) {
  DrawFrame(0);
}

void IonDemoInstance::InitGraphics(int width, int height) {
  if (!glInitializePPAPI(pp::Module::Get()->get_browser_interface())) {
    LOG(ERROR) << "Unable to initialize GL PPAPI!";
  }

  int32_t attribs[] = {
    PP_GRAPHICS3DATTRIB_ALPHA_SIZE, 8,
    PP_GRAPHICS3DATTRIB_DEPTH_SIZE, 24,
    PP_GRAPHICS3DATTRIB_STENCIL_SIZE, 8,
    PP_GRAPHICS3DATTRIB_SAMPLES, 0,
    PP_GRAPHICS3DATTRIB_SAMPLE_BUFFERS, 0,
    PP_GRAPHICS3DATTRIB_WIDTH, width,
    PP_GRAPHICS3DATTRIB_HEIGHT, height,
    PP_GRAPHICS3DATTRIB_NONE
  };

  context_ = pp::Graphics3D(this, attribs);

  if (!BindGraphics(context_)) {
    context_ = pp::Graphics3D();
    glSetCurrentContextPPAPI(0);
    LOG(ERROR) << "Failed to set graphics context.";
    return;
  }
  glSetCurrentContextPPAPI(context_.pp_resource());
}

void IonDemoInstance::DrawFrame(int32_t unused) {
  // Render a frame.
  demo_->Update();
  demo_->Render();

  // SwapBuffers and schedule a new frame.
  context_.SwapBuffers(
      callback_factory_.NewCallback(&IonDemoInstance::DrawFrame));
}

}  // namespace

namespace pp {
pp::Module* CreateModule() {
  return new IonDemoModule();
}
}  // namespace pp
