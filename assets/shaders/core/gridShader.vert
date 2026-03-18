#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;  // 顶点颜色

layout(location = 0) out vec3 fragColor;

layout(set = 0, binding = 0) uniform GlobalUniforms {
    mat4 view;
    mat4 proj;
} global;

layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

void main() {
    gl_Position = global.proj * global.view * pc.model * vec4(inPosition, 1.0);
    fragColor = inColor;
}