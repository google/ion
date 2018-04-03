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

varying vec3 vPosition;
varying vec3 vNormal;
varying vec3 vToLight;
varying vec3 vDiffuseColor;

void main() {
  float kAmbientIntensity = 0.3;
  float kLightIntensity = 1.0;

  vec3 n = normalize(vNormal);
  float diffuse = kLightIntensity * max(0., dot(normalize(vToLight), n));
  gl_FragColor.xyz = vDiffuseColor * min(1.0, kAmbientIntensity + diffuse);
}
