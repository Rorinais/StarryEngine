#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(set = 0, binding = 0) uniform GlobalUniforms {
    mat4 view;
    mat4 proj;
    mat4 invView;
    mat4 invProj;
} global;

layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

void main() {
    gl_Position = global.proj * global.view * pc.model * vec4(inPosition, 1.0);
}