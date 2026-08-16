#pragma once
#include <assets/Assets.hpp>
#include <event/Events.hpp>
#include <logging/Logger.hpp>
#include <scene/Scene.hpp>
#include <core/Clock.hpp>
#include <renderer/RenderTypes.hpp>
#include <renderer/SceneAnalyzer.hpp>
#include <renderer/graph/RenderGraph.hpp>
#include <renderer/graph/ParallelRecording.hpp>
#include <core/JobSystem.hpp>
#include <renderer/interface/RHIFactory.hpp>
#include <renderer/renderPaths/DeferredRenderPath.hpp>
#include <renderer/renderPaths/ForwardRenderPath.hpp>
#include <functional>

namespace StarryEngine {
    class Renderer {
    public:
        Renderer(std::shared_ptr<RHI::IRHI> rhi,RHI::DescriptorPoolHandle globalPool,std::shared_ptr<Scene::Scene> scene);
        ~Renderer() { destroy(); }
        void destroy();

        void initDefaultMaterials();
        void createGlobalSetLayout();
        void createGlobalUniformBuffer();
        void onResize(uint32_t width, uint32_t height);

        void analysisScene();
        void reloadAllShaders();
        void rebuildRenderGraph();
        void reloadShader(const std::string& vertPath, const std::string& fragPath);
        void renderFrame(RHI::RHICommandEncoder* encoder, uint32_t frameIndex, const Clock& clock);

        void setNeedRebuildGraph() { m_needRebuildGraph = true; }
        void setImGuiManager(ImGuiManager* mgr, uint32_t imageCount);
        void setRenderPath(std::shared_ptr<IRenderPath> newRenderPath);

        void addOverlayPass(const std::string& tag, std::shared_ptr<IPassExecutor> executor);
        void addOverlayPass(const OverlayPassDesc& desc);
        void removeOverlayPass(const std::string& tag);
        void clearOverlayPasses();

        RHI::DescriptorSetLayoutHandle getGlobalSetLayout() { return m_globalSetLayout; }
        // per-slot global 描述符集（ADR-6）：槽位版本按帧槽位取；数组版本给消费方一次拿全。
        RHI::DescriptorSetHandle getGlobalDescriptorSet(uint32_t slot) {
            return (slot < RHI::kMaxFramesInFlight) ? m_globalDescriptorSets[slot] : RHI::DescriptorSetHandle::Null();
        }
        const std::array<RHI::DescriptorSetHandle, RHI::kMaxFramesInFlight>& getGlobalDescriptorSets() const {
            return m_globalDescriptorSets;
        }

        void setLightViewProj(const glm::mat4& lightVP) { m_lightVP = lightVP; }

        // 帧内并行线程池（ADR-6 第 2 步）：构造时按 env 创建，nullptr = 串行。
        // demo 侧可借此把骨骼计算提交进池（与渲染器数据 job 共享同一帧 barrier）。
        JobSystem* getJobSystem() const { return m_jobSystem.get(); }

        // 帧间解耦（ADR-6 终点，STARRY_FRAME_IN_FLIGHT=1）：渲染线程形态。
        // 主线程持续渲染 N，worker 在 GPU(N) 执行期算 N+1 的数据（1 帧数据潜伏）。
        bool isFrameInFlight() const { return m_frameInFlight; }
        // demo 注册骨骼数据提供器：渲染器在数据相（GPU(N) 执行期）调用它算 N+1 的骨骼，
        // 而非在 onUpdate（早于 renderFrame，会被开头 waitAll 一并 join，overlap 失效）。
        void setFrameDataBoneProvider(std::function<void(uint32_t dataSlot)> provider) {
            m_boneProvider = std::move(provider);
        }

    private:
        void buildSceneResources();
        // slot 参数 = 帧槽位（ADR-6 per-slot）：本帧 CPU 数据写该槽，GPU(N) 只读该槽
        void updateDynamicBuffers(const Clock& clock, uint32_t slot);
        void updateGlobals(const Clock& clock, uint32_t slot);
        void updateInstanceBuffers(const std::vector<std::shared_ptr<Scene::RenderObject>>& objects, uint32_t slot);
        void submitFrameDataJobs(uint32_t slot);

        // 并行命令录制（ADR-6 第 2 步）：按 env 建 JobSystem + 录制上下文。
        // 无 env → m_parallelRecording 为 null → 原串行路径（行为与今天一致）。
        void ensureParallelRecording();

    private:
        std::shared_ptr<RHI::IRHI> m_rhi;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        std::shared_ptr<IRenderPath> m_renderPath;

        RHI::DescriptorPoolHandle m_globalPool;
        RHI::DescriptorSetLayoutHandle m_globalSetLayout;
        // per-slot（ADR-6）：globals UBO/描述符集按帧槽位双份。帧 N 写槽 N%2、绑槽 N%2，
        // GPU(N-2) 读的是槽 N%2 的旧数据——waitForFrame(N%2) 已放行后才重写 → 无覆盖在途帧。
        std::array<RHI::DescriptorSetHandle, RHI::kMaxFramesInFlight> m_globalDescriptorSets;
        std::array<RHI::BufferHandle, RHI::kMaxFramesInFlight> m_globalUniformBuffers;

        std::shared_ptr<Scene::Scene> m_scene;
        std::unique_ptr<SceneAnalyzer> m_sceneAnalyzer;
        std::shared_ptr<AnalysisSceneResult> m_analysisSceneResult;
        uint32_t m_lastAnalyzedVersion = UINT32_MAX;

        std::shared_ptr<Assets::MaterialInstance> m_defaultMaterial;
        std::shared_ptr<Assets::MaterialInstance> m_errorMaterial;
        bool m_materialsInitialized = false;

        bool m_needRebuildGraph = false;

        // 并行命令录制：线程池 + 录制上下文（env 惰性建，nullptr = 串行）
        std::unique_ptr<JobSystem> m_jobSystem;
        std::unique_ptr<ParallelRecordingContext> m_parallelRecording;

        // 帧间解耦（STARRY_FRAME_IN_FLIGHT=1）：m_boneProvider 是 demo 注册的骨骼数据提供器
        // （GPU(N) 期间算 N+1 骨骼，写槽 (N+1)%2）。m_fidHasKicked 标记首帧已 kick 数据——
        // 首帧没有上一帧的数据可 join，须同步填本帧槽位（材质块首写灌两槽要在 GPU 读之前完成）。
        bool m_frameInFlight = false;
        bool m_fidHasKicked = false;
        std::function<void(uint32_t dataSlot)> m_boneProvider;

        glm::mat4 m_lightVP = glm::mat4(1.0f);   // 未设置时为单位阵（阴影贴图退化为无意义采样，不会崩溃）
    };
}