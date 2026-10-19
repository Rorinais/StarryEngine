#pragma once
// L4 内容层：PBR 场景（原 demo/StarryEngine.cpp 中的 PBRDemo）
#include <renderer/passes/PassWrapper.hpp>
#include <app/AppConfig.hpp>
#include <app/RuntimeSettings.hpp>
#include <renderer/passes/GraphicsPass.hpp>
#include <renderer/passes/MeshPass.hpp>
#include <renderer/passes/ParticlePass.hpp>
#include <renderer/passes/ShadowPass.hpp>
#include <renderer/passes/ComputePass.hpp>
#include <renderer/RasterRenderer.hpp>
#include <content/raytrace/RayTraceScene.hpp>
#include <scene/ParticleEmitter.hpp>
#include <renderer/renderPaths/DeferredRenderPath.hpp>
#include <renderer/passExecutor/SceneDrawExecutor.hpp>
#include <assets/geometry/GeometryGenerator.hpp>
#include <content/materials/BuiltinMaterials.hpp>

#include <application/Application.hpp>
#include <core/JobSystem.hpp>
#include <event/Events.hpp>
#include <renderer/interface/vulkan/VulkanFrameContext.hpp>
#include <renderer/utils/FrameCapture.hpp>
#include <content/pbr/AsyncBoneProducer.hpp>

#include <stb_image_write.h>
#include <glm/gtc/packing.hpp>
#include <limits>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdlib>



#if 0
#endif
namespace {
    constexpr uint32_t kWinW = 800;
    constexpr uint32_t kWinH = 600;

    float computeModelFootY(const std::shared_ptr<StarryEngine::Assets::Geometry>& geometry) {
        if (!geometry) return 0.0f;
        const auto& layout = geometry->getVertexLayout();
        const auto& verts = geometry->getVertices();

        uint32_t posOffset = 0, binding = 0;
        bool found = false;
        for (const auto& a : layout.getAppAttributes()) {
            if (a.semantic == StarryEngine::Assets::VertexSemantic::Position) {
                posOffset = a.offset;
                binding = a.binding;
                found = true;
                break;
            }
        }
        if (!found) return 0.0f;

        uint32_t stride = layout.getBindingStride(binding);
        if (stride == 0) return 0.0f;

        const uint32_t strideF = stride / sizeof(float);
        const uint32_t posF = posOffset / sizeof(float) + 1;  // +1 = 位置 Y 分量
        float footY = std::numeric_limits<float>::max();
        for (size_t i = 0; i + strideF <= verts.size(); i += strideF)
            footY = std::min(footY, verts[i + posF]);
        return (footY == std::numeric_limits<float>::max()) ? 0.0f : footY;
    }
}

using namespace StarryEngine;


class PBRDemo {
public:
    PBRDemo(std::shared_ptr<RHI::IRHI> rhi, RHI::DescriptorPoolHandle descriptorPool, uint32_t width, uint32_t height);

    void createRenderer();

    void initIBL();

    std::shared_ptr<Assets::MaterialInstance> createSkyboxMaterial();

    std::shared_ptr<Assets::MaterialInstance> createGridMaterial();

    std::shared_ptr<Assets::MaterialInstance> createTexturedPbrMaterial();

    void createScene();

    std::shared_ptr<Assets::MaterialInstance> makeParticleMaterial();

    std::shared_ptr<Assets::MaterialInstance> makeStencilQuadMaterial(const std::string& fragPath);

    std::shared_ptr<RasterRenderer> getRenderer() { return m_renderer; }

    std::shared_ptr< Scene::Scene> getScene() { return m_scene; }

    void onUpdate(float deltaTime);
    void captureFrame();
    void updateTitle(GLFWwindow* win, float deltaTime);

    void setExitCallback(std::function<void()> cb) { m_requestExit = std::move(cb); }
private:
    bool loadModelGeometry(Assets::Geometry& outGeometry,std::vector<Assets::MaterialParams>& outParams,Assets::Skeleton* outSkeleton = nullptr);
    std::shared_ptr<Assets::MaterialInstance> makeModelMaterial(const Assets::MaterialParams& param, bool skinned);
    std::shared_ptr<Assets::MaterialInstance> makeOutlineMaterial();
    void addGriseoModel();

    Assets::Skeleton m_modelSkeleton;
    Assets::AnimationClip m_modelClip;
    std::string m_modelTextureDir = "fuxuan";  

    std::unique_ptr<AsyncBoneProducer> m_boneProducer;
    bool m_skeletalReady = false;
    float m_lastDelta = 0.0f;   
    std::vector<std::shared_ptr<Assets::MaterialInstance>> m_skinnedMaterials;
    std::shared_ptr<Assets::Geometry> m_modelGeometry;        
    size_t m_modelMaterialCount = 0;                            
    std::shared_ptr<Assets::MaterialInstance> m_outlineMaterial; 

    uint32_t m_width, m_height;
    std::shared_ptr<RHI::IRHI> m_rhi;
    std::shared_ptr<RasterRenderer> m_renderer;
    std::shared_ptr< Scene::Scene> m_scene;

    std::shared_ptr<DeferredRenderPath> m_renderPath;
    std::shared_ptr<FrameCapture> m_frameCapture;
    bool m_frameExit = false;       
    uint32_t m_frameExitTarget = 0;   
    std::function<void()> m_requestExit;   

    // IBL
    RHI::TextureHandle m_envCubemap;
    RHI::TextureHandle m_irradianceMap;
    RHI::TextureHandle m_prefilteredMap;
    RHI::TextureHandle m_brdfLut;
    RHI::SamplerHandle m_cubeSampler;
    RHI::SamplerHandle m_prefilterSampler;
    RHI::SamplerHandle m_lutSampler;

    StarryEngine::RHI::DescriptorSetHandle m_descriptorSet;
    StarryEngine::RHI::DescriptorPoolHandle m_descriptorPool;
    StarryEngine::RHI::DescriptorSetLayoutHandle m_descriptorSetLayout;

    std::shared_ptr<Assets::IBLBuilder> m_iblBuilder;
};
