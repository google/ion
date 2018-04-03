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

#ifndef ION_EXTERNAL_FREEGLUT_LINUX_JOYSTICK_H_
#define ION_EXTERNAL_FREEGLUT_LINUX_JOYSTICK_H_

/* Overridden since LSB does not contain linux/joystick.h. */
#define _LINUX_JOYSTICK_H

#include <linux/types.h>

#define JS_VERSION    0x000000

struct JS_DATA_TYPE {
  __s32 buttons;
  __s32 x;
  __s32 y;
};

#define JS_RETURN (sizeof(struct JS_DATA_TYPE))

#endif  // ION_EXTERNAL_FREEGLUT_LINUX_JOYSTICK_H_
