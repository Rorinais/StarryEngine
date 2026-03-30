#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) out vec2 vUV;
layout(location = 1) out vec3 worldDir;

layout(set = 0, binding = 0) uniform GlobalUniforms {
    mat4 view;
    mat4 proj;
    mat4 invView;
    mat4 invProj;   
} global;

void main() {
    // 生成全屏三角形的顶点位置（覆盖 NDC）
    vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    vUV = uv;                      // 传递 uv 到片段着色器

    vec2 ndc = vUV * 2.0 - 1.0;
    vec4 clipPos = vec4(ndc, 1.0, 1.0);
    vec4 worldPos = global.invView * global.invProj * clipPos;
    worldDir = normalize(worldPos.xyz);

    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}