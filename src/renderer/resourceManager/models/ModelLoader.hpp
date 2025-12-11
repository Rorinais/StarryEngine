#pragma once 
#include <assimp/scene.h>
#include "../buffers/VertexLayouts.hpp"
#include "../buffers/IndexBuffer.hpp"
#include "../buffers/VertexArrayBuffer.hpp"
#include "../../../renderer/backends/vulkan/vulkanCore/VulkanCore.hpp"
#include "../../../renderer/backends/vulkan/renderContext/CommandPool.hpp"

namespace StarryEngine{
    class ModelLoader{
    public:
        ModelLoader(VulkanCore::Ptr core,CommandPool::Ptr cmdP):vkCore(core),cmd_Pool(cmdP){}
        ~ModelLoader(){}

        void loadMesh(std::string name);
        void generateBuffer();

        std::vector<IndexBuffer::Ptr> getIndexBuffers();
        std::vector<VertexArrayBuffer::Ptr> getVertexBuffers();

    private:
    VulkanCore::Ptr vkCore;
    CommandPool::Ptr cmd_Pool; 
    std::vector<VertexPosNormalTex> VPNT;

    std::vector<IndexBuffer::Ptr> mIBO_S_Ptr;
    std::vector<VertexArrayBuffer::Ptr> mVAO_S_Ptr;
    };
}
