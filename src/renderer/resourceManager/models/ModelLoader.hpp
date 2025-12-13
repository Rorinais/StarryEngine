#pragma once 
#include <assimp/scene.h>
#include "../buffers/VertexLayouts.hpp"
#include "../buffers/IndexBuffer.hpp"
#include "../textures/Texture.hpp"
#include "../buffers/VertexArrayBuffer.hpp"
#include "../../../renderer/backends/vulkan/vulkanCore/VulkanCore.hpp"
#include "../../../renderer/backends/vulkan/renderContext/CommandPool.hpp"

namespace StarryEngine{
    struct MeshEntry{
        unsigned int NumIndices;
        unsigned int BaseVertex;
        unsigned int BaseIndex;
        unsigned int MaterialIndex;
    };

    struct MaterialInfo {
        glm::vec3 ambient;
        glm::vec3 diffuse;
        glm::vec3 specular;
        float shininess;
        std::string diffuseTexture;
        std::string specularTexture;
        std::string normalTexture;
    };

    class ModelLoader{
    public:
        ModelLoader(VulkanCore::Ptr core, CommandPool::Ptr cmdP);
        ~ModelLoader(){}

        bool loadMesh(const std::string& filename);
        void generateBuffer();

        IndexBuffer::Ptr getIndexBuffers() const { return mIBO_S_Ptr; }
        VertexArrayBuffer::Ptr getVertexBuffers() const { return mVAO_S_Ptr; }
        
        const std::vector<MeshEntry>& getMeshEntries() const { return mMeshEntry; }
        const std::vector<MaterialInfo>& getMaterials() const { return mMaterials; }
        size_t getVertexCount() const { return mPos_Normal_Tex.size(); }
        size_t getIndexCount() const { return indices.size(); }

    private:
        void processNode(aiNode* node, const aiScene* scene);
        void processMesh(aiMesh* mesh, const aiScene* scene);
        void processMaterials(const aiScene* scene,const std::string& filename);
        void processBones(aiMesh* mesh);
        
        VulkanCore::Ptr vkCore;
        CommandPool::Ptr cmd_Pool; 
        
        std::vector<VertexPosNormalTex> mPos_Normal_Tex;
        std::vector<uint32_t> indices;
        
        std::vector<MeshEntry> mMeshEntry;
        std::vector<MaterialInfo> mMaterials;
        
        // 用于存储纹理路径
        std::unordered_map<std::string, std::shared_ptr<Texture>> mLoadedTextures;
        
        IndexBuffer::Ptr mIBO_S_Ptr;
        VertexArrayBuffer::Ptr mVAO_S_Ptr;
        
        // 变换矩阵
        glm::mat4 globalInverseTransform;
        
        // 骨骼数据
        struct BoneInfo {
            glm::mat4 boneOffset;
            glm::mat4 finalTransformation;
        };
        std::unordered_map<std::string, uint32_t> mBoneMapping;
        std::vector<BoneInfo> mBoneInfo;
        std::vector<glm::mat4> mBoneTransforms;
    };
}

