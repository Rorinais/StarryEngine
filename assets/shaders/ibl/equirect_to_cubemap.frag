#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D uEquirectMap;

layout(push_constant) uniform PC {
    int face;
    float faceSize;
} pc;

const float PI = 3.14159265359;

void main() {
    // 像素坐标：gl_FragCoord 左下角 (0.5, 0.5)
    float u = gl_FragCoord.x - 0.5;
    float v = gl_FragCoord.y - 0.5;

    // ⚡ 完全复制 CPU 端的 CubemapFaceToDirection
    float x = (u + 0.5) / pc.faceSize * 2.0 - 1.0;
    float y = (v + 0.5) / pc.faceSize * 2.0 - 1.0;

    vec3 dir;
    switch (pc.face) {
        case 0: dir = vec3( 1.0, -y, -x); break; // +X
        case 1: dir = vec3(-1.0, -y,  x); break; // -X
        case 2: dir = vec3( x,  1.0,  y); break; // +Y
        case 3: dir = vec3( x, -1.0, -y); break; // -Y
        case 4: dir = vec3( x, -y,  1.0); break; // +Z
        default:dir = vec3(-x, -y, -1.0); break; // -Z
    }
    dir = normalize(dir);

    // ⚡ 完全复制 CPU 端的 DirectionToEquirectUV
    float phi   = atan(dir.z, dir.x);
    float theta = asin(-dir.y);
    vec2 equirectUV;
    equirectUV.x = phi   / (2.0 * PI) + 0.5;
    equirectUV.y = theta / PI + 0.5;

    outColor = texture(uEquirectMap, equirectUV);
}