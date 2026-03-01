#pragma once
#include <variant>
#include "Geometry.hpp"
#include "Material.hpp"

namespace StarryEngine::RenderGraph {
    class PassContext {
    public:
        PassContext(std::shared_ptr<RHI::ResourceManager> resMgr,
            const std::vector<RHI::PipelineHandle>& pipelines,
            uint32_t frameIndex,
            RHI::FramebufferHandle framebuffer)
            : mResMgr(resMgr), mPipelines(pipelines), mFrameIndex(frameIndex), mFramebuffer(framebuffer) {
        }

        RHI::PipelineHandle getPipeline(uint32_t subpassIndex) const {
            if (subpassIndex < mPipelines.size()) {
                return mPipelines[subpassIndex];
            }
            return RHI::PipelineHandle::Null();
        }

        uint32_t getFrameIndex() const { return mFrameIndex; }
        RHI::FramebufferHandle getFramebuffer() const { return mFramebuffer; }
        std::shared_ptr<RHI::ResourceManager> getResourceManager() const { return mResMgr; }

    private:
        std::shared_ptr<RHI::ResourceManager> mResMgr;
        std::vector<RHI::PipelineHandle> mPipelines;
        uint32_t mFrameIndex = 0;
        RHI::FramebufferHandle mFramebuffer;
    };

    class ISubpassRenderer {
    public:
        virtual ~ISubpassRenderer() = default;
        virtual void recordCommands(RHI::RHICommandEncoder* encoder,
            const PassContext& pctx,
            uint32_t subpassIndex,   
            uint32_t frameIndex) = 0;
    };
}
