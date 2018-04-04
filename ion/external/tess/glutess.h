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

#ifndef ION_EXTERNAL_TESS_GLUTESS_H_
#define ION_EXTERNAL_TESS_GLUTESS_H_

#ifdef __cplusplus
extern "C" {
#endif

#define GLAPIENTRY

/* Errors: (return value 0 = no error) */
#define GLU_INVALID_ENUM        100900
#define GLU_INVALID_VALUE       100901
#define GLU_OUT_OF_MEMORY       100902
#define GLU_INCOMPATIBLE_GL_VERSION     100903

/* TessCallback */
#define GLU_TESS_BEGIN                     100100
#define GLU_BEGIN                          100100
#define GLU_TESS_VERTEX                    100101
#define GLU_VERTEX                         100101
#define GLU_TESS_END                       100102
#define GLU_END                            100102
#define GLU_TESS_ERROR                     100103
#define GLU_TESS_EDGE_FLAG                 100104
#define GLU_EDGE_FLAG                      100104
#define GLU_TESS_COMBINE                   100105
#define GLU_TESS_BEGIN_DATA                100106
#define GLU_TESS_VERTEX_DATA               100107
#define GLU_TESS_END_DATA                  100108
#define GLU_TESS_ERROR_DATA                100109
#define GLU_TESS_EDGE_FLAG_DATA            100110
#define GLU_TESS_COMBINE_DATA              100111

/* TessContour */
#define GLU_CW                             100120
#define GLU_CCW                            100121
#define GLU_INTERIOR                       100122
#define GLU_EXTERIOR                       100123
#define GLU_UNKNOWN                        100124

/* TessProperty */
#define GLU_TESS_WINDING_RULE              100140
#define GLU_TESS_BOUNDARY_ONLY             100141
#define GLU_TESS_TOLERANCE                 100142

/* TessError */
#define GLU_TESS_ERROR1                    100151
#define GLU_TESS_ERROR2                    100152
#define GLU_TESS_ERROR3                    100153
#define GLU_TESS_ERROR4                    100154
#define GLU_TESS_ERROR5                    100155
#define GLU_TESS_ERROR6                    100156
#define GLU_TESS_ERROR7                    100157
#define GLU_TESS_ERROR8                    100158
#define GLU_TESS_MISSING_BEGIN_POLYGON     100151
#define GLU_TESS_MISSING_BEGIN_CONTOUR     100152
#define GLU_TESS_MISSING_END_POLYGON       100153
#define GLU_TESS_MISSING_END_CONTOUR       100154
#define GLU_TESS_COORD_TOO_LARGE           100155
#define GLU_TESS_NEED_COMBINE_CALLBACK     100156

/* TessWinding */
#define GLU_TESS_WINDING_ODD               100130
#define GLU_TESS_WINDING_NONZERO           100131
#define GLU_TESS_WINDING_POSITIVE          100132
#define GLU_TESS_WINDING_NEGATIVE          100133
#define GLU_TESS_WINDING_ABS_GEQ_TWO       100134

#define GLU_TESS_MAX_COORD 1.0e150

#ifdef __cplusplus
class GLUtesselator;
#else
typedef struct GLUtesselator GLUtesselator;
#endif

#if !defined(_GLUfuncptr)
typedef GLvoid (*_GLUfuncptr)();
#endif

#if defined(ION_PLATFORM_ANDROID) || defined(ION_PLATFORM_IOS) || \
    defined(ION_GOOGLE_INTERNAL)
typedef double GLdouble;
#endif

void GLAPIENTRY gluBeginPolygon(GLUtesselator* tess);
void GLAPIENTRY gluDeleteTess(GLUtesselator* tess);
void GLAPIENTRY gluEndPolygon(GLUtesselator* tess);
void GLAPIENTRY
    gluGetTessProperty(GLUtesselator* tess, GLenum which, GLdouble* data);
GLUtesselator* GLAPIENTRY gluNewTess();
void GLAPIENTRY gluNextContour(GLUtesselator* tess, GLenum type);
void GLAPIENTRY gluTessBeginContour(GLUtesselator* tess);
void GLAPIENTRY gluTessBeginPolygon(GLUtesselator* tess, GLvoid* data);
void GLAPIENTRY gluTessCallback(GLUtesselator* tess,
                                GLenum which,
                                _GLUfuncptr CallBackFunc);
void GLAPIENTRY gluTessEndContour(GLUtesselator* tess);
void GLAPIENTRY gluTessEndPolygon(GLUtesselator* tess);
void GLAPIENTRY gluTessNormal(GLUtesselator* tess,
                              GLdouble valueX,
                              GLdouble valueY,
                              GLdouble valueZ);
void GLAPIENTRY
    gluTessProperty(GLUtesselator* tess, GLenum which, GLdouble data);
void GLAPIENTRY
    gluTessVertex(GLUtesselator* tess, GLdouble* location, GLvoid* data);

#ifdef __cplusplus
}
#endif

#endif  // ION_EXTERNAL_TESS_GLUTESS_H_
