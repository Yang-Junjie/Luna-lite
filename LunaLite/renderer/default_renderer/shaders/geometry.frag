#version 450 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vTangent;
in vec3 vBitangent;
in vec2 vUV;

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

layout(binding = 16) uniform sampler2D uAlbedoTexture;
layout(binding = 17) uniform sampler2D uNormalTexture;
layout(binding = 18) uniform sampler2D uMetallicRoughnessTexture;
layout(binding = 19) uniform sampler2D uOcclusionTexture;
layout(binding = 20) uniform sampler2D uEmissionTexture;

layout(location = 0) out vec4 gAlbedo;
layout(location = 1) out vec4 gNormal;
layout(location = 2) out vec4 gMaterial;
layout(location = 3) out vec4 gEmission;

vec3 applyNormalMap(vec3 normal, vec3 tangent, vec3 bitangent, vec2 uv)
{
    vec3 tangentNormal = texture(uNormalTexture, uv).xyz * 2.0 - 1.0;
    vec3 N = normalize(normal);

    vec3 T = tangent - N * dot(N, tangent);
    tangentNormal.xy *= materialNormalScale;

    T = normalize(T);
    vec3 B = bitangent - N * dot(N, bitangent);
    float handedness = dot(cross(N, T), B) < 0.0 ? -1.0 : 1.0;
    B = normalize(cross(N, T) * handedness);
    return normalize(mat3(T, B, N) * tangentNormal);
}

void main()
{
    vec4 albedoSample = texture(uAlbedoTexture, vUV);
    vec4 metallicRoughnessSample = texture(uMetallicRoughnessTexture, vUV);
    float occlusionSample = texture(uOcclusionTexture, vUV).r;
    vec3 emissionSample = texture(uEmissionTexture, vUV).rgb;

    vec3 albedo = materialAlbedo.rgb * albedoSample.rgb;
    vec3 emission = materialEmission * emissionSample * materialEmissionStrength;
    float metallic = clamp(materialMetallic * metallicRoughnessSample.b, 0.0, 1.0);
    float roughness = clamp(materialRoughness * metallicRoughnessSample.g, 0.0, 1.0);
    float occlusionStrength = clamp(materialOcclusionStrength, 0.0, 1.0);
    float occlusion = mix(1.0, clamp(occlusionSample, 0.0, 1.0), occlusionStrength);
    vec3 normal =
        materialHasNormalMap != 0u && materialNormalScale > 0.0 ? applyNormalMap(vNormal, vTangent, vBitangent, vUV)
                                                                : normalize(vNormal);

    gAlbedo = vec4(albedo, 1.0);
    gNormal = vec4(normal * 0.5 + 0.5, 1.0);
    gMaterial = vec4(metallic, roughness, materialShadingModel == 1u ? 1.0 : 0.0, occlusion);
    gEmission = vec4(emission, 1.0);
}
