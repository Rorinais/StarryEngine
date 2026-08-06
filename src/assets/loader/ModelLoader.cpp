#include "ModelLoader.hpp"
#include "../../logging/Logger.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <stb_image.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <limits>
#include <algorithm>
#include <cmath>
#include <functional>
#include <unordered_map>

namespace StarryEngine::Assets {

    // aiMatrix4x4（行主序）→ glm::mat4（列主序）
    static glm::mat4 toGlm(const aiMatrix4x4& m) {
        return glm::mat4(
            m.a1, m.b1, m.c1, m.d1,
            m.a2, m.b2, m.c2, m.d2,
            m.a3, m.b3, m.c3, m.d3,
            m.a4, m.b4, m.c4, m.d4);
    }

    ModelLoader::Config::Config() : calcTangents(true) {}

    ModelLoader::ModelLoader(std::shared_ptr<RHI::ResourceManager> resMgr, Config config)
        : m_resMgr(std::move(resMgr)), m_config(config) {}

    ModelLoader::~ModelLoader() { close(); }

    bool ModelLoader::open(const std::string& path) {
        close();

        m_importer = std::make_unique<Assimp::Importer>();
        const aiScene* scene = m_importer->ReadFile(path,
            aiProcess_Triangulate |
            aiProcess_GenSmoothNormals |
            (m_config.calcTangents ? aiProcess_CalcTangentSpace : 0) |
            aiProcess_FlipUVs |
            aiProcess_JoinIdenticalVertices |
            aiProcess_OptimizeMeshes |
            aiProcess_SortByPType);

        if (!scene || !scene->mRootNode) {
            LOG_ERROR("Assimp failed to load file: {} - {}", path, m_importer->GetErrorString());
            m_importer.reset();
            return false;
        }
        m_scene = scene;
        m_path = path;

        // 单位缩放：扫描顶点（按节点层级预变换后）的高度，cm 单位模型缩放到 ~1.9m
        float minY = std::numeric_limits<float>::max();
        float maxY = -std::numeric_limits<float>::max();
        std::function<void(aiNode*, const glm::mat4&)> measure;
        measure = [&](aiNode* node, const glm::mat4& parentTransform) {
            glm::mat4 global = parentTransform * toGlm(node->mTransformation);
            for (unsigned i = 0; i < node->mNumMeshes; ++i) {
                aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
                for (unsigned v = 0; v < mesh->mNumVertices; ++v) {
                    glm::vec4 p(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z, 1.0f);
                    glm::vec4 t = global * p;
                    minY = std::min(minY, t.y);
                    maxY = std::max(maxY, t.y);
                }
            }
            for (unsigned c = 0; c < node->mNumChildren; ++c)
                measure(node->mChildren[c], global);
        };
        measure(scene->mRootNode, glm::mat4(1.0f));

        float height = maxY - minY;
        m_unitScale = (height > 10.0f) ? 1.9f / height : 1.0f;
        if (m_unitScale != 1.0f)
            LOG_INFO("Model height {:.2f} > 10 units, scaled by {:.4f} → ~1.9m", height, m_unitScale);

        return true;
    }

    void ModelLoader::close() {
        m_scene = nullptr;
        m_path.clear();
        m_unitScale = 1.0f;
        m_skeleton.reset();
        m_importer.reset();
    }

    void ModelLoader::clearCache() { close(); }

    void ModelLoader::buildSkeleton(Skeleton& out) {
        if (!m_scene || !m_scene->mRootNode) return;

        // 递归收集场景节点树 → 骨骼（父索引）。节点 mTransformation = 绑定姿势局部变换
        std::function<void(aiNode*, int)> collect;
        collect = [&](aiNode* node, int parentIndex) {
            uint32_t idx = static_cast<uint32_t>(out.bones.size());
            Bone bone;
            bone.name = node->mName.C_Str();
            bone.parentIndex = parentIndex;
            bone.bindLocalTransform = toGlm(node->mTransformation);
            bone.localTransform = bone.bindLocalTransform;
            out.bones.push_back(bone);
            out.nameToIndex[bone.name] = idx;
            for (unsigned c = 0; c < node->mNumChildren; ++c)
                collect(node->mChildren[c], static_cast<int>(idx));
        };
        collect(m_scene->mRootNode, -1);

        // 逆绑定矩阵 = 绑定姿势全局变换的逆（由节点层级计算）。
        // 顶点已按节点层级预变换到全局(root)空间，invBind 与该空间一致 → 绑定姿势蒙皮恒等。
        for (size_t i = 0; i < out.bones.size(); ++i) {
            auto& bone = out.bones[i];
            glm::mat4 g = (bone.parentIndex >= 0)
                ? out.bones[bone.parentIndex].globalTransform * bone.bindLocalTransform
                : bone.bindLocalTransform;
            bone.globalTransform = g;
            bone.inverseBindMatrix = glm::inverse(g);
        }
        LOG_INFO("[Skeleton] {} bones from node hierarchy", out.bones.size());
    }

