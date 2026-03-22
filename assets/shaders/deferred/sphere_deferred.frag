#version 450
layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outMaterial;

layout(set = 1, binding = 0) uniform MaterialUniforms {
    vec4 baseColor;
    float metallic;
    float roughness;
} material;

void main() {
    outAlbedo = material.baseColor;
    outNormal = vec4(fragNormal * 0.5 + 0.5, 1.0);
    outMaterial = vec4(material.metallic, material.roughness, 0.0, 1.0);
}