#version 450 core

in vec3 vWorldPos;
in vec3 vNormal;

layout(std140, binding = 1) uniform ObjectUniforms {
    mat4 model;
    mat4 normalMatrix;
    vec4 materialAlbedo;
    vec3 materialEmission;
    float materialEmissionStrength;
    float materialMetallic;
    float materialRoughness;
    uint materialShadingModel;
    float _padObject0;
};

layout(location = 0) out vec4 gAlbedo;
layout(location = 1) out vec4 gNormal;
layout(location = 2) out vec4 gMaterial;

void main()
{
    vec3 albedo = materialAlbedo.rgb + materialEmission * materialEmissionStrength;
    gAlbedo = vec4(albedo, materialAlbedo.a);
    gNormal = vec4(normalize(vNormal) * 0.5 + 0.5, 1.0);
    gMaterial = vec4(materialMetallic, materialRoughness, materialShadingModel == 1u ? 1.0 : 0.0, 1.0);
}
