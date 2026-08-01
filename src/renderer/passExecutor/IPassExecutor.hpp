#pragma once
#include <variant>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <any>
#include "../../assets/Assets.hpp"
#include "../../scene/Scene.hpp"
#include "../RenderTypes.hpp"
#include "../interface/RHI_RESOURCE_FACTORY.hpp"
#include "../../logging/Logger.hpp"

namespace StarryEngine{
    class PassNode;

    struct RenderContext {
        glm::mat4 viewMatrix;
        glm::mat4 projMatrix;
        float deltaTime = 0.0f;

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
            RHI::FramebufferHandle framebuffer)
            : mResMgr(resMgr), mFrameIndex(frameIndex), mFramebuffer(framebuffer) {
        }

        uint32_t getFrameIndex() const { return mFrameIndex; }
        RHI::FramebufferHandle getFramebuffer() const { return mFramebuffer; }
        std::shared_ptr<RHI::ResourceManager> getResourceManager() const { return mResMgr; }

    protected:
        std::shared_ptr<RHI::ResourceManager> mResMgr;
        uint32_t mFrameIndex = 0;
        RHI::FramebufferHandle mFramebuffer;
    };

    class IPassExecutor {
    public:
        virtual ~IPassExecutor() = default;

        virtual void clearDrawItems() = 0;
        virtual void setDrawItems(const std::vector<std::shared_ptr<DrawItem>>& items) = 0;
        virtual const std::vector<std::shared_ptr<DrawItem>>& getDrawItems() = 0;
        virtual void setPipeline(RHI::PipelineHandle pipeline) {}
        virtual void setPipelineMapping(const std::unordered_map<uint32_t, RHI::PipelineHandle>& mapping) = 0;
        virtual std::shared_ptr<Assets::MaterialInstance> getMaterial() const { return nullptr; }
        virtual void setMaterial(std::shared_ptr<Assets::MaterialInstance> material) {}

        virtual void addDrawItem(std::shared_ptr<DrawItem> item) {}

        virtual void execute(RHI::RHICommandEncoder* encoder,
            const RenderContext& rctx,
            const PassContext& pctx,
            uint32_t subpassIndex) = 0;
    };

} // namespace StarryEngine::RenderGraph