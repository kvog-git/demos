#version 450

layout (location = 0) in vec3 v_p;
layout (location = 1) in vec3 v_c;

layout (location = 0) out vec3 f_c;

void main()
{
	gl_Position = vec4(v_p, 1.0f);
	f_c = v_c;
}

