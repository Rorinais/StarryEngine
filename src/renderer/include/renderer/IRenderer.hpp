#pragma once
#include <memory>
#include <array>
#include <functional>
#include <core/Clock.hpp>
#include <core/JobSystem.hpp>
#include <renderer/renderPaths/IRenderPath.hpp>
#include <renderer/interface/IRHI.hpp>
#include <renderer/interface/RHIManager.hpp>

namespace StarryEngine {
    class ImGuiManager;

    // ── 渲染器抽象：按渲染技术分化的入口 ──
    // 渲染器 = 场景→技术表示 的转换（SceneAnalyzer / RTPathSceneBuilder）+ 托管渲染路径执行。
    // RasterRenderer（光栅：延迟/前向 + SceneAnalyzer）与 RayTracingRenderer（路径追踪）各自实现。
    class IRenderer {
    public:
        virtual ~IRenderer() = default;

        virtual void destroy() = 0;
        virtual void createGlobalSetLayout() = 0;
        virtual void createGlobalUniformBuffer() = 0;
        virtual void onResize(uint32_t width, uint32_t height) = 0;
        virtual void renderFrame(RHI::RHICommandEncoder* encoder, uint32_t frameIndex, const Clock& clock) = 0;

        virtual void setNeedRebuildGraph() = 0;
        virtual void setImGuiManager(ImGuiManager* mgr, uint32_t imageCount) = 0;
        virtual void setRenderPath(std::shared_ptr<IRenderPath> newRenderPath) = 0;
        virtual std::shared_ptr<IRenderPath> getRenderPath() = 0;

        virtual void addOverlayPass(const std::string& tag, std::shared_ptr<IPassExecutor> executor) = 0;
        virtual void addOverlayPass(const OverlayPassDesc& desc) = 0;
        virtual void removeOverlayPass(const std::string& tag) = 0;
        virtual void clearOverlayPasses() = 0;

        virtual RHI::DescriptorSetLayoutHandle getGlobalSetLayout() = 0;
        virtual RHI::DescriptorSetHandle getGlobalDescriptorSet(uint32_t slot) = 0;
        virtual const std::array<RHI::DescriptorSetHandle, RHI::kMaxFramesInFlight>& getGlobalDescriptorSets() const = 0;

        virtual JobSystem* getJobSystem() const = 0;
        virtual bool isFrameInFlight() const = 0;
        virtual void setFrameDataBoneProvider(std::function<void(uint32_t dataSlot)> provider) = 0;

        // shader 热重载：光栅渲染器实现（遍历分析结果材质）；路径追踪渲染器为 no-op
        virtual void reloadAllShaders() = 0;
    };
}
