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
};

layout(binding = 1) uniform sampler2D gAlbedoTexture;
layout(binding = 2) uniform sampler2D gNormalTexture;
layout(binding = 3) uniform sampler2D gMaterialTexture;
layout(binding = 4) uniform sampler2D gDepthTexture;

in vec2 vUV;
out vec4 outColor;

void main()
{
    vec3 albedo = texture(gAlbedoTexture, vUV).rgb;
    vec3 normal = normalize(texture(gNormalTexture, vUV).rgb * 2.0 - 1.0);
    vec3 material = texture(gMaterialTexture, vUV).rgb;

    if (material.b > 0.5) {
        vec3 unlitColor = pow(max(albedo, vec3(0.0)), vec3(1.0 / 2.2));
        outColor = vec4(unlitColor, 1.0);
        return;
    }

    vec3 color = environmentAmbient * albedo;
    if (directionalLightCount > 0u) {
        vec3 L = normalize(-lightDir);
        float diff = max(dot(normal, L), 0.0);

        vec3 ambient = lightAmbient * albedo;
        vec3 diffuse = lightDiffuse * diff * albedo;
        color += ambient + diffuse;
    }

    color = pow(max(color, vec3(0.0)), vec3(1.0 / 2.2));
    outColor = vec4(color, 1.0);
}
