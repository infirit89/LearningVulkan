#version 450

layout(location = 0) in vec3 i_Color;

layout(location = 0) out vec4 o_Color;

void main() 
{
    o_Color = vec4(i_Color, 1.0);
}