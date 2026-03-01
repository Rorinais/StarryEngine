#pragma once
#include <stb_image.h>
#include <variant>
#include "../interface/RHI_RESOURCE_MANAGER.hpp"

namespace std {
    template<>
    struct hash<StarryEngine::RHI::TextureHandle> {
        size_t operator()(const StarryEngine::RHI::TextureHandle& handle) const noexcept {
            return typename StarryEngine::RHI::TextureHandle::Hash{}(handle);
        }
    };
}

namespace StarryEngine::RenderGraph {

    struct DescriptorResourceInfo {
        std::variant<
            std::monostate, // 空状态
            RHI::DescriptorBufferInfo,   // Uniform/Storage Buffer
            RHI::DescriptorImageInfo      // Combined Image Sampler
        > data;

        RHI::DescriptorType type; // 描述符类型

        DescriptorResourceInfo() : type(RHI::DescriptorType::StorageBuffer) {}
        explicit DescriptorResourceInfo(const RHI::DescriptorBufferInfo& bufferInfo)
            : data(bufferInfo), type(RHI::DescriptorType::UniformBuffer) {
        }
        explicit DescriptorResourceInfo(const RHI::DescriptorImageInfo& imageInfo)
            : data(imageInfo), type(RHI::DescriptorType::CombinedImageSampler) {
        }
    };

    class Material {
    public:
        explicit Material(std::shared_ptr<RHI::ResourceManager> resMgr)
            : mResMgr(resMgr) {
        }

        void setVertexShader(const std::string& sourceCode, const std::string& debugName);

        void setFragmentShader(const std::string& sourceCode, const std::string& debugName);

        void addUniformBuffer(RHI::BufferHandle buffer, size_t size, uint32_t binding,
            RHI::ShaderStage stageFlags = RHI::ShaderStage::Vertex | RHI::ShaderStage::Fragment);

        RHI::BufferHandle createAndAddUniformBuffer(size_t size, uint32_t binding,
            const std::string& debugName = "MaterialUBO",
            RHI::ShaderStage stageFlags = RHI::ShaderStage::Vertex | RHI::ShaderStage::Fragment);

        RHI::TextureHandle addTexture(const std::string& filename,
            RHI::Format format,
            const std::string& debugName,
            uint32_t binding = 1,
            RHI::ShaderStage stageFlags = RHI::ShaderStage::Fragment);

        bool createDescriptorSetLayout();

        bool allocateDescriptorSet(RHI::DescriptorPoolHandle pool,
            RHI::PipelineLayoutHandle pipelineLayout,
            uint32_t setIndex = 0);

        void updateDescriptorSet();

        // --- Getters ---
        RHI::ShaderHandle getVertexShader() const { return mVertexShader; }
        RHI::ShaderHandle getFragmentShader() const { return mFragmentShader; }
        RHI::DescriptorSetLayoutHandle getDescriptorSetLayout() const { return mDescriptorSetLayout; }
        RHI::DescriptorSetHandle getDescriptorSet() const { return mDescriptorSet; }
        RHI::PipelineLayoutHandle getPipelineLayout() const { return mPipelineLayout; }
        uint32_t getSetIndex() const { return mSetIndex; }

    private:
        void addBinding(uint32_t binding, RHI::DescriptorType type, uint32_t count,
            RHI::ShaderStage stageFlags) {
            mBindings[binding] = { type, count, stageFlags };
        }

        struct BindingInfo {
            RHI::DescriptorType type;
            uint32_t count;
            RHI::ShaderStage stageFlags;
        };

        std::shared_ptr<RHI::ResourceManager> mResMgr;

        // 着色器
        RHI::ShaderHandle mVertexShader;
        RHI::ShaderHandle mFragmentShader;

        // 所有描述符资源（按 binding 索引）
        std::unordered_map<uint32_t, DescriptorResourceInfo> mResources;

        // 描述符集布局信息
        std::unordered_map<uint32_t, BindingInfo> mBindings;
        RHI::DescriptorSetLayoutHandle mDescriptorSetLayout;
        RHI::DescriptorSetHandle mDescriptorSet;
        RHI::PipelineLayoutHandle mPipelineLayout;
        uint32_t mSetIndex = 0;
    };
}