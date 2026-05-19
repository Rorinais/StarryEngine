#pragma once
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "../../renderer/interface/RHI_RESOURCE_MANAGER.hpp"
#include "../../renderer/interface/RHI_RESOURCE_FACTORY.hpp"
#include "../../renderer/interface/RHI_TYPES.hpp"

namespace StarryEngine::Assets {

    class IBLBuilder {
    public:
        IBLBuilder(std::shared_ptr<RHI::ResourceManager> resMgr,
            std::shared_ptr<RHI::IRHI> rhi);
        ~IBLBuilder();

        // ═══════════════════════════════════════════════
        // 图形管线版本（Graphics Pipeline）
        //   逐面 RenderPass + Framebuffer + Draw
        // ═══════════════════════════════════════════════

        /// Equirect → Cubemap（图形管线）
        RHI::TextureHandle equirectToCubemap(
            RHI::TextureHandle equirectTex,
            uint32_t            faceSize = 512);

        /// Irradiance Map（图形管线）
        RHI::TextureHandle generateIrradianceMap(
            RHI::TextureHandle envCubemap,
            uint32_t            outputSize = 32);

        /// Prefiltered Environment Map（图形管线）
        RHI::TextureHandle generatePrefilteredMap(
            RHI::TextureHandle envCubemap,
            uint32_t            baseSize = 128,
            uint32_t            mipLevels = 5);

        /// BRDF LUT（图形管线）
        RHI::TextureHandle generateBrdfLut(uint32_t size = 512);

        // ═══════════════════════════════════════════════
        // 计算管线版本（Compute Shader）
        //   单次 Dispatch，无 Framebuffer/RenderPass
        // ═══════════════════════════════════════════════

        /// Equirect → Cubemap（计算着色器）
        RHI::TextureHandle equirectToCubemapCS(
            RHI::TextureHandle equirectTex,
            uint32_t            faceSize = 512);

        /// Irradiance Map（计算着色器）
        RHI::TextureHandle generateIrradianceMapCS(
            RHI::TextureHandle envCubemap,
            uint32_t            outputSize = 32);

        /// Prefiltered Environment Map（计算着色器）
        RHI::TextureHandle generatePrefilteredMapCS(
            RHI::TextureHandle envCubemap,
            uint32_t            baseSize = 128,
            uint32_t            mipLevels = 5);

        /// BRDF LUT（计算着色器）
        RHI::TextureHandle generateBrdfLutCS(uint32_t size = 512);

        // ═══════════════════════════════════════════════
        // 便捷方法
        // ═══════════════════════════════════════════════

        /// 从 HDR 文件生成环境 Cubemap（使用计算管线）
        RHI::TextureHandle buildEnvCubemap(
            const std::string& hdrPath,
            uint32_t            faceSize = 512);

        /// 从 HDR 文件生成环境 Cubemap（可指定管线）
        RHI::TextureHandle buildEnvCubemap(
            const std::string& hdrPath,
            uint32_t            faceSize,
            bool                useComputeShader);

    private:
        // ── 图形管线内部辅助 ──
        RHI::DescriptorSetHandle createDescriptorSet(
            RHI::TextureHandle      texture,
            RHI::SamplerHandle      sampler,
            RHI::DescriptorSetLayoutHandle layout,
            RHI::DescriptorPoolHandle      pool);

        // ── 计算管线内部辅助 ──
        RHI::DescriptorSetHandle createComputeDescriptorSet(
            RHI::TextureHandle      inputTex,
            RHI::SamplerHandle      sampler,
            RHI::TextureHandle      outputTex,
            RHI::DescriptorSetLayoutHandle layout,
            RHI::DescriptorPoolHandle      pool,
            RHI::ImageLayout        outputLayout);

        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        std::shared_ptr<RHI::IRHI>            m_rhi;

        // 图形管线版本的共享资源
        RHI::ShaderHandle             m_vs, m_fs;
        RHI::PipelineLayoutHandle     m_pipelineLayout;
        RHI::DescriptorSetLayoutHandle m_descLayout;
    };

} // namespace StarryEngine::Assets