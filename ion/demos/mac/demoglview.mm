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
#include "ion/demos/mac/demoglview.h"
#include "ion/math/utils.h"

static NSTimer* timer = nil;

@implementation DemoGLView {
  std::unique_ptr<DemoBase> _demo;
}

- (void)prepareOpenGL {
  // Enable double buffering
  GLint swapInterval = 1;
  [[self openGLContext] setValues:&swapInterval
                     forParameter:NSOpenGLCPSwapInterval];

  // Enable hidpi rendering
  [self setWantsBestResolutionOpenGLSurface:YES];

  NSSize pixelSize = [self backingPixelSize];
  _demo = CreateDemo(static_cast<int>(pixelSize.width),
                     static_cast<int>(pixelSize.height));
  timer = [NSTimer scheduledTimerWithTimeInterval:1.0/60.0
                                           target:self
                                         selector:@selector(timedUpdate)
                                         userInfo:nil
                                          repeats:YES];
}

- (void)drawRect:(NSRect)dirtyRect {
  if (!_demo) return;
  _demo->Render();
  [[self openGLContext] flushBuffer];
}

- (void)reshape {
  if (!_demo) return;
  NSSize pixelSize = [self backingPixelSize];
  _demo->Resize(static_cast<int>(pixelSize.width),
               static_cast<int>(pixelSize.height));
  [self setNeedsDisplay:YES];
}

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (void)scrollWheel:(NSEvent *)theEvent {
  if (!_demo) return;
  static const float kScaleFactor = 0.01f;
  static float scale = 1.f;
  scale -= kScaleFactor * [theEvent deltaY];
  scale = ion::math::Clamp(scale, 0.05f, 5.0f);
  _demo->ProcessScale(scale);
  [self setNeedsDisplay:YES];
}

- (void)mouseDown:(NSEvent *)theEvent {
  if (!_demo) return;
  NSEventType type = [theEvent type];
  NSPoint point = [self localPointOfEvent:theEvent];
  if (type == NSLeftMouseDown) {
    _demo->ProcessMotion(static_cast<float>(point.x),
                         static_cast<float>(point.y), true);
  }
  [self setNeedsDisplay:YES];
}

- (void)mouseDragged:(NSEvent *)theEvent {
  if (!_demo) return;
  NSEventType type = [theEvent type];
  NSPoint point = [self localPointOfEvent:theEvent];
  if (type == NSLeftMouseDragged) {
    _demo->ProcessMotion(static_cast<float>(point.x),
                         static_cast<float>(point.y), false);
  }
  [self setNeedsDisplay:YES];
}

- (void)keyDown:(NSEvent *)theEvent {
  if (!_demo) return;
  if ([theEvent isARepeat]) return;

  // If this was the escape key, then quit
  if ([theEvent keyCode] == 53) {
    _demo.reset();
    [NSApp terminate:self];
  }

  NSString *str = [theEvent charactersIgnoringModifiers];
  unsigned char c = static_cast<unsigned char>([str characterAtIndex:0]);
  NSPoint point = [self localPointOfEvent:theEvent];
  _demo->Keyboard(c, static_cast<int>(point.x), static_cast<int>(point.y), true);
}

- (void)keyUp:(NSEvent *)theEvent {
  if (!_demo) return;
  NSString *str = [theEvent charactersIgnoringModifiers];
  unsigned char c = static_cast<unsigned char>([str characterAtIndex:0]);
  NSPoint point = [self localPointOfEvent:theEvent];
  _demo->Keyboard(c, static_cast<int>(point.x), static_cast<int>(point.y),
                  false);
}

#pragma mark Private Methods

- (NSPoint)localPointOfEvent:(NSEvent *)theEvent {
  NSPoint point = [self convertPoint:[theEvent locationInWindow] fromView:nil];
  point.y = [self frame].size.height - point.y;
  return point;
}

- (NSSize)backingPixelSize {
  // Get view dimensions in pixels
  NSRect backingBounds = [self convertRectToBacking:[self bounds]];
  return backingBounds.size;
}

- (void)timedUpdate {
  _demo->Update();
  [self setNeedsDisplay:YES];
}

@end
