#version 450 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

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
    mat4 inverseViewProjection;
    float exposure;
    float _pad9;
    float _pad10;
    float _pad11;
};

layout(std140, binding = 1) uniform ObjectUniforms {
    mat4 model;
    mat4 normalMatrix;
    vec4 materialAlbedo;
    vec3 materialEmission;
    float materialEmissionStrength;
    float materialMetallic;
    float materialRoughness;
    uint materialShadingModel;
    float materialNormalScale;
    float materialOcclusionStrength;
    float _padObject0;
    float _padObject1;
};

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;

void main()
{
    vec4 worldPosition = model * vec4(aPosition, 1.0);
    vWorldPos = worldPosition.xyz;
    vNormal = normalize(mat3(normalMatrix) * aNormal);
    vUV = aUV;
    gl_Position = projection * view * worldPosition;
}
