#pragma once
#include <variant>
#include "../../assets/geometry/Geometry.hpp"
#include "../../assets/material/Material.hpp"
#include "../interface/RHI_RESOURCE_FACTORY.hpp"
#include "../../logging/Logger.hpp"

namespace StarryEngine::RenderGraph {

    class PassNode;

    class PassContext {
    public:
        PassContext(std::shared_ptr<RHI::ResourceManager> resMgr,
            const std::vector<RHI::PipelineHandle>& pipelines,
            uint32_t frameIndex,
            RHI::FramebufferHandle framebuffer)
            : mResMgr(resMgr), mPipelines(pipelines), mFrameIndex(frameIndex), mFramebuffer(framebuffer) {
        }

        RHI::PipelineHandle getPipeline(uint32_t subpassIndex) const {
            return (subpassIndex < mPipelines.size()) ? mPipelines[subpassIndex] : RHI::PipelineHandle::Null();
        }

        uint32_t getFrameIndex() const { return mFrameIndex; }
        RHI::FramebufferHandle getFramebuffer() const { return mFramebuffer; }
        std::shared_ptr<RHI::ResourceManager> getResourceManager() const { return mResMgr; }

    private:
        std::shared_ptr<RHI::ResourceManager> mResMgr;
        std::vector<RHI::PipelineHandle> mPipelines;
        uint32_t mFrameIndex = 0;
        RHI::FramebufferHandle mFramebuffer;
    };

    class ISubpassRecorder {
    public:
        explicit ISubpassRecorder(std::shared_ptr<RHI::ResourceManager> resMgr);
        virtual ~ISubpassRecorder();

        // 设置着色器
        void setVertexShader(const std::string& sourceCode, const std::string& debugName);
        void setFragmentShader(const std::string& sourceCode, const std::string& debugName);

        // 设置顶点/索引数据
        void setVertexBuffer(uint32_t binding, const std::vector<float>& vertices,
            const Assets::VertexLayout& layout, const std::string& debugName);
        void setIndexBuffer(const std::vector<uint32_t>& indices, const std::string& debugName);

        // 设置外部 Geometry
        void setGeometry(std::shared_ptr<Assets::Geometry> geometry);

        // 创建 Uniform Buffer 并添加到材质
        RHI::BufferHandle createAndAddUniformBuffer(size_t size, uint32_t binding,
            const std::string& debugName = "MaterialUBO",
            RHI::ShaderStage stageFlags = RHI::ShaderStage::Vertex | RHI::ShaderStage::Fragment);

        // 添加纹理
        RHI::TextureHandle addTexture(const std::string& filename,
            RHI::Format format,
            const std::string& debugName,
            uint32_t binding = 1,
            RHI::ShaderStage stageFlags = RHI::ShaderStage::Fragment);

        // 手动添加描述符绑定
        void addBinding(uint32_t binding, RHI::DescriptorType type, uint32_t count,
            RHI::ShaderStage stageFlags);

        // 创建管线布局（内部自动创建描述符集布局）
        RHI::PipelineLayoutHandle createPipelineLayout(const std::string& debugName);

        // 分配描述符集
        bool allocateDescriptorSet(RHI::DescriptorPoolHandle descriptorPool, uint32_t setIndex = 0);

        // 更新描述符集
        void updateDescriptorSet();

        // 纯虚函数：子类实现具体的录制命令逻辑
        virtual void recordCommands(RHI::RHICommandEncoder* encoder,
            const PassContext& pctx,
            uint32_t subpassIndex,
            uint32_t frameIndex) = 0;

        // ----- 内联 getter -----
        RHI::VertexInputState getVertexInputState() const { return mGeometry->getVertexInputState(); }
        RHI::ShaderHandle getVertexShader() const { return mMaterial->getVertexShader(); }
        RHI::ShaderHandle getFragmentShader() const { return mMaterial->getFragmentShader(); }
        RHI::DescriptorSetLayoutHandle getDescriptorSetLayout() const { return mDescriptorSetLayout; }
        RHI::DescriptorSetHandle getDescriptorSet() const { return mDescriptorSet; }
        RHI::PipelineLayoutHandle getPipelineLayout() const { return mPipelineLayout; }
        RHI::BufferHandle getVertexBufferHandle(uint32_t binding) const { return mGeometry->getVertexBufferHandle(binding); }
        RHI::BufferHandle getIndexBufferHandle() const { return mGeometry->getIndexBufferHandle(); }
        uint32_t getIndexCount() const { return mGeometry->getIndexCount(); }
        PassNode* getPassNode() const { return mPassNode; }
        void setPassNode(PassNode* pass) { mPassNode = pass; }

    protected:
        PassNode* mPassNode = nullptr;
        std::shared_ptr<Assets::Geometry> mGeometry;
        std::shared_ptr<Assets::Material> mMaterial;
        RHI::PipelineLayoutHandle mPipelineLayout;
        RHI::DescriptorSetLayoutHandle mDescriptorSetLayout;
        RHI::DescriptorSetHandle mDescriptorSet;
        std::shared_ptr<RHI::ResourceManager> mResMgr;
    };

} // namespace StarryEngine::RenderGraph