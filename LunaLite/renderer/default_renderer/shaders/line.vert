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
    vec3 lightRadiance;
    float _pad2;
    uint directionalLightCount;
    float environmentIntensity;
    float _pad3;
    float _pad4;
    mat4 inverseViewProjection;
    float exposure;
    float _pad5;
    float _pad6;
    float _pad7;
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
