#include <renderer/RasterRenderer.hpp>
#include <logging/Logger.hpp>
#include <assets/Assets.hpp>
#include <algorithm>
#include <unordered_set>

namespace StarryEngine {

    void RasterRenderer::initDefaultMaterials() {
        if (m_materialsInitialized) return;
        RHI::DescriptorSetHandle gs0 = m_globalDescriptorSets[0];

        m_defaultMaterial = Assets::MaterialInstance::createDefault(m_resMgr, m_globalSetLayout, m_globalPool, gs0);
        if (!m_defaultMaterial) {
            LOG_ERROR("Failed to create default material");
        }

        m_errorMaterial = Assets::MaterialInstance::createError(m_resMgr, m_globalSetLayout, m_globalPool, gs0);
        if (!m_errorMaterial) {
            LOG_ERROR("Failed to create error material");
        }

        m_materialsInitialized = true;
    }

    void RasterRenderer::buildSceneResources() {
        uint32_t version = m_scene->getContentVersion();
        if (!m_analysisSceneResult || version != m_lastAnalyzedVersion || m_needRebuildGraph) {
            if (version != m_lastAnalyzedVersion || !m_analysisSceneResult) {
                analysisScene();
                m_lastAnalyzedVersion = version;
            }

            if (m_needRebuildGraph) {
                doRebuildRenderGraph();
                m_needRebuildGraph = false;
            }
            else if (m_renderPath && m_analysisSceneResult) {
                m_renderPath->rebuildResources(*m_analysisSceneResult);
            }
        }
    }

    void RasterRenderer::updateInstanceBuffers(const std::vector<std::shared_ptr<Scene::RenderObject>>& objects, uint32_t slot) {
        for (auto& obj : objects) {
            if (!obj->isInstanced || obj->instanceTransforms.empty()) continue;
            if (slot >= RHI::kMaxFramesInFlight) continue;
            auto* buf = m_resMgr->getBuffer(obj->instanceBuffers[slot]);
            if (buf) {
                buf->update(obj->instanceTransforms.data(), obj->instanceTransforms.size() * sizeof(glm::mat4), 0);
            }
        }
    }

    void RasterRenderer::updateDynamicBuffers(const Clock& clock, uint32_t slot) {
        if (m_analysisSceneResult) {
            for (auto& mat : m_analysisSceneResult->materials) {
                mat->applyAllDirtyBlocks(slot);
            }
        }
    }

    void RasterRenderer::submitFrameDataJobs(uint32_t slot) {
        auto* jobs = m_jobSystem.get();
        if (!jobs) return;
        if (slot >= RHI::kMaxFramesInFlight) return;

        auto submitInstances = [jobs, this, slot](const std::vector<std::shared_ptr<Scene::RenderObject>>& objects) {
            for (const auto& obj : objects) {
                if (!(obj->isInstanced && !obj->instanceTransforms.empty())) continue;
                auto* buf = m_resMgr->getBuffer(obj->instanceBuffers[slot]);
                if (!buf) continue;
                jobs->submit([buf, obj]() {
                    buf->update(obj->instanceTransforms.data(),
                        obj->instanceTransforms.size() * sizeof(glm::mat4), 0);
                });
            }
        };
        submitInstances(m_scene->getOpaqueObjects());
        submitInstances(m_scene->getTransparentObjects());

        if (m_analysisSceneResult) {
            for (const auto& mat : m_analysisSceneResult->materials) {
                jobs->submit([mat, slot]() { mat->applyAllDirtyBlocks(slot); });
            }
        }
    }

    void RasterRenderer::analysisScene() {
        AnalysisSceneResult result;
        std::unordered_map<GraphicsPipelineState, uint32_t, std::hash<GraphicsPipelineState>> pipelineIndexMap;
        std::unordered_set<Assets::MaterialInstance*> uniqueMaterials;
        m_sceneAnalyzer = std::make_unique<SceneAnalyzer>(
            m_resMgr.get(),
            m_globalDescriptorSets,
            m_defaultMaterial,
            m_errorMaterial
        );

        m_analysisSceneResult = m_sceneAnalyzer->analyze(*m_scene);
    }

    void RasterRenderer::reloadShader(const std::string& vertPath, const std::string& fragPath) {
        if (!m_analysisSceneResult) return;

        std::unordered_set<Assets::MaterialTemplate*> affectedTemplates;
        for (auto& matInst : m_analysisSceneResult->materials) {
            auto tmpl = matInst->getTemplate();
            if (auto* dmpl = dynamic_cast<Assets::DefaultMaterialTemplate*>(tmpl.get())) {
                if (dmpl->getVSPath() == vertPath || dmpl->getFSPath() == fragPath) {
                    affectedTemplates.insert(tmpl.get());
                }
            }
        }

        if (affectedTemplates.empty()) return;

        bool anySuccess = false;
        for (auto* tmpl : affectedTemplates) {
            auto* dmpl = dynamic_cast<Assets::DefaultMaterialTemplate*>(tmpl);
            if (!dmpl) continue;
            if (dmpl->reloadShaders(vertPath, fragPath)) {
                anySuccess = true;
            }
            else {
                dmpl->invalidate();
                LOG_ERROR("Shader reload failed, switching to error material");
            }
        }

        for (auto& matInst : m_analysisSceneResult->materials) {
            if (affectedTemplates.count(matInst->getTemplate().get())) {
                matInst->recreateDescriptorSets();
            }
        }

        if (anySuccess || !affectedTemplates.empty()) {
            Assets::PipelineCache::invalidateAll(m_resMgr.get());
            m_lastAnalyzedVersion = UINT32_MAX;
        }
    }

    void RasterRenderer::reloadAllShaders() {
        if (!m_analysisSceneResult) return;
        std::unordered_set<std::string> processed;
        for (auto& matInst : m_analysisSceneResult->materials) {
            auto tmpl = matInst->getTemplate();
            auto* dmpl = dynamic_cast<Assets::DefaultMaterialTemplate*>(tmpl.get());
            if (!dmpl) continue;
            std::string key = dmpl->getVSPath() + "|" + dmpl->getFSPath();
            if (processed.count(key)) continue;
            processed.insert(key);
            reloadShader(dmpl->getVSPath(), dmpl->getFSPath());
        }
        if (m_renderPath && m_analysisSceneResult) {
            m_renderPath->rebuildResources(*m_analysisSceneResult);
        }
    }

    void RasterRenderer::doRebuildRenderGraph() {
        if (m_renderPath && m_analysisSceneResult) {
            m_renderPath->initialize();
            m_renderPath->rebuildResources(*m_analysisSceneResult);
        }
        else {
            LOG_ERROR("rebuildRenderGraph called but no scene result");
        }
    }

} // namespace StarryEngine
