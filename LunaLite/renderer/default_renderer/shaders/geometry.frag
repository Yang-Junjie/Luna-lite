#version 450 core

in vec3 vWorldPos;
in vec3 vNormal;

layout(location = 0) out vec4 gAlbedo;
layout(location = 1) out vec4 gNormal;
layout(location = 2) out vec4 gMaterial;

void main()
{
    gAlbedo = vec4(0.8, 0.65, 0.5, 1.0);
    gNormal = vec4(normalize(vNormal) * 0.5 + 0.5, 1.0);
    gMaterial = vec4(0.5, 0.5, 0.0, 1.0);
}
