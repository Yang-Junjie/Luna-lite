#version 450 core

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

layout(binding = 16) uniform samplerCube uEnvironmentMap;

in vec2 vUV;
out vec4 outColor;

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

vec3 skyDirection(vec2 uv)
{
    vec4 clipPosition = vec4(uv * 2.0 - 1.0, 1.0, 1.0);
    vec4 worldPosition = inverseViewProjection * clipPosition;
    float invW = abs(worldPosition.w) > 1e-6 ? 1.0 / worldPosition.w : 1.0;
    return normalize(worldPosition.xyz * invW - cameraPos);
}

void main()
{
    vec3 direction = skyDirection(vUV);
    vec3 color = textureLod(uEnvironmentMap, direction, 0.0).rgb * environmentIntensity;
    outColor = vec4(toDisplayColor(color), 1.0);
}
