#include "ModelLoader.hpp"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/material.h>
#include <iostream>
#include <filesystem>

namespace StarryEngine {

    ModelLoader::ModelLoader(VulkanCore::Ptr core, CommandPool::Ptr cmdP)
        : vkCore(core), cmd_Pool(cmdP) {
    }

    bool ModelLoader::loadMesh(const std::string& filename) {
        // 清空之前的数据
        mPos_Normal_Tex.clear();
        indices.clear();
        mMeshEntry.clear();
        mMaterials.clear();
        mBoneMapping.clear();
        mBoneInfo.clear();
        mLoadedTextures.clear();

        Assimp::Importer importer;
        
        // 设置导入标志
        unsigned int flags = 
            aiProcess_Triangulate |                // 将所有图元转换为三角形
            aiProcess_GenSmoothNormals |           // 生成平滑法线
            aiProcess_FlipUVs |                    // 翻转UV坐标（用于OpenGL/Vulkan坐标系）
            aiProcess_CalcTangentSpace |           // 计算切线空间（用于法线贴图）
            aiProcess_JoinIdenticalVertices |      // 合并相同顶点
            aiProcess_ImproveCacheLocality |       // 优化缓存局部性
            aiProcess_SortByPType |                // 按类型排序
            aiProcess_FindInvalidData |            // 查找无效数据
            aiProcess_GenUVCoords |                // 生成UV坐标
            aiProcess_TransformUVCoords |          // 变换UV坐标
            aiProcess_OptimizeMeshes |             // 优化网格
            aiProcess_OptimizeGraph |              // 优化场景图
            aiProcess_ValidateDataStructure;       // 验证数据结构
        
        const aiScene* scene = importer.ReadFile(filename.c_str(), flags);
        
        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            std::cerr << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
            return false;
        }
        
        // 获取文件目录路径（用于加载纹理）
        std::filesystem::path filepath(filename);
        std::string directory = filepath.parent_path().string();
        
        // 处理全局逆变换
        globalInverseTransform = glm::mat4(1.0f);
        if (scene->mRootNode) {
            aiMatrix4x4 aiMat = scene->mRootNode->mTransformation;
            globalInverseTransform = glm::inverse(glm::mat4(
                aiMat.a1, aiMat.b1, aiMat.c1, aiMat.d1,
                aiMat.a2, aiMat.b2, aiMat.c2, aiMat.d2,
                aiMat.a3, aiMat.b3, aiMat.c3, aiMat.d3,
                aiMat.a4, aiMat.b4, aiMat.c4, aiMat.d4
            ));
        }
        
        // 处理材质
        processMaterials(scene,filename);
        
        // 处理节点和网格
        processNode(scene->mRootNode, scene);
        
        std::cout << "Model loaded successfully: " << filename << std::endl;
        std::cout << "Vertices: " << mPos_Normal_Tex.size() << std::endl;
        std::cout << "Indices: " << indices.size() << std::endl;
        std::cout << "Meshes: " << mMeshEntry.size() << std::endl;
        std::cout << "Materials: " << mMaterials.size() << std::endl;
        
