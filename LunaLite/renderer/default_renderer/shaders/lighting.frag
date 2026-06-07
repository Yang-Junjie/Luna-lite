#version 450 core

layout(std140, binding = 0) uniform FrameUniforms
{
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
    float maxPrefilterMip;
    float _pad4;
    mat4 inverseViewProjection;
    float exposure;
    float _pad5;
    float _pad6;
    float _pad7;
};

layout(binding = 1) uniform sampler2D gAlbedoTexture;
layout(binding = 2) uniform sampler2D gNormalTexture;
layout(binding = 3) uniform sampler2D gMaterialTexture;
layout(binding = 4) uniform sampler2D gDepthTexture;
layout(binding = 5) uniform sampler2D gEmissionTexture;
layout(binding = 17) uniform samplerCube uIrradianceMap;
layout(binding = 18) uniform samplerCube uPrefilterMap;
layout(binding = 19) uniform sampler2D uBrdfLut;

layout(std140, binding = 32) uniform ShadowLightingUniforms
{
    mat4 shadowLightViewProjections[4];
    vec4 shadowCascadeSplits;
    vec4 shadowTexelSizeBiasNormalBias;
    vec4 shadowCascadeBlendPadding;
    uvec4 shadowEnabledPcfRadiusCascadeCount;
};

layout(binding = 33) uniform sampler2DArray uShadowMap;

in vec2 vUV;
out vec4 outColor;

const float PI = 3.14159265359;

float saturate(float value)
{
    return clamp(value, 0.0, 1.0);
}

vec3 reconstructWorldPosition(vec2 uv, float depth)
{
    vec4 clipPosition = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 worldPosition = inverseViewProjection * clipPosition;
    float invW = abs(worldPosition.w) > 1e-6 ? 1.0 / worldPosition.w : 1.0;
    return worldPosition.xyz * invW;
}

uint selectShadowCascade(float viewDepth)
{
    uint cascadeCount = clamp(shadowEnabledPcfRadiusCascadeCount.z, 1u, 4u);
    uint cascadeIndex = 0u;
    for (uint i = 0u; i + 1u < cascadeCount; ++i) {
        if (viewDepth > shadowCascadeSplits[i]) {
            cascadeIndex = i + 1u;
        }
    }

    return cascadeIndex;
}

float sampleDirectionalShadowCascade(vec3 shadowWorldPos, uint cascadeIndex)
{
    vec4 lightClip = shadowLightViewProjections[cascadeIndex] * vec4(shadowWorldPos, 1.0);
    if (abs(lightClip.w) <= 1e-6) {
        return 1.0;
    }

    vec3 shadowCoord = lightClip.xyz / lightClip.w;
    shadowCoord = shadowCoord * 0.5 + 0.5;
    if (shadowCoord.x < 0.0 || shadowCoord.x > 1.0 || shadowCoord.y < 0.0 || shadowCoord.y > 1.0 ||
        shadowCoord.z < 0.0 || shadowCoord.z > 1.0) {
        return 1.0;
    }

    vec2 texelSize = shadowTexelSizeBiasNormalBias.xy;
    float receiverDepth = shadowCoord.z - shadowTexelSizeBiasNormalBias.z;
    int radius = int(min(shadowEnabledPcfRadiusCascadeCount.y, 4u));
    float lit = 0.0;
    float samples = 0.0;
    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            vec2 sampleUV = shadowCoord.xy + offset;
            if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0) {
                lit += 1.0;
                samples += 1.0;
                continue;
            }

            float closestDepth = texture(uShadowMap, vec3(sampleUV, float(cascadeIndex))).r;
            lit += receiverDepth <= closestDepth ? 1.0 : 0.0;
            samples += 1.0;
        }
    }

    return samples > 0.0 ? lit / samples : 1.0;
}

float directionalShadowFactor(vec3 worldPos, vec3 N, vec3 L, float viewDepth)
{
    if (shadowEnabledPcfRadiusCascadeCount.x == 0u || shadowEnabledPcfRadiusCascadeCount.z == 0u) {
        return 1.0;
    }

    uint cascadeCount = clamp(shadowEnabledPcfRadiusCascadeCount.z, 1u, 4u);
    uint cascadeIndex = selectShadowCascade(viewDepth);
    float NoL = saturate(dot(N, L));
    float normalOffset = shadowTexelSizeBiasNormalBias.w * (1.0 - NoL);
    vec3 shadowWorldPos = worldPos + N * normalOffset;
    float shadow = sampleDirectionalShadowCascade(shadowWorldPos, cascadeIndex);

    float blendDistance = max(shadowCascadeBlendPadding.x, 0.0);
    if (blendDistance > 0.0 && cascadeIndex + 1u < cascadeCount) {
        float splitDistance = shadowCascadeSplits[cascadeIndex];
        float blendStart = splitDistance - blendDistance;
        float blendWeight = saturate((viewDepth - blendStart) / blendDistance);
        if (blendWeight > 0.0) {
            float nextShadow = sampleDirectionalShadowCascade(shadowWorldPos, cascadeIndex + 1u);
            shadow = mix(shadow, nextShadow, blendWeight);
        }
    }

    return shadow;
}

