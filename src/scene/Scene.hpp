#pragma once
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include"../assets/Assets.hpp"
#include "SceneType.hpp"
#include"camera/OrthographicCamera.hpp"
#include"camera/PerspectiveCamera.hpp"


namespace StarryEngine::Scene {
    struct RenderObject {
        glm::mat4 transform = glm::mat4(1.0f);
        std::shared_ptr<StarryEngine::Assets::Geometry> geometry;
        std::vector<std::shared_ptr<Assets::MaterialInstance>> materials;

        std::vector<glm::mat4> instanceTransforms;  
        std::shared_ptr<RHI::BufferHandle> instanceBuffer; 
        bool isInstanced = false;
    };

    class Scene {
    public:
        void addObject(std::shared_ptr<RenderObject> object);
        bool removeObject(std::shared_ptr<RenderObject> object);
        void clear();
        void update(float deltaTime);

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

        std::vector<std::shared_ptr<ICamera>> m_cameras;
        std::shared_ptr<ICamera> m_activeCamera;
    };

} // namespace StarryEngine::Scene