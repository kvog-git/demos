#version 330 core

layout (location = 0) in vec3 v_p;

out vec3 f_p;

uniform mat4 u_view;

void main() {
  f_p = v_p;
  gl_Position = u_view * vec4(v_p, 1.0f);
}