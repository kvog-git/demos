#version 450

layout (location = 0) in vec3 f_c;

layout (location = 0) out vec4 f_out_color;

void main()
{
	f_out_color = vec4(f_c, 1.0f);
}

