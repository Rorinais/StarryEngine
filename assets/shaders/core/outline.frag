#version 450
#extension GL_KHR_vulkan_glsl : enable
layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

// 与 shader.frag 相同的 set1/binding0 纹理（描边材质的白纹理，只取 alpha 保绑定有效）
layout(set = 1, binding = 0) uniform sampler2D texSampler;

void main() {
    // 描边：忽略纹理颜色，直接输出高亮青色；模板反选区（NotEqual）只画在本体外圈
    vec4 tex = texture(texSampler, fragTexCoord);
    outColor = vec4(0.0, 1.0, 1.0, tex.a);
}
