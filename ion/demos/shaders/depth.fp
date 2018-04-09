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
#extension GL_OES_standard_derivatives : enable
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
#endif

// x: min depth
// y: inverse depth range
uniform vec2 uDepthAndInverseRange;

varying vec2 vTexCoords;
varying vec3 vToLight;

void main() {
  float dist = length(vToLight);
  float depth = (dist - uDepthAndInverseRange.x) * uDepthAndInverseRange.y;
  float depth_sq = depth * depth;

  float dx = dFdx(depth);
  float dy = dFdy(depth);
  depth_sq += 0.25 * (dx * dx + dy * dy);

  // store depths and tex coords
  gl_FragColor.rg = vec2(depth, depth_sq);
}
