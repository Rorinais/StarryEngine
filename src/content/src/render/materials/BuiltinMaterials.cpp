#include <content/render/materials/BuiltinMaterials.hpp>
#include <renderer/renderPaths/LayerRegistry.hpp>
#include <assets/loader/MaterialDesc.hpp>
#include <logging/Logger.hpp>

namespace StarryEngine::Content {

    std::shared_ptr<Assets::MaterialInstance> BuiltinMaterials::createSkybox(
        std::shared_ptr<RHI::ResourceManager> resMgr,
        RHI::DescriptorSetLayoutHandle globalSetLayout,
        RHI::DescriptorPoolHandle pool,
        RHI::DescriptorSetHandle globalSet,
        RHI::TextureHandle envCubemap,
        RHI::SamplerHandle cubeSampler)
    {
        if (!resMgr) return nullptr;

        Assets::MaterialDesc d;
        d.name = "Skybox";
        d.vertexShaderPath   = "assets/shaders/deferred/skybox.vert";
        d.fragmentShaderPath = "assets/shaders/deferred/skybox.frag";
        d.baseSetLayout = globalSetLayout;
        d.globalSet = globalSet;
        d.renderLayer = RenderLayers::Surface;
        d.renderQueue = 0;
        d.depthTest = true;
        d.depthCompareOp = RHI::CompareOp::LessOrEqual;

        if (envCubemap != RHI::TextureHandle::Null()) {
            Assets::MaterialTextureSlot slot;
            slot.name    = "uSkybox";
            slot.texture = envCubemap;
            slot.sampler = cubeSampler;
            slot.format  = RHI::Format::RGBA8_sRGB;
            d.textures.push_back(slot);
        }

        auto material = Assets::MaterialFactory::create(resMgr, pool, d);
        if (!material) LOG_ERROR("BuiltinMaterials::createSkybox: 材质创建失败");
        return material;
    }

    std::shared_ptr<Assets::MaterialInstance> BuiltinMaterials::createGrid(
        std::shared_ptr<RHI::ResourceManager> resMgr,
        RHI::DescriptorSetLayoutHandle globalSetLayout,
        RHI::DescriptorPoolHandle pool,
        RHI::DescriptorSetHandle globalSet)
    {
        if (!resMgr) return nullptr;

        Assets::MaterialDesc d;
        d.name = "Grid";
        d.vertexShaderPath   = "assets/shaders/core/gridShader.vert";
        d.fragmentShaderPath = "assets/shaders/core/gridShader.frag";
        d.baseSetLayout = globalSetLayout;
        d.globalSet = globalSet;
        d.renderLayer = RenderLayers::Surface;
        d.renderQueue = 1;
        d.depthTest = true;

        auto material = Assets::MaterialFactory::create(resMgr, pool, d);
        if (!material) LOG_ERROR("BuiltinMaterials::createGrid: 材质创建失败");
        return material;
    }
}
