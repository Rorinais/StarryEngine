#include <content/materials/BuiltinMaterials.hpp>
#include <assets/material/DefaultMaterialTemplate.hpp>
#include <logging/Logger.hpp>

namespace StarryEngine::Content {

    std::shared_ptr<Assets::MaterialInstance> BuiltinMaterials::createSkybox(
        std::shared_ptr<RHI::ResourceManager> resMgr,
        RHI::DescriptorSetLayoutHandle globalSetLayout,
        RHI::DescriptorPoolHandle pool,
        RHI::DescriptorSetHandle globalSet,
        RHI::TextureHandle envCubemap,
        RHI::SamplerHandle cubeSampler,
        const std::string& subpassTag)
    {
        if (!resMgr || subpassTag.empty()) return nullptr;

        auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(resMgr, globalSetLayout);
        if (!tmpl->loadShaders("assets/shaders/deferred/skybox.vert",
                                "assets/shaders/deferred/skybox.frag")) {
            LOG_ERROR("BuiltinMaterials::createSkybox: 天空盒着色器加载失败");
            return nullptr;
        }

        auto material = std::make_shared<Assets::MaterialInstance>(tmpl, pool, resMgr.get(), globalSet);
        if (envCubemap != RHI::TextureHandle::Null()) {
            material->setTexture("uSkybox", envCubemap, cubeSampler);
        }
        material->setSubpassTag(subpassTag);
        material->enableDepthTest(true);
        material->setDepthCompareOp(RHI::CompareOp::LessOrEqual);
        return material;
    }

    std::shared_ptr<Assets::MaterialInstance> BuiltinMaterials::createGrid(
        std::shared_ptr<RHI::ResourceManager> resMgr,
        RHI::DescriptorSetLayoutHandle globalSetLayout,
        RHI::DescriptorPoolHandle pool,
        RHI::DescriptorSetHandle globalSet,
        const std::string& subpassTag)
    {
        if (!resMgr || subpassTag.empty()) return nullptr;

        auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(resMgr, globalSetLayout);
        if (!tmpl->loadShaders("assets/shaders/core/gridShader.vert",
                                "assets/shaders/core/gridShader.frag")) {
            LOG_ERROR("BuiltinMaterials::createGrid: 网格着色器加载失败");
            return nullptr;
        }

        auto material = std::make_shared<Assets::MaterialInstance>(tmpl, pool, resMgr.get(), globalSet);
        material->enableDepthTest(true);
        material->setSubpassTag(subpassTag);
        return material;
    }

} // namespace StarryEngine::Content
