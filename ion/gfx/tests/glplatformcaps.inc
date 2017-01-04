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

#ifndef ION_GFX_TESTS_GLPLATFORMCAPS_H_
#define ION_GFX_TESTS_GLPLATFORMCAPS_H_

#if !defined(ION_PLATFORM_CAP)
#  error "ION_PLATFORM_CAP must be defined to include glplatformcaps.h"
#endif

ION_PLATFORM_CAP(GLfloat, MinAliasedLineWidth);
ION_PLATFORM_CAP(GLfloat, MaxAliasedLineWidth);
ION_PLATFORM_CAP(GLfloat, MinAliasedPointSize);
ION_PLATFORM_CAP(GLfloat, MaxAliasedPointSize);
ION_PLATFORM_CAP(GLenum, ImplementationColorReadFormat);
ION_PLATFORM_CAP(GLenum, ImplementationColorReadType);
ION_PLATFORM_CAP(GLint, Max3dTextureSize);
ION_PLATFORM_CAP(GLint, MaxArrayTextureLayers);
ION_PLATFORM_CAP(GLuint, MaxClipDistances);
ION_PLATFORM_CAP(GLuint, MaxColorAttachments);
ION_PLATFORM_CAP(GLuint, MaxCombinedTextureImageUnits);
ION_PLATFORM_CAP(GLint, MaxCubeMapTextureSize);
ION_PLATFORM_CAP(GLuint, MaxDrawBuffers);
ION_PLATFORM_CAP(GLuint, MaxFragmentUniformComponents);
ION_PLATFORM_CAP(GLuint, MaxFragmentUniformVectors);
ION_PLATFORM_CAP(GLint, MaxRenderbufferSize);
ION_PLATFORM_CAP(GLuint, MaxSampleMaskWords);
ION_PLATFORM_CAP(GLint, MaxSamples);
ION_PLATFORM_CAP(GLuint, MaxTextureImageUnits);
ION_PLATFORM_CAP(GLfloat, MaxTextureMaxAnisotropy);
ION_PLATFORM_CAP(GLint, MaxTextureSize);
ION_PLATFORM_CAP(GLint, MaxTransformFeedbackBuffers);
ION_PLATFORM_CAP(GLint, MaxTransformFeedbackInterleavedComponents);
ION_PLATFORM_CAP(GLint, MaxTransformFeedbackSeparateAttribs);
ION_PLATFORM_CAP(GLint, MaxTransformFeedbackSeparateComponents);
ION_PLATFORM_CAP(GLuint, MaxVaryingVectors);
ION_PLATFORM_CAP(GLuint, MaxVertexAttribs);
ION_PLATFORM_CAP(GLuint, MaxVertexTextureImageUnits);
ION_PLATFORM_CAP(GLuint, MaxVertexUniformComponents);
ION_PLATFORM_CAP(GLuint, MaxVertexUniformVectors);
ION_PLATFORM_CAP(GLuint, MaxViewportDims);
ION_PLATFORM_CAP(GLint, MaxViews);
ION_PLATFORM_CAP(GLint, TransformFeedbackVaryingMaxLength);
ION_PLATFORM_CAP(GLint, MaxDebugLoggedMessages);
ION_PLATFORM_CAP(GLint, MaxDebugMessageLength);

#undef ION_PLATFORM_CAP

#endif  // ION_GFX_TESTS_GLPLATFORMCAPS_H_

// Allow this file to be included again.
#undef ION_GFX_TESTS_GLPLATFORMCAPS_H_
