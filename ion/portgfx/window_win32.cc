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

#include "ion/portgfx/window_win32.h"

#include <wingdi.h>

#include "ion/base/logging.h"
#include "ion/base/staticsafedeclare.h"

namespace ion {
namespace portgfx {
namespace {

const char kIonWindowClassName[] = "ION";

// Windows window class creator.
class IonWindowClass {
 public:
  IonWindowClass() {
    WNDCLASSEXA window_class;
    memset(&window_class, 0, sizeof(window_class));
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_OWNDC;
    window_class.lpfnWndProc = &DefWindowProc;
    window_class.hInstance = GetModuleHandle(nullptr);
    window_class.lpszClassName = kIonWindowClassName;
    atom_ = RegisterClassExA(&window_class);
  }

  ~IonWindowClass() {
    UnregisterClassA(kIonWindowClassName, GetModuleHandle(nullptr));
  }

  ATOM GetAtom() const { return atom_; }

 private:
  ATOM atom_;
};

ATOM GetIonWindowClass() {
  ION_DECLARE_SAFE_STATIC_POINTER(IonWindowClass, window_class);
  return window_class->GetAtom();
}

}  // namespace

WindowWin32::WindowWin32() : hwnd_(nullptr), hdc_(nullptr) {}

WindowWin32::~WindowWin32() {
  if (hwnd_ != nullptr) {
    DestroyWindow(hwnd_);
  }
  // |hdc_| is retrieved from a window created with CS_OWNDC style, and does not
  // need to be released.
}

// static
std::unique_ptr<WindowWin32> WindowWin32::Create() {
  return Create(CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT);
}

// static
std::unique_ptr<WindowWin32> WindowWin32::Create(int x, int y, int w, int h) {
  std::unique_ptr<WindowWin32> window(new WindowWin32());

  const DWORD dwExStyle = 0;
  const char* const lpClassName =
      reinterpret_cast<const char*>(GetIonWindowClass());
  const char* const lpWindowName = "ION";
  const DWORD dwStyle = 0;
  const HWND hWndParent = nullptr;
  const HMENU hMenu = nullptr;
  const HINSTANCE hInstance = GetModuleHandle(nullptr);
  void* const lpParam = nullptr;
  window->hwnd_ =
      CreateWindowExA(dwExStyle, lpClassName, lpWindowName, dwStyle, x, y, w, h,
                      hWndParent, hMenu, hInstance, lpParam);
  if (window->hwnd_ == nullptr) {
    LOG(ERROR) << "Failed to create window.";
    return nullptr;
  }

  // |window->hdc_| is retrieved from a window created with CS_OWNDC style,
  // and does not need to be released.
  window->hdc_ = GetDC(window->hwnd_);
  if (window->hdc_ == nullptr) {
    LOG(ERROR) << "Failed to get device context.";
    return nullptr;
  }

  // Choose a pixel format for the window.
  PIXELFORMATDESCRIPTOR format_descriptor;
  memset(&format_descriptor, 0, sizeof(format_descriptor));
  format_descriptor.nSize = sizeof(format_descriptor);
  format_descriptor.nVersion = 1;
  format_descriptor.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
  format_descriptor.iPixelType = PFD_TYPE_RGBA;
  format_descriptor.cColorBits = 24;
  format_descriptor.cAlphaBits = 8;
  format_descriptor.cDepthBits = 8;
  format_descriptor.iLayerType = PFD_MAIN_PLANE;
  const int pixel_format = ChoosePixelFormat(window->hdc_, &format_descriptor);
  if (pixel_format == 0) {
    LOG(ERROR) << "Could not choose pixel format.";
    return nullptr;
  }

  if (!SetPixelFormat(window->hdc_, pixel_format, &format_descriptor)) {
    LOG(ERROR) << "Could not set pixel format.";
    return nullptr;
  }

  return window;
}

}  // namespace portgfx
}  // namespace ion
