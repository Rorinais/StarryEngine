#pragma once
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include"../assets/Assets.hpp"
#include"camera/OrthographicCamera.hpp"
#include"camera/PerspectiveCamera.hpp"

namespace StarryEngine::Scene {
    struct RenderObject {
        glm::mat4 transform;
        std::shared_ptr<Assets::Geometry> geometry;
        std::vector<std::shared_ptr<Assets::Material>> materials;
    };

    struct DrawItem {
        std::shared_ptr<Assets::Geometry> geometry;
        std::shared_ptr<Assets::Material> material;
        uint32_t indexOffset;   // 子网格在全局索引缓冲区中的偏移
        uint32_t indexCount;    // 子网格的索引数量
        glm::mat4 transform;    // 物体的模型矩阵
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

        void setActiveCamera(std::shared_ptr<ICamera> camera) { m_activeCamera = camera; }
        std::shared_ptr<ICamera> getActiveCamera() const { return m_activeCamera; }

    private:
        void updateObjectClassification(std::shared_ptr<RenderObject> object);

        std::vector<std::shared_ptr<RenderObject>> m_allObjects;
        std::vector<std::shared_ptr<RenderObject>> m_opaqueObjects;
        std::vector<std::shared_ptr<RenderObject>> m_transparentObjects;

        std::vector<std::shared_ptr<ICamera>> m_cameras;
        std::shared_ptr<ICamera> m_activeCamera;
    };

} // namespace StarryEngine::Scene