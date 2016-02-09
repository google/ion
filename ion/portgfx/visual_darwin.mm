/**
Copyright 2016 Google Inc. All Rights Reserved.

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

// File used on only iOS and Mac, which must use a .mm extension to invoke the
// objective-c compiler.

#include "ion/base/logging.h"
#include "ion/portgfx/visual.h"

#if defined(ION_PLATFORM_IOS)
#  include <OpenGLES/EAGL.h>
#else
#  include <AppKit/NSOpenGL.h>
#endif

namespace ion {
namespace portgfx {

struct Visual::VisualInfo {
  // These are defined in visual.cc.
  static void* GetCurrentContext();
  static void ClearCurrentContext();

  VisualInfo() : weakContext(NULL), ownedContext(NULL) {}
  // Use weak reference for unowned context, strong reference for owned context.
#if defined(ION_PLATFORM_IOS)
  __weak EAGLContext* weakContext;
  EAGLContext* ownedContext;
#else
  __weak NSOpenGLContext* weakContext;
  NSOpenGLContext* ownedContext;
#endif
};

#if defined(ION_PLATFORM_IOS)
void* Visual::VisualInfo::GetCurrentContext() {
  return (__bridge void*)[EAGLContext currentContext];
}

Visual::Visual(Type type)
    : visual_(new VisualInfo), type_(type) {
  EAGLContext* current_context = [EAGLContext currentContext];

  if (type == kCurrent) {
    // Store a weak reference to contexts we don't own.
    visual_->weakContext = current_context;
    MakeCurrent(this);
  } else {
    EAGLSharegroup* share_group =
        (type == kShare && current_context) ? current_context.sharegroup : NULL;
    // Only store a strong reference to contexts we create and own.
    visual_->ownedContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2
                                                  sharegroup:share_group];
    visual_->weakContext = visual_->ownedContext;
  }
}

void Visual::VisualInfo::ClearCurrentContext() {
  [EAGLContext setCurrentContext:nil];
}

bool Visual::MakeCurrent() const {
  EAGLContext *strongContext = visual_->weakContext;
  if (strongContext == nil) {
    LOG(ERROR) << "Unable to make Visual current.  Unowned context has been released.";
    return false;
  }
  const BOOL success = [EAGLContext setCurrentContext:strongContext];
  if (success == NO) {
    LOG(ERROR) << "Unable to make Visual current.";
    return false;
  } else {
    return true;
  }
}

#else
void* Visual::VisualInfo::GetCurrentContext() {
  return (__bridge void*)[NSOpenGLContext currentContext];
}

void Visual::VisualInfo::ClearCurrentContext() {
  [NSOpenGLContext clearCurrentContext];
}

Visual::Visual(Type type)
    : visual_(new VisualInfo), type_(type) {
  // Save the old context.
  NSOpenGLContext* current_context = [NSOpenGLContext currentContext];

  if (type == kCurrent) {
    // Store a weak reference to contexts we don't own.
    visual_->weakContext = current_context;
    MakeCurrent(this);
  } else {
    NSOpenGLPixelFormatAttribute pixelAttrs[] = {
      NSOpenGLPFAColorSize, 24,
      NSOpenGLPFAAlphaSize, 8,
      NSOpenGLPFADepthSize, 24,
      NSOpenGLPFAStencilSize, 8,
      NSOpenGLPFASampleBuffers, 0,
      0,
    };

    NSOpenGLPixelFormat* pixelFormat =
        [[NSOpenGLPixelFormat alloc] initWithAttributes:pixelAttrs];
    NSOpenGLContext* share_context = (type == kShare) ? current_context : NULL;
    // Only store a strong reference to contexts we create and own.
    visual_->ownedContext = [[NSOpenGLContext alloc] initWithFormat:pixelFormat
                                                       shareContext:share_context];
    visual_->weakContext = visual_->ownedContext;
  }
}

bool Visual::MakeCurrent() const {
  NSOpenGLContext *strongContext = visual_->weakContext;
  if (strongContext == nil) {
    LOG(ERROR) << "Unable to make Visual current.  Unowned context has been released.";
    return false;
  }
  [strongContext makeCurrentContext];
  if (strongContext != [NSOpenGLContext currentContext]) {
    LOG(ERROR) << "Unable to make Visual current.";
    return false;
  } else {
    return true;
  }
}

#endif

Visual::Visual() : visual_(new VisualInfo), type_(kMock) {}

void Visual::UpdateId() {
  id_ = reinterpret_cast<size_t>(visual_->weakContext);
}

bool Visual::IsValid() const {
  return visual_->weakContext != NULL;
}

void Visual::TeardownContextNew() {
  visual_->weakContext = nil;
  visual_->ownedContext = nil;
}

void Visual::TeardownContextShared() {
  visual_->weakContext = nil;
  visual_->ownedContext = nil;
}

Visual::~Visual() {
  TeardownVisual(this);
}

}  // namespace portgfx
}  // namespace ion
