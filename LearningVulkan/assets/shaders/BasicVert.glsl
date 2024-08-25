#version 450

layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec3 a_Color;

layout(binding = 0) uniform Camera {
    mat4 Projection;
    mat4 View;
    mat4 Model;
} camera;

layout(location = 0) out vec3 o_Color;

void main()
{
    vec4 position = vec4(a_Position, 0.0, 1.0);
    gl_Position = camera.Projection * camera.View * camera.Model * position;
    o_Color = a_Color;
}