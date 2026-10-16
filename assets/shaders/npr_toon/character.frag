#version 450
#extension GL_KHR_vulkan_glsl : enable
#extension GL_GOOGLE_include_directive : enable
#include "common.glsl"

// ── 角色通用片元（宏变体裁剪贴图绑定）──
// CPU 端通过 DefaultMaterialTemplate::setMacros 注入：
//   USE_NORMAL_MAP   → 启用法线贴图（binding 1）
//   USE_RIM          → 启用边缘光贴图/遮罩（binding 2）
// 每种宏组合 = 一个独立 shader 变体 + 独立描述符布局 → 材质只绑实际用的贴图。

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D uAlbedo;
#ifdef USE_NORMAL_MAP
layout(set = 1, binding = 1) uniform sampler2D uNormalMap;   // 切线空间法线
#endif
#ifdef USE_RIM
layout(set = 1, binding = 2) uniform sampler2D uRimMask;     // 边缘光遮罩
#endif

void main() {
    vec4 albedo = texture(uAlbedo, fragTexCoord);
    vec3 N = normalize(fragNormal);

#ifdef USE_NORMAL_MAP
    vec3 nTex = texture(uNormalMap, fragTexCoord).xyz * 2.0 - 1.0;   // 简化：当作世界空间扰动
    N = normalize(N + nTex * 0.5);
#endif

    // NPR 漫反射
    vec3 L = vec3(1.0, 0.5, 0.0);
    float diff = halfLambert(N, L);

#ifdef USE_RIM
    float rim = texture(uRimMask, fragTexCoord).r;
    diff += rim * 0.3;
#endif

    outColor = vec4(albedo.rgb * diff, albedo.a);
}
