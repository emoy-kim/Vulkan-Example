#version 460

layout(binding = 0) uniform UniformBufferObject
{
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_tex_coord;

layout(location = 0) out vec2 tex_coord;

void main()
{
    tex_coord = v_tex_coord;

    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(v_position, 1.0);
}