    void ModelLoader::ensureSkeleton() {
        if (m_skeleton) return;
        Skeleton skel;
        buildSkeleton(skel);

        // 骨骼矩阵同步缩放到米（只缩平移列；scale 列保持 S⁻¹ 抵消）
        if (m_unitScale != 1.0f) {
            for (auto& bone : skel.bones) {
                bone.bindLocalTransform[3][0] *= m_unitScale;
                bone.bindLocalTransform[3][1] *= m_unitScale;
                bone.bindLocalTransform[3][2] *= m_unitScale;
                bone.localTransform[3][0] *= m_unitScale;
                bone.localTransform[3][1] *= m_unitScale;
                bone.localTransform[3][2] *= m_unitScale;
                bone.inverseBindMatrix[3][0] *= m_unitScale;
                bone.inverseBindMatrix[3][1] *= m_unitScale;
                bone.inverseBindMatrix[3][2] *= m_unitScale;
            }
        }
        m_skeleton = std::move(skel);
    }

    std::optional<Skeleton> ModelLoader::loadSkeleton() {
        if (!isOpen()) return std::nullopt;
        ensureSkeleton();
        return *m_skeleton;   // 复制返回（内部保留副本供 geometry/clip 复用）
    }

    std::optional<Geometry> ModelLoader::loadGeometry() {
        if (!isOpen()) return std::nullopt;
        ensureSkeleton();
        const Skeleton& skeleton = *m_skeleton;

        std::vector<float> vertices;
        std::vector<uint32_t> indices;
        std::vector<Submesh> submeshes;

        // 顶点布局：位置/法线/UV/切线/骨骼属性
        bool hasPos = false, hasNormal = false, hasUV = false, hasTangent = false, hasBone = false;
        for (unsigned int i = 0; i < m_scene->mNumMeshes; ++i) {
            aiMesh* mesh = m_scene->mMeshes[i];
            if (mesh->HasPositions()) hasPos = true;
            if (mesh->HasNormals()) hasNormal = true;
            if (mesh->HasTextureCoords(0)) hasUV = true;
            if (mesh->HasTangentsAndBitangents()) hasTangent = true;
            if (mesh->mNumBones > 0) hasBone = true;
        }

        VertexLayout layout;
        layout.setSemanticMapping({
            { VertexSemantic::BoneIndices, 4 },
            { VertexSemantic::BoneWeights, 5 },
        });
        uint32_t binding = 0, currentOffset = 0;
        layout.addBinding(binding, 0, RHI::VertexInputRate::PerVertex);
        if (hasPos) { layout.addAttribute(VertexSemantic::Position, binding, RHI::Format::RGB32_Float); currentOffset += 12; }
        if (hasNormal) { layout.addAttribute(VertexSemantic::Normal, binding, RHI::Format::RGB32_Float); currentOffset += 12; }
        if (hasUV) { layout.addAttribute(VertexSemantic::TexCoord0, binding, RHI::Format::RG32_Float); currentOffset += 8; }
        if (hasTangent) { layout.addAttribute(VertexSemantic::Tangent, binding, RHI::Format::RGB32_Float); currentOffset += 12; }
        if (hasBone) {
            layout.addAttribute(VertexSemantic::BoneIndices, binding, RHI::Format::RGBA32_Float); currentOffset += 16;
            layout.addAttribute(VertexSemantic::BoneWeights, binding, RHI::Format::RGBA32_Float); currentOffset += 16;
        }
        layout.addBinding(binding, currentOffset, RHI::VertexInputRate::PerVertex);

        // 递归处理节点，填充顶点和索引（顶点预变换到 root 空间；骨骼索引按名映射到 Skeleton 下标）
        std::function<void(aiNode*, const glm::mat4&)> processNode;
        processNode = [&](aiNode* node, const glm::mat4& parentTransform) {
            glm::mat4 global = parentTransform * toGlm(node->mTransformation);
            for (unsigned i = 0; i < node->mNumMeshes; ++i)
                processMesh(m_scene->mMeshes[node->mMeshes[i]], vertices, indices, submeshes, layout, currentOffset, global);
            for (unsigned c = 0; c < node->mNumChildren; ++c)
                processNode(node->mChildren[c], global);
        };
        processNode(m_scene->mRootNode, glm::mat4(1.0f));

        // 顶点缩放到米
        if (m_unitScale != 1.0f && currentOffset > 0) {
            uint32_t floatStride = currentOffset / sizeof(float);
            for (size_t i = 0; i + 2 < vertices.size(); i += floatStride) {
                vertices[i] *= m_unitScale;
                vertices[i + 1] *= m_unitScale;
                vertices[i + 2] *= m_unitScale;
            }
        }

        Geometry geo(m_resMgr);
        geo.setVertices(vertices);
        geo.setIndices(indices);
        geo.setSubmeshes(submeshes);
        geo.setVertexLayout(layout);
        return geo;
    }

