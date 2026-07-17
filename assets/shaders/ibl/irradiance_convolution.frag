#version 450

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform samplerCube uEnvironmentMap;

layout(push_constant) uniform PC {
    int   face;
    float faceSize;
} pc;

const float PI = 3.14159265359;
const uint  SAMPLE_COUNT = 1024u;   // 重要性采样 1024 已足够

// ── Hammersley 低差异序列 ──
float RadicalInverse_VdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec2 Hammersley(uint i, uint N) {
    return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

// ── 余弦加权重要性采样（半球，概率密度 = cosθ/π）──
vec3 CosineSampleHemisphere(vec2 Xi, vec3 N) {
    float r = sqrt(Xi.x);
    float phi = 2.0 * PI * Xi.y;
    vec3 localDir = vec3(r * cos(phi), r * sin(phi), sqrt(1.0 - Xi.x));
    // 构建 TBN 矩阵
    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);
    return tangent * localDir.x + bitangent * localDir.y + N * localDir.z;
}

// ── 像素 → 法线方向（与 equirect_to_cubemap 保持一致）──
vec3 FaceToDirection(int face, float u, float v, float size) {
    float x = (u + 0.5) / size * 2.0 - 1.0;
    float y = (v + 0.5) / size * 2.0 - 1.0;
    vec3 dir;
    switch (face) {
        case 0: dir = vec3( 1.0, -y, -x); break;
        case 1: dir = vec3(-1.0, -y,  x); break;
        case 2: dir = vec3( x,  1.0,  y); break;
        case 3: dir = vec3( x, -1.0, -y); break;
        case 4: dir = vec3( x, -y,  1.0); break;
        default:dir = vec3(-x, -y, -1.0); break;
    }
    return normalize(dir);
}

void main() {
    float u = gl_FragCoord.x - 0.5;
    float v = gl_FragCoord.y - 0.5;
    vec3 N = FaceToDirection(pc.face, u, v, pc.faceSize);

    // ── 余弦加权 Importance Sampling（标准 Cook-Torrance 漫反射 IBL）──
    // PDF(ω) = cosθ / π
    // 蒙特卡洛: ∫ L_i·cosθ dω ≈ (1/N)·Σ [L_i·cosθ / PDF]
    //           L_i·cosθ / (cosθ/π) = L_i·π
    //           ∴ 积分 = (π/N)·Σ L_i
    vec3 irradiance = vec3(0.0);
    for (uint i = 0u; i < SAMPLE_COUNT; i++) {
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 L  = CosineSampleHemisphere(Xi, N);
        irradiance += texture(uEnvironmentMap, L).rgb;   // 只加 L_i，不乘 cosθ（PDF 已含）
    }
    irradiance = PI * irradiance / float(SAMPLE_COUNT);

    outColor = vec4(irradiance, 1.0);
}