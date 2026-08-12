#include <renderer/Renderer.hpp>
#include <logging/Logger.hpp>
#include <assets/Assets.hpp>

namespace StarryEngine {

    Renderer::Renderer(std::shared_ptr<RHI::IRHI> rhi,
        RHI::DescriptorPoolHandle globalPool,
        std::shared_ptr<Scene::Scene> scene)
        : m_rhi(rhi), m_resMgr(rhi->getResourceManager()),
        m_globalPool(globalPool), m_scene(scene) {


    }

    void Renderer::destroy() {
        m_rhi->waitIdle();
        if (m_renderPath) {
            m_renderPath.reset();
        }
    }

    void Renderer::initDefaultMaterials() {
        if (m_materialsInitialized) return;

        m_defaultMaterial = Assets::MaterialInstance::createDefault(m_resMgr, m_globalSetLayout, m_globalPool, m_globalDescriptorSet);
        if (!m_defaultMaterial) {
            LOG_ERROR("Failed to create default material");
        }

        m_errorMaterial = Assets::MaterialInstance::createError(m_resMgr, m_globalSetLayout, m_globalPool, m_globalDescriptorSet);
        if (!m_errorMaterial) {
            LOG_ERROR("Failed to create error material");
        }

        m_materialsInitialized = true;
    }

    void Renderer::createGlobalSetLayout() {
        RHI::DescriptorSetLayoutDesc layoutDesc;
        layoutDesc.bindings = {
            {0, RHI::DescriptorType::UniformBuffer, 1, RHI::ShaderStage::Vertex | RHI::ShaderStage::Fragment}
        };
        m_globalSetLayout = m_resMgr->createDescriptorSetLayout(layoutDesc);
        if (!m_globalSetLayout.isValid()) {
            LOG_ERROR("Failed to create global descriptor set layout");
            return;
        }

        RHI::DescriptorSetDesc setDesc;
        setDesc.descriptorSetLayout = m_globalSetLayout;
        setDesc.descriptorPool = m_globalPool;
        m_globalDescriptorSet = m_resMgr->createDescriptorSet(setDesc);
        if (!m_globalDescriptorSet.isValid()) {
            LOG_ERROR("Failed to create global descriptor set");
        }
    }

    void Renderer::createGlobalUniformBuffer() {
        RHI::BufferDesc bufDesc;
        bufDesc.size = sizeof(Assets::GlobalUniforms);
        bufDesc.type = RHI::BufferType::Uniform;
        bufDesc.memoryType = RHI::MemoryType::CPU_To_GPU;
        bufDesc.allowUpdate = true;
        m_globalUniformBuffer = m_resMgr->createBuffer(bufDesc);
        if (!m_globalUniformBuffer.isValid()) {
            LOG_ERROR("Failed to create global uniform buffer");
            return;
        }

        auto* descSet = m_resMgr->getDescriptorSet(m_globalDescriptorSet);
        if (!descSet) {
            LOG_ERROR("Global descriptor set is invalid");
            return;
        }
        descSet->writeBuffer(0, 0, m_resMgr->getBuffer(m_globalUniformBuffer),
            0, sizeof(Assets::GlobalUniforms));
        descSet->update();
    }

    void Renderer::onResize(uint32_t width, uint32_t height) {
        m_rhi->waitIdle();
        if (m_renderPath) {
            m_renderPath->onResize(width, height);
        }

        m_needRebuildGraph = false;
        m_lastAnalyzedVersion = UINT32_MAX;
    }

    void Renderer::renderFrame(RHI::RHICommandEncoder* encoder, uint32_t frameIndex, const Clock& clock) {
        updateInstanceBuffers(m_scene->getOpaqueObjects());
        updateInstanceBuffers(m_scene->getTransparentObjects());

        buildSceneResources();

        updateDynamicBuffers(clock);

        if (m_renderPath) {
            m_renderPath->render(encoder, frameIndex);
        }
    }

    void Renderer::updateInstanceBuffers(const std::vector<std::shared_ptr<Scene::RenderObject>>& objects) {
        for (auto& obj : objects) {
            if (obj->isInstanced && obj->instanceBuffer.isValid() && !obj->instanceTransforms.empty()) {
                auto* buf = m_resMgr->getBuffer(obj->instanceBuffer);
                if (buf) {
                    buf->update(obj->instanceTransforms.data(), obj->instanceTransforms.size() * sizeof(glm::mat4), 0);
                }
            }
        }
    }

