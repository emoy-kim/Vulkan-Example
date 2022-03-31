#version 460

layout (binding = 1) uniform sampler2D BaseTexture;
layout (binding = 2) uniform MateralInfo
{
   vec4 EmissionColor;
   vec4 AmbientColor;
   vec4 DiffuseColor;
   vec4 SpecularColor;
   float SpecularExponent;
};

layout(location = 0) in vec2 tex_coord;

layout(location = 0) out vec4 final_color;

void main()
{
    final_color = texture( BaseTexture, tex_coord ).bgra;
}