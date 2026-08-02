#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;   // 如果有法线
layout(location = 2) in vec2 inTexCoord;
layout(location = 4) in vec4 inBoneIndices;   // 骨骼索引（4 根影响骨骼）
layout(location = 5) in vec4 inBoneWeights;   // 权重

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 fragNormal;

// set 0：全局 UBO（view / proj）
layout(set = 0, binding = 0) uniform GlobalUniforms {
    mat4 view;
    mat4 proj;
} global;

// 骨骼矩阵 SSBO（每帧由 Animator 更新，50KB > UBO 限制故用 StorageBuffer）
// readonly：顶点阶段只读，否则需 vertexPipelineStoresAndAtomics 特性
layout(set = 1, binding = 2) readonly buffer BoneMatrices {
    mat4 bones[];
} boneMats;

// push constant：模型矩阵
layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

void main() {
    // 蒙皮：4 根骨骼矩阵按权重加权
    mat4 skin = boneMats.bones[int(inBoneIndices.x)] * inBoneWeights.x
              + boneMats.bones[int(inBoneIndices.y)] * inBoneWeights.y
              + boneMats.bones[int(inBoneIndices.z)] * inBoneWeights.z
              + boneMats.bones[int(inBoneIndices.w)] * inBoneWeights.w;

    vec4 pos = skin * vec4(inPosition, 1.0);
    gl_Position = global.proj * global.view * pc.model * pos;
    fragTexCoord = inTexCoord;
    fragNormal = mat3(skin) * inNormal;  // 法线随骨骼变换
}
