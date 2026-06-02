#version 450 core

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

layout(binding = 1) uniform sampler2D gAlbedoTexture;
layout(binding = 2) uniform sampler2D gNormalTexture;
layout(binding = 3) uniform sampler2D gMaterialTexture;
layout(binding = 4) uniform sampler2D gDepthTexture;
layout(binding = 5) uniform sampler2D gEmissionTexture;

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

void main()
{
    vec3 baseColor = texture(gAlbedoTexture, vUV).rgb;
    vec3 emission = texture(gEmissionTexture, vUV).rgb;
    vec3 N = normalize(texture(gNormalTexture, vUV).rgb * 2.0 - 1.0);
    vec4 material = texture(gMaterialTexture, vUV);
    float depth = texture(gDepthTexture, vUV).r;
    vec3 worldPos = reconstructWorldPosition(vUV, depth);
    vec3 V = normalize(cameraPos - worldPos);

    float metallic = saturate(material.r);
    float roughness = clamp(material.g, 0.04, 1.0);
    float occlusion = material.a;

    if (material.b > 0.5) {
        vec3 unlitColor = toneMapACES(max(baseColor + emission, vec3(0.0)) * exposure);
        unlitColor = pow(unlitColor, vec3(1.0 / 2.2));
        outColor = vec4(unlitColor, 1.0);
        return;
    }

    vec3 F0 = mix(vec3(0.04), baseColor, metallic);
    vec3 diffuseColor = baseColor * (1.0 - metallic);
    vec3 color = (environmentAmbient + lightAmbient) * diffuseColor * occlusion;

    if (directionalLightCount > 0u) {
        vec3 L = normalize(-lightDir);
        vec3 H = normalize(V + L);
        float NoV = saturate(dot(N, V));
        float NoL = saturate(dot(N, L));
        float NoH = saturate(dot(N, H));
        float VoH = saturate(dot(V, H));

        float D = distributionGGX(NoH, roughness);
        float G = geometrySmith(NoV, NoL, roughness);
        vec3 F = fresnelSchlick(VoH, F0);
        vec3 diffuseBRDF = diffuseColor / PI;
        vec3 specularBRDF = (D * G * F) / max(4.0 * NoV * NoL, 1e-6);

        vec3 directDiffuse = diffuseBRDF * lightDiffuse;
        vec3 directSpecular = specularBRDF * lightDiffuse * lightSpecular;
        color += (directDiffuse + directSpecular) * NoL;
    }

    color += emission;
    color = toneMapACES(max(color, vec3(0.0)) * exposure);
    color = pow(color, vec3(1.0 / 2.2));
    outColor = vec4(color, 1.0);
}
