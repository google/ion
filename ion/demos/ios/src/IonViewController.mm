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

//  Based on code created by David Phillip Oster on 4/14/11. Revised 4/25/13.
//  Used with permission.

#include "ion/demos/ios/src/IonViewController.h"

#import <QuartzCore/QuartzCore.h>
#import <OpenGLES/EAGL.h>

#include "ion/demos/demobase_ios.h"
#include "ion/demos/ios/src/IonGL2View.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support. Compile with -fobjc-arc"
#endif

@interface IonViewController ()
@property (nonatomic, strong) EAGLContext* context;
@property (nonatomic, strong) CADisplayLink* displayLink;
@property (nonatomic, strong) IonDemo* render;
@end

@implementation IonViewController{
  NSInteger _animationFrameInterval;
  NSTimeInterval _then;
  CGFloat _scale;
}

@synthesize isAnimating = _isAnimating;
@synthesize context = _context;
@synthesize displayLink = _displayLink;

- (void)loadView {
  CGRect frame = [[UIScreen mainScreen] bounds];
  IonGL2View* view = [[IonGL2View alloc] initWithFrame:frame];
  _scale = [view contentScaleFactor];
  [view setAutoresizingMask:UIViewAutoresizingFlexibleLeftMargin |
       UIViewAutoresizingFlexibleWidth |
       UIViewAutoresizingFlexibleRightMargin |
       UIViewAutoresizingFlexibleTopMargin |
       UIViewAutoresizingFlexibleHeight |
       UIViewAutoresizingFlexibleBottomMargin];
  [view setMultipleTouchEnabled:YES];
  UIPinchGestureRecognizer* pinch = [[UIPinchGestureRecognizer alloc]
      initWithTarget:self action:@selector(pinch:)];
  [view addGestureRecognizer:pinch];
  UIPanGestureRecognizer* pan = [[UIPanGestureRecognizer alloc]
      initWithTarget:self action:@selector(pan:)];
  [view addGestureRecognizer:pan];

  [self setView:view];
}

- (void)pan:(UIPanGestureRecognizer*)pan {
  CGPoint delta = [pan translationInView:[self view]];
  bool isBegin = [pan state] == UIGestureRecognizerStateBegan;
  [_render setSwipe:delta isBegin:isBegin];
}

- (void)pinch:(UIPinchGestureRecognizer*)pinch {
  CGFloat scale = [pinch scale];
  scale = 1.f / scale;
  bool isBegin = [pinch state] == UIGestureRecognizerStateBegan;
  [_render setScale:scale isBegin:isBegin];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  [self setUpContext];
  [[self ionGL2View] setContext:_context];
  [[self ionGL2View] setFramebufferAsCurrent];
  if (nil == _render) {
    CGSize size = [[self view] bounds].size;
    size.width *= _scale;
    size.height *= _scale;
    [self setRender:[[IonDemo alloc] initWithSize:size]];
  }

  _animationFrameInterval = 1;
//  _isAnimating = NO; // Is NO, already.
//  [self setDisplayLink:nil];  // Is nil, already.
}

- (void)setDisplayLink:(CADisplayLink*)link {
  if (link != _displayLink) {
    [_displayLink invalidate];
    _displayLink = link;
  }
}

- (void)dealloc {
  if ([EAGLContext currentContext] == [self context]) {
    [EAGLContext setCurrentContext:nil];
  }
  [self setContext:nil];
  [self setDisplayLink:nil];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self setUpContext];
  [self setAnimating:YES];
  CGSize size = [[self view] bounds].size;
  size.width *= _scale;
  size.height *= _scale;
  [_render setSize:size];
}

- (void)viewWillDisappear:(BOOL)animated {
  [self setAnimating:NO];
  [super viewWillDisappear:animated];
}

- (void)setUpContext {
  if (nil == _context) {
    EAGLContext* aContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];

    if (nil == aContext) {
      NSLog(@"Failed to create ES context");
    } else if ( ! [EAGLContext setCurrentContext:aContext]) {
      NSLog(@"Failed to set ES context current");
    }

    [self setContext:aContext];
  }
}

- (void)willAnimateRotationToInterfaceOrientation:(UIInterfaceOrientation)toInterfaceOrientation duration:(NSTimeInterval)duration {
  [self didRotate:@(toInterfaceOrientation)];
}

- (void)didRotate:(NSNumber*)toInterfaceOrientation {
  CGSize size = [[self view] bounds].size;
  size.width *= _scale;
  size.height *= _scale;
  [_render setSize:size];
  [self drawFrame];
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)orientation {
  if (UIUserInterfaceIdiomPad == UI_USER_INTERFACE_IDIOM()) {
    return YES;
  }
  return UIInterfaceOrientationPortraitUpsideDown != orientation;
}

- (void)didReceiveMemoryWarning {
  [super didReceiveMemoryWarning];
  if ([self isViewLoaded] && nil == [[self view] window]) {
    [self setView:nil];
    [self setDisplayLink:nil];
    [self setContext:nil];
  }
}

#pragma mark -

- (IonGL2View*)ionGL2View {
  return (IonGL2View*) [self view];
}

- (void)setContext:(EAGLContext*)context {
  if (_context != context) {
    _context = context;
    if ([self isViewLoaded]) {
      [[self ionGL2View] setContext:context];
    }
  }
}

- (NSInteger)animationFrameInterval {
  return _animationFrameInterval;
}

- (void)setAnimationFrameInterval:(NSInteger)frameInterval {
  if (_animationFrameInterval != frameInterval) {
    if (1 <= frameInterval) {
      _animationFrameInterval = frameInterval;
      if (_isAnimating) {
        [self setAnimating:NO];
        [self setAnimating:YES];
      }
    }
  }
}

- (void)startAnimation {
  CADisplayLink* aDisplayLink =
      [[UIScreen mainScreen] displayLinkWithTarget:self selector:@selector(updateWorld:)];
  [aDisplayLink setFrameInterval:_animationFrameInterval];
  [aDisplayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
  [self setDisplayLink:aDisplayLink];
  _then = [aDisplayLink timestamp];
}

- (void)stopAnimation {
  [self setDisplayLink:nil];
}

- (void)setAnimating:(BOOL)isAnimating {
  if (_isAnimating != isAnimating) {
    _isAnimating = isAnimating;
    if (_isAnimating) {
      [self startAnimation];
    } else {
      [self stopAnimation];
    }
  }
}

- (void)drawFrame {
  [[self ionGL2View] setFramebufferAsCurrent];
  [_render drawFrame];
  [[self ionGL2View] presentFramebuffer];
}

- (void)updateWorld:(CADisplayLink*)link {
  NSTimeInterval now = [link timestamp];
  [_render updateAnimation:now - _then];
  _then = now;
  [self drawFrame];
}

@end
