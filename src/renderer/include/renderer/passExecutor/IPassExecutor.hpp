#pragma once
#include <variant>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <any>
#include <assets/Assets.hpp>
#include <scene/Scene.hpp>
#include <renderer/RenderTypes.hpp>
#include <renderer/interface/IRHI.hpp>
#include <renderer/interface/RHIFactory.hpp>
#include <renderer/interface/RHICommandEncoder.hpp>
#include <logging/Logger.hpp>

namespace StarryEngine{
    namespace RenderGraph { class RenderGraph; }
    class PassNode;

    struct RenderContext {
        glm::mat4 viewMatrix;
        glm::mat4 projMatrix;
        float deltaTime = 0.0f;

        // 帧槽位（0/1，ADR-6 per-slot）：录制时绑 slot 对应的描述符集/实例缓冲
        uint32_t frameSlot = 0;

        Assets::GlobalUniforms globalUniforms;
        std::shared_ptr<AnalysisSceneResult> sceneData;
        const std::unordered_map<std::string, std::any>& customData;

        RenderContext(const std::unordered_map<std::string, std::any>& customDataRef)
            : customData(customDataRef) {
        }

        template<typename T>
        const T* getCustomData(const std::string& key) const {
            auto it = customData.find(key);
            if (it == customData.end()) return nullptr;
            return std::any_cast<T>(&it->second);
        }
    };

    class PassContext {
    public:
        PassContext(std::shared_ptr<RHI::ResourceManager> resMgr,
            uint32_t frameIndex,
            RHI::FramebufferHandle framebuffer,
            uint32_t frameSlot)
            : mResMgr(resMgr), mFrameIndex(frameIndex), mFramebuffer(framebuffer), mFrameSlot(frameSlot) {
        }

        uint32_t getFrameIndex() const { return mFrameIndex; }     // 交换链图像索引
        uint32_t getFrameSlot() const { return mFrameSlot; }       // 帧槽位（per-slot 数据索引）
        RHI::FramebufferHandle getFramebuffer() const { return mFramebuffer; }
        std::shared_ptr<RHI::ResourceManager> getResourceManager() const { return mResMgr; }

    protected:
        std::shared_ptr<RHI::ResourceManager> mResMgr;
        uint32_t mFrameIndex = 0;
        RHI::FramebufferHandle mFramebuffer;
        uint32_t mFrameSlot = 0;
    };

    class IPassExecutor {
    public:
        virtual ~IPassExecutor() = default;

        virtual void clearDrawItems() = 0;
        virtual void setDrawItems(const std::vector<std::shared_ptr<DrawItem>>& items) = 0;
        virtual const std::vector<std::shared_ptr<DrawItem>>& getDrawItems() = 0;
        virtual void setPipelineMapping(const std::unordered_map<uint32_t, RHI::PipelineHandle>& mapping) = 0;
        virtual std::shared_ptr<Assets::MaterialInstance> getMaterial() const { return nullptr; }
        virtual void setMaterial(std::shared_ptr<Assets::MaterialInstance> material) {}

        virtual void addDrawItem(std::shared_ptr<DrawItem> item) {}

        // 图编译后：自包含 executor（如呈现/粒子）在这里构建自己的管线/描述符。
        // 与场景相关的管线（网格）由 pass 在 onSceneData 里构建，executor 只需接收 setPipelineMapping。
        struct ExecutorPrepareContext {
            std::shared_ptr<RHI::ResourceManager> resMgr;
            std::shared_ptr<RHI::IRHI> rhi;
            RenderGraph::RenderGraph* renderGraph = nullptr;
            RHI::DescriptorSetLayoutHandle globalSetLayout;
            // 按帧槽位的 global 描述符集（ADR-6）：execute 时按 pctx.getFrameSlot() 取
            std::vector<RHI::DescriptorSetHandle> globalDescSets;
            RHI::RenderPassHandle renderPass;
            uint32_t subpassIndex = 0;
        };
        virtual void onPrepare(const ExecutorPrepareContext& /*ctx*/) {}

        virtual void execute(RHI::RHICommandEncoder* encoder,
            const RenderContext& rctx,
            const PassContext& pctx,
            uint32_t subpassIndex) = 0;
    };

} // namespace StarryEngine::RenderGraph