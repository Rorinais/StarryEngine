#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) in vec4 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform ColorBlock {
    vec3 color;
} ubo;

void main() {
    outColor = vec4(ubo.color,1.0);
}