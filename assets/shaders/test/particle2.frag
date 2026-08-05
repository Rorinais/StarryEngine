#version 450

layout(location = 0) in vec4 vColor;
layout(location = 0) out vec4 outColor;

// 与 particle.frag 接口相同、视觉不同：方形粒子（硬边缘），验证"不同 shader 也能共享 render pass"
void main() {
    // 方形：中心区域完全不透，边缘锐利
    vec2 p = gl_PointCoord - 0.5;
    float alpha = step(0.48, 0.5 - max(abs(p.x), abs(p.y)));   // 硬方形
    outColor = vec4(vColor.rgb, vColor.a * alpha);
}
