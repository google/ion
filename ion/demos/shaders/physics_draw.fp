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

varying vec4 vColor;

void main(void) {
  vec2 pc = 2.0 * (gl_PointCoord - 0.5);
  float r2 = dot(pc, pc);
  float intensity = max(1.0 - r2, 0.0);
  vec3 color = intensity * vColor.rgb;
  gl_FragColor = 0.25 * vec4(color, vColor.a);
}
