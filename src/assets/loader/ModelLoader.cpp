#include "ModelLoader.hpp"
#include "../../logging/Logger.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <stb_image.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <limits>
#include <cmath>
#include <assimp/postprocess.h>
#include <functional>
#include <unordered_map>

namespace StarryEngine::Assets {

    bool ModelLoader::loadFromFile(
        std::shared_ptr<RHI::ResourceManager> resMgr,
        const std::string& path,Geometry& outGeometry,
        std::vector<MaterialParams>& outMaterials,
        Skeleton* outSkeleton,
        AnimationClip* outClip) {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(path,
            aiProcess_Triangulate |
            aiProcess_GenSmoothNormals |
            aiProcess_CalcTangentSpace |
            aiProcess_FlipUVs |
            aiProcess_JoinIdenticalVertices |
            aiProcess_OptimizeMeshes |
            aiProcess_SortByPType);

        if (!scene || !scene->mRootNode) {
            LOG_ERROR("Assimp failed to load file: {} - {}", path, importer.GetErrorString());
            return false;
        }

        std::vector<float> vertices;
        std::vector<uint32_t> indices;
        std::vector<Submesh> submeshes;
        VertexLayout layout;

        bool hasPos = false, hasNormal = false, hasUV = false, hasTangent = false, hasBone = false;
        for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
            aiMesh* mesh = scene->mMeshes[i];
            if (mesh->HasPositions()) hasPos = true;
            if (mesh->HasNormals()) hasNormal = true;
            if (mesh->HasTextureCoords(0)) hasUV = true;
            if (mesh->HasTangentsAndBitangents()) hasTangent = true;
            if (mesh->mNumBones > 0) hasBone = true;
        }

        layout.setSemanticMapping({
            { VertexSemantic::BoneIndices, 4 },
            { VertexSemantic::BoneWeights, 5 },
        });

        uint32_t binding = 0;
        uint32_t currentOffset = 0;

        layout.addBinding(binding, 0, RHI::VertexInputRate::PerVertex);

        // 约定 location 顺序：位置=0，法线=1，UV=2，切线=3
        if (hasPos) {
            layout.addAttribute(VertexSemantic::Position, binding, RHI::Format::RGB32_Float);
            currentOffset += 12;
        }
        if (hasNormal) {
            layout.addAttribute(VertexSemantic::Normal, binding, RHI::Format::RGB32_Float);
            currentOffset += 12;
        }
        if (hasUV) {
            layout.addAttribute(VertexSemantic::TexCoord0, binding, RHI::Format::RG32_Float);
            currentOffset += 8;
        }
        if (hasTangent) {
            layout.addAttribute(VertexSemantic::Tangent, binding, RHI::Format::RGB32_Float);
            currentOffset += 12;
        }
        
        if (hasBone) {
            layout.addAttribute(VertexSemantic::BoneIndices, binding, RHI::Format::RGBA32_Float);
            currentOffset += 16;
            layout.addAttribute(VertexSemantic::BoneWeights, binding, RHI::Format::RGBA32_Float);
            currentOffset += 16;
        }
        layout.addBinding(binding, currentOffset, RHI::VertexInputRate::PerVertex);

        // aiMatrix4x4 → glm::mat4（assimp 行主序 → glm 列主序，即转置）
        auto toGlm = [](const aiMatrix4x4& m) {
            return glm::mat4(
                m.a1, m.b1, m.c1, m.d1,
                m.a2, m.b2, m.c2, m.d2,
                m.a3, m.b3, m.c3, m.d3,
                m.a4, m.b4, m.c4, m.d4);
        };

