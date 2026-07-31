#include "DefaultMaterialTemplate.hpp"
#include "../../assets/loader/ShaderLoader.hpp"
#include "../../logging/Logger.hpp"

namespace StarryEngine::Assets {
    DefaultMaterialTemplate::DefaultMaterialTemplate(
        std::shared_ptr<RHI::ResourceManager> resMgr,
        const std::unordered_map<uint32_t, RHI::DescriptorSetLayoutHandle>& layouts,
        const std::vector<RHI::PushConstantRange>& pushConstants)
        : m_resMgr(resMgr), m_layouts(layouts), m_pushConstants(pushConstants) {}

    DefaultMaterialTemplate::DefaultMaterialTemplate(
        std::shared_ptr<RHI::ResourceManager> resMgr,
        RHI::DescriptorSetLayoutHandle globalSetLayout,
        const std::vector<RHI::PushConstantRange>& pushConstants)
        : m_resMgr(resMgr), m_pushConstants(pushConstants) {
        if (globalSetLayout.isValid()) {
            m_layouts[0] = globalSetLayout;
        }
    }

    void DefaultMaterialTemplate::invalidate() {
        m_vertexShader = RHI::ShaderHandle::Null();
        m_fragmentShader = RHI::ShaderHandle::Null();
    }

    void DefaultMaterialTemplate::setShaderPaths(const std::string& vsPath, const std::string& fsPath) {
        m_vsPath = vsPath;
        m_fsPath = fsPath;
    }

    bool DefaultMaterialTemplate::loadShaders(const std::string& vsPath, const std::string& fsPath) {
        Assets::ShaderLoader loader(m_resMgr);
        auto vertInfo = loader.loadFromFile(vsPath, RHI::ShaderStage::Vertex);
        auto fragInfo = loader.loadFromFile(fsPath, RHI::ShaderStage::Fragment);
        if (!vertInfo || !fragInfo) return false;

        m_vertexShader = vertInfo->module;
        m_fragmentShader = fragInfo->module;
        m_vsReflection = std::move(vertInfo->reflection);
        m_fsReflection = std::move(fragInfo->reflection);

        auto mergeLayouts = [&](const ShaderCreateInfo& info) {
            for (const auto& [setIdx, desc] : info.layoutDescs) {
                if (setIdx == 0) continue;
                if (m_layouts.find(setIdx) == m_layouts.end()) {
                    auto layout = Assets::DescriptorSetLayoutCache::getOrCreateLayout(m_resMgr.get(), desc);
                    if (layout.isValid()) m_layouts[setIdx] = layout;
                    else LOG_ERROR("Failed to create descriptor set layout for set {}", setIdx);
                }
                LOG_INFO("Merge layout set={}, bindingCount={}", setIdx, desc.bindings.size());
            }
            };
        mergeLayouts(*vertInfo);
        mergeLayouts(*fragInfo);

        if (m_pushConstants.empty()) {
            auto addPush = [&](const RHI::ShaderReflectionInfo& refl) {
                for (const auto& pc : refl.pushConstants) {
                    RHI::PushConstantRange range;
                    range.stageFlags = static_cast<RHI::ShaderStageFlags>(pc.stageFlags);
                    range.offset = 0;
                    range.size = pc.size;
                    auto it = std::find_if(m_pushConstants.begin(), m_pushConstants.end(),
                        [&](const RHI::PushConstantRange& r) { return r.size == range.size; });
                    if (it != m_pushConstants.end()) {
                        it->stageFlags = static_cast<RHI::ShaderStageFlags>(
                            static_cast<uint32_t>(it->stageFlags) | static_cast<uint32_t>(range.stageFlags));
                    }
                    else if (m_pushConstants.empty()) {
                        m_pushConstants.push_back(range);
                    }
                    else {
                        LOG_ERROR("Incompatible push constant sizes");
                    }
                }
                };
            addPush(m_vsReflection);
            addPush(m_fsReflection);
        }

        LOG_DEBUG(m_fsReflection);

        fillMissingLayouts(m_layouts, m_resMgr);

        MaterialTemplate::clearCache();
        RHI::PipelineLayoutHandle testLayout = getPipelineLayout(m_resMgr.get());
        if (!testLayout.isValid()) {
            LOG_ERROR("Failed to create pipeline layout after loading shaders");
            return false;
        }

        setShaderPaths(vsPath, fsPath);
        return true;
    }

