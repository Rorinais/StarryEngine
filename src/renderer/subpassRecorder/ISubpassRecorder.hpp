#pragma once
#include <variant>
#include "../../assets/geometry/Geometry.hpp"
#include "../../scene/SceneType.hpp"
#include "../interface/RHI_RESOURCE_FACTORY.hpp"
#include "../../logging/Logger.hpp"

namespace StarryEngine::RenderGraph {
    class PassNode;

    class PassContext {
    public:
        PassContext(std::shared_ptr<RHI::ResourceManager> resMgr,
            uint32_t frameIndex,
            RHI::FramebufferHandle framebuffer)
            : mResMgr(resMgr), mFrameIndex(frameIndex), mFramebuffer(framebuffer) {
        }

        uint32_t getFrameIndex() const { return mFrameIndex; }
        RHI::FramebufferHandle getFramebuffer() const { return mFramebuffer; }
        std::shared_ptr<RHI::ResourceManager> getResourceManager() const { return mResMgr; }

    private:
        std::shared_ptr<RHI::ResourceManager> mResMgr;
        uint32_t mFrameIndex = 0;
        RHI::FramebufferHandle mFramebuffer;
    };

    class ISubpassRecorder {
    public:
        ISubpassRecorder(std::shared_ptr<RHI::ResourceManager> resMgr) : mResMgr(resMgr) {}
        virtual ~ISubpassRecorder() {}

        virtual void clearDrawItems() = 0;
        virtual void setPipelines(const std::vector<RHI::PipelineHandle>& pipelines) = 0;
        virtual void setDrawItems(const std::vector<std::shared_ptr<Scene::DrawItem>>& items) = 0;
        virtual const std::vector<std::shared_ptr<Scene::DrawItem>>& getDrawItems() = 0;

        virtual void recordCommands(RHI::RHICommandEncoder* encoder,
            const PassContext& pctx,
            uint32_t subpassIndex,
            uint32_t frameIndex) = 0;

    protected:
        std::shared_ptr<RHI::ResourceManager> mResMgr;
    };

} // namespace StarryEngine::RenderGraph