#version 450
#extension GL_KHR_vulkan_glsl : enable
#extension GL_GOOGLE_include_directive : enable
#include "common.glsl"

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D texSampler;
#ifdef USE_SDF_MAP
layout(set = 1, binding = 1) uniform sampler2D uSdfMap;
#endif
#ifdef USE_MASK_MAP
layout(set = 1, binding = 3) uniform sampler2D uMaskMap;
#endif

struct Light {
    vec4 position;  
    vec4 color;     
};

layout(set = 1, binding = 4) uniform LightingUBO {
    Light lights;
    uint lightCount;
    float ambientStrength;
} lighting;

void main() {
    vec4 baseColor = texture(texSampler,fragTexCoord);
    vec4 sdf = texture(uSdfMap,vec2(fragTexCoord.x,-fragTexCoord.y));
    vec4 mask = texture(uMaskMap,vec2(fragTexCoord.x,-fragTexCoord.y));


    outColor = baseColor;
}