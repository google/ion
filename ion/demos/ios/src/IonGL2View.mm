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

//  Based on code created by David Phillip Oster on 4/14/11. Revised 4/13/13.
//  Used with permission.

#include "ion/demos/ios/src/IonGL2View.h"

#import <QuartzCore/QuartzCore.h>
#import <OpenGLES/ES2/gl.h>
#import <OpenGLES/ES2/glext.h>

@implementation IonGL2View {
  // The pixel dimensions of the CAEAGLLayer.
  GLint _framebufferWidth;
  GLint _framebufferHeight;

  // The OpenGL ES names for the framebuffer and renderbuffer used to render to
  // this view.
  GLuint _defaultFramebuffer;
  GLuint _colorRenderbuffer;
  GLuint _depthStencilRenderbuffer;
}

@synthesize context = _context;

+ (Class)layerClass {
  return [CAEAGLLayer class];
}

- (id)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self ionGL2ViewInit];
  }
  return self;
}

// When this is stored in a nib file, this is called when it's unarchived.
- (id)initWithCoder:(NSCoder*)coder {
  self = [super initWithCoder:coder];
  if (self) {
    [self ionGL2ViewInit];
  }
  return self;
}

- (id)init {
  self = [super init];
  if (self) {
    [self ionGL2ViewInit];
  }
  return self;
}

- (void)dealloc {
  [self deleteFramebuffer];
}

- (void)ionGL2ViewInit {
  CAEAGLLayer *eaglLayer = (CAEAGLLayer *)[self layer];
  [eaglLayer setOpaque:TRUE];
  [eaglLayer setDrawableProperties:@{
      kEAGLDrawablePropertyRetainedBacking : @0,
      kEAGLDrawablePropertyColorFormat : kEAGLColorFormatRGBA8
  }];

  // Scale contents appropriately for retina and non-retina displays.
  self.contentScaleFactor = [[UIScreen mainScreen] scale];
}

- (void)setContext:(EAGLContext *)newContext {
  if (_context != newContext) {
    [self deleteFramebuffer];
    _context = newContext;
    [EAGLContext setCurrentContext:nil];
  }
}

// iOS apps require a framebuffer for regular drawing.
- (void)createFramebuffer {
  if (_context && 0 == _defaultFramebuffer) {
    [EAGLContext setCurrentContext:_context];

    // Create default framebuffer object.
    glGenFramebuffers(1, &_defaultFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, _defaultFramebuffer);

    // Create color render buffer and allocate backing store.
    glGenRenderbuffers(1, &_colorRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, _colorRenderbuffer);
    [_context renderbufferStorage:GL_RENDERBUFFER
         fromDrawable:(CAEAGLLayer *)[self layer]];
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH,
                                 &_framebufferWidth);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT,
                                 &_framebufferHeight);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, _colorRenderbuffer);

    glGenRenderbuffers(1, &_depthStencilRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, _depthStencilRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES,
                          _framebufferWidth, _framebufferHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, _depthStencilRenderbuffer);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
      glViewport(0, 0, _framebufferWidth, _framebufferHeight);
    } else {
      NSLog(@"Failed to make complete framebuffer object %x",
            glCheckFramebufferStatus(GL_FRAMEBUFFER));
    }
  }
}

- (void)deleteFramebuffer {
  if (_context) {
    [EAGLContext setCurrentContext:_context];
    if (_defaultFramebuffer) {
      glDeleteFramebuffers(1, &_defaultFramebuffer);
      _defaultFramebuffer = 0;
    }
    if (_colorRenderbuffer) {
      glDeleteRenderbuffers(1, &_colorRenderbuffer);
      _colorRenderbuffer = 0;
    }
    if (_depthStencilRenderbuffer) {
      glDeleteRenderbuffers(1, &_depthStencilRenderbuffer);
      _depthStencilRenderbuffer = 0;
    }
  }
}

- (void)setFramebufferAsCurrent {
  if (_context) {
    [EAGLContext setCurrentContext:_context];

    if (0 == _defaultFramebuffer) {
      [self createFramebuffer];
    }
    glBindFramebuffer(GL_FRAMEBUFFER, _defaultFramebuffer);
  }
}

- (BOOL)presentFramebuffer {
  BOOL success = FALSE;
  if (_context) {
    [EAGLContext setCurrentContext:_context];
    glBindRenderbuffer(GL_RENDERBUFFER, _colorRenderbuffer);
    success = [_context presentRenderbuffer:GL_RENDERBUFFER];
  }
  return success;
}

// The framebuffer will be re-created at the beginning of the next
// setFramebuffer method call.
- (void)layoutSubviews {
  [self deleteFramebuffer];
}

@end
