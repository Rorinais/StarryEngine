#pragma once
#include <stb_image.h>
#include <variant>
#include "../AssetType.hpp"
#include "../../assets/loader/ShaderLoader.hpp"

namespace std {
    template<>
    struct hash<StarryEngine::RHI::TextureHandle> {
        size_t operator()(const StarryEngine::RHI::TextureHandle& handle) const noexcept {
            return typename StarryEngine::RHI::TextureHandle::Hash{}(handle);
        }
    };

    template<>
    struct hash<StarryEngine::RHI::ShaderHandle> {
        size_t operator()(const StarryEngine::RHI::ShaderHandle& handle) const noexcept {
            return typename StarryEngine::RHI::ShaderHandle::Hash{}(handle);
        }
    };

}

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

        void setVertexShader(RHI::ShaderHandle shader);
        void setFragmentShader(RHI::ShaderHandle shader);

        void loadShaders(const std::string& vsPath, const std::string& fsPath);

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

        RHI::ShaderHandle getVertexShader() const { return m_vertexShader; }
        RHI::ShaderHandle getFragmentShader() const { return m_fragmentShader; }
        const auto& getResources() const { return m_resources; }
        std::vector<RHI::DescriptorSetLayoutBinding> getBindings() const;
        void enableTransparent() { m_alphaBlend = true; }
        bool isTransparent() const { return m_alphaBlend; }

        RHI::BufferHandle getUniformBuffer(uint32_t binding) const;

        uint64_t getSortKey() const;

        void createDescriptorSetLayout();

        bool allocateDescriptorSet(RHI::DescriptorPoolHandle pool, uint32_t setIndex = 1);

        void updateDescriptorSet();

        RHI::DescriptorSetLayoutHandle getDescriptorSetLayout() const { return m_descriptorSetLayout; }
        RHI::DescriptorSetHandle getDescriptorSet() const { return m_descriptorSet; } 
        void setExternalDescriptorSetLayout(RHI::DescriptorSetLayoutHandle layout) { m_externalDescriptorSetLayout = layout; }
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

        bool m_alphaBlend = false;

        RHI::DescriptorSetHandle m_descriptorSet;
        RHI::DescriptorSetLayoutHandle m_descriptorSetLayout;
        RHI::DescriptorSetLayoutHandle m_externalDescriptorSetLayout;
    };

}