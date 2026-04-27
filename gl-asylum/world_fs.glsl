#version 330 core

in vec3 v_p;
in vec3 v_n;
in vec2 v_t;

in vec4 v_p_lightspace;

out vec4 f_color;

uniform vec3      u_cam;
uniform float     u_fog_start;
uniform float     u_fog_end;
uniform vec3      u_fog_color;
uniform vec3      u_sun;

uniform sampler2D u_diffuse;
uniform sampler2D u_shadow_depth;

uniform vec4 u_diffusion;

float calc_shadow(vec4 p_lightspace) {
  // ref: https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping
  vec3 pcoord = (p_lightspace.xyz / p_lightspace.w) * 0.5f + 0.5f;
  float closest_depth = texture(u_shadow_depth, pcoord.xy).r;
  float current_depth = pcoord.z;
  return current_depth - 0.0005f > closest_depth ? 1.0 : 0.0;
}

float calc_diffuse(vec4 a, vec2 uv)
{
  float top    = mix(a.x, a.y, uv.x);
  float bottom = mix(a.w, a.z, uv.x);
  return mix(bottom, top, uv.y);
}

void main()
{
  vec4 diffuse = texture(u_diffuse, v_t);

  float shadow = 1.0 - calc_shadow(v_p_lightspace);
  vec3 sun_dir = normalize(u_sun);
  if (dot(sun_dir, v_n) < 0) {
    shadow = 0.0f;
  }

  //shadow -= dot()
  //float shadow = 1.0 - calc_shadow2(v_p_lightspace, v_n, normalize(v_p - u_cam));
  shadow *= 0.25;
  shadow += 0.75;
  diffuse.r *= shadow;
  diffuse.g *= shadow;
  diffuse.b *= shadow;

  float diff = calc_diffuse(u_diffusion, v_t);
  if (v_p.y > 0) {
    diff = clamp(1.0f - v_p.y, 0.0f, 1.0f);
  }
  diff = pow(diff, 2.0f) * 0.25f;
  diffuse.rgb -= diff;

  float d   = distance(v_p, u_cam);
  float fog = clamp((d - u_fog_start) / max(u_fog_end - u_fog_start, 1e-6), 0.0f, 1.0f);

  vec3 rgb = mix(diffuse.rgb, u_fog_color, fog);
  f_color = vec4(rgb, 1.0f);
}
