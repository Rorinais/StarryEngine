#include "Scene.hpp"
#include <algorithm>
#include "../logging/Logger.hpp"

namespace StarryEngine::Scene {

    void Scene::addObject(std::shared_ptr<RenderObject> object) {
        m_allObjects.push_back(object);
        updateObjectClassification(object);
        markContentDirty();
    }

    bool Scene::removeObject(std::shared_ptr<RenderObject> object) {
        auto it = std::find(m_allObjects.begin(), m_allObjects.end(), object);
        if (it == m_allObjects.end()) return false;
        m_allObjects.erase(it);

        auto opaqueIt = std::find(m_opaqueObjects.begin(), m_opaqueObjects.end(), object);
        if (opaqueIt != m_opaqueObjects.end()) m_opaqueObjects.erase(opaqueIt);
        auto transIt = std::find(m_transparentObjects.begin(), m_transparentObjects.end(), object);
        if (transIt != m_transparentObjects.end()) m_transparentObjects.erase(transIt);

        markContentDirty();
        return true;
    }

    void Scene::clearObject() {
        m_allObjects.clear();
        m_opaqueObjects.clear();
        m_transparentObjects.clear();
        markContentDirty();
    }

    void Scene::update(float deltaTime) {
        // 可以添加动画更新等逻辑，暂时为空
    }

    void Scene::updateObjectClassification(std::shared_ptr<RenderObject> object) {
        bool hasTransparent = false;
        for (auto& mat : object->materials) {
            if (mat && mat->isTransparent()) {  
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

    void Scene::addProceduralEffect(std::shared_ptr<ProceduralEffect> effect) {
        if (!effect) return;
        m_proceduralEffects.push_back(effect);
        markContentDirty();
    }

    bool Scene::removeProceduralEffect(std::shared_ptr<ProceduralEffect> effect) {
        auto it = std::find(m_proceduralEffects.begin(), m_proceduralEffects.end(), effect);
        if (it == m_proceduralEffects.end()) return false;
        m_proceduralEffects.erase(it);
        markContentDirty();
        return true;
    }

    const std::vector<std::shared_ptr<ProceduralEffect>>& Scene::getProceduralEffects() const {
        return m_proceduralEffects;
    }

    void Scene::clearProceduralEffects() {
        if (m_proceduralEffects.empty()) return;
        m_proceduralEffects.clear();
        markContentDirty();
    }

} // namespace StarryEngine::Scene