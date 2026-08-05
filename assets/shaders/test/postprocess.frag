#version 450
#extension GL_KHR_vulkan_glsl : enable
// 测试后处理：读 Processed（输入附件）→ 变亮 → 写 SceneColor。
// 验证"图形 pass 读 compute 输出作为输入附件"的跨 pass 依赖。
layout(input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput uInput;
layout(location = 0) out vec4 outColor;

void main() {
    vec4 c = subpassLoad(uInput);
    outColor = vec4(c.rgb * 0.5 + 0.5, c.a);   // 变亮（能看出后处理生效）
}
