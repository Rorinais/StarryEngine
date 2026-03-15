#include "ISubpassRecorder.hpp"
#include "../../assets/loader/ShaderLoader.hpp"
#include "../../assets/loader/TextureLoader.hpp"
#include "../../logging/Logger.hpp"

namespace StarryEngine::RenderGraph {

    ISubpassRecorder::ISubpassRecorder(std::shared_ptr<RHI::ResourceManager> resMgr)
        : mResMgr(resMgr) {
        mGeometry = std::make_shared<Assets::Geometry>(resMgr);
        mMaterial = std::make_shared<Assets::Material>(resMgr);
    }

    ISubpassRecorder::~ISubpassRecorder() {
        if (mPipelineLayout.isValid()) mResMgr->destroy(mPipelineLayout);
        if (mDescriptorSetLayout.isValid()) mResMgr->destroy(mDescriptorSetLayout);
        if (mDescriptorSet.isValid()) mResMgr->destroy(mDescriptorSet);
    }

    void ISubpassRecorder::setVertexShader(const std::string& sourceCode, const std::string& debugName) {
        Assets::ShaderLoader shaderLoader(mResMgr);
        auto vertInfo = shaderLoader.loadFromSource(sourceCode, RHI::ShaderStage::Vertex, debugName);
        if (vertInfo) {
            mMaterial->setVertexShader(vertInfo->module);
        }
        else {
            LOG_ERROR("Failed to load vertex shader: {}", debugName);
        }
    }

    void ISubpassRecorder::setFragmentShader(const std::string& sourceCode, const std::string& debugName) {
        Assets::ShaderLoader shaderLoader(mResMgr);
        auto fragInfo = shaderLoader.loadFromSource(sourceCode, RHI::ShaderStage::Fragment, debugName);
        if (fragInfo) {
            mMaterial->setFragmentShader(fragInfo->module);
        }
        else {
            LOG_ERROR("Failed to load fragment shader: {}", debugName);
        }
    }

    void ISubpassRecorder::setVertexBuffer(uint32_t binding, const std::vector<float>& vertices,
        const Assets::VertexLayout& layout, const std::string& debugName) {
        mGeometry->setVertexBuffer(binding, vertices, layout, debugName);
    }

    void ISubpassRecorder::setIndexBuffer(const std::vector<uint32_t>& indices, const std::string& debugName) {
        mGeometry->setIndexBuffer(indices, debugName);
    }

    void ISubpassRecorder::setGeometry(std::shared_ptr<Assets::Geometry> geometry) {
        mGeometry = geometry;
    }

    RHI::BufferHandle ISubpassRecorder::createAndAddUniformBuffer(size_t size, uint32_t binding,
        const std::string& debugName,
        RHI::ShaderStage stageFlags) {
        return mMaterial->createAndAddUniformBuffer(size, binding, debugName, stageFlags);
    }

    RHI::TextureHandle ISubpassRecorder::addTexture(const std::string& filename,
        RHI::Format format,
        const std::string& debugName,
        uint32_t binding,
        RHI::ShaderStage stageFlags) {
        return mMaterial->addTexture(filename, format, debugName, binding, stageFlags);
    }

    void ISubpassRecorder::addBinding(uint32_t binding, RHI::DescriptorType type, uint32_t count,
        RHI::ShaderStage stageFlags) {
        mMaterial->addBinding(binding, type, count, stageFlags);
    }

    RHI::PipelineLayoutHandle ISubpassRecorder::createPipelineLayout(const std::string& debugName) {
        auto bindings = mMaterial->getBindings();
        if (bindings.empty()) {
            LOG_WARN("No bindings defined in material, descriptor set layout will be empty.");
        }

        RHI::DescriptorSetLayoutDesc layoutDesc;
        layoutDesc.bindings = bindings;
        layoutDesc.debugName = debugName + "_DSLayout";
        mDescriptorSetLayout = mResMgr->createDescriptorSetLayout(layoutDesc);
        if (!mDescriptorSetLayout.isValid()) {
            LOG_ERROR("Failed to create descriptor set layout for {}", debugName);
            return RHI::PipelineLayoutHandle::Null();
        }

        RHI::PipelineLayoutDesc pipelineDesc;
        pipelineDesc.descriptorSetLayouts = { mDescriptorSetLayout };
        pipelineDesc.pushConstants = {};
        pipelineDesc.debugName = debugName + "_PipelineLayout";
        mPipelineLayout = mResMgr->createPipelineLayout(pipelineDesc);
        return mPipelineLayout;
    }

    bool ISubpassRecorder::allocateDescriptorSet(RHI::DescriptorPoolHandle descriptorPool, uint32_t setIndex) {
        if (!mDescriptorSetLayout.isValid()) {
            LOG_ERROR("Descriptor set layout not created. Call createPipelineLayout first.");
            return false;
        }

        RHI::DescriptorSetDesc desc;
        desc.descriptorPool = descriptorPool;
        desc.pipelineLayout = mPipelineLayout;  // 可选，某些实现可能不需要
        desc.setIndex = setIndex;
        desc.debugName = "RecorderDescriptorSet";

        mDescriptorSet = mResMgr->createDescriptorSet(desc);
        if (!mDescriptorSet.isValid()) {
            LOG_ERROR("Failed to allocate descriptor set");
            return false;
        }
        return true;
    }

    void ISubpassRecorder::updateDescriptorSet() {
        if (!mDescriptorSet.isValid()) {
            LOG_ERROR("Descriptor set not allocated, cannot update.");
            return;
        }

        auto* set = mResMgr->getDescriptorSet(mDescriptorSet);
        if (!set) return;

        for (const auto& [binding, resource] : mMaterial->getResources()) {
            if (std::holds_alternative<RHI::DescriptorBufferInfo>(resource.data)) {
                const auto& bufInfo = std::get<RHI::DescriptorBufferInfo>(resource.data);
                auto* buffer = mResMgr->getBuffer(bufInfo.buffer);
                if (buffer) {
                    set->writeBuffer(binding, 0, buffer, bufInfo.offset, bufInfo.range);
                }
            }
            else if (std::holds_alternative<RHI::DescriptorImageInfo>(resource.data)) {
                const auto& imgInfo = std::get<RHI::DescriptorImageInfo>(resource.data);
                auto* texture = mResMgr->getTexture(imgInfo.texture);
                if (!texture) continue;

                if (resource.type == RHI::DescriptorType::InputAttachment) {
                    set->writeInputAttachment(binding, 0, texture, imgInfo.imageLayout);
                }
                else {
                    auto* sampler = mResMgr->getSampler(imgInfo.sampler);
                    if (sampler) {
                        set->writeTexture(binding, 0, texture, sampler, imgInfo.imageLayout);
                    }
                }
            }
        }
        set->update();
    }

} // namespace StarryEngine::RenderGraph