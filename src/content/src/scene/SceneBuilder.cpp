#include <content/scene/SceneBuilder.hpp>
#include <content/render/materials/BuiltinMaterials.hpp>
#include <assets/geometry/GeometryGenerator.hpp>
#include <scene/camera/PerspectiveCamera.hpp>
#include <logging/Logger.hpp>

namespace StarryEngine::Content {

    SceneBuilder::SceneBuilder(std::shared_ptr<Scene::Scene> scene, const SceneResources& res)
        : m_scene(std::move(scene)), m_res(res) {
        if (!m_scene) {
            LOG_ERROR("SceneBuilder: 场景为空");
        }
    }

    SceneBuilder& SceneBuilder::addSkybox(const std::string& hdrPath) {
        if (!m_scene || !m_res.rhi) return *this;

        if (!m_ibl) {
            m_ibl = std::make_unique<Assets::IBLBuilder>(m_res.rhi->getResourceManager(), m_res.rhi);
        }
        auto cubemap = m_ibl->buildEnvCubemap(hdrPath, 128);

        RHI::SamplerDesc sampDesc;
        sampDesc.minFilter = RHI::SamplerFilter::Linear;
        sampDesc.magFilter = RHI::SamplerFilter::Linear;
        sampDesc.addressU = RHI::SamplerAddressMode::ClampToEdge;
        sampDesc.addressV = RHI::SamplerAddressMode::ClampToEdge;
        sampDesc.addressW = RHI::SamplerAddressMode::ClampToEdge;
        sampDesc.maxLod = 1.0f;
        auto sampler = m_res.rhi->getResourceManager()->createSampler(sampDesc);

        auto effect = std::make_shared<Scene::ProceduralEffect>();
        effect->material = Content::BuiltinMaterials::createSkybox(
            m_res.rhi->getResourceManager(), m_res.globalSetLayout,
            m_res.pool, m_res.globalSet, cubemap, sampler);
        if (effect->material) {
            m_scene->addProceduralEffect(effect);
            LOG_INFO("[scene] 天空盒已添加: {}", hdrPath);
        } else {
            LOG_ERROR("[scene] 天空盒材质创建失败: {}", hdrPath);
        }
        return *this;
    }

    SceneBuilder& SceneBuilder::addGrid(float size, uint32_t divisions) {
        if (!m_scene || !m_res.rhi) return *this;

        auto material = Content::BuiltinMaterials::createGrid(
            m_res.rhi->getResourceManager(), m_res.globalSetLayout,
            m_res.pool, m_res.globalSet);
        if (!material) {
            LOG_ERROR("[scene] 网格材质创建失败");
            return *this;
        }
        auto object = std::make_shared<Scene::RenderObject>();
        object->geometry = Assets::GeometryGenerator::createGrid(
            m_res.rhi->getResourceManager(), size, divisions);
        object->materials = { material };
        m_scene->addObject(object);
        LOG_INFO("[scene] 网格地面已添加: size={} divisions={}", size, divisions);
        return *this;
    }

    SceneBuilder& SceneBuilder::addObject(std::shared_ptr<Assets::Geometry> geometry,
                                          std::shared_ptr<Assets::MaterialInstance> material,
                                          const glm::mat4& transform) {
        if (!m_scene || !geometry) return *this;
        auto object = std::make_shared<Scene::RenderObject>();
        object->geometry = std::move(geometry);
        if (material) object->materials = { std::move(material) };
        object->transform = transform;
        m_scene->addObject(object);
        return *this;
    }

    SceneBuilder& SceneBuilder::addCamera(const CameraParams& params) {
        if (!m_scene) return *this;
        auto camera = std::make_shared<Scene::PerspectiveCamera>();
        camera->setPerspective(glm::radians(params.fovDeg), params.aspect, params.nearZ, params.farZ);
        camera->lookAt(params.eye, params.target, params.up);
        m_scene->addCamera(camera);
        m_scene->setActiveCamera(camera);
        LOG_INFO("[scene] 相机已添加");
        return *this;
    }

    std::shared_ptr<Scene::Scene> SceneBuilder::build() {
        if (m_scene && m_scene->getActiveCamera()) {
            LOG_INFO("[scene] 场景构建完成");
        }
        return m_scene;
    }

} // namespace StarryEngine::Content
