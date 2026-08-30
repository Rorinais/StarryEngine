#pragma once
#include <renderer/IRenderer.hpp>
#include <renderer/graph/ParallelRecording.hpp>
#include <renderer/interface/RHIFactory.hpp>
#include <functional>

namespace StarryEngine {
    // ── 渲染器共享底座 ──
    // 全局描述符集/统一缓冲、渲染路径生命周期、并行命令录制、帧循环骨架。
    // 技术专属部分（场景分析、实例/材质上传）通过 protected 虚钩子由子类实现：
    //   RasterRenderer：SceneAnalyzer 场景→draw items + 实例/材质缓冲
    //   RayTracingRenderer：无场景分析（RT 场景在 shader），钩子为空
    class BaseRenderer : public IRenderer {
    public:
        BaseRenderer(std::shared_ptr<RHI::IRHI> rhi, RHI::DescriptorPoolHandle globalPool,
                     std::shared_ptr<Scene::Scene> scene);
        ~BaseRenderer() override { destroy(); }
        void destroy() override;

        void createGlobalSetLayout() override;
        void createGlobalUniformBuffer() override;
        void onResize(uint32_t width, uint32_t height) override;
        void renderFrame(RHI::RHICommandEncoder* encoder, uint32_t frameIndex, const Clock& clock) override;

        void setNeedRebuildGraph() override { m_needRebuildGraph = true; }
        void setImGuiManager(ImGuiManager* mgr, uint32_t imageCount) override;
        void setRenderPath(std::shared_ptr<IRenderPath> newRenderPath) override;
        std::shared_ptr<IRenderPath> getRenderPath() override { return m_renderPath; }

        void addOverlayPass(const std::string& tag, std::shared_ptr<IPassExecutor> executor) override;
        void addOverlayPass(const OverlayPassDesc& desc) override;
        void removeOverlayPass(const std::string& tag) override;
        void clearOverlayPasses() override;

        RHI::DescriptorSetLayoutHandle getGlobalSetLayout() override { return m_globalSetLayout; }
        RHI::DescriptorSetHandle getGlobalDescriptorSet(uint32_t slot) override {
            return (slot < RHI::kMaxFramesInFlight) ? m_globalDescriptorSets[slot] : RHI::DescriptorSetHandle::Null();
        }
        const std::array<RHI::DescriptorSetHandle, RHI::kMaxFramesInFlight>& getGlobalDescriptorSets() const override {
            return m_globalDescriptorSets;
        }

        JobSystem* getJobSystem() const override { return m_jobSystem.get(); }
        bool isFrameInFlight() const override { return m_frameInFlight; }
        void setFrameDataBoneProvider(std::function<void(uint32_t dataSlot)> provider) override {
            m_boneProvider = std::move(provider);
        }
        void reloadAllShaders() override {}   // 光栅渲染器覆盖（遍历分析结果材质）

    protected:
        // ── 技术专属帧钩子（默认空：路径追踪）──
        virtual void buildSceneResources() {}
        virtual void submitFrameDataJobs(uint32_t slot) {}
        virtual void updateInstanceBuffers(const std::vector<std::shared_ptr<Scene::RenderObject>>& objects, uint32_t slot) {}
        virtual void updateDynamicBuffers(const Clock& clock, uint32_t slot) {}

        void rebuildRenderGraph();
        void ensureParallelRecording();
        void updateGlobals(const Clock& clock, uint32_t slot);

        // 光源投影矩阵（阴影贴图用）：光栅渲染器持有（demo 设置）；默认单位矩阵
        virtual const glm::mat4& getLightViewProj() const {
            static const glm::mat4 identity(1.0f);
            return identity;
        }

        // 重建渲染图：光栅渲染器覆盖（initialize + rebuildResources(分析结果)）
        virtual void doRebuildRenderGraph();

        std::shared_ptr<RHI::IRHI> m_rhi;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        std::shared_ptr<IRenderPath> m_renderPath;

        RHI::DescriptorPoolHandle m_globalPool;
        RHI::DescriptorSetLayoutHandle m_globalSetLayout;
        std::array<RHI::DescriptorSetHandle, RHI::kMaxFramesInFlight> m_globalDescriptorSets;
        std::array<RHI::BufferHandle, RHI::kMaxFramesInFlight> m_globalUniformBuffers;

        std::shared_ptr<Scene::Scene> m_scene;
        bool m_needRebuildGraph = false;

        std::unique_ptr<JobSystem> m_jobSystem;
        std::unique_ptr<ParallelRecordingContext> m_parallelRecording;

        bool m_frameInFlight = false;
        bool m_fidHasKicked = false;
        std::function<void(uint32_t dataSlot)> m_boneProvider;
    };
}
