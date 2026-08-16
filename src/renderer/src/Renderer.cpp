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
        // 帧内并行线程池（ADR-6 第 2 步）：构造时按 env 建，demo 侧骨骼 job 才能
        // 在 onUpdate（早于 renderFrame）就提交进池。无 env → 串行（零行为变化）。
        const char* env = std::getenv("STARRY_PARALLEL_RECORD");
        if (env) {
            const bool threaded = (std::strcmp(env, "threads") == 0 || std::strcmp(env, "1") == 0);
            JobSystem::Config jcfg;
            jcfg.enableThreads = threaded;
            if (const char* workersEnv = std::getenv("STARRY_JOB_WORKERS"))
                jcfg.workerCount = static_cast<uint32_t>(std::atoi(workersEnv));
            m_jobSystem = std::make_unique<JobSystem>(jcfg);
            LOG_INFO("[render] 帧内并行: {}（数据 job + 录制 job，worker={}）",
                threaded ? "threads" : "inline", m_jobSystem->workerCount());
        }

        // 帧间解耦（ADR-6 终点，STARRY_FRAME_IN_FLIGHT=1）：渲染线程形态。
        // 数据相（实例/材质/骨骼）在 GPU(N) 执行期算 N+1 → 需要 worker 才有重叠；
        // 无 STARRY_PARALLEL_RECORD 时隐式开线程池（FID 自足，不用额外 env）。
        m_frameInFlight = (std::getenv("STARRY_FRAME_IN_FLIGHT") != nullptr);
        if (m_frameInFlight && !m_jobSystem) {
            JobSystem::Config jcfg;
            jcfg.enableThreads = true;
            if (const char* workersEnv = std::getenv("STARRY_JOB_WORKERS"))
                jcfg.workerCount = static_cast<uint32_t>(std::atoi(workersEnv));
            m_jobSystem = std::make_unique<JobSystem>(jcfg);
            LOG_INFO("[render] 帧间解耦(FID): 无 STARRY_PARALLEL_RECORD，隐式开线程池（worker={}）",
                m_jobSystem->workerCount());
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
        // 构造注入的 globalSet 是渲染期废码（SceneAnalyzer 用自己持有的 per-slot 全局集覆盖 set0），
        // 这里给槽 0 的句柄即可。
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

        // per-slot global 描述符集（ADR-6）：每槽一个，录制时绑 slot 对应的那个。
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
        // ── 帧间解耦（ADR-6 终点，STARRY_FRAME_IN_FLIGHT=1）：渲染线程形态 ──
        //   主线程持续渲染 N，worker 在 GPU(N) 执行期算 N+1 的数据（1 帧数据潜伏）。
        //   waitAll() 先 join 上帧 renderFrame 末尾 kick 的 data(N)，再碰共享场景——
        //   否则上一波 worker 还在读场景时 analysisScene 重建 m_analysisSceneResult 会 UAF。
        if (m_frameInFlight && m_jobSystem) {
            m_jobSystem->waitAll();
            buildSceneResources();
            ensureParallelRecording();

            uint32_t slot = m_rhi->getCurrentFrameIndex();
            if (slot >= RHI::kMaxFramesInFlight) slot = 0;
            uint32_t nextSlot = (slot + 1) % RHI::kMaxFramesInFlight;

            if (!m_fidHasKicked) {
                // 首帧：没有上一帧 kick 的数据可 join，同步填本帧槽位再录。
                // 关键：材质块首写会灌满两槽（writtenOnce 翻转），必须在 GPU(0) 读之前完成，
                // 否则 worker memcpy 与 GPU 读同槽位竞争。此后每帧才走解耦。
                submitFrameDataJobs(slot);
                updateGlobals(clock, slot);
                m_jobSystem->waitAll();
            } else {
                updateGlobals(clock, slot);   // 本帧相机/时间 → 本帧槽位（主线程，录制前）
            }

            if (m_renderPath) {
                m_renderPath->setParallelRecording(m_parallelRecording.get());
                m_renderPath->render(encoder, frameIndex);   // 录制(N) + Phase1 waitAll + execute(N)
            }

            // GPU(N) 执行期：算 N+1 的数据（不等待，帧 N+1 开头 waitAll join）。
            // 残余 race（诚实标注）：data(N+1) 写槽 (N+1)%2 时 GPU(N-1) 可能仍读同槽位
            // （(N-1)%2==(N+1)%2）；仅 GPU 帧超时(>16ms)才显形，@60fps 有大量余量。
            // airtight = 帧潜伏 3（ADR-8，后续）。
            submitFrameDataJobs(nextSlot);          // 实例缓冲×对象 + 材质 dirty×材质实例 → (N+1)%2
            if (m_boneProvider) m_boneProvider(nextSlot);   // demo 骨骼 job → (N+1)%2
            m_fidHasKicked = true;
            return;
        }

        // ── 默认 join-before-submit（Stage 1 已把 per-slot 穿好，行为与今天一致）──
        buildSceneResources();          // 串行：场景分析 + 资源建/毁（ResourceManager map 写）
        ensureParallelRecording();      // 幂等；JobSystem 已在构造时按 env 建好

        // 帧槽位（ADR-6）：renderFrame 在 VulkanRHI::renderFrame 的 beginFrame 之后、submitFrame 之前
        // 执行，getCurrentFrameIndex() 已 == 本帧槽位 N%2。所有 CPU 数据写该槽，录制绑该槽。
        uint32_t slot = m_rhi->getCurrentFrameIndex();
        if (slot >= RHI::kMaxFramesInFlight) slot = 0;

        if (m_jobSystem) {
            // ── Phase 0：帧内 CPU 数据 job（ADR-6 第 2 步剩余块）──
            // 骨骼 job 已由 demo 在 onUpdate 提交进同一池；这里提交其余数据 job，
            // waitAll 一并兜住（join-before-submit，数据全部就绪才进录制）。
            submitFrameDataJobs(slot);  // 实例缓冲×对象 + 材质 dirty×材质实例（显式传槽）
            updateGlobals(clock, slot); // 主线程上传 globals UBO（与 worker 重叠）
            m_jobSystem->waitAll();     // 数据帧 barrier
        } else {
            // 原串行路径（无 env，零行为变化；同样写本帧槽位）
            updateInstanceBuffers(m_scene->getOpaqueObjects(), slot);
            updateInstanceBuffers(m_scene->getTransparentObjects(), slot);
            updateDynamicBuffers(clock, slot);
        }

        if (m_renderPath) {
            m_renderPath->setParallelRecording(m_parallelRecording.get());   // 指针稳定，幂等
            m_renderPath->render(encoder, frameIndex);   // Phase1 录制 jobs + waitAll + Phase2
        }
    }

    // 帧内并行（ADR-6 第 2 步）：JobSystem 已在构造函数按 env 建好，这里只搭录制上下文。
    //   无 STARRY_PARALLEL_RECORD      → m_jobSystem 为 null → 原串行路径（零行为变化）。
    //   STARRY_PARALLEL_RECORD=inline   → secondary CB + 数据 job 同步内联（验数据流）。
    //   STARRY_PARALLEL_RECORD=threads  → 数据 job + 录制 job 全部真并行（同一线程池）。
    void Renderer::ensureParallelRecording() {
        if (m_parallelRecording) return;
        if (!m_jobSystem) return;   // 无 env：串行

        const bool threaded = m_jobSystem->threadingEnabled();

        auto prc = std::make_unique<ParallelRecordingContext>();
        prc->jobs = m_jobSystem.get();
        prc->workerCount = threaded ? std::max(1u, m_jobSystem->workerCount()) : 1u;
        prc->allocateSecondary = [this](uint32_t workerIndex) {
            return m_rhi->allocateSecondaryCommandEncoder(workerIndex);
        };
        m_parallelRecording = std::move(prc);

        // 主线程预建 [帧槽位][worker] 命令池骨架：worker 只在录制期从自己的池取 secondary，
        // 且不在池里并发分配（避免验证层 UNASSIGNED-Threading-MultipleThreads-Write）。
        m_rhi->prepareParallelRecording(m_parallelRecording->workerCount);

        LOG_INFO("[render] 并行命令录制: {}（per-worker 池数={}）",
            threaded ? "threads" : "inline", m_parallelRecording->workerCount);
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
        // camera + globals UBO：per-slot buffer，主线程写（不能并行）。
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

        // 统一提交所有材质的脏 UBO（显式槽位：槽位在 kick 时捕获，job 内不得读可变成员）
        if (m_analysisSceneResult) {
            for (auto& mat : m_analysisSceneResult->materials) {
                mat->applyAllDirtyBlocks(slot);
            }
        }
    }

    // ── Phase 0 数据 job（ADR-6 第 2 步）：实例缓冲 × 对象 + 材质 dirty × 材质实例 ──
    // 线程安全依据：每 job 只写自己的输出（不同 buffer 的 persistent-mapped memcpy），
    // 只读共享常量（ResourceManager 查找在数据 phase 内无写）。骨骼 job 由 demo 提交。
    // slot 显式传入：JobSystem inline 模式 job 在 submit() 同步执行，job 内读成员读到的是
    // submit 时的旧值 → 槽位在 kick 时捕获进 lambda。
    void Renderer::submitFrameDataJobs(uint32_t slot) {
        auto* jobs = m_jobSystem.get();
        if (!jobs) return;
        if (slot >= RHI::kMaxFramesInFlight) return;

        // 实例缓冲 × 对象：检查在提交线程做，job 只 memcpy（写本帧槽位）
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

        // 材质 dirty × 材质实例：applyAllDirtyBlocks 只 memcpy，零描述符写
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

        // per-slot 全局描述符集 → BaseRenderPath（供 PresentationPass 使用；ADR-6：呈现 execute 按槽绑）
        if (auto* bp = dynamic_cast<BaseRenderPath*>(m_renderPath.get())) {
            bp->setPresentationDescriptorData(m_globalSetLayout, std::vector<RHI::DescriptorSetHandle>(
                m_globalDescriptorSets.begin(), m_globalDescriptorSets.end()));
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