    void Renderer::buildSceneResources() {
        uint32_t version = m_scene->getContentVersion();
        if (!m_analysisSceneResult || version != m_lastAnalyzedVersion) {
            analysisScene();
            m_lastAnalyzedVersion = version;

            if (m_needRebuildGraph) {
                rebuildRenderGraph();
                m_needRebuildGraph = false;
            }
            else if (m_renderPath && m_analysisSceneResult) {
                m_renderPath->rebuildResources(*m_analysisSceneResult);
            }
        }
    }

    void Renderer::updateDynamicBuffers(const Clock& clock) {
        auto camera = m_scene->getActiveCamera();
        if (camera) {
            camera->update();
            Assets::GlobalUniforms globals;
            globals.view = camera->getViewMatrix();
            globals.proj = camera->getProjMatrix();
            globals.invView = glm::inverse(globals.view);
            globals.invProj = glm::inverse(globals.proj);
            globals.time = clock.getTime();
            globals.lightVP = m_lightVP;

            auto* buf = m_resMgr->getBuffer(m_globalUniformBuffer);
            if (buf) buf->update(&globals, sizeof(globals), 0);
        }

        // 统一提交所有材质的脏 UBO
        if (m_analysisSceneResult) {
            for (auto& mat : m_analysisSceneResult->materials) {
                mat->applyAllDirtyBlocks();
            }
        }
    }

    void Renderer::analysisScene() {
        AnalysisSceneResult result;
        std::unordered_map<GraphicsPipelineState, uint32_t, std::hash<GraphicsPipelineState>> pipelineIndexMap;
        std::unordered_set<Assets::MaterialInstance*> uniqueMaterials; 
        m_sceneAnalyzer = std::make_unique<SceneAnalyzer>(
            m_resMgr.get(),
            m_globalDescriptorSet,
            m_defaultMaterial,
            m_errorMaterial
        );

        m_analysisSceneResult = m_sceneAnalyzer->analyze(*m_scene);
    }

    void Renderer::reloadShader(const std::string& vertPath, const std::string& fragPath) {
        if (!m_analysisSceneResult) return;

        // 收集需要重载的模板（去重）
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

        // 对每个受影响的模板执行一次 reloadShaders
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

        // 然后对所有引用了这些模板的材质实例，重建描述符集
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

    void Renderer::reloadAllShaders() {
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
        if (m_renderPath) {
            m_renderPath->rebuildResources(*m_analysisSceneResult);
        }
    }

    void Renderer::rebuildRenderGraph() {
        if (m_renderPath && m_analysisSceneResult) {
            m_renderPath->initialize();
            m_renderPath->rebuildResources(*m_analysisSceneResult);
        }
        else {
            LOG_ERROR("rebuildRenderGraph called but no scene result");
        }
    }

    void Renderer::setRenderPath(std::shared_ptr<IRenderPath> newRenderPath) {
        if (!newRenderPath) return;
        destroy();
        m_renderPath = std::move(newRenderPath);

        // 全局描述符集 → BaseRenderPath（供 PresentationPass 使用）
        if (auto* bp = dynamic_cast<BaseRenderPath*>(m_renderPath.get())) {
            bp->setPresentationDescriptorData(m_globalSetLayout, m_globalDescriptorSet);
        }

        // 确保至少 rebuild 一次（presentation pipeline 需要 descriptor 数据就绪）
        m_needRebuildGraph = true;
    }

    void Renderer::setImGuiManager(ImGuiManager* mgr, uint32_t imageCount) {
        if (m_renderPath) {
            if (auto* dp = dynamic_cast<DeferredRenderPath*>(m_renderPath.get())) {
                dp->setImGuiManager(mgr, imageCount);
            }
        }
    }

    void Renderer::addOverlayPass(const std::string& tag, std::shared_ptr<IPassExecutor> executor) {
        if (m_renderPath) {
            m_renderPath->addOverlayPass(tag, std::move(executor));
        }
    }

    void Renderer::addOverlayPass(const OverlayPassDesc& desc) {
        if (m_renderPath) {
            LOG_INFO("Renderer: adding overlay '{}', {} color outputs", desc.tag, desc.colorOutputs.size());
            m_renderPath->addOverlayPass(desc);
        }
    }

    void Renderer::removeOverlayPass(const std::string& tag) {
        if (m_renderPath) {
            m_renderPath->removeOverlayPass(tag);
        }
    }

    void Renderer::clearOverlayPasses() {
        if (m_renderPath) {
            m_renderPath->clearOverlayPasses();
        }
    }

} // namespace StarryEngine