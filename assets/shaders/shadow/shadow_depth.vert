#version 450

layout(location = 0) in vec3 inPosition;

layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

// 与 GlobalUniforms（C++ 侧）字段对齐：lightVP 在 time 之后
layout(set = 0, binding = 0) uniform GlobalUniforms {
    mat4 view;
    mat4 proj;
    mat4 invView;
    mat4 invProj;
    float time;
    mat4 lightVP;
} global;

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    gl_Position = global.lightVP * worldPos;
}
