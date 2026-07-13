#version 450

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform samplerCube uEnvironmentMap;

layout(push_constant) uniform PC {
    int   face;
    float faceSize;
    float roughness;
    float envResolution;   // 源 cubemap 每面分辨率（用于 mip 选择）
} pc;

const float PI = 3.14159265359;
const uint  SAMPLE_COUNT = 4096u;

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

// ── GGX 重要性采样 ──
vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness) {
    float a = roughness * roughness;
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangentX = normalize(cross(up, N));
    vec3 tangentY = cross(N, tangentX);

    return tangentX * H.x + tangentY * H.y + N * H.z;
}

// ── 像素 → 法线方向 ──
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

// ════════════════════════ 主函数 ════════════════════════

void main() {
    float u = gl_FragCoord.x - 0.5;
    float v = gl_FragCoord.y - 0.5;
    vec3 N = FaceToDirection(pc.face, u, v, pc.faceSize);
    vec3 V = N;  // 反射方向 = 法线方向 = 视线方向

    // 源 cubemap 最大 mip 层级（log2 分辨率）
    float maxSourceMip = log2(pc.envResolution);

    float r = max(pc.roughness, 0.001);

    // 低粗糙度时 GGX lobe 极窄，采样难以收敛 → 直接从源 mip 0 读取
    if (r < 0.05) {
        outColor = textureLod(uEnvironmentMap, N, 0.0);
        return;
    }

    // ── 粗糙度压缩映射 mip level ──
    float remapped = mix(0.25, 0.75, r);
    float mipLevel = remapped * maxSourceMip;

    vec3 prefilteredColor = vec3(0.0);

    for (uint i = 0u; i < SAMPLE_COUNT; i++) {
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H  = ImportanceSampleGGX(Xi, N, r);
        vec3 L  = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);

        if (NdotL > 0.0) {
            prefilteredColor += textureLod(uEnvironmentMap, L, mipLevel).rgb;
        }
    }
    prefilteredColor /= float(SAMPLE_COUNT);

    outColor = vec4(prefilteredColor, 1.0);
}