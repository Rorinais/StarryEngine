#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(set = 1, binding = 0) uniform samplerCube uSkybox;

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec3 worldDir;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(uSkybox, worldDir);;
}