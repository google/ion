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

#include "ion/base/logging.h"
#include "ion/base/stringutils.h"
#include "ion/math/utils.h"

#if defined(ION_PLATFORM_ASMJS)
#include <emscripten.h>
#include "GL/glut.h"
#else
#include "GL/freeglut.h"
#endif


//-----------------------------------------------------------------------------
//
// Glut interface functions.
//
//-----------------------------------------------------------------------------

// GLUT programs never return from main(), they always call exit(). To avoid
// crashes at program exit, we have to explicitly clean up the demo object in a
// handler registered with std::atexit().
static DemoBase* demo = nullptr;
enum {
  LEFT_BUTTON = 1,
  RIGHT_BUTTON = 2,
  MIDDLE_BUTTON = 4,
};
static int buttons = 0;
static int last_x = 0, last_y = 0;

static void Init(int w, int h) {
  demo = CreateDemo(w, h).release();
}

static void Done() {
  delete demo;
  demo = nullptr;
}

static void Resize(int w, int h) {
  demo->Resize(w, h);
  glutPostRedisplay();
}

static void Render() {
  if (demo)
    demo->Render();
  glutSwapBuffers();
}

static void Update() {
  if (demo)
    demo->Update();
  glutPostRedisplay();
}

static void Keyboard(unsigned char key, int x, int y) {
  demo->Keyboard(key, x, y, true);
  glutPostRedisplay();
}

static void KeyboardUp(unsigned char key, int x, int y) {
  demo->Keyboard(key, x, y, false);
  switch (key) {
    case 27:  // Escape.
#ifndef ION_PLATFORM_ASMJS
      glutLeaveMainLoop();
#else
      exit(0);
#endif
      break;
  }
  glutPostRedisplay();
}

static void Motion(int x, int y) {
  if (buttons & LEFT_BUTTON) {
    demo->ProcessMotion(static_cast<float>(x), static_cast<float>(y), false);
  } else if (buttons & RIGHT_BUTTON) {
    static const float kScaleFactor = 0.005f;
    static float scale = 1.f;
    const int delta_x = x - last_x;
    const int delta_y = y - last_y;
    scale += kScaleFactor * static_cast<float>(delta_y - delta_x);
    scale = ion::math::Clamp(scale, 0.05f, 5.0f);
    demo->ProcessScale(scale);
  }
  last_x = x;
  last_y = y;
  glutPostRedisplay();
}

static void Mouse(int button, int state, int x, int y) {
  if (button == GLUT_LEFT_BUTTON) {
    if (state == GLUT_DOWN) {
      buttons |= LEFT_BUTTON;
    } else {
      buttons &= ~LEFT_BUTTON;
    }
  } else if (button == GLUT_RIGHT_BUTTON) {
    if (state == GLUT_DOWN) {
      buttons |= RIGHT_BUTTON;
    } else {
      buttons &= ~RIGHT_BUTTON;
    }
  }
  last_x = x;
  last_y = y;
  demo->ProcessMotion(static_cast<float>(x), static_cast<float>(y), true);
  glutPostRedisplay();
}

int main(int argc, char* argv[]) {
  static const int kWidth  = 800;
  static const int kHeight = 800;
  glutInit(&argc, argv);

#if defined(ION_PLATFORM_ASMJS)
  glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
#else
  glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH | GLUT_MULTISAMPLE);
  glutSetOption(GLUT_MULTISAMPLE, 16);
#endif
  glutInitWindowSize(kWidth, kHeight);

  glutCreateWindow("ION Demo");

  glutDisplayFunc(Render);
  glutReshapeFunc(Resize);
  glutKeyboardFunc(Keyboard);
  glutKeyboardUpFunc(KeyboardUp);
  glutIdleFunc(Update);
  glutMotionFunc(Motion);
  glutMouseFunc(Mouse);

  Init(kWidth, kHeight);
  std::atexit(Done);

#if defined(ION_PLATFORM_ASMJS)
  // When testing asmjs with Scuba, we need to render a single frame, then
  // trigger an event so that the test knows when it can capture a screenshot.
  Render();
  EM_ASM({ Module.canvas.dispatchEvent(new Event('rendered')); });
#else
  // The glutSetWindowTitle function is not defined in the emscripten
  // implementation of GLUT.
  const std::string demo_name = "ION Demo: " + demo->GetDemoAppName();
  glutSetWindowTitle(demo_name.c_str());
#endif

  // The GLUT main loop never returns.
  glutMainLoop();
}
