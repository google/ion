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
varying vec3 vToCamera;
varying vec3 vToLight;

uniform sampler2D uDiffuse;
uniform sampler2D uNormalMap;
uniform sampler2D uIrradianceMap;
uniform sampler2D uScattered;
// x: min depth
// y: inverse depth range
// z: depth range
uniform vec3 uDepthAndRanges;
uniform float uExposure;
uniform vec3 uLightPos;
// x: roughness
// y: rho_s
uniform vec4 uSkinParams;
uniform vec2 uInvWindowDims;

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


// Returns a decoded normal in [-1:1]^3 from an encoded RGB color.
vec3 DecodeNormal(vec3 n) {
  return (n - vec3(0.5)) * 2.;
}

// Returns a low dynamic range pixel color corresponding to the given high
// dynamic range radiance.
vec3 ToneMapping(vec3 L, float exposure) {
  L = L * exposure;
  L = vec3(1.0) - exp(-L);
  return L;
}

void main() {
  vec3 diffuse = texture2D(uDiffuse, vTexCoords).rgb;
  vec3 normal = DecodeNormal(texture2D(uNormalMap, vTexCoords).rgb);
  vec2 coords = gl_FragCoord.xy * uInvWindowDims;
  float irrad = texture2D(uIrradianceMap, coords).r;
  coords.y = 1. - coords.y;
  vec3 scattered = texture2D(uScattered, coords).rgb;

  vec3 to_light = normalize(vToLight);
  float n_dot_l = max(dot(to_light, normal), 0.);
  vec3 color = diffuse * scattered;

  if (irrad > 0.) {
    vec3 to_camera = normalize(vToCamera);
    float brdf = Brdf(normal, to_light, n_dot_l, to_camera,
                      uSkinParams.x, uSkinParams.y);
    color += vec3(brdf);
  }

  gl_FragColor.rgb = ToneMapping(color, uExposure);
  gl_FragColor.a = 1.;
}
