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

#ifndef ION_EXTERNAL_TESS_GLUOS_H_
#define ION_EXTERNAL_TESS_GLUOS_H_

/* Include OpenGL, but not glext or glu. */
#define __glext_h_
#define __glu_h__
#include "ion/portgfx/glheaders.h"

/* Include enums and function prototypes. */
#include "ion/external/tess/glutess.h"

#endif  // ION_EXTERNAL_TESS_GLUOS_H_
