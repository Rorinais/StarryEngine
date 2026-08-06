#version 450
#extension GL_KHR_vulkan_glsl : enable
layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

// 与 shader.frag 相同的 set1/binding0 纹理（乘白纹理 → 硬编码红，验证模板蒙版区）
layout(set = 1, binding = 0) uniform sampler2D texSampler;

void main() {
    vec4 tex = texture(texSampler, fragTexCoord);
    outColor = vec4(tex.rgb * vec3(1.0, 0.0, 0.0), 1.0);
}
