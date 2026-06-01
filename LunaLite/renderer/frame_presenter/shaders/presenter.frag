#version 450 core

layout(binding = 0) uniform sampler2D uFrameTexture;

in vec2 vUV;
out vec4 outColor;

void main()
{
    outColor = texture(uFrameTexture, vUV);
}
