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
uniform sampler2D uLastPass;
uniform vec3 uAccumWeights;

// Simple transmittance function. See, e.g.:
// http://www.iryoku.com/translucency/
vec3 T(float s)
{
    return vec3(0.233, 0.455, 0.649) * exp(-s*s/0.0064) +
           vec3(0.1, 0.336, 0.344) * exp(-s*s/0.0484) +
           vec3(0.118, 0.198, 0.0) * exp(-s*s/0.187) +
           vec3(0.113, 0.007, 0.007) * exp(-s*s/0.567) +
           vec3(0.358, 0.004, 0.0) * exp(-s*s/1.99) +
           vec3(0.078, 0.0, 0.0) * exp(-s*s/7.41);
}

void main() {
  // Gaussian weights for a 3-wide 7-tap filter.
  float weight = texture2D(uTexture, vBlurTexCoords[0]).r * 0.005977036247;
  weight += texture2D(uTexture, vBlurTexCoords[1]).r * 0.06059753594;
  weight += texture2D(uTexture, vBlurTexCoords[2]).r * 0.2417303375;
  vec4 center = texture2D(uTexture, vBlurTexCoords[3]);
  weight += center.r * 0.3829249226;
  weight += texture2D(uTexture, vBlurTexCoords[4]).r * 0.2417303375;
  weight += texture2D(uTexture, vBlurTexCoords[5]).r * 0.06059753594,
  weight += texture2D(uTexture, vBlurTexCoords[6]).r * 0.005977036247;

  vec4 last = texture2D(uLastPass, vBlurTexCoords[3]);
  vec3 color = weight * uAccumWeights;
  // Transittance and transmitted distance are in center.ba.
  vec3 trans_color = vec3(0.);
  if (center.a > 1.) {
    vec3 trans = T(center.a);
    float power = 0.05;
    trans_color.r = pow(trans.r, power);
    trans_color.g = pow(trans.g, power);
    trans_color.b = pow(trans.b, power);
    trans_color *= .15 * center.b;
  }
  gl_FragColor.rgb = color + trans_color + last.rgb;
  gl_FragColor.a = 1.;
}
