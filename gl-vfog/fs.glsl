#version 330 core

in vec3 f_p;

out vec4 f_color;

// -----------------------------------------------------------------------------
// Uniforms
// -----------------------------------------------------------------------------
uniform vec3  u_cam;
uniform float u_time;
uniform int   u_mode;
uniform float u_sigma;
uniform int   u_steps;

// -----------------------------------------------------------------------------
// Ray-AABB intersection
// ref: https://tavianator.com/2022/ray_box_boundary.html
// -----------------------------------------------------------------------------
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

// -----------------------------------------------------------------------------
// Mode 0: Basic volumetric fog
// -----------------------------------------------------------------------------
float sample_basic(vec3 pos)
{
  return 1.0;
}

// -----------------------------------------------------------------------------
// Mode 1: Fade along y axis
// -----------------------------------------------------------------------------

float sample_vfade(vec3 pos)
{
  return 1.0f - pos.y;
}

// -----------------------------------------------------------------------------
// Mode 2: Some waves
// -----------------------------------------------------------------------------

float sample_waves(vec3 pos)
{
  // Fractal sum
  // ref: https://thebookofshaders.com/13/
  float h = 0.75f; // baseline

  // Big waves
  h += 0.08f * sin(pos.x * 2.0f + u_time);
  h += 0.08f * cos(pos.z * 2.0f + u_time * -1.1f);

  // Medium waves
  h += 0.05f * sin(pos.x * 5.0f + u_time * 1.7f);
  h += 0.05f * cos(pos.z * 4.0f + u_time * -2.0f);

  // Small but choppy waves
  h += 0.03f * sin(pos.x * 12.0f + u_time * 3.3f);
  h += 0.03f * cos(pos.z * 11.0f + u_time * -2.5f);

  return float(pos.y < h);
}

void main()
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

  float density = 0;
  for (int i = 0; i < u_steps; ++i) {
    float t = mix(t_enter, t_exit, (float(i) + 0.5f) / float(u_steps));
    vec3  p = ro + rd * t;
    switch (u_mode) {
      case 0: { density += sample_basic(p); } break;
      case 1: { density += sample_vfade(p); } break;
      case 2: { density += sample_waves(p); } break;
    }
  }
  density /= float(u_steps);

  // Beer-Lambert attenuation
  // ref: https://en.wikipedia.org/wiki/Beer%E2%80%93Lambert_law
  float a = 1.0f - exp(-u_sigma * t_len * density);

  f_color = vec4(vec3(1.0f), a);
}