float distributionGGX(float NoH, float roughness)
{
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denom = NoH * NoH * (alpha2 - 1.0) + 1.0;
    return alpha2 / max(PI * denom * denom, 1e-6);
}

float geometrySchlickGGX(float NoX, float roughness)
{
    float remappedRoughness = roughness + 1.0;
    float k = (remappedRoughness * remappedRoughness) / 8.0;
    return NoX / max(NoX * (1.0 - k) + k, 1e-6);
}

float geometrySmith(float NoV, float NoL, float roughness)
{
    return geometrySchlickGGX(NoV, roughness) * geometrySchlickGGX(NoL, roughness);
}

vec3 fresnelSchlick(float VoH, vec3 F0)
{
    float Fc = pow(1.0 - VoH, 5.0);
    return F0 + (vec3(1.0) - F0) * Fc;
}

vec3 toneMapACES(vec3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

vec3 toDisplayColor(vec3 color)
{
    color = toneMapACES(max(color, vec3(0.0)) * exposure);
    return pow(color, vec3(1.0 / 2.2));
}

void main()
{
    vec4 albedoSample = texture(gAlbedoTexture, vUV);
    vec3 baseColor = albedoSample.rgb;
    vec3 emission = texture(gEmissionTexture, vUV).rgb;
    vec3 N = normalize(texture(gNormalTexture, vUV).rgb * 2.0 - 1.0);
    vec4 material = texture(gMaterialTexture, vUV);
    float depth = texture(gDepthTexture, vUV).r;

    if (albedoSample.a < 0.5) {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec3 worldPos = reconstructWorldPosition(vUV, depth);
    float viewDepth = max(-(view * vec4(worldPos, 1.0)).z, 0.0);
    vec3 V = normalize(cameraPos - worldPos);
    float NoV = saturate(dot(N, V));

    float metallic = saturate(material.r);
    float roughness = clamp(material.g, 0.04, 1.0);
    float occlusion = material.a;

    if (material.b > 0.5) {
        outColor = vec4(toDisplayColor(baseColor + emission), 1.0);
        return;
    }

    vec3 F0 = mix(vec3(0.04), baseColor, metallic);
    vec3 diffuseColor = baseColor * (1.0 - metallic);
    vec3 color = vec3(0.0);

    vec3 irradiance = texture(uIrradianceMap, N).rgb * environmentIntensity;
    vec3 diffuseIBL = irradiance * diffuseColor;
    vec3 reflection = reflect(-V, N);
    float prefilterLod = clamp(roughness * maxPrefilterMip, 0.0, maxPrefilterMip);
    vec3 prefilteredColor = textureLod(uPrefilterMap, reflection, prefilterLod).rgb * environmentIntensity;
    vec2 brdf = texture(uBrdfLut, vec2(NoV, roughness)).rg;
    vec3 specularIBL = prefilteredColor * (F0 * brdf.x + brdf.y);
    color += (diffuseIBL + specularIBL) * occlusion;

    if (directionalLightCount > 0u) {
        vec3 L = normalize(-lightDir);
        vec3 H = normalize(V + L);
        float NoL = saturate(dot(N, L));
        float NoH = saturate(dot(N, H));
        float VoH = saturate(dot(V, H));

        float D = distributionGGX(NoH, roughness);
        float G = geometrySmith(NoV, NoL, roughness);
        vec3 F = fresnelSchlick(VoH, F0);
        vec3 diffuseBRDF = diffuseColor / PI;
        vec3 specularBRDF = (D * G * F) / max(4.0 * NoV * NoL, 1e-6);

        vec3 directDiffuse = diffuseBRDF * lightRadiance;
        vec3 directSpecular = specularBRDF * lightRadiance;
        float shadowFactor = directionalShadowFactor(worldPos, N, L, viewDepth);
        color += (directDiffuse + directSpecular) * NoL * shadowFactor;
    }

    color += emission;
    outColor = vec4(toDisplayColor(color), 1.0);
}
