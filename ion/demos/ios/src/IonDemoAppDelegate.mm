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

#include "ion/demos/ios/src/IonDemoAppDelegate.h"

#include "ion/demos/ios/src/IonViewController.h"

@implementation IonDemoAppDelegate

- (BOOL)application:(UIApplication *)application
      didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
  self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  IonViewController* myViewController = [[IonViewController alloc] init];
  self.viewController = myViewController;
  self.window.rootViewController = myViewController;
  myViewController.title = @"IonDemo";
  [self.window makeKeyAndVisible];
  return YES;
}

// Sent when the application is about to move from active to inactive state.
// This can occur for certain types of temporary interruptions (such as an
// incoming phone call or SMS message) or when the user quits the application
// and it begins the transition to the background state.
// Use this method to pause ongoing tasks, disable timers, and throttle down
// OpenGL ES frame rates. Games should use this method to pause the game.
- (void)applicationWillResignActive:(UIApplication *)application {
  [_viewController setAnimating:NO];
}

// Restart any tasks that were paused (or not yet started) while the application
// was inactive. If the application was previously in the background, optionally
// refresh the user interface.
- (void)applicationDidBecomeActive:(UIApplication *)application {
  [_viewController setAnimating:YES];
}

@end
