#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D uAlbedo;
layout(set = 1, binding = 1) uniform sampler2D uNormal;
layout(set = 1, binding = 2) uniform sampler2D uMaterial;

void main() {
    vec4 albedo = texture(uAlbedo, vUV);
    vec4 normalTex = texture(uNormal, vUV);
    vec3 normal = normalTex.xyz * 2.0 - 1.0;
    vec4 materialTex = texture(uMaterial, vUV);

    // 检查是否有效区域（Material 纹理的 Alpha 通道）
    if (materialTex.a < 0.5) {
        outColor = vec4(0.2, 0.3, 0.5, 1.0); // 您想要的背景色
        return;
    }

    float metallic = materialTex.r;
    float roughness = materialTex.g;

    // 正常光照计算（示例）
    vec3 lightDir = normalize(vec3(1.0, 2.0, 1.0));
    float diff = max(dot(normal, lightDir), 0.0);
    outColor = albedo * diff;
}