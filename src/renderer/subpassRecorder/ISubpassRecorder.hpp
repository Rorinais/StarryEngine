#pragma once
#include <variant>
#include "../../assets/geometry/Geometry.hpp"
#include "../interface/RHI_RESOURCE_FACTORY.hpp"
#include "../../logging/Logger.hpp"

namespace StarryEngine::RenderGraph {

    class PassNode;

    class PassContext {
    public:
        PassContext(std::shared_ptr<RHI::ResourceManager> resMgr,
            const std::vector<RHI::PipelineHandle>& pipelines,
            uint32_t frameIndex,
            RHI::FramebufferHandle framebuffer)
            : mResMgr(resMgr), mPipelines(pipelines), mFrameIndex(frameIndex), mFramebuffer(framebuffer) {
        }

        RHI::PipelineHandle getPipeline(uint32_t subpassIndex) const {
            return (subpassIndex < mPipelines.size()) ? mPipelines[subpassIndex] : RHI::PipelineHandle::Null();
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

    class ISubpassRecorder {
    public:
        ISubpassRecorder(std::shared_ptr<RHI::ResourceManager> resMgr): mResMgr(resMgr) {}
        virtual ~ISubpassRecorder(){}

        virtual void recordCommands(RHI::RHICommandEncoder* encoder,
            const PassContext& pctx,
            uint32_t subpassIndex,
            uint32_t frameIndex) = 0;

    protected:
        std::shared_ptr<RHI::ResourceManager> mResMgr;
    };

} // namespace StarryEngine::RenderGraph