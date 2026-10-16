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
        // ── 编译期变体（shader keyword）──
        // loadShaders 前调用。底层是宏注入（setMacros），这里提供 Unity 风格的关键字集合封装：
        //   shader 用 #ifdef KEYWORD 判断"是否开启"，值固定 "1"（存在即开，与值无关）。
        // 不同关键字组合 = 独立变体 + 独立描述符布局 → 材质只绑实际用到的贴图/绑定。
        void setMacros(std::unordered_map<std::string, std::string> macros) { m_macros = std::move(macros); }
        const std::unordered_map<std::string, std::string>& getMacros() const { return m_macros; }
        // 关键字集合：每个关键字编译为 #define KW 1；空集合 = 全部关闭（shader 走默认分支）
        void setKeywords(std::initializer_list<const char*> keywords) {
            m_macros.clear();
            for (const char* kw : keywords) m_macros[kw] = "1";
        }
        // 在现有关键字基础上追加（不改动已开的关键字）
        void addKeyword(const char* kw) { m_macros[kw] = "1"; }
        bool hasKeyword(const std::string& kw) const { return m_macros.find(kw) != m_macros.end(); }
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
        std::unordered_map<std::string, std::string> m_macros;   // 编译期宏变体 
    };
}