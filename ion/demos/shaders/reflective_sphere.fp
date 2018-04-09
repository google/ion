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

varying vec3 vNormal;
varying vec3 vFromEye;
uniform samplerCube uReflectionCubeMap;

void main(void) {
  vec3 cube_coords = reflect(vFromEye, vNormal);
  vec4 reflection = textureCube(uReflectionCubeMap, cube_coords);
  vec4 ambient_color = vec4(0.5, 0.5, 0.5, 1.0);
  gl_FragColor = mix(ambient_color, reflection, 0.4);
}
