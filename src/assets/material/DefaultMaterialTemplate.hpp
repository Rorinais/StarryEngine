#pragma once
#include "MaterialTemplate.hpp"

namespace StarryEngine::Assets {
    class DefaultMaterialTemplate : public MaterialTemplate {
    public:
        DefaultMaterialTemplate(
            std::shared_ptr<RHI::ResourceManager> resMgr,
            const std::unordered_map<uint32_t, RHI::DescriptorSetLayoutHandle>& layouts,
            const std::vector<RHI::PushConstantRange>& pushConstants);

        ~DefaultMaterialTemplate() = default;

        std::unordered_map<uint32_t, RHI::DescriptorSetLayoutHandle> getLayouts() const override { return m_layouts; }
        std::vector<RHI::PushConstantRange> getPushConstants() const override { return m_pushConstants; }

        bool loadShaders(const std::string& vsPath, const std::string& fsPath);

        RHI::ShaderHandle getVertexShader() const override { return m_vertexShader; }
        RHI::ShaderHandle getFragmentShader() const override { return m_fragmentShader; }

        const InstancingLayout* getInstancingLayout() const override;
    private:
        std::shared_ptr<RHI::ResourceManager> m_resMgr;

        std::unordered_map<uint32_t, RHI::DescriptorSetLayoutHandle> m_layouts;
        std::vector<RHI::PushConstantRange> m_pushConstants;

        RHI::ShaderHandle m_vertexShader, m_fragmentShader;
    };
}