#version 460

layout (binding = 0) uniform MVP
{
    mat4 WorldMatrix;
    mat4 ViewMatrix;
    mat4 ProjectionMatrix;
} mvp;
layout (binding = 1) uniform sampler2D BaseTexture;
layout (binding = 2) uniform MateralInfo
{
   vec4 EmissionColor;
   vec4 AmbientColor;
   vec4 DiffuseColor;
   vec4 SpecularColor;
   float SpecularExponent;
} material;
layout (binding = 3) uniform LightInfo
{
   vec4 Position;
   vec4 AmbientColor;
   vec4 DiffuseColor;
   vec4 SpecularColor;
   vec3 AttenuationFactors;
   vec3 SpotlightDirection;
   float SpotlightExponent;
   float SpotlightCutoffAngle;
} light;

layout (location = 0) in vec3 position_in_ec;
layout (location = 1) in vec3 normal_in_ec;
layout (location = 2) in vec2 tex_coord;

layout(location = 0) out vec4 final_color;

void main()
{
    final_color = texture( BaseTexture, tex_coord ).bgra;
}