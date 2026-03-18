#pragma once
#include <stb_image.h>
#include <variant>
#include "../AssetType.hpp"
#include "../../scene/SceneType.hpp"
#include "../../assets/loader/ShaderLoader.hpp"

namespace StarryEngine::Assets {
    struct DescriptorResourceInfo {
        std::variant<
            std::monostate,
            RHI::DescriptorBufferInfo,
            RHI::DescriptorImageInfo
        > data;
        RHI::DescriptorType type;

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
        explicit Material(std::shared_ptr<RHI::ResourceManager> resMgr);

        void addUniformBuffer(RHI::BufferHandle buffer, size_t size, uint32_t binding,
            RHI::ShaderStage stageFlags = RHI::ShaderStage::Vertex | RHI::ShaderStage::Fragment);
        RHI::BufferHandle createAndAddUniformBuffer(size_t size, uint32_t binding,
            const std::string& debugName = "MaterialUBO",
            RHI::ShaderStage stageFlags = RHI::ShaderStage::Vertex | RHI::ShaderStage::Fragment);

        RHI::TextureHandle addTexture(const std::string& filename,
            RHI::Format format,
            const std::string& debugName,
            uint32_t binding,
            RHI::ShaderStage stageFlags = RHI::ShaderStage::Fragment);

        void addBinding(uint32_t binding, RHI::DescriptorType type, uint32_t count,
            RHI::ShaderStage stageFlags);

        void createDescriptorSetLayout();
        bool allocateDescriptorSet(RHI::DescriptorPoolHandle pool, uint32_t setIndex = 1);
        void updateDescriptorSet();

        void loadShaders(const std::string& vsPath, const std::string& fsPath);

        void setVertexShader(RHI::ShaderHandle shader);
        void setFragmentShader(RHI::ShaderHandle shader);
        void setExternalDescriptorSetLayout(RHI::DescriptorSetLayoutHandle layout) { m_externalDescriptorSetLayout = layout; }
        void setCullMode(RHI::CullMode mode) { m_cullMode = mode; }
        void setDepthTest(bool enable) { m_depthTestEnable = enable; }
        void setDepthWrite(bool enable) { m_depthWriteEnable = enable; }
        void setDepthCompareOp(RHI::CompareOp op) { m_depthCompareOp = op; }
        void setBlendState(const RHI::BlendAttachmentState& state) { m_blendState = state; }
        void enableTransparent(bool enable = true) { m_alphaBlend = enable; }
        void enableDepthTest(bool enable = true) { m_depthTestEnable = enable; }
        void enableDepthWrite(bool enable = true) { m_depthWriteEnable = enable; }

        RHI::DescriptorSetLayoutHandle getDescriptorSetLayout() const { return m_descriptorSetLayout; }
        RHI::DescriptorSetHandle getDescriptorSet() const { return m_descriptorSet; } 
        RHI::BufferHandle getUniformBuffer(uint32_t binding) const;
        RHI::ShaderHandle getVertexShader() const { return m_vertexShader; }
        RHI::ShaderHandle getFragmentShader() const { return m_fragmentShader; }
        RHI::CullMode getCullMode() { return m_cullMode; }
        RHI::FrontFace getFrontFace() { return m_frontFace; }
        RHI::CompareOp getDethCompareOp() { return m_depthCompareOp; }
        RHI::BlendAttachmentState getBlendAttachmentState() { return m_blendState; }
        bool isTransparent() const { return m_alphaBlend; }
        bool isDepthTestEnable() const { return m_depthTestEnable; }
        bool isDepthWriteEnable() const { return m_depthWriteEnable; }

        Scene::GraphicsPipelineState generatePipelineState(const RHI::VertexInputState& vertexInput) const;

        std::vector<RHI::DescriptorSetLayoutBinding> getBindings() const;
        const std::unordered_map<uint32_t, DescriptorResourceInfo>& getResources() const { return m_resources; }

        uint64_t getSortKey() const;
    private:
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        RHI::ShaderHandle m_vertexShader;
        RHI::ShaderHandle m_fragmentShader;

        // 资源映射（binding -> DescriptorResourceInfo）
        std::unordered_map<uint32_t, DescriptorResourceInfo> m_resources;

        struct BindingInfo {
            RHI::DescriptorType type;
            uint32_t count;
            RHI::ShaderStage stageFlags;
        };
        std::unordered_map<uint32_t, BindingInfo> m_bindings;

        RHI::DescriptorSetHandle m_descriptorSet;
        RHI::DescriptorSetLayoutHandle m_descriptorSetLayout;
        RHI::DescriptorSetLayoutHandle m_externalDescriptorSetLayout;

        RHI::CullMode m_cullMode = RHI::CullMode::None;
        RHI::FrontFace m_frontFace = RHI::FrontFace::CounterClockwise;
        RHI::CompareOp m_depthCompareOp = RHI::CompareOp::Less;
        RHI::BlendAttachmentState m_blendState= RHI::BlendAttachmentState{};

        bool m_alphaBlend = false;
        bool m_depthTestEnable = true;
        bool m_depthWriteEnable = true;
    };

}