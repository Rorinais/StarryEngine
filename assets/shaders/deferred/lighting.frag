#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput uAlbedo;
layout(input_attachment_index = 1, set = 1, binding = 1) uniform subpassInput uNormal;
layout(input_attachment_index = 2, set = 1, binding = 2) uniform subpassInput uMaterial;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 albedo = subpassLoad(uAlbedo);
    vec4 normalTex = subpassLoad(uNormal);
    vec3 normal = normalTex.xyz * 2.0 - 1.0;
    vec4 materialTex = subpassLoad(uMaterial);

    if (materialTex.a < 0.5) {
        outColor = vec4(0.2, 0.3, 0.5, 1.0);
        return;
    }

    float metallic = materialTex.r;
    float roughness = materialTex.g;

    vec3 lightDir = normalize(vec3(1.0, 2.0, 1.0));
    float diff = max(dot(normal, lightDir), 0.0);
    outColor = albedo * diff;
}