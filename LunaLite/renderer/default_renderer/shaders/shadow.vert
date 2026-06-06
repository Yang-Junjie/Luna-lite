#version 450 core

layout(location = 0) in vec3 a_position;

layout(std140, binding = 0) uniform ShadowFrameUniforms {
    mat4 u_light_view_projection;
};

layout(std140, binding = 1) uniform ShadowObjectUniforms {
    mat4 u_model;
};

void main()
{
    gl_Position = u_light_view_projection * u_model * vec4(a_position, 1.0);
}
