#version 450

vec2 points[3] = vec2[]
(
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5)
);

vec3 colors[3] = vec3[] 
(
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

layout(location = 0) out vec3 o_Color;

void main()
{
    gl_Position = vec4(points[gl_VertexIndex], 0.0, 1.0);
    o_Color = colors[gl_VertexIndex];
}