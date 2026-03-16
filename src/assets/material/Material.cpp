#include "Material.hpp"
#include "../loader/ShaderLoader.hpp"  
#include "../loader/TextureLoader.hpp"
#include "../../logging/Logger.hpp"

namespace StarryEngine::Assets {

    Material::Material(std::shared_ptr<RHI::ResourceManager> resMgr)
        : m_resMgr(std::move(resMgr)) {
    }

    void Material::setVertexShader(RHI::ShaderHandle shader) {
        m_vertexShader = shader;
    }

    void Material::setFragmentShader(RHI::ShaderHandle shader) {
        m_fragmentShader = shader;
    }

    void Material::loadShaders(const std::string& vsPath, const std::string& fsPath) {
        Assets::ShaderLoader loader(m_resMgr);
        auto vert = loader.loadFromFile(vsPath, RHI::ShaderStage::Vertex);
        auto frag = loader.loadFromFile(fsPath, RHI::ShaderStage::Fragment);
        if (vert) setVertexShader(vert->module);
        if (frag) setFragmentShader(frag->module);
    }

    void Material::addUniformBuffer(RHI::BufferHandle buffer, size_t size, uint32_t binding,
        RHI::ShaderStage stageFlags) {
        RHI::DescriptorBufferInfo info{ buffer, 0, size };
        addBinding(binding, RHI::DescriptorType::UniformBuffer, 1, stageFlags);
        m_resources[binding] = DescriptorResourceInfo(info);
    }

    RHI::BufferHandle Material::createAndAddUniformBuffer(size_t size, uint32_t binding,
        const std::string& debugName,
        RHI::ShaderStage stageFlags) {
        RHI::BufferDesc desc;
        desc.size = size;
        desc.type = RHI::BufferType::Uniform;
        desc.memoryType = RHI::MemoryType::CPU_To_GPU;
        desc.allowUpdate = true;
        desc.persistentMapped = true;
        desc.debugName = debugName;

        RHI::BufferHandle buffer = m_resMgr->createBuffer(desc);
        if (buffer.isValid()) {
            addUniformBuffer(buffer, size, binding, stageFlags);
        }
        return buffer;
    }

    RHI::TextureHandle Material::addTexture(const std::string& filename,
        RHI::Format format,
        const std::string& debugName,
        uint32_t binding,
        RHI::ShaderStage stageFlags) {
        TextureLoader loader(m_resMgr);
        auto result = loader.loadTexture2D(filename, format, debugName);
        if (!result.texture.isValid()) {
            LOG_ERROR("Failed to load texture: {}", filename);
            return RHI::TextureHandle::Null();
        }

        RHI::DescriptorImageInfo info{ result.texture, result.sampler, RHI::ImageLayout::ShaderReadOnly };
        addBinding(binding, RHI::DescriptorType::CombinedImageSampler, 1, stageFlags);
        m_resources[binding] = DescriptorResourceInfo(info);
        return result.texture;
    }

    void Material::addBinding(uint32_t binding, RHI::DescriptorType type, uint32_t count,
        RHI::ShaderStage stageFlags) {
        m_bindings[binding] = { type, count, stageFlags };
    }

    std::vector<RHI::DescriptorSetLayoutBinding> Material::getBindings() const {
        std::vector<RHI::DescriptorSetLayoutBinding> result;
        for (const auto& [binding, info] : m_bindings) {
            RHI::DescriptorSetLayoutBinding b{};
            b.binding = binding;
            b.type = info.type;
            b.count = info.count;
            b.stageFlags = info.stageFlags;
            b.immutableSamplers = false; 
            result.push_back(b);
        }
        return result;
    }

    RHI::BufferHandle Material::getUniformBuffer(uint32_t binding) const {
        auto it = m_resources.find(binding);
        if (it != m_resources.end() && std::holds_alternative<RHI::DescriptorBufferInfo>(it->second.data)) {
            return std::get<RHI::DescriptorBufferInfo>(it->second.data).buffer;
        }
        return RHI::BufferHandle::Null();
    }
    
    uint64_t Material::getSortKey() const {
        uint64_t key = 0;
        key ^= std::hash<RHI::ShaderHandle>{}(m_vertexShader);
        key ^= std::hash<RHI::ShaderHandle>{}(m_fragmentShader) << 1;
        return key;
    }

    void Material::createDescriptorSetLayout() {
        RHI::DescriptorSetLayoutDesc desc;
        desc.bindings = getBindings();
        desc.debugName = "MaterialDSLayout";
        m_descriptorSetLayout = m_resMgr->createDescriptorSetLayout(desc);
    }

    bool Material::allocateDescriptorSet(RHI::DescriptorPoolHandle pool, uint32_t setIndex) {
        RHI::DescriptorSetLayoutHandle layout = m_externalDescriptorSetLayout.isValid() ? m_externalDescriptorSetLayout : m_descriptorSetLayout;
        if (!layout.isValid()) {
            LOG_ERROR("No descriptor set layout available.");
            return false;
        }
        RHI::DescriptorSetDesc desc;
        desc.descriptorPool = pool;
        desc.descriptorSetLayout = layout; 
        desc.setIndex = setIndex;
        desc.debugName = "MaterialSet";
        m_descriptorSet = m_resMgr->createDescriptorSet(desc);
        return m_descriptorSet.isValid();
    }

    void Material::updateDescriptorSet() {
        if (!m_descriptorSet.isValid()) return;
        auto* set = m_resMgr->getDescriptorSet(m_descriptorSet);
        if (!set) return;

        for (const auto& [binding, resource] : m_resources) {
            if (std::holds_alternative<RHI::DescriptorBufferInfo>(resource.data)) {
                const auto& bufInfo = std::get<RHI::DescriptorBufferInfo>(resource.data);
                auto* buffer = m_resMgr->getBuffer(bufInfo.buffer);
                if (buffer) set->writeBuffer(binding, 0, buffer, bufInfo.offset, bufInfo.range);
            }
            else if (std::holds_alternative<RHI::DescriptorImageInfo>(resource.data)) {
                const auto& imgInfo = std::get<RHI::DescriptorImageInfo>(resource.data);
                auto* texture = m_resMgr->getTexture(imgInfo.texture);
                if (!texture) continue;
                auto* sampler = m_resMgr->getSampler(imgInfo.sampler);
                set->writeTexture(binding, 0, texture, sampler, imgInfo.imageLayout);
            }
        }
        set->update();
    }
} // namespace StarryEngine::Assets