#version 330 core

// --------------------------------------------------------------------------------
// Basic subpixel rendering with LCD filtering.
// Assumes R-G-B LCD with no compositor scaling
//
// ref: https://www.shadertoy.com/view/NtVXWc (spinning quad)
//
// SPDX-License-Identifier: 0BSD
// --------------------------------------------------------------------------------

// --------------------------------------------------------------------------------
// Uniforms
// --------------------------------------------------------------------------------
uniform float u_time;
uniform vec2  u_res;
uniform vec2  u_quad;

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

vec2 rotate(vec2 v, float theta)
{
  // ref: https://en.wikipedia.org/wiki/Rotation_matrix
  return mat2(cos(theta), -sin(theta), sin(theta), cos(theta)) * v;
}

float box_d(vec2 v)
{
  return max(v.x, v.y);
}

float spinning_quad(vec2 pos)
{
  pos = rotate(pos, u_time * -0.25f);
  vec2 halfsize = u_quad / 2.0f;
  vec2 q = abs(pos) - halfsize;
  return 1.0f - step(0.0f, box_d(q));
}

vec3 spinning_quad_subpixel(vec2 pos)
{
  vec2 one_third_x = vec2(1.0f / 3.0f, 0.0f);

  float sL = spinning_quad(pos - one_third_x);
  float sC = spinning_quad(pos);
  float sR = spinning_quad(pos + one_third_x);

#if 1
  // 3-tap low pass filter
  float weights[3] = float[3](0.25f, 0.50f, 0.25f);
  float r = weights[1] * sL + weights[0] * sC;
  float g = weights[0] * sL + weights[1] * sC + weights[2] * sR;
  float b = weights[0] * sC + weights[1] * sR;

  r *= (1.0f / 0.75f);
  b *= (1.0f / 0.75f);
#else
  float r = sL;
  float g = sC;
  float b = sR;
#endif

  return vec3(r, g, b);
}

void main()
{
  vec2 cursor = gl_FragCoord.xy - (u_res / 2.0f);

  // top quad
  vec3 c1 = vec3(spinning_quad(cursor - vec2(0, u_res.y / 4.0f)));

  // bottom quad
  vec3 c2 = spinning_quad_subpixel(cursor + vec2(0, u_res.y / 4.0f));

  f_c = vec4(mix(c2, c1, step(u_res.y / 2.0f, gl_FragCoord.y)), 1.0f);
}
