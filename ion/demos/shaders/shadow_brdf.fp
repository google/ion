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
varying vec3 vPosition;
varying vec4 vShadowPos;
varying vec2 vTexCoords;
varying vec3 vToCamera;
varying vec3 vToLight;

uniform sampler2D uDiffuse;
uniform sampler2D uDepthMap;
// x: min depth
// y: inverse depth range
// z: depth range
uniform vec3 uDepthAndRanges;
uniform float uExposure;
uniform vec3 uLightPos;
uniform vec2 uInvWindowDims;
// x: roughness
// y: rho_s
uniform vec4 uBrdfParams;

const float kShadowBias = 0.9;

// Returns a value from the Beckman normal distribution given a normal dotted
// with a half vector and a roughness value m.
float Beckmann(float n_dot_h, float m) {
  float alpha = acos(n_dot_h);
  float ta = tan( alpha );
  float val = 1.0 / (m * m * pow(n_dot_h, 4.0)) * exp(-(ta * ta) / (m * m));
  return val;
}

// Returns Schlick's Fresnel approximation, where h is the half vector, v a unit
// vector pointing towards the eye, and F0 is the Fresnel reflectance at normal
// indicence.
float Fresnel(vec3 h, vec3 v, float F0) {
  float base = 1.0 - dot(h, v);
  float exponential = pow(base, 5.0);
  return exponential + F0 * (1.0 - exponential);
}

// Returns the Kelemen and Szirmay-Kalos [2001] BRDF given the light_direction,
// the dot product of the surface normal and light direction, the eye direction,
// a roughness value m, and a overall scalar rho_s.
float Brdf(vec3 normal, vec3 to_light, float n_dot_l, vec3 to_eye, float m,
           float rho_s) {
  float result = 0.0;
  if (n_dot_l > 0.0) {
    vec3 half_vec = normalize(to_light + to_eye);  // Half-way vector.
    float n_dot_h = dot(normal, half_vec);
    float b = Beckmann(n_dot_h, m);
    float fres = Fresnel(half_vec, to_eye, 0.028);
    float specular = max(b * fres / dot(half_vec, half_vec), 0.);
    result = n_dot_l * rho_s * specular;
  }
  return result;
}

// Returns how likely this fragment is to be lit using Chebyshev's upper bound,
float Chebyshev(float depth, float depth_sq, float dist)
{
  float variance = depth_sq - depth * depth;
  variance = max(variance, 1e-5);

  float delta = max(dist * .99 - depth, 0.);
  return variance / (variance + delta * delta);
}

void main() {
  vec3 diffuse = texture2D(uDiffuse, vTexCoords).rgb;
  vec3 normal = normalize(vNormal);

  vec2 moments = texture2DProj(uDepthMap, vShadowPos).rg;

  vec3 to_light = uLightPos - vPosition;
  float dist_to_light = length(to_light);
  to_light /= dist_to_light;

  float scaled_dist = (dist_to_light - uDepthAndRanges.x) * uDepthAndRanges.y;
  float shadow = Chebyshev(moments.x, moments.y, scaled_dist);
  shadow = clamp((shadow - kShadowBias) / (1.0 - kShadowBias), 0.0, 1.0);
  float n_dot_l = max(dot(to_light, normal), 0.);
  shadow *= n_dot_l;

  vec3 color = diffuse * shadow;
  vec3 to_camera = normalize(vToCamera);
  float brdf = Brdf(normal, to_light, n_dot_l, to_camera,
                    uBrdfParams.x, uBrdfParams.y);
  color += vec3(brdf);
  gl_FragColor.rgb =  color * uExposure;
  gl_FragColor.a = 1.;
}
