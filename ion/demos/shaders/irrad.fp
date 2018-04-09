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

varying vec2 vTexCoords;
varying vec4 vShadowPos;
varying vec3 vPosition;

uniform vec3 uLightPos;
uniform sampler2D uNormalMap;
uniform sampler2D uDepthMap;
// x: min depth
// y: inverse depth range
// z: depth range
uniform vec3 uDepthAndRanges;
// x: roughness
// y: rho_s
uniform vec4 uSkinParams;

const float kShadowBias = 0.9;

// Returns how likely this fragment is to be lit using Chebyshev's upper bound,
float Chebyshev(float depth, float depth_sq, float dist)
{
  float variance = depth_sq - depth * depth;
  variance = max(variance, 1e-5);

  float delta = max(dist * .99 - depth, 0.);
  return variance / (variance + delta * delta);
}

// Returns a decoded normal in [-1:1]^3 from an encoded RGB color.
vec3 DecodeNormal(vec3 n) {
  return (n - vec3(0.5)) * 2.;
}

void main() {
  vec3 normal = DecodeNormal(texture2D(uNormalMap, vTexCoords).rgb);
  vec2 moments = texture2DProj(uDepthMap, vShadowPos).rg;

  vec3 to_light = uLightPos - vPosition;
  float dist_to_light = length(to_light);
  to_light /= dist_to_light;

  float lit_dist_to_light = moments.x * uDepthAndRanges.z + uDepthAndRanges.x;

  float scaled_dist = (dist_to_light - uDepthAndRanges.x) * uDepthAndRanges.y;
  float shadow = Chebyshev(moments.x, moments.y, scaled_dist);
  shadow = clamp((shadow - kShadowBias) / (1.0 - kShadowBias), 0.0, 1.0);
  float n_dot_l = dot(to_light, normal);
  shadow *= n_dot_l;

  // Transmittance.
  float trans = clamp(-0.3 - n_dot_l, 0., 1.);
  vec3 shrunken = vPosition - 0.005 * normal - uLightPos;
  float shrunken_dist = length(shrunken);
  float trans_dist = max(shrunken_dist - lit_dist_to_light - 0.5, -0.5) + 0.5;

  gl_FragColor = vec4(shadow, shadow, trans, trans_dist);
}