        return true;
    }

    void ModelLoader::processNode(aiNode* node, const aiScene* scene) {
        // 处理当前节点中的所有网格
        for (unsigned int i = 0; i < node->mNumMeshes; i++) {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            processMesh(mesh, scene);
        }
        
        // 递归处理子节点
        for (unsigned int i = 0; i < node->mNumChildren; i++) {
            processNode(node->mChildren[i], scene);
        }
    }

    void ModelLoader::processMesh(aiMesh* mesh, const aiScene* scene) {
        MeshEntry entry;
        entry.BaseVertex = static_cast<uint32_t>(mPos_Normal_Tex.size());
        entry.BaseIndex = static_cast<uint32_t>(indices.size());
        entry.MaterialIndex = mesh->mMaterialIndex;
        
        // 记录顶点数据前的数量，用于计算实际加载的顶点数
        size_t startVertex = mPos_Normal_Tex.size();
        
        // 处理顶点数据
        for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
            VertexPosNormalTex vertex;
            
            // 位置
            vertex.position = glm::vec3(
                mesh->mVertices[i].x,
                mesh->mVertices[i].y,
                mesh->mVertices[i].z
            );
            
            // 法线
            if (mesh->HasNormals()) {
                vertex.normal = glm::vec3(
                    mesh->mNormals[i].x,
                    mesh->mNormals[i].y,
                    mesh->mNormals[i].z
                );
            }
            
            // 纹理坐标（只使用第一组UV）
            if (mesh->HasTextureCoords(0)) {
                vertex.texCoord = glm::vec2(
                    mesh->mTextureCoords[0][i].x,
                    mesh->mTextureCoords[0][i].y
                );
            } else {
                vertex.texCoord = glm::vec2(0.0f, 0.0f);
            }
            
            mPos_Normal_Tex.push_back(vertex);
        }
        
        // 处理索引数据
        for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++) {
                indices.push_back(face.mIndices[j] + entry.BaseVertex);
            }
        }
        
        entry.NumIndices = static_cast<uint32_t>(indices.size() - entry.BaseIndex);
        mMeshEntry.push_back(entry);
        
        // 处理骨骼
        if (mesh->HasBones()) {
            processBones(mesh);
        }
    }

    void ModelLoader::processMaterials(const aiScene* scene,const std::string& filename) {
        std::filesystem::path filepath(filename);
        std::string directory = filepath.parent_path().string();
        
        mMaterials.resize(scene->mNumMaterials);
        
        for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
            aiMaterial* material = scene->mMaterials[i];
            MaterialInfo& matInfo = mMaterials[i];
            
            aiColor3D color;
            float shininess;
            
            // 环境光
            if (material->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS) {
                matInfo.ambient = glm::vec3(color.r, color.g, color.b);
            } else {
                matInfo.ambient = glm::vec3(0.1f, 0.1f, 0.1f);
            }
            
            // 漫反射
            if (material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
                matInfo.diffuse = glm::vec3(color.r, color.g, color.b);
            } else {
                matInfo.diffuse = glm::vec3(0.8f, 0.8f, 0.8f);
            }
            
            // 镜面反射
            if (material->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS) {
                matInfo.specular = glm::vec3(color.r, color.g, color.b);
            } else {
                matInfo.specular = glm::vec3(0.5f, 0.5f, 0.5f);
            }
            
            // 高光强度
            if (material->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS) {
                matInfo.shininess = shininess;
            } else {
                matInfo.shininess = 32.0f;
            }
            
            // 漫反射纹理
            aiString texturePath;
            if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS) {
                matInfo.diffuseTexture = directory + "/" + texturePath.C_Str();
            }
            
            // 镜面反射纹理
            if (material->GetTexture(aiTextureType_SPECULAR, 0, &texturePath) == AI_SUCCESS) {
                matInfo.specularTexture = directory + "/" + texturePath.C_Str();
            }
            
            // 法线纹理
            if (material->GetTexture(aiTextureType_NORMALS, 0, &texturePath) == AI_SUCCESS ||
                material->GetTexture(aiTextureType_HEIGHT, 0, &texturePath) == AI_SUCCESS) {
                matInfo.normalTexture = directory + "/" + texturePath.C_Str();
            }
        }
    }

    void ModelLoader::processBones(aiMesh* mesh) {
        for (unsigned int i = 0; i < mesh->mNumBones; i++) {
            aiBone* bone = mesh->mBones[i];
            std::string boneName(bone->mName.data);
            
            uint32_t boneIndex = 0;
            
            if (mBoneMapping.find(boneName) == mBoneMapping.end()) {
                boneIndex = static_cast<uint32_t>(mBoneInfo.size());
                mBoneMapping[boneName] = boneIndex;
                
                BoneInfo info;
                aiMatrix4x4 offsetMatrix = bone->mOffsetMatrix;
                info.boneOffset = glm::mat4(
                    offsetMatrix.a1, offsetMatrix.b1, offsetMatrix.c1, offsetMatrix.d1,
                    offsetMatrix.a2, offsetMatrix.b2, offsetMatrix.c2, offsetMatrix.d2,
                    offsetMatrix.a3, offsetMatrix.b3, offsetMatrix.c3, offsetMatrix.d3,
                    offsetMatrix.a4, offsetMatrix.b4, offsetMatrix.c4, offsetMatrix.d4
                );
                mBoneInfo.push_back(info);
            } else {
                boneIndex = mBoneMapping[boneName];
            }
            
            // 为每个受骨骼影响的顶点分配权重
            for (unsigned int j = 0; j < bone->mNumWeights; j++) {
                aiVertexWeight weight = bone->mWeights[j];
                uint32_t vertexID = weight.mVertexId;
                
                // 这里需要为顶点添加骨骼权重数据
                // 由于VertexPosNormalTex结构没有骨骼权重，你可能需要扩展它
                // 或者创建新的顶点结构
            }
        }
    }

    void ModelLoader::generateBuffer() {
        if (mPos_Normal_Tex.empty() || indices.empty()) {
            std::cerr << "No vertex or index data to generate buffers!" << std::endl;
            return;
        }

        try {
            auto logicalDevice = vkCore->getLogicalDevice();
            if (!logicalDevice) {
                throw std::runtime_error("Failed to get logical device");
            }
            
            if (!cmd_Pool) {
                throw std::runtime_error("CommandPool is null");
            }
            
            // 1. 创建顶点数组缓冲区
            mVAO_S_Ptr = VertexArrayBuffer::create(logicalDevice, cmd_Pool);
            mVAO_S_Ptr->upload<VertexPosNormalTex>(0, mPos_Normal_Tex);
            
            // 2. 创建索引缓冲区
            mIBO_S_Ptr = std::make_shared<IndexBuffer>(logicalDevice, cmd_Pool);
            mIBO_S_Ptr->loadData(indices);
            
            std::cout << "Buffers created successfully!" << std::endl;
            std::cout << "Vertices: " << mPos_Normal_Tex.size() << std::endl;
            std::cout << "Indices: " << indices.size() << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "Failed to generate buffers: " << e.what() << std::endl;
            throw;
        }
    }
}