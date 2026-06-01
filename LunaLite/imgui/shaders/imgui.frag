#version 450 core
layout(binding = 0) uniform sampler2D uTexture;

in vec2 vertexUV;
in vec4 vertexColor;
out vec4 outColor;

void main()
{
    outColor = vertexColor * texture(uTexture, vertexUV);
}
