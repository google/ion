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

varying vec2 vBlurTexCoords[7];

uniform sampler2D uTexture;

void main() {
  // Gaussian weights for a 3-wide 7-tap filter.
  vec2 weights = texture2D(uTexture, vBlurTexCoords[0]).rg * 0.005977036247;
  weights += texture2D(uTexture, vBlurTexCoords[1]).rg * 0.06059753594;
  weights += texture2D(uTexture, vBlurTexCoords[2]).rg * 0.2417303375;
  vec4 center = texture2D(uTexture, vBlurTexCoords[3]);
  weights += center.rg * 0.3829249226;
  weights += texture2D(uTexture, vBlurTexCoords[4]).rg * 0.2417303375;
  weights += texture2D(uTexture, vBlurTexCoords[5]).rg * 0.06059753594;
  weights += texture2D(uTexture, vBlurTexCoords[6]).rg * 0.005977036247;

  gl_FragColor.rg = weights;
  gl_FragColor.ba = center.ba;
}
