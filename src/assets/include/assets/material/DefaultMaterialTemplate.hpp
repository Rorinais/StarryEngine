#pragma once
#include <assets/material/MaterialTemplate.hpp>

namespace StarryEngine::Assets {
    class DefaultMaterialTemplate : public MaterialTemplate {
    public:
        DefaultMaterialTemplate(std::shared_ptr<RHI::ResourceManager> resMgr,const std::unordered_map<uint32_t, RHI::DescriptorSetLayoutHandle>& layouts,const std::vector<RHI::PushConstantRange>& pushConstants = {});

        DefaultMaterialTemplate(std::shared_ptr<RHI::ResourceManager> resMgr,RHI::DescriptorSetLayoutHandle globalSetLayout,const std::vector<RHI::PushConstantRange>& pushConstants = {});

        ~DefaultMaterialTemplate() = default;
        void invalidate();

        const InstancingLayout* getInstancingLayout() const override;
        std::unordered_map<uint32_t, RHI::DescriptorSetLayoutHandle> getLayouts() const override { return m_layouts; }
        std::vector<RHI::PushConstantRange> getPushConstants() const override { return m_pushConstants; }

        void setShaderPaths(const std::string& vsPath, const std::string& fsPath);
        bool loadShaders(const std::string& vsPath, const std::string& fsPath);
        bool reloadShaders(const std::string& vsPath, const std::string& fsPath);

        const std::string& getVSPath() const { return m_vsPath; }
        const std::string& getFSPath() const { return m_fsPath; }
        RHI::ShaderHandle getVertexShader() const override { return m_vertexShader; }
        RHI::ShaderHandle getFragmentShader() const override { return m_fragmentShader; }
        const RHI::ShaderReflectionInfo& getVSReflection() const override { return m_vsReflection; }
        const RHI::ShaderReflectionInfo& getFSReflection() const override { return m_fsReflection; }

    private:
        void fillMissingLayouts(std::unordered_map<uint32_t, RHI::DescriptorSetLayoutHandle>& layouts,std::shared_ptr<RHI::ResourceManager> resMgr);

        std::shared_ptr<RHI::ResourceManager> m_resMgr;

        std::unordered_map<uint32_t, RHI::DescriptorSetLayoutHandle> m_layouts;
        std::vector<RHI::PushConstantRange> m_pushConstants;

        RHI::ShaderHandle m_vertexShader, m_fragmentShader;

        RHI::ShaderReflectionInfo m_vsReflection;
        RHI::ShaderReflectionInfo m_fsReflection;

        std::string m_vsPath, m_fsPath; 
    };
}