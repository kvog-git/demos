#version 330 core

// --------------------------------------------------------------------------------
// Regular polygon shader
//
// ref: https://thndl.com/square-shaped-shaders.html
// ref: https://thebookofshaders.com/07/
//
// SPDX-License-Identifier: 0BSD
// --------------------------------------------------------------------------------

// --------------------------------------------------------------------------------
// Uniforms
// --------------------------------------------------------------------------------
uniform float u_time;
uniform vec2  u_res;
uniform int   u_n;
uniform vec4  u_bg;
uniform vec4  u_fg;

// --------------------------------------------------------------------------------
// Vertex outputs
// --------------------------------------------------------------------------------
in vec2 v_p;
in vec2 v_t;

// --------------------------------------------------------------------------------
// Fragment outputs
// --------------------------------------------------------------------------------
out vec4 f_c;

// --------------------------------------------------------------------------------
// Entry point
// --------------------------------------------------------------------------------

#define PI 3.14159265359

void main()
{
  // Centered with square aspect ratio
  vec2 st      = gl_FragCoord.xy / u_res.xy;
  float aspect = u_res.x / u_res.y;
  if (aspect > 1.0f) { st.x = (st.x - 0.5f) * aspect + 0.5f; }
  else               { st.y = (st.y - 0.5f) / aspect + 0.5f; }

  // Remap [0, 1] to [-1, 1]
  st = st * 2.0f - 1.0f;

  // Angle of pixel from center + rotation
  float a = atan(st.y, st.x) + u_time * 0.2f;

  // Angle of each sector from center
  float r = (PI * 2.0f) / float(u_n);

  // Polar polygon edge test. cos(theta) = dot product.
  float d = cos(floor(0.5f + a / r) * r - a) * length(st);

  f_c = vec4(vec3(mix(u_bg, u_fg, 1.0f - step(0.5f, d))), 1.0f);
}