        // ── 构建 Skeleton：节点树层级 + 逆绑定矩阵 ──
        // 必须在顶点处理之前：processMesh 需要按骨骼名映射到 Skeleton 下标（非 mesh->mBones 数组下标）
        if (outSkeleton && scene->mRootNode) {
            // 递归收集场景节点树 → 骨骼（父索引）。节点 mTransformation = 绑定姿势局部变换
            std::function<void(aiNode*, int)> collect;
            collect = [&](aiNode* node, int parentIndex) {
                uint32_t idx = static_cast<uint32_t>(outSkeleton->bones.size());
                Bone bone;
                bone.name = node->mName.C_Str();
                bone.parentIndex = parentIndex;
                bone.bindLocalTransform = toGlm(node->mTransformation);   // 绑定姿势
                bone.localTransform = bone.bindLocalTransform;
                outSkeleton->bones.push_back(bone);
                outSkeleton->nameToIndex[bone.name] = idx;
                for (unsigned c = 0; c < node->mNumChildren; ++c)
                    collect(node->mChildren[c], static_cast<int>(idx));
            };
            collect(scene->mRootNode, -1);

            // 逆绑定矩阵 = 绑定姿势全局变换的逆（由节点层级计算）。
            // 顶点已按节点层级预变换到全局(root)空间，invBind 与该空间一致 → 绑定姿势蒙皮恒等。
            for (size_t i = 0; i < outSkeleton->bones.size(); ++i) {
                auto& bone = outSkeleton->bones[i];
                glm::mat4 g = (bone.parentIndex >= 0)
                    ? outSkeleton->bones[bone.parentIndex].globalTransform * bone.bindLocalTransform
                    : bone.bindLocalTransform;
                bone.globalTransform = g;
                bone.inverseBindMatrix = glm::inverse(g);
            }
            LOG_INFO("[Skeleton] {} bones from node hierarchy", outSkeleton->bones.size());
        }

