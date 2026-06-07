#version 450 core

layout(binding = 16) uniform sampler2D uSpriteTexture;

in vec2 vUV;
in vec4 vColor;

layout(location = 0) out vec4 outColor;

vec3 linearToDisplay(vec3 color)
{
    return pow(max(color, vec3(0.0)), vec3(1.0 / 2.2));
}

void main()
{
    vec4 color = texture(uSpriteTexture, vUV) * vColor;
    outColor = vec4(linearToDisplay(color.rgb), color.a);
}