    std::optional<AnimationClip> ModelLoader::loadClip() {
        if (!isOpen() || m_scene->mNumAnimations == 0) return std::nullopt;
        ensureSkeleton();
        const Skeleton& skeleton = *m_skeleton;

        aiAnimation* anim = m_scene->mAnimations[0];   // 取第一个 clip
        AnimationClip clip;
        clip.name = anim->mName.C_Str();
        clip.duration = anim->mDuration;
        clip.ticksPerSecond = anim->mTicksPerSecond;

        // 整段 clip 的坏键统计（只打印一条汇总，不逐条刷屏）
        int totalSkipPos = 0, totalSkipRot = 0, totalSkipScl = 0;

        for (unsigned c = 0; c < anim->mNumChannels; ++c) {
            aiNodeAnim* chan = anim->mChannels[c];
            const std::string nodeName = chan->mNodeName.C_Str();

            if (nodeName.size() >= 4 && nodeName.compare(nodeName.size() - 4, 4, "_end") == 0)
                continue;

            BoneTrack track;
            track.boneIndex = static_cast<int>(skeleton.getBoneIndex(nodeName));

            int skippedPosKeys = 0;
            for (unsigned k = 0; k < chan->mNumPositionKeys; ++k) {
                const auto& pk = chan->mPositionKeys[k];
                glm::vec3 p(pk.mValue.x, pk.mValue.y, pk.mValue.z);
                if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) { ++skippedPosKeys; continue; }
                track.positions.push_back({ static_cast<float>(pk.mTime), p });
            }
            totalSkipPos += skippedPosKeys;

            int skippedRotKeys = 0;
            for (unsigned k = 0; k < chan->mNumRotationKeys; ++k) {
                const auto& rk = chan->mRotationKeys[k];
                // 标准 assimp 读法：aiQuaternion 是 (w, x, y, z)
                glm::quat q(rk.mValue.w, rk.mValue.x, rk.mValue.y, rk.mValue.z);
                float n = glm::length(q);
                if (!std::isfinite(q.w) || !std::isfinite(q.x) || !std::isfinite(q.y) || !std::isfinite(q.z)) { ++skippedRotKeys; continue; }
                if (n < 0.1f || n > 2.0f) { ++skippedRotKeys; continue; }   // ★坏键拒绝
                q = glm::normalize(q);
                float n2 = glm::length(q);
                if (!std::isfinite(n2) || n2 < 0.01f) { ++skippedRotKeys; continue; }
                track.rotations.push_back({ static_cast<float>(rk.mTime), q });
            }
            totalSkipRot += skippedRotKeys;

            int skippedSclKeys = 0;
            for (unsigned k = 0; k < chan->mNumScalingKeys; ++k) {
                const auto& sk = chan->mScalingKeys[k];
                glm::vec3 s(sk.mValue.x, sk.mValue.y, sk.mValue.z);
                if (!std::isfinite(s.x) || !std::isfinite(s.y) || !std::isfinite(s.z)) { ++skippedSclKeys; continue; }
                track.scales.push_back({ static_cast<float>(sk.mTime), s });
            }
            totalSkipScl += skippedSclKeys;

            // 清理动画键：排序 + 去掉 NaN/Inf/负时间/重复时间键
            auto cleanKeys = [](auto& keys) {
                std::sort(keys.begin(), keys.end(),
                    [](const auto& a, const auto& b) { return a.time < b.time; });
                size_t w = 0;
                float lastTime = -1e9f;
                for (size_t r = 0; r < keys.size(); ++r) {
                    auto& k = keys[r];
                    if (!std::isfinite(k.time)) continue;
                    if (k.time < 0.0f) continue;
                    if (std::fabs(k.time - lastTime) < 0.01f) continue;
                    keys[w++] = std::move(k);
                    lastTime = k.time;
                }
                keys.resize(w);
            };
            cleanKeys(track.positions);
            cleanKeys(track.rotations);
            cleanKeys(track.scales);
            clip.tracks.push_back(std::move(track));
        }

