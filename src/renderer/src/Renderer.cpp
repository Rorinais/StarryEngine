#include <renderer/Renderer.hpp>
#include <logging/Logger.hpp>
#include <assets/Assets.hpp>
#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace StarryEngine {

    Renderer::Renderer(std::shared_ptr<RHI::IRHI> rhi,
        RHI::DescriptorPoolHandle globalPool,
        std::shared_ptr<Scene::Scene> scene)
        : m_rhi(rhi), m_resMgr(rhi->getResourceManager()),
        m_globalPool(globalPool), m_scene(scene) {
        const char* env = std::getenv("STARRY_PARALLEL_RECORD");
        if (env) {
            const bool threaded = (std::strcmp(env, "threads") == 0 || std::strcmp(env, "1") == 0);
            JobSystem::Config jcfg;
            jcfg.enableThreads = threaded;
            if (const char* workersEnv = std::getenv("STARRY_JOB_WORKERS"))
                jcfg.workerCount = static_cast<uint32_t>(std::atoi(workersEnv));
            m_jobSystem = std::make_unique<JobSystem>(jcfg);
            LOG_INFO("[render] 帧内并行: {}（数据 job + 录制 job，worker={}）",threaded ? "threads" : "inline", m_jobSystem->workerCount());
        }

        m_frameInFlight = (std::getenv("STARRY_FRAME_IN_FLIGHT") != nullptr);
        if (m_frameInFlight && !m_jobSystem) {
            JobSystem::Config jcfg;
            jcfg.enableThreads = true;
            if (const char* workersEnv = std::getenv("STARRY_JOB_WORKERS"))
                jcfg.workerCount = static_cast<uint32_t>(std::atoi(workersEnv));
            m_jobSystem = std::make_unique<JobSystem>(jcfg);
            LOG_INFO("[render] 帧间解耦(FID): 无 STARRY_PARALLEL_RECORD，隐式开线程池（worker={}）",m_jobSystem->workerCount());
        }
    }

    void Renderer::destroy() {
        m_rhi->waitIdle();
        if (m_renderPath) {
            m_renderPath.reset();
        }
    }

    void Renderer::initDefaultMaterials() {
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

        for (uint32_t s = 0; s < RHI::kMaxFramesInFlight; ++s) {
            RHI::DescriptorSetDesc setDesc;
            setDesc.descriptorSetLayout = m_globalSetLayout;
            setDesc.descriptorPool = m_globalPool;
            setDesc.debugName = "GlobalSet_slot" + std::to_string(s);
            m_globalDescriptorSets[s] = m_resMgr->createDescriptorSet(setDesc);
            if (!m_globalDescriptorSets[s].isValid()) {
                LOG_ERROR("Failed to create global descriptor set slot {}", s);
            }
        }
    }

    void Renderer::createGlobalUniformBuffer() {
        for (uint32_t s = 0; s < RHI::kMaxFramesInFlight; ++s) {
            RHI::BufferDesc bufDesc;
            bufDesc.size = sizeof(Assets::GlobalUniforms);
            bufDesc.type = RHI::BufferType::Uniform;
            bufDesc.memoryType = RHI::MemoryType::CPU_To_GPU;
            bufDesc.allowUpdate = true;
            m_globalUniformBuffers[s] = m_resMgr->createBuffer(bufDesc);
            if (!m_globalUniformBuffers[s].isValid()) {
                LOG_ERROR("Failed to create global uniform buffer slot {}", s);
                continue;
            }

            auto* descSet = m_resMgr->getDescriptorSet(m_globalDescriptorSets[s]);
            if (!descSet) {
                LOG_ERROR("Global descriptor set slot {} is invalid", s);
                continue;
            }
            descSet->writeBuffer(0, 0, m_resMgr->getBuffer(m_globalUniformBuffers[s]),
                0, sizeof(Assets::GlobalUniforms));
            descSet->update();
        }
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

        if (m_frameInFlight && m_jobSystem) {
            m_jobSystem->waitAll();
            buildSceneResources();
            ensureParallelRecording();

            uint32_t slot = m_rhi->getCurrentFrameIndex();
            if (slot >= RHI::kMaxFramesInFlight) slot = 0;
            uint32_t nextSlot = (slot + 1) % RHI::kMaxFramesInFlight;

            if (!m_fidHasKicked) {
                submitFrameDataJobs(slot);
                updateGlobals(clock, slot);
                m_jobSystem->waitAll();
            } else {
                updateGlobals(clock, slot); 
            }

            if (m_renderPath) {
                m_renderPath->setParallelRecording(m_parallelRecording.get());
                m_renderPath->render(encoder, frameIndex);  
            }

            submitFrameDataJobs(nextSlot);        
            if (m_boneProvider) m_boneProvider(nextSlot);   
            m_fidHasKicked = true;
            return;
        }

        buildSceneResources();          
        ensureParallelRecording();    

        uint32_t slot = m_rhi->getCurrentFrameIndex();
        if (slot >= RHI::kMaxFramesInFlight) slot = 0;

        if (m_jobSystem) {

            submitFrameDataJobs(slot); 
            updateGlobals(clock, slot);
            m_jobSystem->waitAll();    
        } else {
            updateInstanceBuffers(m_scene->getOpaqueObjects(), slot);
            updateInstanceBuffers(m_scene->getTransparentObjects(), slot);
            updateDynamicBuffers(clock, slot);
        }

        if (m_renderPath) {
            m_renderPath->setParallelRecording(m_parallelRecording.get());   // 指针稳定，幂等
            m_renderPath->render(encoder, frameIndex);   // Phase1 录制 jobs + waitAll + Phase2
        }
    }

    void Renderer::ensureParallelRecording() {
        if (m_parallelRecording) return;
        if (!m_jobSystem) return;   

        const bool threaded = m_jobSystem->threadingEnabled();

        auto prc = std::make_unique<ParallelRecordingContext>();
        prc->jobs = m_jobSystem.get();
        prc->workerCount = threaded ? std::max(1u, m_jobSystem->workerCount()) : 1u;
        prc->allocateSecondary = [this](uint32_t workerIndex) {
            return m_rhi->allocateSecondaryCommandEncoder(workerIndex);
        };
        m_parallelRecording = std::move(prc);

        m_rhi->prepareParallelRecording(m_parallelRecording->workerCount);

        LOG_INFO("[render] 并行命令录制: {}（per-worker 池数={}）",threaded ? "threads" : "inline", m_parallelRecording->workerCount);
    }

    void Renderer::updateInstanceBuffers(const std::vector<std::shared_ptr<Scene::RenderObject>>& objects, uint32_t slot) {
        for (auto& obj : objects) {
            if (!obj->isInstanced || obj->instanceTransforms.empty()) continue;
            if (slot >= RHI::kMaxFramesInFlight) continue;
            auto* buf = m_resMgr->getBuffer(obj->instanceBuffers[slot]);
            if (buf) {
                buf->update(obj->instanceTransforms.data(), obj->instanceTransforms.size() * sizeof(glm::mat4), 0);
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

    void Renderer::updateGlobals(const Clock& clock, uint32_t slot) {
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

            auto* buf = m_resMgr->getBuffer(m_globalUniformBuffers[slot]);
            if (buf) buf->update(&globals, sizeof(globals), 0);
        }
    }

    void Renderer::updateDynamicBuffers(const Clock& clock, uint32_t slot) {
        updateGlobals(clock, slot);

        if (m_analysisSceneResult) {
            for (auto& mat : m_analysisSceneResult->materials) {
                mat->applyAllDirtyBlocks(slot);
            }
        }
    }

    void Renderer::submitFrameDataJobs(uint32_t slot) {
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

    void Renderer::analysisScene() {
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

    void Renderer::reloadShader(const std::string& vertPath, const std::string& fragPath) {
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

        if (auto* bp = dynamic_cast<BaseRenderPath*>(m_renderPath.get())) {
            bp->setPresentationDescriptorData(m_globalSetLayout, std::vector<RHI::DescriptorSetHandle>(
                m_globalDescriptorSets.begin(), m_globalDescriptorSets.end()));
        }

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