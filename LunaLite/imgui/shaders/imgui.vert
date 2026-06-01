#version 450 core
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inColor;

uniform vec4 uPushConstants[4];

out vec2 vertexUV;
out vec4 vertexColor;

void main()
{
    mat4 projection = mat4(uPushConstants[0], uPushConstants[1], uPushConstants[2], uPushConstants[3]);
    gl_Position = projection * vec4(inPosition, 0.0, 1.0);
    vertexUV = inUV;
    vertexColor = inColor;
}
