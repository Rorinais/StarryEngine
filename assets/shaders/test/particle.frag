#version 450

layout(location = 0) in vec4 vColor;
layout(location = 0) out vec4 outColor;

void main() {
    // 柔光圆点：中心亮、边缘衰减
    float d = length(gl_PointCoord - 0.5) * 2.0;  // [0, 1]
    float alpha = 1.0 - smoothstep(0.0, 1.0, d);
    alpha = pow(alpha, 1.5);  // 更集中的光晕
    outColor = vec4(vColor.rgb, vColor.a * alpha);
}
