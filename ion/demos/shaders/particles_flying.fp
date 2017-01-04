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

varying vec4 v_color;
varying float v_rotation;
varying vec2 v_point_center;

// Intensity texture.
// viewport.x - inverse viewport width
// viewport.y - inverse viewport height
// viewport.z - viewport width / point_size
// viewport.w - viewport height / point_size
uniform vec4 uViewport;
uniform sampler2D uSprite;

void main(void) {
  // Ideally, we would do the following:
  // vec2 center_diff = 2.*(gl_PointCoord - vec2(0.5, 0.5));
  // but some older clients only support GLSL 110, which doesn't have
  // gl_PointCoord.  We can calculate center_diff using gl_FragCoord and
  // the projected center of the point (v_point_center), so long as we
  // know the dimensions of the viewport (which we have as a uniform).

  // The current position on the window in 0..1.
  vec2 local_position = gl_FragCoord.xy * uViewport.xy;
  // The center of the point in window coordinates from 0..1.
  vec2 point_center = 0.5 * v_point_center + vec2(0.5);
  // The difference between the points, converted to 0..1 over the point
  // radius.
  vec2 center_diff = (local_position - point_center) * uViewport.zw;
  mat2 rot =
      mat2(cos(v_rotation), -sin(v_rotation), sin(v_rotation), cos(v_rotation));
  center_diff = rot * (center_diff) + vec2(0.5);

  // Read the intensity of this pixel from the sprite texture.
  float intensity = texture2D(uSprite, center_diff).r;
  vec3 color = intensity * v_color.rgb;
  gl_FragColor = vec4(color, v_color.a);
}