        // 递归处理节点，填充顶点和索引
        std::function<void(aiNode*, const glm::mat4&)> processNode;
        processNode = [&](aiNode* node, const glm::mat4& parentTransform) {
            glm::mat4 nodeTransform = toGlm(node->mTransformation);
            glm::mat4 globalTransform = parentTransform * nodeTransform;

            for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
                aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
                processMesh(mesh, vertices, indices, submeshes, layout, currentOffset, globalTransform, outSkeleton);
            }
            // 子节点继承当前节点的全局变换（修复：之前误传 parentTransform，子节点会错位）
            for (unsigned int i = 0; i < node->mNumChildren; ++i) {
                processNode(node->mChildren[i], globalTransform);
            }
            };
        processNode(scene->mRootNode, glm::mat4(1.0f));

        // ── 读取动画：aiAnimation → AnimationClip（骨骼轨道）──
        if (outClip && scene->mNumAnimations > 0) {
            aiAnimation* anim = scene->mAnimations[0];   // 取第一个 clip
            outClip->name = anim->mName.C_Str();
            outClip->duration = anim->mDuration;
            outClip->ticksPerSecond = anim->mTicksPerSecond;

            for (unsigned c = 0; c < anim->mNumChannels; ++c) {
                aiNodeAnim* chan = anim->mChannels[c];
                const std::string nodeName = chan->mNodeName.C_Str();

                if (nodeName.size() >= 4 && nodeName.compare(nodeName.size() - 4, 4, "_end") == 0)
                    continue;

                BoneTrack track;
                track.boneIndex = outSkeleton
                    ? static_cast<int>(outSkeleton->getBoneIndex(nodeName))
                    : -1;

                for (unsigned k = 0; k < chan->mNumPositionKeys; ++k) {
                    const auto& pk = chan->mPositionKeys[k];
                    glm::vec3 p(pk.mValue.x, pk.mValue.y, pk.mValue.z);
                    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) {
                        LOG_WARN("Skipping NaN/Inf position keyframe (track '{}', key {} time {:.4f})",
                            chan->mNodeName.C_Str(), k, static_cast<float>(pk.mTime));
                        continue;
                    }
                    track.positions.push_back({ static_cast<float>(pk.mTime), p });
                }
                for (unsigned k = 0; k < chan->mNumRotationKeys; ++k) {
                    const auto& rk = chan->mRotationKeys[k];
                    // 标准 assimp 读法：aiQuaternion 是 (w, x, y, z)
                    glm::quat q(rk.mValue.w, rk.mValue.x, rk.mValue.y, rk.mValue.z);
                    // 跳过 NaN/Inf 和退化四元数（Blender 可能在 t≈0 写 (0,0,0,0) 或 norm 极小的键）。
                    // slerp 遇退化四元数会产生非单位结果 → mat4_cast 引入缩放 → 网格压扁/翻转。
                    float n = glm::length(q);
                    if (!std::isfinite(q.w) || !std::isfinite(q.x) || !std::isfinite(q.y) || !std::isfinite(q.z) ||
                        n < 0.01f) {
                        LOG_WARN("Skipping invalid rotation keyframe (track '{}', key {} time {:.4f}, norm={:.6f})",
                            chan->mNodeName.C_Str(), k, static_cast<float>(rk.mTime), n);
                        continue;
                    }
                    // 归一化：旋转四元数应为单位长度
                    q = glm::normalize(q);
                    // 二次检查：归一化后仍退化（NaN/零）的键必须丢弃
                    float n2 = glm::length(q);
                    if (!std::isfinite(n2) || n2 < 0.01f) {
                        LOG_WARN("Skipping degenerate rotation after normalize (track '{}', key {} t={:.4f}, preNorm={:.6f}, postNorm={:.6f})",
                            chan->mNodeName.C_Str(), k, static_cast<float>(rk.mTime), n, n2);
                        continue;
                    }
                    track.rotations.push_back({ static_cast<float>(rk.mTime), q });
                }
                for (unsigned k = 0; k < chan->mNumScalingKeys; ++k) {
                    const auto& sk = chan->mScalingKeys[k];
                    glm::vec3 s(sk.mValue.x, sk.mValue.y, sk.mValue.z);
                    if (!std::isfinite(s.x) || !std::isfinite(s.y) || !std::isfinite(s.z)) {
                        LOG_WARN("Skipping NaN/Inf scaling keyframe (track '{}', key {} time {:.4f})",
                            chan->mNodeName.C_Str(), k, static_cast<float>(sk.mTime));
                        continue;
                    }
                    track.scales.push_back({ static_cast<float>(sk.mTime), s });
                }

                // ── 清理动画键：排序 + 去掉负时间/重复时间 ──
                // FBX 导出常带乱序、负时间（≈-0）、t=0 处多个重复键，采样会挑中错误键。
                auto cleanKeys = [](auto& keys) {
                    std::sort(keys.begin(), keys.end(),
                        [](const auto& a, const auto& b) { return a.time < b.time; });
                    size_t w = 0;
                    float lastTime = -1e9f;
                    for (size_t r = 0; r < keys.size(); ++r) {
                        auto& k = keys[r];
                        if (!std::isfinite(k.time)) continue;      // NaN/Inf 时间键丢弃
                        if (k.time < 0.0f) continue;              // 负时间键丢弃
                        // 近重复时间键（t=0 附近常有多键，采样会挑错键）保留第一个
                        if (std::fabs(k.time - lastTime) < 0.01f) continue;
                        keys[w++] = std::move(k);
                        lastTime = k.time;
                    }
                    keys.resize(w);
                };
                cleanKeys(track.positions);
                cleanKeys(track.rotations);
                cleanKeys(track.scales);

                // ── 修复第一帧：frame-0 旋转键是坏的（与绑定差 48-175°，翻跟头），
                // 但后面所有帧都正确。只把第一个旋转键覆盖成绑定旋转，其余键不动 →
                // 角色从绑定姿势开始，动画从第二帧起正常播放。
                if (!track.rotations.empty() && outSkeleton &&
                    track.boneIndex >= 0 &&
                    track.boneIndex < static_cast<int>(outSkeleton->bones.size())) {
                    glm::quat bindRot = glm::normalize(
                        glm::quat_cast(outSkeleton->bones[track.boneIndex].bindLocalTransform));
                    track.rotations.front().value = bindRot;
                }

                outClip->tracks.push_back(std::move(track));
            }
            LOG_INFO("[Clip] Loaded '{}' — {:.2f}s, {} bone tracks", outClip->name,
                outClip->durationSeconds(), outClip->tracks.size());
        }

        extractMaterials(scene, outMaterials, resMgr);

        std::vector<MaterialParams> filteredMaterials;
        std::unordered_map<uint32_t, uint32_t> oldToNewIndex;

        for (uint32_t i = 0; i < outMaterials.size(); ++i) {
            if (outMaterials[i].name != "DefaultMaterial") {
                oldToNewIndex[i] = static_cast<uint32_t>(filteredMaterials.size());
                filteredMaterials.push_back(outMaterials[i]);
            }
            else {
                LOG_INFO("Skipping default material at index {}", i);
            }
        }

        // 更新子网格的材质索引
        for (auto& submesh : submeshes) {
            auto it = oldToNewIndex.find(submesh.materialIndex);
            if (it != oldToNewIndex.end()) {
                submesh.materialIndex = it->second; // 映射到新索引
            }
            else if (!filteredMaterials.empty()) {
                // 如果子网格使用了被跳过的默认材质，映射到第一个有效材质
                LOG_WARN("Submesh with default material index {} mapped to first valid material", submesh.materialIndex);
                submesh.materialIndex = 0;
            }
            else {
                LOG_ERROR("No valid materials found, submesh index remains invalid");
            }
        }

        for (uint32_t newIdx = 0; newIdx < filteredMaterials.size(); ++newIdx) {
            filteredMaterials[newIdx].index = newIdx;
        }
        outMaterials = std::move(filteredMaterials);

        // 单位归一化：assimp 对 cm 单位的 fbx 不自动换算，模型可能巨大。
        // 扫描顶点 bbox，若高度异常（> 10 单位，说明非米单位），缩放到约 1.9 米。
        // obj 等已是米单位的模型（高度 < 10）不受影响。
        if (!vertices.empty()) {
            uint32_t floatStride = currentOffset / sizeof(float);   // 每顶点 float 数
            float minY = std::numeric_limits<float>::max();
            float maxY = -std::numeric_limits<float>::max();
            for (size_t i = 0; i + 1 < vertices.size(); i += floatStride) {
                minY = std::min(minY, vertices[i + 1]);
                maxY = std::max(maxY, vertices[i + 1]);
            }
            float height = maxY - minY;
            if (height > 10.0f) {
                float s = 1.9f / height;
                for (size_t i = 0; i + 2 < vertices.size(); i += floatStride) {
                    vertices[i]     *= s;
                    vertices[i + 1] *= s;
                    vertices[i + 2] *= s;
                }
                LOG_INFO("Model height {:.2f} > 10 units, scaled by {:.4f} → ~1.9m", height, s);

                // 骨骼矩阵同步缩放到米（否则骨骼仍为 cm 尺度，蒙皮矩阵爆炸）。
                // 只缩平移列；scale 列保持（inverseBindMatrix 的 S⁻¹ 会抵消
                // globalTransform 的 S，破坏抵消会导致蒙皮矩阵巨大）。
                if (outSkeleton) {
                    auto scaleBone = [&](glm::mat4& m) {
                        m[3][0] *= s; m[3][1] *= s; m[3][2] *= s;
                    };
                    for (auto& bone : outSkeleton->bones) {
                        scaleBone(bone.bindLocalTransform);
                        scaleBone(bone.localTransform);
                        scaleBone(bone.inverseBindMatrix);
                    }

                    // 动画轨道位置关键帧同步缩放到米（否则采样出的局部变换是 cm 量级，
                    // 与缩放后的 bind 姿势尺度不一致，层级传播时全局矩阵爆炸）。
                    if (outClip) {
                        for (auto& track : outClip->tracks) {
                            for (auto& kf : track.positions)
                                kf.value *= s;
                        }
                        LOG_INFO("Scaled {} bone-track position keyframes to meters (s={:.4f})",
                            outClip->tracks.size(), s);
                    }
                }
            }
        }

        outGeometry.setVertices(vertices);
        outGeometry.setIndices(indices);
        outGeometry.setSubmeshes(submeshes);
        outGeometry.setVertexLayout(layout);

        return true;
    }

    void ModelLoader::processMesh(aiMesh* mesh,
        std::vector<float>& outVertices,
        std::vector<uint32_t>& outIndices,
        std::vector<Submesh>& outSubmeshes,
        const VertexLayout& layout,
        uint32_t stride, const glm::mat4& transform,
        const Skeleton* skeleton) {
        Submesh submesh;
        submesh.indexOffset = static_cast<uint32_t>(outIndices.size());
        submesh.materialIndex = mesh->mMaterialIndex;

        uint32_t vertexStart = static_cast<uint32_t>(outVertices.size() / (stride / sizeof(float)));

        glm::mat3 normalMatrix = glm::mat3(transform); // 法线变换矩阵

        // 预处理每顶点的骨骼影响（最多 4 个，取权重最大的前 4 个）。
        // 无骨骼 mesh 的 boneIndices/weights 为空，写恒等 {0, 1,0,0,0}。
        std::vector<glm::ivec4> boneIndices;
        std::vector<glm::vec4> boneWeights;
        if (mesh->mNumBones > 0) {
            boneIndices.resize(mesh->mNumVertices, glm::ivec4(0));
            boneWeights.resize(mesh->mNumVertices, glm::vec4(0.0f));
            for (unsigned b = 0; b < mesh->mNumBones; ++b) {
                aiBone* bone = mesh->mBones[b];

                // 顶点存的必须是 Skeleton 下标（SSBO 矩阵按它索引），
                // 不是 mesh->mBones 的数组下标 —— 两者通常不一致（mesh 骨只含蒙皮节点）。
                int skeletonIndex = skeleton
                    ? static_cast<int>(skeleton->getBoneIndex(bone->mName.C_Str()))
                    : static_cast<int>(b);
                if (skeletonIndex < 0 || static_cast<uint32_t>(skeletonIndex) == UINT32_MAX)
                    skeletonIndex = 0;   // 找不到的骨骼回退到根，避免越界

                for (unsigned w = 0; w < bone->mNumWeights; ++w) {
                    uint32_t vid = bone->mWeights[w].mVertexId;
                    float wt = bone->mWeights[w].mWeight;
                    if (vid >= mesh->mNumVertices) continue;
                    // 填充到第一个空槽（assimp 权重通常已按降序）
                    for (int s = 0; s < 4; ++s) {
                        if (boneWeights[vid][s] == 0.0f) {
                            boneIndices[vid][s] = skeletonIndex;
                            boneWeights[vid][s] = wt;
                            break;
                        }
                    }
                }
            }
        }

        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
            // 位置
            if (mesh->HasPositions()) {
                glm::vec4 pos(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z, 1.0f);
                pos = transform * pos;
                outVertices.push_back(pos.x);
                outVertices.push_back(pos.y);
                outVertices.push_back(pos.z);
            }
            else {
                outVertices.push_back(0.0f); outVertices.push_back(0.0f); outVertices.push_back(0.0f);
            }

            // 法线
            if (mesh->HasNormals()) {
                glm::vec3 n(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
                n = normalMatrix * n;
                n = glm::normalize(n);
                outVertices.push_back(n.x); outVertices.push_back(n.y); outVertices.push_back(n.z);
            }
            else {
                outVertices.push_back(0.0f); outVertices.push_back(1.0f); outVertices.push_back(0.0f);
            }

            // UV
            if (mesh->HasTextureCoords(0)) {
                outVertices.push_back(mesh->mTextureCoords[0][i].x);
                outVertices.push_back(mesh->mTextureCoords[0][i].y);
            }
            else {
                outVertices.push_back(0.0f); outVertices.push_back(0.0f);
            }

            // 切线
            if (mesh->HasTangentsAndBitangents()) {
                glm::vec3 t(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
                t = normalMatrix * t;
                t = glm::normalize(t);
                outVertices.push_back(t.x); outVertices.push_back(t.y); outVertices.push_back(t.z);
            }
            else if (layout.getBindings().size() > 0) {
                outVertices.push_back(1.0f); outVertices.push_back(0.0f); outVertices.push_back(0.0f);
            }

            // 蒙皮：只有 layout 声明了骨骼属性才写（与顶点布局严格对应）
            if (layout.getLocationForSemantic(VertexSemantic::BoneIndices) != UINT32_MAX) {
                if (!boneIndices.empty()) {
                    // 有骨骼的 mesh：写实际 boneIndices + boneWeights
                    outVertices.push_back(static_cast<float>(boneIndices[i].x));
                    outVertices.push_back(static_cast<float>(boneIndices[i].y));
                    outVertices.push_back(static_cast<float>(boneIndices[i].z));
                    outVertices.push_back(static_cast<float>(boneIndices[i].w));
                    outVertices.push_back(boneWeights[i].x);
                    outVertices.push_back(boneWeights[i].y);
                    outVertices.push_back(boneWeights[i].z);
                    outVertices.push_back(boneWeights[i].w);
                }
                else {
                    // 无骨骼 mesh（混合场景）：写恒等 {0, 1,0,0,0}
                    outVertices.push_back(0.0f); outVertices.push_back(0.0f); outVertices.push_back(0.0f); outVertices.push_back(0.0f);
                    outVertices.push_back(1.0f); outVertices.push_back(0.0f); outVertices.push_back(0.0f); outVertices.push_back(0.0f);
                }
            }
        }

        // 索引
        for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
            aiFace& face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; ++j) {
                outIndices.push_back(vertexStart + face.mIndices[j]);
            }
        }

        submesh.indexCount = static_cast<uint32_t>(outIndices.size() - submesh.indexOffset);
        outSubmeshes.push_back(submesh);
    }

    void ModelLoader::extractMaterials(const aiScene* scene,
        std::vector<MaterialParams>& outMaterials, 
        std::shared_ptr<RHI::ResourceManager> resMgr) {
        outMaterials.clear();
        outMaterials.reserve(scene->mNumMaterials);

        TextureLoader texLoader(resMgr);

        for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
            aiMaterial* aiMat = scene->mMaterials[i];
            MaterialParams mat;
            mat.index = i;
            // 材质名称
            aiString name;
            if (aiMat->Get(AI_MATKEY_NAME, name) == AI_SUCCESS) {
                mat.name = name.C_Str();
            }
            else {
                mat.name = "Material_" + std::to_string(i);
            }

            // 基础颜色
            aiColor3D color(1.0f, 1.0f, 1.0f);
            if (aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
                mat.baseColor = glm::vec4(color.r, color.g, color.b, 1.0f);
            }
            else {
                LOG_WARN("Material '{}' missing diffuse color, using default white", mat.name);
            }

            // 金属度和粗糙度
            float metallic = 0.0f, roughness = 0.5f;
            if (aiMat->Get(AI_MATKEY_METALLIC_FACTOR, metallic) != AI_SUCCESS) {
                LOG_WARN("Material '{}' missing metallic factor, using 0.0", mat.name);
            }
            if (aiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) != AI_SUCCESS) {
                LOG_WARN("Material '{}' missing roughness factor, using 0.5", mat.name);
            }
            mat.metallic = metallic;
            mat.roughness = roughness;

            // 自发光
            aiColor3D emissive(0.0f, 0.0f, 0.0f);
            if (aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS) {
                mat.emissiveColor = glm::vec3(emissive.r, emissive.g, emissive.b);
                mat.emissiveIntensity = 1.0f;
            }

            // 透明度
            float opacity = 1.0f;
            if (aiMat->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS) {
                mat.baseColor.a = opacity;
                if (opacity < 1.0f) {
                    mat.alphaBlend = true;
                }
            }

            auto processTexture = [&](aiTextureType type, const std::string& debugName,
                std::string& outPath, RHI::TextureHandle& outHandle) {
                    aiString texPath;
                    if (aiMat->GetTexture(type, 0, &texPath) == AI_SUCCESS) {
                        std::string path = texPath.C_Str();
                        if (!path.empty() && path[0] == '*') {
                            int index = std::stoi(path.substr(1));
                            if (index >= 0 && index < scene->mNumTextures) {
                                aiTexture* embeddedTex = scene->mTextures[index];
                                auto result = loadEmbeddedTexture(texLoader, embeddedTex, debugName);
                                if (result.texture.isValid()) {
                                    outHandle = result.texture;
                                    outPath = ""; // 标记为已处理
                                }
                                else {
                                    LOG_ERROR("Failed to load embedded texture for material {}", mat.name);
                                }
                            }
                            else {
                                LOG_ERROR("Invalid embedded texture index {} for material {}", index, mat.name);
                            }
                        }
                        else {
                            outPath = path;
                        }
                    }
                };

            processTexture(aiTextureType_DIFFUSE, "albedo", mat.albedoTexture, mat.albedoTextureHandle);
            processTexture(aiTextureType_NORMALS, "normal", mat.normalTexture, mat.normalTextureHandle);
            processTexture(aiTextureType_METALNESS, "metallic", mat.metallicTexture, mat.metallicTextureHandle);
            processTexture(aiTextureType_DIFFUSE_ROUGHNESS, "roughness", mat.roughnessTexture, mat.roughnessTextureHandle);
            processTexture(aiTextureType_AMBIENT_OCCLUSION, "occlusion", mat.occlusionTexture, mat.occlusionTextureHandle);
            processTexture(aiTextureType_EMISSION_COLOR, "emissive", mat.emissiveTexture, mat.emissiveTextureHandle);
            processTexture(aiTextureType_OPACITY, "opacity", mat.opacityTexture, mat.opacityTextureHandle);

            // 设置标志
            mat.useNormalMap = !mat.normalTexture.empty();
            mat.useEmissiveMap = !mat.emissiveTexture.empty();

            outMaterials.push_back(mat);
        }
    }

    TextureLoadResult ModelLoader::loadEmbeddedTexture(TextureLoader& loader, aiTexture* tex, const std::string& debugName) {
        if (tex->mHeight == 0) {
            int width, height, channels;
            stbi_uc* pixels = stbi_load_from_memory(
                reinterpret_cast<stbi_uc*>(tex->pcData),
                tex->mWidth,
                &width, &height, &channels,
                STBI_rgb_alpha);
            if (!pixels) {
                LOG_ERROR("Failed to decode embedded compressed texture");
                return { RHI::TextureHandle::Null(), RHI::SamplerHandle::Null() };
            }
            auto result = loader.loadTextureFromMemory(pixels, width, height,
                RHI::Format::RGBA8_UNorm,
                debugName);
            stbi_image_free(pixels);
            return result;
        }
        else {
            return loader.loadTextureFromMemory(tex->pcData, tex->mWidth, tex->mHeight,
                RHI::Format::RGBA8_UNorm,
                debugName);
        }
    }
} // namespace StarryEngine::Assets