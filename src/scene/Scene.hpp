#pragma once
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include"../assets/Assets.hpp"
#include "../core/Clock.hpp"
#include"camera/OrthographicCamera.hpp"
#include"camera/PerspectiveCamera.hpp"
#include"animation/Animator.hpp"
#include "ParticleEmitter.hpp"


namespace StarryEngine::Scene {
    struct RenderObject {
        glm::mat4 transform = glm::mat4(1.0f);
        std::shared_ptr<StarryEngine::Assets::Geometry> geometry;
        std::vector<std::shared_ptr<Assets::MaterialInstance>> materials;

        std::vector<glm::mat4> instanceTransforms;
        RHI::BufferHandle instanceBuffer;   // 轻量句柄，值语义（与 DrawItem 一致）
        bool isInstanced = false;

        // 变换动画组件（可选）。由 Scene::update 驱动，改写 transform。
        std::shared_ptr<Animator> animator;
    };

    struct ProceduralEffect {
        std::shared_ptr<Assets::MaterialInstance> material;
        uint32_t vertexCount = 3;
        uint32_t instanceCount = 1;
        int order = 0;
    };

    class Scene {
    public:
        void addObject(std::shared_ptr<RenderObject> object);
        bool removeObject(std::shared_ptr<RenderObject> object);
        void clearObject();

        // 每帧推进场景逻辑（动画等）。由 Application 渲染循环调用。
        void update(const Clock& clock);

        void addProceduralEffect(std::shared_ptr<ProceduralEffect> effect);
        bool removeProceduralEffect(std::shared_ptr<ProceduralEffect> effect);
        const std::vector<std::shared_ptr<ProceduralEffect>>& getProceduralEffects() const;
        void clearProceduralEffects();

        // 粒子发射器：像 RenderObject 一样进场景，可增删
        void addParticleEmitter(std::shared_ptr<ParticleEmitter> emitter);
        bool removeParticleEmitter(std::shared_ptr<ParticleEmitter> emitter);
        void clearParticleEmitters();
        const std::vector<std::shared_ptr<ParticleEmitter>>& getParticleEmitters() const { return m_particleEmitters; }

        const std::vector<std::shared_ptr<RenderObject>>& getAllObjects() const { return m_allObjects; }
        const std::vector<std::shared_ptr<RenderObject>>& getOpaqueObjects() const { return m_opaqueObjects; }
        const std::vector<std::shared_ptr<RenderObject>>& getTransparentObjects() const { return m_transparentObjects; }
        

        void addCamera(std::shared_ptr<ICamera> camera) { m_cameras.push_back(camera); }
        const std::vector<std::shared_ptr<ICamera>>& getCameras() const { return m_cameras; }
        const std::shared_ptr<ICamera>& getCamera(uint32_t& index) { return m_cameras[index]; }
        std::shared_ptr<ICamera> getCamera(uint32_t index) const {
            return (index < m_cameras.size()) ? m_cameras[index] : m_activeCamera;
        }

        void setActiveCamera(std::shared_ptr<ICamera> camera) { m_activeCamera = camera; }
        std::shared_ptr<ICamera> getActiveCamera() const { return m_activeCamera; }

        void markContentDirty() { ++m_contentVersion; }
        uint32_t getContentVersion() const { return m_contentVersion; }

    private:
        void updateObjectClassification(std::shared_ptr<RenderObject> object);

    private:
        uint32_t m_contentVersion = 0;

        std::vector<std::shared_ptr<RenderObject>> m_allObjects;
        std::vector<std::shared_ptr<RenderObject>> m_opaqueObjects;
        std::vector<std::shared_ptr<RenderObject>> m_transparentObjects;
        std::vector<std::shared_ptr<ProceduralEffect>> m_proceduralEffects;
        std::vector<std::shared_ptr<ParticleEmitter>> m_particleEmitters;

        std::vector<std::shared_ptr<ICamera>> m_cameras;
        std::shared_ptr<ICamera> m_activeCamera;
    };

} // namespace StarryEngine::Scene