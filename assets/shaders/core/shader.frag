#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec4 outColor;


void main() {
    outColor = vec4(inNormal,1.0);
}