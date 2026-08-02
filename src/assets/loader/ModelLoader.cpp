#include "ModelLoader.hpp"
#include "../../logging/Logger.hpp"
#include <glm/glm.hpp>
#include <stb_image.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <limits>
#include <assimp/postprocess.h>
#include <functional>
#include <unordered_map>

namespace StarryEngine::Assets {

    bool ModelLoader::loadFromFile(
        std::shared_ptr<RHI::ResourceManager> resMgr,
        const std::string& path,Geometry& outGeometry,
        std::vector<MaterialParams>& outMaterials,
        Skeleton* outSkeleton) {
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

        // 递归处理节点，填充顶点和索引
        std::function<void(aiNode*, const glm::mat4&)> processNode;
        processNode = [&](aiNode* node, const glm::mat4& parentTransform) {
            glm::mat4 nodeTransform = toGlm(node->mTransformation);
            glm::mat4 globalTransform = parentTransform * nodeTransform;

            for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
                aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
                processMesh(mesh, vertices, indices, submeshes, layout, currentOffset, globalTransform);
            }
            // 子节点继承当前节点的全局变换（修复：之前误传 parentTransform，子节点会错位）
            for (unsigned int i = 0; i < node->mNumChildren; ++i) {
                processNode(node->mChildren[i], globalTransform);
            }
            };
        processNode(scene->mRootNode, glm::mat4(1.0f));

        // ── 构建 Skeleton：节点树层级 + 逆绑定矩阵 ──
        if (outSkeleton && scene->mRootNode) {
            // 递归收集场景节点树 → 骨骼（父索引）。节点 mTransformation = 绑定姿势局部变换
            std::function<void(aiNode*, int)> collect;
            collect = [&](aiNode* node, int parentIndex) {
                uint32_t idx = static_cast<uint32_t>(outSkeleton->bones.size());
                Bone bone;
                bone.name = node->mName.C_Str();
                bone.parentIndex = parentIndex;
                bone.localTransform = toGlm(node->mTransformation);
                outSkeleton->bones.push_back(bone);
                outSkeleton->nameToIndex[bone.name] = idx;
                for (unsigned c = 0; c < node->mNumChildren; ++c)
                    collect(node->mChildren[c], static_cast<int>(idx));
            };
            collect(scene->mRootNode, -1);

            // 按名字把 mesh->mBones 的逆绑定矩阵（mOffsetMatrix）应用到对应骨骼
            for (unsigned i = 0; i < scene->mNumMeshes; ++i) {
                aiMesh* mesh = scene->mMeshes[i];
                for (unsigned b = 0; b < mesh->mNumBones; ++b) {
                    aiBone* abone = mesh->mBones[b];
                    uint32_t bidx = outSkeleton->getBoneIndex(abone->mName.C_Str());
                    if (bidx != UINT32_MAX)
                        outSkeleton->bones[bidx].inverseBindMatrix = toGlm(abone->mOffsetMatrix);
                }
            }
            LOG_INFO("[Skeleton] {} bones from node hierarchy ({} skinned with inverse bind)",outSkeleton->bones.size(), outSkeleton->getBoneCount());
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
        uint32_t stride, const glm::mat4& transform) {
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
                for (unsigned w = 0; w < bone->mNumWeights; ++w) {
                    uint32_t vid = bone->mWeights[w].mVertexId;
                    float wt = bone->mWeights[w].mWeight;
                    if (vid >= mesh->mNumVertices) continue;
                    // 填充到第一个空槽（assimp 权重通常已按降序）
                    for (int s = 0; s < 4; ++s) {
                        if (boneWeights[vid][s] == 0.0f) {
                            boneIndices[vid][s] = static_cast<int>(b);
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