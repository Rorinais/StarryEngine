#include "Scene.hpp"
#include <algorithm>
#include "../logging/Logger.hpp"

namespace StarryEngine::Scene {

    void Scene::addObject(std::shared_ptr<RenderObject> object) {
        m_allObjects.push_back(object);
        updateObjectClassification(object);
    }

    bool Scene::removeObject(std::shared_ptr<RenderObject> object) {
        auto it = std::find(m_allObjects.begin(), m_allObjects.end(), object);
        if (it == m_allObjects.end()) return false;
        m_allObjects.erase(it);

        auto opaqueIt = std::find(m_opaqueObjects.begin(), m_opaqueObjects.end(), object);
        if (opaqueIt != m_opaqueObjects.end()) m_opaqueObjects.erase(opaqueIt);
        auto transIt = std::find(m_transparentObjects.begin(), m_transparentObjects.end(), object);
        if (transIt != m_transparentObjects.end()) m_transparentObjects.erase(transIt);
        return true;
    }

    void Scene::clear() {
        m_allObjects.clear();
        m_opaqueObjects.clear();
        m_transparentObjects.clear();
    }

    void Scene::update(float deltaTime) {
        // 可以添加动画更新等逻辑，暂时为空
    }

    void Scene::updateObjectClassification(std::shared_ptr<RenderObject> object) {
        bool hasTransparent = false;
        for (auto& mat : object->materials) {
            if (mat->isTransparent()) { 
                hasTransparent = true;
                break;
            }
        }
        if (hasTransparent) {
            m_transparentObjects.push_back(object);
        }
        else {
            m_opaqueObjects.push_back(object);
        }
    }

} // namespace StarryEngine::Scene