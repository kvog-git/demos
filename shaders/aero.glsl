#version 330 core

// --------------------------------------------------------------------------------
// Windows 7-style "aero" effect
//
// SPDX-License-Identifier: 0BSD
// --------------------------------------------------------------------------------

// --------------------------------------------------------------------------------
// Uniforms
// --------------------------------------------------------------------------------
uniform float u_time;
uniform vec2  u_res;

uniform int  u_checker;
uniform vec4 u_checker_c1;
uniform vec4 u_checker_c2;

uniform float u_ww;
uniform float u_wh;

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

vec3 from_rgb(int r, int g, int b)
{
  return vec3(float(r) / 255.0f, float(g) / 255.0f, float(b) / 255.0f);
}

vec3 checker(vec2 xy)
{
  vec2 p = floor(xy / u_checker);
  float f = mod(p.x + p.y, 2.0f);
  return mix(u_checker_c1, u_checker_c2, f).xyz;
}

vec3 checker_blurred(vec2 xy)
{
  // 9-point Gaussian kernel
  // Tried Kawase blur but got artifacts since I'm blurring a checkerboard.
  // ref: https://www.rastergrid.com/blog/2010/09/efficient-gaussian-blur-with-linear-sampling
  // Mofified to sample in 8 directions + center
  float weights[3] = float[](0.15217391f, 0.13043478f, 0.08152174f);
  float r = 5.0f;
  vec3 c = checker(xy) * weights[0];
  c += checker(xy + vec2(+r, 0.0f)) * weights[1];
  c += checker(xy + vec2(-r, 0.0f)) * weights[1];
  c += checker(xy + vec2(0.0f, +r)) * weights[1];
  c += checker(xy + vec2(0.0f, -r)) * weights[1];
  c += checker(xy + vec2(+r, +r)) * weights[2];
  c += checker(xy + vec2(-r, +r)) * weights[2];
  c += checker(xy + vec2(+r, -r)) * weights[2];
  c += checker(xy + vec2(-r, -r)) * weights[2];
  return c;
}

vec3 window(vec2 bl, vec2 tr, vec2 xy)
{
  vec3 c1 = from_rgb(174, 199, 222);
  vec3 c2 = from_rgb(227, 237, 246);

  float div = 0.3f;
  float div_edge = tr.y - ((tr.y - bl.y) * div);
  vec3 g1 = mix(c2, c1, 1.0f - (abs(xy.y - tr.y) / ((tr.y - bl.y) * div)));
  vec3 g2 = mix(c2, c1, 1.0f - (abs(xy.y - tr.y) / (tr.y - bl.y)));
  vec3 g = mix(g2, g1, step(div_edge, xy.y));

  vec3 c = g;
  // highlights
  float hbr = 3.0f;
  float hl = 
            step(tr.y - hbr, xy.y) + 
    (1.0f - step(bl.x + hbr, xy.x));
  hl = clamp(hl, 0.0f, 1.0f);
  c += vec3(hl);

  // border
  float br = 1.0f;
  float border = 
    (1.0f - step(bl.x + br, xy.x)) +
            step(tr.x - br, xy.x)  +
    (1.0f - step(bl.y + br, xy.y)) +
            step(tr.y - br, xy.y);
  border = clamp(border, 0.0f, 1.0f);
  c *= (1.0f - border);

  vec3 content = checker_blurred(xy);
  vec2 cbl = bl + vec2(25.0f);
  vec2 ctr = tr - vec2(25.0f);
  float l =        step(cbl.x, xy.x);
  float r = 1.0f - step(ctr.x, xy.x);
  float b =        step(cbl.y, xy.y);
  float t = 1.0f - step(ctr.y, xy.y);
  float hit = l * r * b * t;

  return mix(c, content, hit);
}

void main()
{
  vec2 xy = gl_FragCoord.xy;

  vec2 bl = vec2((u_res.x - u_ww) / 2.0f + sin(u_time) * 500.0f, (u_res.y - u_wh) / 2.0f);
  vec2 tr = bl + vec2(u_ww, u_wh);

  float l =        step(bl.x, xy.x);
  float r = 1.0f - step(tr.x, xy.x);
  float b =        step(bl.y, xy.y);
  float t = 1.0f - step(tr.y, xy.y);
  float hit = l * r * b * t;

  vec3 bg = checker(xy);
  vec3 fg = window(bl, tr, xy);

  f_c = vec4(mix(bg, fg, hit), 1.0f);
}
