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

#include "ion/demos/mac/appdelegate.h"
#include "ion/demos/mac/demoglview.h"

@implementation AppDelegate : NSObject

- (id)init {
  if (self = [super init]) {
    // Create main window
    NSRect windowRect = NSMakeRect(0.0f, 0.0f, 800.0f, 800.0f);
    _window = [[NSWindow alloc] initWithContentRect:windowRect
                                          styleMask:
        (NSResizableWindowMask | NSClosableWindowMask | NSTitledWindowMask)
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
    [_window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];

    // Create app menu and populate with quit command
    id appMenu = [[NSMenu alloc] init];
    id appName = [[NSProcessInfo processInfo] processName];
    id quitTitle = [@"Quit " stringByAppendingString:appName];
    id quitMenuItem = [[NSMenuItem alloc] initWithTitle:quitTitle
                                                 action:@selector(terminate:)
                                          keyEquivalent:@"q"];
    [appMenu addItem:quitMenuItem];

    // Add app menu to menu bar.
    id menuBar = [[NSMenu alloc] init];
    [NSApp setMainMenu:menuBar];
    id appMenuItem = [NSMenuItem new];
    [menuBar addItem:appMenuItem];
    [appMenuItem setSubmenu:appMenu];
  }
  return self;
}

- (void)applicationWillFinishLaunching:(NSNotification *)notification {
  [_window makeKeyAndOrderFront:self];

  NSOpenGLPixelFormatAttribute attr[] = {

#if ION_DEMO_CORE_PROFILE
    // Some demos require the 3.2 Core profile.  We can't unconditionally use the
    // core profile, because not all demos have modern shaders.
    // 
    // auto-modernization for shaders.
    NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
#endif

    NSOpenGLPFADoubleBuffer,
    NSOpenGLPFAAccelerated,
    NSOpenGLPFAColorSize, 32,
    NSOpenGLPFADepthSize, 16,
    0
  };
  NSOpenGLPixelFormat* format =
      [[NSOpenGLPixelFormat alloc] initWithAttributes:attr];
  DemoGLView* view = [[DemoGLView alloc] initWithFrame:[_window frame]
                                            pixelFormat:format];
  [view setWantsBestResolutionOpenGLSurface:YES];
  [_window setContentView:view];
  [_window makeFirstResponder:view];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:
    (NSApplication *)theApplication {
  return YES;
}

@end
