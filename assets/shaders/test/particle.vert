#version 450

layout(set = 0, binding = 0) uniform GlobalUniforms {
    mat4 view;
    mat4 proj;
    mat4 invView;
    mat4 invProj;
    float time;
} global;

layout(set = 1, binding = 0) readonly buffer ParticleBuffer {
    vec4 particles[];
} uParticles;

layout(push_constant) uniform PC {
    // vec3 按 16B 对齐，把颜色放在 deltaTime/count/padding 之后
    layout(offset = 64) vec3 colorYoung;   // 新生颜色
    layout(offset = 80) vec3 colorMiddle;  // 中年颜色
    layout(offset = 96) vec3 colorOld;     // 衰老颜色
    layout(offset = 112) float pointSizeMin;
    layout(offset = 116) float pointSizeMax;
} pc;

layout(location = 0) out vec4 vColor;

void main() {
    vec4 p = uParticles.particles[gl_VertexIndex];
    float life = p.w;
    float t = 1.0 - life;

    gl_Position = global.proj * global.view * vec4(p.xyz, 1.0);
    gl_PointSize = mix(pc.pointSizeMin, pc.pointSizeMax, sin(t * 3.14159)) * mix(1.2, 0.3, t);

    float mid = smoothstep(0.0, 0.5, t);
    float end = smoothstep(0.5, 1.0, t);
    vec3 rgb = mix(pc.colorYoung, pc.colorMiddle, mid);
    rgb = mix(rgb, pc.colorOld, end);

    vColor = vec4(rgb, mix(0.9, 0.1, t));
}
