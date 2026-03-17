#include "ModelLoader.hpp"
#include "../../logging/Logger.hpp"
#include <glm/glm.hpp>
#include <stb_image.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <functional>
#include <unordered_map>

namespace StarryEngine::Assets {

    bool ModelLoader::loadFromFile(
        std::shared_ptr<RHI::ResourceManager> resMgr,
        const std::string& path,Geometry& outGeometry,
        std::vector<MaterialParams>& outMaterials) {
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

        // 临时存储：所有顶点数据
        std::vector<float> vertices;
        std::vector<uint32_t> indices;
        std::vector<Submesh> submeshes;

        // 顶点布局构建器
        VertexLayout layout;

        // 收集所有网格中出现的属性，构建统一布局
        bool hasPos = false, hasNormal = false, hasUV = false, hasTangent = false;
        for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
            aiMesh* mesh = scene->mMeshes[i];
            if (mesh->HasPositions()) hasPos = true;
            if (mesh->HasNormals()) hasNormal = true;
            if (mesh->HasTextureCoords(0)) hasUV = true;
            if (mesh->HasTangentsAndBitangents()) hasTangent = true;
        }

        uint32_t binding = 0;
        uint32_t stride = 0;
        uint32_t offset = 0;

        // 约定 location 顺序：位置=0，法线=1，UV=2，切线=3
        if (hasPos) {
            layout.addAttribute(0, binding, RHI::Format::RGB32_Float);
            offset += 12;
        }
        if (hasNormal) {
            layout.addAttribute(1, binding, RHI::Format::RGB32_Float);
            offset += 12;
        }
        if (hasUV) {
            layout.addAttribute(2, binding, RHI::Format::RG32_Float);
            offset += 8;
        }
        if (hasTangent) {
            layout.addAttribute(3, binding, RHI::Format::RGB32_Float);
            offset += 12;
        }
        layout.addBinding(binding, stride = offset, RHI::VertexInputRate::PerVertex);

        // 递归处理节点，填充顶点和索引
        std::function<void(aiNode*, const glm::mat4&)> processNode;
        processNode = [&](aiNode* node, const glm::mat4& parentTransform) {
            aiMatrix4x4 aiTransform = node->mTransformation;
            glm::mat4 nodeTransform(
                aiTransform.a1, aiTransform.b1, aiTransform.c1, aiTransform.d1,
                aiTransform.a2, aiTransform.b2, aiTransform.c2, aiTransform.d2,
                aiTransform.a3, aiTransform.b3, aiTransform.c3, aiTransform.d3,
                aiTransform.a4, aiTransform.b4, aiTransform.c4, aiTransform.d4
            );
            glm::mat4 globalTransform = parentTransform * nodeTransform;

            for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
                aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
                processMesh(mesh, vertices, indices, submeshes, layout, stride, globalTransform);
            }
            for (unsigned int i = 0; i < node->mNumChildren; ++i) {
                processNode(node->mChildren[i], parentTransform);
            }
            };
        processNode(scene->mRootNode, glm::mat4(1.0f));

        // 提取原始材质参数
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

        // --- 一次性将最终数据设置到 outGeometry ---
        outGeometry.setVertices(vertices);
        outGeometry.setIndices(indices);
        outGeometry.setSubmeshes(submeshes);
        outGeometry.setVertexLayout(layout);

        return true;
    }

    // 辅助函数：处理单个网格，填充顶点/索引，并创建 Submesh
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

    // 辅助函数：提取材质参数
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
            // 压缩格式（如 PNG、JPG）
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
            // 原始 RGBA 格式
            return loader.loadTextureFromMemory(tex->pcData, tex->mWidth, tex->mHeight,
                RHI::Format::RGBA8_UNorm,
                debugName);
        }
    }
} // namespace StarryEngine::Assets