        // 注：duration 直接用 FBX 声明的动画时长（anim->mDuration），不按旋转键最大
        // 时间截断。曾有代码把时长截到"最后一个有效旋转键"来治 32s 后空档——那是
        // assimp 头/库不匹配（aiQuatKey 布局 24 vs 32 字节）导致的误读，已修复。
        // 正常动画尾部若有定格姿势（骨键提前结束、后续保持不动），截断反而会砍掉它。
        int totalBad = totalSkipPos + totalSkipRot + totalSkipScl;
        if (totalBad > 0)
            LOG_WARN("[{}] Cleaned {} bad animation keyframes ({} pos, {} rot, {} scl)",
                clip.name, totalBad, totalSkipPos, totalSkipRot, totalSkipScl);
        LOG_INFO("[Clip] Loaded '{}' — {:.2f}s, {} bone tracks", clip.name,
            clip.durationSeconds(), clip.tracks.size());

        // 动画轨道位置关键帧同步缩放到米
        if (m_unitScale != 1.0f) {
            for (auto& track : clip.tracks)
                for (auto& kf : track.positions)
                    kf.value *= m_unitScale;
        }

        return clip;
    }

    std::vector<MaterialParams> ModelLoader::loadMaterials() {
        if (!isOpen()) return {};
        std::vector<MaterialParams> mats;
        extractMaterials(m_scene, mats);

        // 过滤默认材质
        std::vector<MaterialParams> filtered;
        for (uint32_t i = 0; i < mats.size(); ++i) {
            if (mats[i].name != "DefaultMaterial")
                filtered.push_back(mats[i]);
        }
        return filtered;
    }

    void ModelLoader::processMesh(aiMesh* mesh,
        std::vector<float>& outVertices,
        std::vector<uint32_t>& outIndices,
        std::vector<Submesh>& outSubmeshes,
        const VertexLayout& layout,
        uint32_t stride, const glm::mat4& transform) {
        Submesh submesh;
        submesh.indexOffset = static_cast<uint32_t>(outIndices.size());
        submesh.materialIndex = mesh->mMaterialIndex;

        uint32_t vertexStart = static_cast<uint32_t>(outVertices.size() / (stride / sizeof(float)));

        glm::mat3 normalMatrix = glm::mat3(transform);

        // 预处理每顶点的骨骼影响（最多 4 个，权重最大的前 4 个）
        std::vector<glm::ivec4> boneIndices;
        std::vector<glm::vec4> boneWeights;
        if (mesh->mNumBones > 0) {
            boneIndices.resize(mesh->mNumVertices, glm::ivec4(0));
            boneWeights.resize(mesh->mNumVertices, glm::vec4(0.0f));
            for (unsigned b = 0; b < mesh->mNumBones; ++b) {
                aiBone* bone = mesh->mBones[b];
                int skeletonIndex = m_skeleton
                    ? static_cast<int>(m_skeleton->getBoneIndex(bone->mName.C_Str()))
                    : static_cast<int>(b);
                if (skeletonIndex < 0 || static_cast<uint32_t>(skeletonIndex) == UINT32_MAX)
                    skeletonIndex = 0;

                for (unsigned w = 0; w < bone->mNumWeights; ++w) {
                    uint32_t vid = bone->mWeights[w].mVertexId;
                    float wt = bone->mWeights[w].mWeight;
                    if (vid >= mesh->mNumVertices) continue;
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
            // 位置（预变换到 root 空间）
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

            // 蒙皮：只有 layout 声明了骨骼属性才写
            if (layout.getLocationForSemantic(VertexSemantic::BoneIndices) != UINT32_MAX) {
                if (!boneIndices.empty()) {
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
                    outVertices.push_back(0.0f); outVertices.push_back(0.0f); outVertices.push_back(0.0f); outVertices.push_back(0.0f);
                    outVertices.push_back(1.0f); outVertices.push_back(0.0f); outVertices.push_back(0.0f); outVertices.push_back(0.0f);
                }
            }
        }

        // 索引（偏移到全局顶点缓冲）
        for (unsigned f = 0; f < mesh->mNumFaces; ++f) {
            const aiFace& face = mesh->mFaces[f];
            for (unsigned i = 0; i < face.mNumIndices; ++i)
                outIndices.push_back(vertexStart + face.mIndices[i]);
        }

        submesh.indexCount = static_cast<uint32_t>(outIndices.size()) - submesh.indexOffset;
        outSubmeshes.push_back(submesh);
    }

    void ModelLoader::extractMaterials(const aiScene* scene,
        std::vector<MaterialParams>& outMaterials) {
        outMaterials.clear();
        outMaterials.reserve(scene->mNumMaterials);

        TextureLoader texLoader(m_resMgr);

        int missingDiffuse = 0, missingMetallic = 0, missingRoughness = 0;

        for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
            aiMaterial* aiMat = scene->mMaterials[i];
            MaterialParams mat;
            mat.index = i;
            aiString name;
            if (aiMat->Get(AI_MATKEY_NAME, name) == AI_SUCCESS) {
                mat.name = name.C_Str();
            }
            else {
                mat.name = "Material_" + std::to_string(i);
            }

            aiColor3D color(1.0f, 1.0f, 1.0f);
            if (aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
                mat.baseColor = glm::vec4(color.r, color.g, color.b, 1.0f);
            }
            else {
                ++missingDiffuse;
            }

            float metallic = 0.0f, roughness = 0.5f;
            if (aiMat->Get(AI_MATKEY_METALLIC_FACTOR, metallic) != AI_SUCCESS) {
                ++missingMetallic;
            }
            if (aiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) != AI_SUCCESS) {
                ++missingRoughness;
            }
            mat.metallic = metallic;
            mat.roughness = roughness;

            aiColor3D emissive(0.0f, 0.0f, 0.0f);
            if (aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS) {
                mat.emissiveColor = glm::vec3(emissive.r, emissive.g, emissive.b);
                mat.emissiveIntensity = 1.0f;
            }

            float opacity = 1.0f;
            if (aiMat->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS) {
                mat.baseColor.a = opacity;
                if (opacity < 1.0f) mat.alphaBlend = true;
            }

            auto processTexture = [&](aiTextureType type, const std::string& debugName,
                std::string& outPath, RHI::TextureHandle& outHandle) {
                    aiString texPath;
                    if (aiMat->GetTexture(type, 0, &texPath) == AI_SUCCESS) {
                        std::string path = texPath.C_Str();
                        if (path[0] == '*') {
                            int index = std::atoi(path.c_str() + 1);
                            if (index >= 0 && index < (int)scene->mNumTextures) {
                                auto result = loadEmbeddedTexture(texLoader, scene->mTextures[index], debugName);
                                if (result.texture.isValid()) {
                                    outHandle = result.texture;
                                    outPath = "";
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

            mat.useNormalMap = !mat.normalTexture.empty();
            mat.useEmissiveMap = !mat.emissiveTexture.empty();

            outMaterials.push_back(mat);
        }

        int totalMissing = missingDiffuse + missingMetallic + missingRoughness;
        if (totalMissing > 0)
            LOG_WARN("{} materials missing params ({} diffuse, {} metallic, {} roughness) — using defaults",
                totalMissing, missingDiffuse, missingMetallic, missingRoughness);
    }

    TextureLoadResult ModelLoader::loadEmbeddedTexture(TextureLoader& loader, aiTexture* tex, const std::string& debugName) {
        if (tex->mHeight == 0) {
            int width, height, channels;
            stbi_uc* pixels = stbi_load_from_memory(
                reinterpret_cast<stbi_uc*>(tex->pcData), tex->mWidth, &width, &height, &channels, 4);
            if (pixels) {
                auto res = loader.loadTextureFromMemory(pixels, width, height, RHI::Format::RGBA8_UNorm, debugName);
                stbi_image_free(pixels);
                return res;
            }
        }
        else {
            return loader.loadTextureFromMemory(
                reinterpret_cast<stbi_uc*>(tex->pcData), tex->mWidth, tex->mHeight,
                RHI::Format::RGBA8_UNorm, debugName);
        }
        return {};
    }

} // namespace StarryEngine::Assets
