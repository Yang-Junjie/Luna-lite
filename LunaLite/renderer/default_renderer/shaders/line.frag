#version 450 core

in vec3 vColor;

layout(location = 0) out vec4 gAlbedo;
layout(location = 1) out vec4 gNormal;
layout(location = 2) out vec4 gMaterial;
layout(location = 3) out vec4 gEmission;

void main()
{
    vec3 normal = vec3(0.0, 1.0, 0.0);
    gAlbedo = vec4(vColor, 1.0);
    gNormal = vec4(normal * 0.5 + 0.5, 1.0);
    gMaterial = vec4(0.0, 0.0, 1.0, 1.0);
    gEmission = vec4(0.0, 0.0, 0.0, 1.0);
}
