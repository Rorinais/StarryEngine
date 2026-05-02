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

    bool DefaultMaterialTemplate::loadShaders(const std::string& vsPath, const std::string& fsPath) {
        Assets::ShaderLoader loader(m_resMgr);
        auto vertInfo = loader.loadFromFile(vsPath, RHI::ShaderStage::Vertex);
        auto fragInfo = loader.loadFromFile(fsPath, RHI::ShaderStage::Fragment);

        if (!vertInfo || !fragInfo) return false;

        m_vertexShader = vertInfo->module;
        m_fragmentShader = fragInfo->module;

        m_vsReflection = std::move(vertInfo->reflection);
        m_fsReflection = std::move(fragInfo->reflection);

        // ───── 1. 合并除 set0 以外的所有 descriptor set 布局 ─────
        auto mergeLayouts = [&](const ShaderCreateInfo& info) {
            for (const auto& [setIdx, desc] : info.layoutDescs) {
                if (setIdx == 0) continue; // set0 由外部提供，不重复创建
                if (m_layouts.find(setIdx) == m_layouts.end()) {
                    auto layout = Assets::DescriptorSetLayoutCache::getOrCreateLayout(
                        m_resMgr.get(), desc);
                    if (layout.isValid()) {
                        m_layouts[setIdx] = layout;
                    }
                    else {
                        LOG_ERROR("Failed to create descriptor set layout for set {}", setIdx);
                    }
                }
                LOG_INFO("Merge layout set={}, bindingCount={}", setIdx, desc.bindings.size());
            }
            };
        mergeLayouts(*vertInfo);
        mergeLayouts(*fragInfo);

        // ───── 2. 如果外部未提供推送常量，则从反射自动生成 ─────
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
                        // 合并可见阶段
                        it->stageFlags = static_cast<RHI::ShaderStageFlags>(
                            static_cast<uint32_t>(it->stageFlags) | static_cast<uint32_t>(range.stageFlags));
                    }
                    else if (m_pushConstants.empty()) {
                        m_pushConstants.push_back(range);
                    }
                    else {
                        // 大小不同！报错，因为当前引擎不支持多个独立的推送常量范围
                        LOG_ERROR("Incompatible push constant block sizes between shader stages ({} vs {}). "
                            "Use UBO for per‑stage data.", range.size, m_pushConstants[0].size);
                    }
                }
                };
            addPush(m_vsReflection);
            addPush(m_fsReflection);
        }
        LOG_INFO(m_vsReflection);
        LOG_INFO(m_fsReflection);

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
}