    bool DefaultMaterialTemplate::reloadShaders(const std::string& vsPath, const std::string& fsPath) {
        auto oldVert = m_vertexShader;
        auto oldFrag = m_fragmentShader;
        auto oldLayouts = m_layouts;
        auto oldPushConstants = m_pushConstants;
        auto oldVSRefl = std::move(m_vsReflection);
        auto oldFSRefl = std::move(m_fsReflection);

        Assets::ShaderLoader loader(m_resMgr);
        auto vertInfo = loader.loadFromFile(vsPath, RHI::ShaderStage::Vertex);
        auto fragInfo = loader.loadFromFile(fsPath, RHI::ShaderStage::Fragment);
        if (!vertInfo || !fragInfo) {
            m_vertexShader = oldVert;
            m_fragmentShader = oldFrag;
            m_layouts = oldLayouts;
            m_pushConstants = oldPushConstants;
            m_vsReflection = std::move(oldVSRefl);
            m_fsReflection = std::move(oldFSRefl);
            LOG_ERROR("Shader reload failed, keeping previous");
            return false;
        }

        m_vertexShader = vertInfo->module;
        m_fragmentShader = fragInfo->module;
        m_vsReflection = std::move(vertInfo->reflection);
        m_fsReflection = std::move(fragInfo->reflection);

        RHI::DescriptorSetLayoutHandle globalLayout;
        if (auto it = oldLayouts.find(0); it != oldLayouts.end()) globalLayout = it->second;
        m_layouts.clear();
        if (globalLayout.isValid()) m_layouts[0] = globalLayout;

        auto mergeLayouts = [&](const ShaderCreateInfo& info) {
            for (auto& [setIdx, desc] : info.layoutDescs) {
                if (setIdx == 0) continue;
                if (m_layouts.find(setIdx) == m_layouts.end()) {
                    auto layout = Assets::DescriptorSetLayoutCache::getOrCreateLayout(m_resMgr.get(), desc);
                    if (layout.isValid()) m_layouts[setIdx] = layout;
                }
            }
            };
        mergeLayouts(*vertInfo);
        mergeLayouts(*fragInfo);

        if (oldPushConstants.empty()) {
            m_pushConstants.clear();
            auto addPush = [&](const RHI::ShaderReflectionInfo& refl) {
                for (const auto& pc : refl.pushConstants) {
                    RHI::PushConstantRange range;
                    range.stageFlags = static_cast<RHI::ShaderStageFlags>(pc.stageFlags);
                    range.offset = 0;
                    range.size = pc.size;

                    auto it = std::find_if(m_pushConstants.begin(), m_pushConstants.end(),
                        [&](const RHI::PushConstantRange& r) { return r.size == range.size; });
                    if (it != m_pushConstants.end()) {
                        it->stageFlags = static_cast<RHI::ShaderStageFlags>(
                            static_cast<uint32_t>(it->stageFlags) | static_cast<uint32_t>(range.stageFlags));
                    }
                    else if (m_pushConstants.empty()) {
                        m_pushConstants.push_back(range);
                    }
                    else {
                        LOG_ERROR("Incompatible push constant block sizes between shader stages ({} vs {}). "
                            "Use UBO for per‑stage data.", range.size, m_pushConstants[0].size);
                    }
                }
                };
            addPush(m_vsReflection);
            addPush(m_fsReflection);
        }

        fillMissingLayouts(m_layouts, m_resMgr);

        MaterialTemplate::clearCache();
        if (!getPipelineLayout(m_resMgr.get()).isValid())
            LOG_ERROR("Pipeline layout creation failed after reload");

        if (oldVert.isValid())
            m_resMgr->scheduleDestroy([oldVert, resMgr = m_resMgr]() { resMgr->destroy(oldVert); }, 2);
        if (oldFrag.isValid())
            m_resMgr->scheduleDestroy([oldFrag, resMgr = m_resMgr]() { resMgr->destroy(oldFrag); }, 2);

        setShaderPaths(vsPath, fsPath);
        return true;
    }

    const InstancingLayout* DefaultMaterialTemplate::getInstancingLayout() const {
        static InstancingLayout defaultLayout = []() {
            InstancingLayout layout;
            layout.binding = 1;
            layout.attributes = {
                {3, RHI::Format::RGBA32_Float, 0},
                {4, RHI::Format::RGBA32_Float, 16},
                {5, RHI::Format::RGBA32_Float, 32},
                {6, RHI::Format::RGBA32_Float, 48}
            };
            layout.autoCalculateOffsets(); // stride = 64
            return layout;
            }();
        return &defaultLayout;
    }

    void DefaultMaterialTemplate::fillMissingLayouts(
        std::unordered_map<uint32_t, RHI::DescriptorSetLayoutHandle>& layouts,
        std::shared_ptr<RHI::ResourceManager> resMgr)
    {
        uint32_t maxSet = 0;
        for (const auto& [setIdx, _] : layouts)
            if (setIdx > maxSet) maxSet = setIdx;

        for (uint32_t i = 1; i <= maxSet; ++i) {
            if (layouts.find(i) == layouts.end()) {
                RHI::DescriptorSetLayoutDesc emptyDesc;
                auto layout = Assets::DescriptorSetLayoutCache::getOrCreateLayout(resMgr.get(), emptyDesc);
                if (layout.isValid()) {
                    layouts[i] = layout;
                    LOG_INFO("Created empty placeholder layout for set {}", i);
                }
                else {
                    LOG_ERROR("Failed to create placeholder layout for set {}", i);
                }
            }
        }
    }
}