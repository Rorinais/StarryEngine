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

layout(location = 0) out vec4 vColor;

void main() {
    vec4 p = uParticles.particles[gl_VertexIndex];
    float life = p.w;

    gl_Position = global.proj * global.view * vec4(p.xyz, 1.0);

    // 生命周期控制大小：新生小→中年大→衰老小
    float t = 1.0 - life;  // 0=新生, 1=即将死亡
    gl_PointSize = mix(2.0, 8.0, sin(t * 3.14159)) * mix(1.2, 0.3, t);

    // 颜色渐变：新生白/黄 → 中年橙 → 衰老红/暗
    vec3 young  = vec3(1.0, 0.95, 0.5);   // 亮黄白
    vec3 middle = vec3(1.0, 0.45, 0.05);  // 橙
    vec3 old    = vec3(0.7, 0.1, 0.02);   // 暗红
    float mid = smoothstep(0.0, 0.5, t);
    float end = smoothstep(0.5, 1.0, t);
    vec3 rgb = mix(young, middle, mid);
    rgb = mix(rgb, old, end);

    vColor = vec4(rgb, mix(0.9, 0.1, t));  // alpha 随老化衰减
}
