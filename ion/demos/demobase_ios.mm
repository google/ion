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

#include "ion/demos/demobase_ios.h"

#include "ion/demos/demobase.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support. Compile with -fobjc-arc"
#endif


//-----------------------------------------------------------------------------
//
// Renderer.
//
//-----------------------------------------------------------------------------

@implementation IonDemo {
  std::unique_ptr<DemoBase> _demo;
  float _begin_scale;
  float _scale;
  CGPoint _rotate;
}

- (id)init {
  CGRect screenBounds = [UIScreen mainScreen].bounds;
  return [self initWithSize:screenBounds.size];
}

- (id)initWithSize:(CGSize)size {
  self = [super init];
  if (self) {
    _scale = _begin_scale = 1.f;
    _rotate.x = _rotate.y = 0;
    _demo = CreateDemo(static_cast<int>(size.width),
                       static_cast<int>(size.height));
  }
  return self;
}

- (void)drawFrame {
  _demo->Render();
}

- (void)updateAnimation:(double)deltaSeconds {
  _demo->Update();
}

- (void)setSwipe:(CGPoint)delta isBegin:(BOOL)begin {
  _demo->ProcessMotion(static_cast<float>(delta.x), static_cast<float>(delta.y), begin);
}

- (void)setScale:(CGFloat)scale isBegin:(BOOL)begin {
  if (begin)
    _begin_scale = _scale;
  _scale = static_cast<float>(scale) * _begin_scale;

  // Don't let the object get too small or too large.
  _scale = std::max(0.1f, std::min(_scale, 5.0f));

  _demo->ProcessScale(_scale);
}

- (void)setSize:(CGSize)size {
  _demo->Resize(static_cast<int>(size.width), static_cast<int>(size.height));
}

@end
