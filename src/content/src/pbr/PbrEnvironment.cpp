// PbrEnvironment.cpp — PBRDemo 的职责分片（工作区）
#include <content/pbr/PbrScene.hpp>
#include <assets/material/MaterialDesc.hpp>
#include <assets/loader/TextureLoader.hpp>

    void PBRDemo::initIBL() {
        m_envCubemap      = m_iblBuilder->buildEnvCubemap("assets/textures/pbr/kloofendal_48d_partly_cloudy_puresky_1k.hdr", 128);
        m_irradianceMap   = m_iblBuilder->generateIrradianceMapCS(m_envCubemap, 64);
        m_prefilteredMap  = m_iblBuilder->generatePrefilteredMapCS(m_envCubemap, 256, 6);
        m_brdfLut         = m_iblBuilder->generateBrdfLutCS(128);

        RHI::SamplerDesc cubeSampDesc;
        cubeSampDesc.minFilter = RHI::SamplerFilter::Linear;
        cubeSampDesc.magFilter = RHI::SamplerFilter::Linear;
        cubeSampDesc.addressU = RHI::SamplerAddressMode::ClampToEdge;
        cubeSampDesc.addressV = RHI::SamplerAddressMode::ClampToEdge;
        cubeSampDesc.addressW = RHI::SamplerAddressMode::ClampToEdge;
        cubeSampDesc.maxLod = 1.0f;
        m_cubeSampler = m_rhi->getResourceManager()->createSampler(cubeSampDesc);

        RHI::SamplerDesc prefilterSampDesc = cubeSampDesc;
        prefilterSampDesc.mipFilter = RHI::SamplerFilter::Linear;
        prefilterSampDesc.maxLod = 6.0f;
        m_prefilterSampler = m_rhi->getResourceManager()->createSampler(prefilterSampDesc);

        RHI::SamplerDesc lutSampDesc;
        lutSampDesc.minFilter = RHI::SamplerFilter::Linear;
        lutSampDesc.magFilter = RHI::SamplerFilter::Linear;
        lutSampDesc.addressU = RHI::SamplerAddressMode::ClampToEdge;
        lutSampDesc.addressV = RHI::SamplerAddressMode::ClampToEdge;
        lutSampDesc.maxLod = 1.0f;
        m_lutSampler = m_rhi->getResourceManager()->createSampler(lutSampDesc);
    }

    std::shared_ptr<Assets::MaterialInstance> PBRDemo::createSkyboxMaterial() {
        // 天空盒是引擎自带基础件；内容层只提供自己的环境 cubemap 与 subpass 名
        return Content::BuiltinMaterials::createSkybox(
            m_rhi->getResourceManager(), m_descriptorSetLayout,
            m_descriptorPool, m_descriptorSet,
            m_envCubemap, m_cubeSampler);
    }

    std::shared_ptr<Assets::MaterialInstance> PBRDemo::createGridMaterial() {
        // 网格地面同样是引擎自带基础件
        auto gridMaterialInst = Content::BuiltinMaterials::createGrid(
            m_rhi->getResourceManager(), m_descriptorSetLayout,
            m_descriptorPool, m_descriptorSet);
        if (!gridMaterialInst) return nullptr;

        return gridMaterialInst;
    }

    std::shared_ptr<Assets::MaterialInstance> PBRDemo::createTexturedPbrMaterial() {
        auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(m_rhi->getResourceManager(), m_descriptorSetLayout);
        tmpl->loadShaders("assets/shaders/pbr/shpere_pbr.vert", "assets/shaders/pbr/shpere_pbr (2).frag");

        auto material = std::make_shared<Assets::MaterialInstance>(tmpl, m_descriptorPool, m_rhi->getResourceManager().get(), m_descriptorSet);
        material->setSubpassTag("Forward_Opaque");
        material->enableDepthTest(true);
        material->enableDepthWrite(true);

        Assets::TextureLoader texLoader(m_rhi->getResourceManager());
        auto armT    = texLoader.loadTexture2D("assets/textures/pbr/metal_plate_02_arm_1k.png", RHI::Format::RGBA8_UNorm, "ARM");
        auto albedoT = texLoader.loadTexture2D("assets/textures/pbr/metal_plate_02_diff_1k.png", RHI::Format::RGBA8_sRGB, "Albedo");
        auto normalT = texLoader.loadTexture2D("assets/textures/pbr/metal_plate_02_nor_dx_1k.png", RHI::Format::RGBA8_UNorm, "Normal");

        material->setTexture("armMap", armT.texture, armT.sampler);
        material->setTexture("albedoMap", albedoT.texture, albedoT.sampler);
        material->setTexture("normalMap", normalT.texture, normalT.sampler);
        material->setTexture("uIrradianceMap", m_irradianceMap, m_cubeSampler);
        material->setTexture("uPrefilteredMap", m_prefilteredMap, m_prefilterSampler);
        material->setTexture("uBrdfLut", m_brdfLut, m_lutSampler);
        material->addTextureDependency("ShadowMap", 2, 6);

        auto* lightBlock = material->getBlock("LightingUBO");
        if (lightBlock) {
            lightBlock->setVec4("lights.position", glm::vec4(-0.5f, -1.0f, -0.8f, 0.0f));  
            lightBlock->setVec4("lights.color", glm::vec4(3.0f, 2.7f, 2.3f, 1.0f));
            lightBlock->setFloat("lightCount", 1.0f);
            lightBlock->setFloat("ambientStrength", 0.15f);
        }
        material->applyAllDirtyBlocks();
        return material;
    }
