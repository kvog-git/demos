#version 330 core

in vec3 f_p;

out vec4 f_color;

uniform vec3  u_cam;
uniform float u_time;
uniform int   u_mode;
uniform float u_sigma;

// Ray-AABB intersection
// ref: https://tavianator.com/2022/ray_box_boundary.html
bool intersect(vec3 ro, vec3 rd, vec3 bbmin, vec3 bbmax, out float t0, out float t1)
{
  vec3 inv = 1.0f / rd;
  vec3 ta = (bbmin - ro) * inv;
  vec3 tb = (bbmax - ro) * inv;
  vec3 tmin = min(ta, tb);
  vec3 tmax = max(ta, tb);
  t0 = max(max(tmin.x, tmin.y), tmin.z);
  t1 = min(min(tmax.x, tmax.y), tmax.z);
  return t1 >= max(t0, 0.0f);
}

// Mode 0: Basic
void main_0()
{
  vec3 bbmin = vec3(0.0f, 0.0f, 0.0f);
  vec3 bbmax = vec3(1.0f, 1.0f, 1.0f);

  vec3 ro = u_cam;
  vec3 rd = normalize(f_p - ro);

  float t0 = 0.0f;
  float t1 = 0.0f;
  if (!intersect(ro, rd, bbmin, bbmax, t0, t1)) {
    f_color = vec4(0.0f, 0.0f, 0.0f, 0.0f);
    return;
  }

  // Clamp entry point in front of camera
  float t_enter = max(t0, 0.0f);
  float t_exit  = t1;
  float t_len   = max(0.0f, t_exit - t_enter);

  // Beer-Lambert attenuation
  // ref: https://en.wikipedia.org/wiki/Beer%E2%80%93Lambert_law
  float a = 1.0f - exp(-u_sigma * t_len);

  f_color = vec4(vec3(1.0f), a);
}

void main()
{
  switch (u_mode) {
  case 0: { main_0(); } break;
  default: { f_color = vec4(0.0f, 1.0f, 0.0f, 1.0f); } break;
  }
}
