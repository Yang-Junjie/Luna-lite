#version 450 core

layout(location = 0) in vec3 a_position;

layout(std140, binding = 0) uniform ShadowFrameUniforms {
    mat4 u_light_view_projection;
};

uniform vec4 uPushConstants[4];

void main()
{
    mat4 model = mat4(uPushConstants[0], uPushConstants[1], uPushConstants[2], uPushConstants[3]);
    gl_Position = u_light_view_projection * model * vec4(a_position, 1.0);
}
