#version 450 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aColor;

layout(std140, binding = 0) uniform FrameUniforms {
    mat4 view;
    mat4 projection;
    vec3 cameraPos;
    float _pad0;
    vec3 lightDir;
    float _pad1;
    vec3 lightAmbient;
    float _pad2;
    vec3 lightDiffuse;
    float _pad3;
    vec3 lightSpecular;
    float _pad4;
    uint directionalLightCount;
    float _pad5;
    float _pad6;
    float _pad7;
    vec3 environmentAmbient;
    float _pad8;
};

layout(std140, binding = 1) uniform ObjectUniforms {
    mat4 model;
    mat4 normalMatrix;
};

out vec3 vColor;

void main()
{
    vColor = aColor;
    gl_Position = projection * view * model * vec4(aPosition, 1.0);
}
