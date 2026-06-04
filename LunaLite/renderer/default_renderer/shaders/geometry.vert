#version 450 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in vec3 aBitangent;

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
    vec4 materialAlbedo;
    vec3 materialEmission;
    float materialEmissionStrength;
    float materialMetallic;
    float materialRoughness;
    uint materialShadingModel;
    float materialNormalScale;
    float materialOcclusionStrength;
    uint materialHasNormalMap;
    float _padObject1;
};

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vTangent;
out vec3 vBitangent;
out vec2 vUV;

void main()
{
    vec4 worldPosition = model * vec4(aPosition, 1.0);
    vWorldPos = worldPosition.xyz;
    vNormal = normalize(mat3(normalMatrix) * aNormal);
    vTangent = mat3(model) * aTangent;
    vBitangent = mat3(model) * aBitangent;
    vUV = aUV;
    gl_Position = projection * view * worldPosition;
}
