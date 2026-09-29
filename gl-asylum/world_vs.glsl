#version 330 core

layout (location = 0) in vec3 p;
layout (location = 1) in vec3 n;
layout (location = 2) in vec2 t;
layout (location = 3) in int  i;

out vec3 v_p;
out vec3 v_n;
out vec2 v_t;

out vec4 v_p_lightspace;

out vec4 v_diffusion_corners;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_lightspace;

void main()
{
  v_p = vec3(u_model * vec4(p, 1.0f));
  v_n = n;
  v_t = t;
  v_p_lightspace = u_lightspace * vec4(v_p, 1.0f);
  gl_Position = u_view * u_model * vec4(p, 1.0f);
}

