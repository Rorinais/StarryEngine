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

        RHI::TextureHandle equirectToCubemap(RHI::TextureHandle equirectTex,uint32_t faceSize = 512);

        RHI::TextureHandle generateIrradianceMap(RHI::TextureHandle envCubemap,uint32_t outputSize = 32);

        RHI::TextureHandle generatePrefilteredMap(RHI::TextureHandle envCubemap,uint32_t baseSize = 128,uint32_t mipLevels = 5);

        RHI::TextureHandle generateBrdfLut(uint32_t size = 512);

        RHI::TextureHandle equirectToCubemapCS(RHI::TextureHandle equirectTex,uint32_t faceSize = 512);

        RHI::TextureHandle generateIrradianceMapCS(RHI::TextureHandle envCubemap,uint32_t outputSize = 32);

        RHI::TextureHandle generatePrefilteredMapCS(RHI::TextureHandle envCubemap,uint32_t baseSize = 128,uint32_t mipLevels = 5);

        RHI::TextureHandle generateBrdfLutCS(uint32_t size = 512);

        RHI::TextureHandle buildEnvCubemap(const std::string& hdrPath,uint32_t aceSize = 512);

        RHI::TextureHandle buildEnvCubemap(const std::string& hdrPath,uint32_t aceSize,bool useComputeShader);

    private:
        RHI::DescriptorSetHandle createDescriptorSet(
            RHI::TextureHandle      texture,
            RHI::SamplerHandle      sampler,
            RHI::DescriptorSetLayoutHandle layout,
            RHI::DescriptorPoolHandle      pool);

        RHI::DescriptorSetHandle createComputeDescriptorSet(
            RHI::TextureHandle      inputTex,
            RHI::SamplerHandle      sampler,
            RHI::TextureHandle      outputTex,
            RHI::DescriptorSetLayoutHandle layout,
            RHI::DescriptorPoolHandle      pool,
            RHI::ImageLayout        outputLayout);

        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        std::shared_ptr<RHI::IRHI>            m_rhi;

        RHI::ShaderHandle             m_vs, m_fs;
        RHI::PipelineLayoutHandle     m_pipelineLayout;
        RHI::DescriptorSetLayoutHandle m_descLayout;
    };

} // namespace StarryEngine::Assets