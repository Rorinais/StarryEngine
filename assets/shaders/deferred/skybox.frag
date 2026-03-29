#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(set = 0, binding = 0) uniform GlobalUniforms {
    mat4 view;
    mat4 proj;
    mat4 invView;     
} global;

//layout(set = 1, binding = 0) uniform samplerCube uSkybox;
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

void main() {
    vec2 ndc = vUV * 2.0 - 1.0;
    vec4 clipPos = vec4(ndc, 1.0, 1.0);
    vec4 worldPos = global.invView * clipPos;
    vec3 worldDir = normalize(worldPos.xyz);
    //outColor = texture(uSkybox, worldDir);
    outColor = vec4(1.0);
}