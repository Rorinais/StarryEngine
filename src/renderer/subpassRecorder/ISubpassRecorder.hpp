#pragma once
#include <variant>
#include "Geometry.hpp"
#include "Material.hpp"
#include"../interface/RHI_RESOURCE_FACTORY.hpp"

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
            if (subpassIndex < mPipelines.size()) {
                return mPipelines[subpassIndex];
            }
            return RHI::PipelineHandle::Null();
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
        ISubpassRecorder(std::shared_ptr<RHI::ResourceManager> resMgr)
            :mResMgr(resMgr) {
            mGeometry = std::make_shared<Geometry>(resMgr);
            mMaterial = std::make_shared<Material>(resMgr);
        }

        void setVertexShader(const std::string& sourceCode, const std::string& debugName) {
            mMaterial->setVertexShader(sourceCode, debugName);
        }
        void setFragmentShader(const std::string& sourceCode, const std::string& debugName) {
            mMaterial->setFragmentShader(sourceCode, debugName);
        }

        void setVertexBuffer(uint32_t binding, const std::vector<float>& vertices,
            const VertexLayout& layout, const std::string& debugName) {
            mGeometry->setVertexBuffer(binding, vertices, layout, debugName);
        }

        void setIndexBuffer(const std::vector<uint32_t>& indices, const std::string& debugName) {
            mGeometry->setIndexBuffer(indices, debugName);
        }

        RHI::BufferHandle createAndAddUniformBuffer(size_t size, uint32_t binding,
            const std::string& debugName = "MaterialUBO",
            RHI::ShaderStage stageFlags = RHI::ShaderStage::Vertex | RHI::ShaderStage::Fragment) {
            return mMaterial->createAndAddUniformBuffer(size, binding, debugName, stageFlags);
        }

        RHI::TextureHandle addTexture(const std::string& filename,
            RHI::Format format,
            const std::string& debugName,
            uint32_t binding = 1,
            RHI::ShaderStage stageFlags = RHI::ShaderStage::Fragment) {
            return mMaterial->addTexture(filename, format, debugName, binding, stageFlags);
        }

        RHI::PipelineLayoutHandle createPipelineLayout(const std::string& debugName) {
            RHI::PipelineLayoutDesc layoutDesc;
            layoutDesc.descriptorSetLayouts = { mMaterial->getDescriptorSetLayout() };
            layoutDesc.pushConstants = {};
            layoutDesc.debugName = debugName;
            mPipelineLayout = mResMgr->createPipelineLayout(layoutDesc, debugName);
            return mPipelineLayout;
        }

        void allocateDescriptorSet(RHI::DescriptorPoolHandle descriptorPool, uint32_t setIndex = 0) {
            mMaterial->allocateDescriptorSet(descriptorPool, mPipelineLayout, setIndex);
        }
        void updateDescriptorSet() {
            mMaterial->updateDescriptorSet();
        }

        void createDescriptorSetLayout() {
            mMaterial->createDescriptorSetLayout();
        }

        void destroyFramebuffers() {
            for (auto & fbo :mframeBuffers){
                mResMgr->destroy(fbo);
            }
            mframeBuffers.clear();
        }

        std::vector<RHI::FramebufferHandle> getFramebuffers() { return mframeBuffers; }

        void addInputAttachmentBinding(uint32_t binding, RHI::ShaderStage stageFlags = RHI::ShaderStage::Fragment) {
            mMaterial->addInputAttachmentBinding(binding, stageFlags);
        }

        void setPassNode(PassNode* pass) { mPassNode = pass; }

        virtual ~ISubpassRecorder() = default;
        virtual void recordCommands(RHI::RHICommandEncoder* encoder,
            const PassContext& pctx,
            uint32_t subpassIndex,   
            uint32_t frameIndex) = 0;

        RHI::BufferHandle getVertexBufferHandle(uint32_t binding) const {
            return mGeometry->getVertexBufferHandle(binding);
        }
        RHI::BufferHandle getIndexBufferHandle() const { return mGeometry->getIndexBufferHandle(); }
        uint32_t getIndexCount() const { return mGeometry->getIndexCount(); }

        // 获取顶点输入状态（用于管线创建）
        RHI::VertexInputState getVertexInputState() const { return mGeometry->getVertexInputState(); }

        RHI::ShaderHandle getVertexShader() const { return mMaterial->getVertexShader(); }
        RHI::ShaderHandle getFragmentShader() const { return mMaterial->getFragmentShader(); }
        RHI::DescriptorSetLayoutHandle getDescriptorSetLayout() const { return mMaterial->getDescriptorSetLayout(); }
        RHI::DescriptorSetHandle getDescriptorSet() const { return mMaterial->getDescriptorSet(); }
        RHI::PipelineLayoutHandle getPipelineLayout() const { return mPipelineLayout; }
        PassNode* getPassNode() { return mPassNode; }

    protected:
        PassNode* mPassNode = nullptr;
        std::shared_ptr<Geometry> mGeometry;
        std::shared_ptr<Material> mMaterial;
        RHI::PipelineLayoutHandle mPipelineLayout;
        std::shared_ptr<RHI::ResourceManager> mResMgr;

        std::vector<RHI::FramebufferHandle> mframeBuffers;
    };
}
