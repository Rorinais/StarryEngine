// PbrMaterials.cpp — PBRDemo 的职责分片（工作区）
#include <content/scenes/pbr/PbrScene.hpp>
#include <assets/loader/MaterialDesc.hpp>
#include <assets/loader/TextureLoader.hpp>

    std::shared_ptr<Assets::MaterialInstance> PBRDemo::makeParticleMaterial() {
        auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(
            m_rhi->getResourceManager(), m_descriptorSetLayout);
        if (!tmpl->loadShaders("assets/shaders/test/particle.vert", "assets/shaders/test/particle.frag")) {
            LOG_ERROR("Failed to load particle shaders");
            return nullptr;
        }
        auto mat = std::make_shared<Assets::MaterialInstance>(
            tmpl, m_descriptorPool, m_rhi->getResourceManager().get(), m_descriptorSet);
        RHI::BlendAttachmentState blend;
        blend.blendEnable = true;   
        blend.dstAlphaBlendFactor = RHI::BlendFactor::OneMinusSrcAlpha; 
        mat->setAttachments({ blend });
        mat->setDepthTest(false);
        mat->setDepthWrite(false);
        return mat;
    }

    std::shared_ptr<Assets::MaterialInstance> PBRDemo::makeStencilQuadMaterial(const std::string& fragPath) {
        auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(m_rhi->getResourceManager(), m_descriptorSetLayout);
        if (!tmpl->loadShaders("assets/shaders/npr_toon/shader.vert", fragPath)) {
            LOG_ERROR("Failed to load stencil quad shaders: {}", fragPath);
            return nullptr;
        }
        auto mat = std::make_shared<Assets::MaterialInstance>(
            tmpl, m_descriptorPool, m_rhi->getResourceManager().get(), m_descriptorSet);
        Assets::TextureLoader loader(m_rhi->getResourceManager());
        static const uint8_t kWhite[4] = { 255, 255, 255, 255 };
        auto white = loader.loadTextureFromMemory(kWhite, 1, 1, RHI::Format::RGBA8_UNorm, "White");
        if (white.texture.isValid())
            mat->setTexture("texSampler", white.texture, white.sampler);
        mat->setDepthTest(false);
        mat->setDepthWrite(false);
        return mat;
    }
