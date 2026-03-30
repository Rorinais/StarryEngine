#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(set = 1, binding = 0) uniform samplerCube uSkybox;
layout(input_attachment_index = 0, set = 1, binding = 1) uniform subpassInput inputLightAccum;

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec3 worldDir;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 lightColor = subpassLoad(inputLightAccum);
    vec4 skyColor = texture(uSkybox, worldDir);
    
    vec3 final = mix(skyColor.rgb,lightColor.rgb,lightColor.a);  
    outColor = vec4(final, 1.0);
}