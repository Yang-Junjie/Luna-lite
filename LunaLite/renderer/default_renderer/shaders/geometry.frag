#version 450 core

in vec3 vWorldPos;
in vec3 vNormal;
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
    float _padObject0;
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

vec3 applyNormalMap(vec3 normal, vec3 worldPos, vec2 uv)
{
    vec3 tangentNormal = texture(uNormalTexture, uv).xyz * 2.0 - 1.0;
    vec3 N = normalize(normal);

    vec3 dp1 = dFdx(worldPos);
    vec3 dp2 = dFdy(worldPos);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);
    float det = duv1.x * duv2.y - duv1.y * duv2.x;
    if (abs(det) < 1e-12) {
        return N;
    }

    tangentNormal.xy *= materialNormalScale;
    vec3 T = normalize((dp1 * duv2.y - dp2 * duv1.y) / det);
    T = T - N * dot(N, T);
    if (dot(T, T) < 1e-12) {
        return N;
    }

    T = normalize(T);
    vec3 B = normalize(cross(N, T) * sign(det));
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
    vec3 normal = applyNormalMap(vNormal, vWorldPos, vUV);

    gAlbedo = vec4(albedo, 1.0);
    gNormal = vec4(normal * 0.5 + 0.5, 1.0);
    gMaterial = vec4(metallic, roughness, materialShadingModel == 1u ? 1.0 : 0.0, occlusion);
    gEmission = vec4(emission, 1.0);
}
