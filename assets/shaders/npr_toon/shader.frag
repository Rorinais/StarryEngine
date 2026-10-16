#version 450
#extension GL_KHR_vulkan_glsl : enable
#extension GL_GOOGLE_include_directive : enable
#include "common.glsl"

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragWorldPos; 

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

layout(set = 0, binding = 0) uniform GlobalUniforms {
    mat4 view;
    mat4 proj;
} global;

layout(set = 1, binding = 0) uniform sampler2D texSampler;

// ── 宏变体（keyword，CPU setKeywords 注入，与 CPU 契约一致）──
// 每种宏组合 = 一个 shader 变体，只有启用的宏才会编译进这段 sampler 声明。
// 注意：binding 2 被骨骼 SSBO(BoneMatrices) 占用，新槽位避开 2。
#ifdef USE_ILM_MAP
layout(set = 1, binding = 1) uniform sampler2D uIlmMap;
#endif
#ifdef USE_RAMP_MAP
layout(set = 1, binding = 3) uniform sampler2D uRampMap;
#endif
#ifdef USE_MATCAP_MAP
layout(set = 1, binding = 4) uniform sampler2D uMatcapMap;
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
    vec4 ilm = texture(uIlmMap,vec2(fragTexCoord.x,-fragTexCoord.y));
    vec4 ramp = texture(uRampMap,fragTexCoord);

    float ao_mask = ilm.x;
    float diff_mask = ilm.y;
    float specular_mask = ilm.z;


    //shading modle = ambient(const) + diff(halfLambert) + specular(blinn-phong)
    vec3 L = -lighting.lights.position.xyz;
    vec3 V = normalize((inverse(global.view) * vec4(0.0, 0.0, 0.0, 1.0)).xyz - fragWorldPos);

    float diff = halfLambert(fragNormal, L);
    diff =smoothstep(0.5,0.6,diff);
    diff =diff*ao_mask;
    
    vec4 brightColor = vec4(baseColor.xyz * baseColor.xyz, 1.0);
    vec4 darkColor = 0.2*baseColor;
    vec4 diff_color = mix(darkColor,brightColor,diff);

    diff_color = (1-diff_mask)*diff_color+ diff_mask*brightColor;

    float specular = blinnPhong(fragNormal,HalfwayVecter(V,L),5);

    float ks = 2.0;
    specular = specular*specular_mask;
    vec4 specular_color =mix(diff_color,ks*brightColor,specular);

    outColor =specular_color;
    //outColor = toFloat4(specular);
}