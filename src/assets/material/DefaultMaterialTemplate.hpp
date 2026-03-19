#pragma once
#include "MaterialTemplate.hpp"

namespace StarryEngine::Assets {
    class DefaultMaterialTemplate : public MaterialTemplate {
    public:
        DefaultMaterialTemplate() {}
        ~DefaultMaterialTemplate() {}

        void loadShaders(const std::string& vsPath, const std::string& fsPath);


        std::vector<RHI::DescriptorSetLayoutHandle> getLayouts() const override { return m_layouts; }

        std::vector<RHI::PushConstantRange> getPushConstants() const override { return m_pushConstants; }


    private:
        std::vector<RHI::DescriptorSetLayoutHandle> m_layouts;
        std::vector<RHI::PushConstantRange> m_pushConstants;
        RHI::ShaderHandle m_vertexShader, m_fragmentShader;
    };
}