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

#ifndef ION_GFX_GLFUNCTIONTYPES_H_
#define ION_GFX_GLFUNCTIONTYPES_H_

#include "ion/portgfx/glheaders.h"

// These types are necessary to distinguish between types with overlapping
// values, e.g., GL_ZERO, GL_NONE, and GL_POINTS are the same number. Having
// specially named types allows TracingHelper determine which value to print.
typedef GLenum GLblendenum;
typedef GLenum GLstencilenum;
typedef GLint GLintenum;
typedef GLint GLtextureenum;
typedef GLbitfield GLmapaccess;
// These special types let TracingHelper know the type of pointer being passed.
typedef GLfloat GLfloat1;
typedef GLfloat GLfloat2;
typedef GLfloat GLfloat3;
typedef GLfloat GLfloat4;
typedef GLfloat GLmatrix2;
typedef GLfloat GLmatrix3;
typedef GLfloat GLmatrix4;
typedef GLint GLint1;
typedef GLint GLint2;
typedef GLint GLint3;
typedef GLint GLint4;
typedef GLuint GLuint1;
typedef GLuint GLuint2;
typedef GLuint GLuint3;
typedef GLuint GLuint4;

#endif  // ION_GFX_GLFUNCTIONTYPES_H_
