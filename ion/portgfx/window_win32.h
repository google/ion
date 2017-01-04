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

#ifndef ION_PORTGFX_WINDOW_WIN32_H_
#define ION_PORTGFX_WINDOW_WIN32_H_

#if defined(NOGDI)
#undef NOGDI
#endif

#include <windows.h>
#include <memory>

namespace ion {
namespace portgfx {

// This utility class creates and holds a Win32 window.  This is useful for
// clients to create graphics contexts.
class WindowWin32 {
 public:
  // Create a window with default geometry.  Specifically, this uses
  // CW_USEDEFAULT for x, w, width, and height.
  static std::unique_ptr<WindowWin32> Create();

  // Create a window with specified geometry.
  static std::unique_ptr<WindowWin32> Create(int x, int y, int w, int h);
  ~WindowWin32();

  HWND hwnd() const { return hwnd_; }
  HDC hdc() const { return hdc_; }

 private:
  WindowWin32();

  // The window.
  HWND hwnd_;

  // The window's device context.
  HDC hdc_;
};

}  // namespace portgfx
}  // namespace ion

#endif  // ION_PORTGFX_WINDOW_WIN32_H_
