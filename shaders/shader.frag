#version 460

layout (location = 0) out vec4 final_color;

layout (push_constant) uniform Push
{
   mat2 Transform;
   vec2 Offset;
   vec3 Color;
} push;

void main()
{
   final_color = vec4(push.Color, 1.0f);
}