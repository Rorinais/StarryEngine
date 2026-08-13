#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 4) in vec4 inBoneIndices;   // 骨骼索引（4 根影响骨骼）
layout(location = 5) in vec4 inBoneWeights;   // 权重

layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

// 与 GlobalUniforms（C++ 侧）字段对齐：lightVP 在 time 之后
layout(set = 0, binding = 0) uniform GlobalUniforms {
    mat4 view;
    mat4 proj;
    mat4 invView;
    mat4 invProj;
    float time;
    mat4 lightVP;
} global;

// 骨骼矩阵 SSBO（与 shader_skinned.vert 同布局：set1/binding2，每帧由 Animator 更新）
layout(set = 1, binding = 2) readonly buffer BoneMatrices {
    mat4 bones[];
} boneMats;

void main() {
    // 蒙皮：4 根骨骼矩阵按权重加权（与 shader_skinned.vert 一致，否则阴影姿态不跟动画）
    mat4 skin = boneMats.bones[int(inBoneIndices.x)] * inBoneWeights.x
              + boneMats.bones[int(inBoneIndices.y)] * inBoneWeights.y
              + boneMats.bones[int(inBoneIndices.z)] * inBoneWeights.z
              + boneMats.bones[int(inBoneIndices.w)] * inBoneWeights.w;

    vec4 worldPos = pc.model * (skin * vec4(inPosition, 1.0));
    gl_Position = global.lightVP * worldPos;
}
