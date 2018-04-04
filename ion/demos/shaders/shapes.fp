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

#ifdef GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
#endif

varying vec3 vPosition;
varying vec2 vTexCoords;
varying vec3 vNormal;
varying vec3 vToLight0;
varying vec3 vToLight1;

uniform sampler2D uTexture;
uniform samplerCube uCubeMap;
uniform int uUseCubeMap;

void main(void) {
  float kAmbientIntensity = 0.8;
  float kLight0Intensity = 1.0;
  float kLight1Intensity = 0.8;

  vec3 n = normalize(vNormal);
  float diffuse0 = kLight0Intensity * max(0., dot(normalize(vToLight0), n));
  float diffuse1 = kLight1Intensity * max(0., dot(normalize(vToLight1), n));
  float intensity = min(1., kAmbientIntensity + diffuse0 + diffuse1);
  if (uUseCubeMap != 0) {
    gl_FragColor = intensity * textureCube(uCubeMap, vPosition);
  } else {
    gl_FragColor = intensity * texture2D(uTexture, vTexCoords);
  }
}
