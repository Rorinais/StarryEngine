#pragma once
#include "../interface/TypedPipelineComponent.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <stdexcept>

namespace StarryEngine {

    struct ViewportScissor {
        VkViewport viewport{};
        VkRect2D scissor{};

        ViewportScissor() = default;

        ViewportScissor(float x, float y, float width, float height,const VkExtent2D& extent, bool isOpenGLCoord = true);

        ViewportScissor(const VkExtent2D& extent, bool isOpenGLCoord = true);
    };

    class ViewportComponent: public 
        TypedPipelineComponent<ViewportComponent,PipelineComponentType::VIEWPORT_STATE>{
    public:
        ViewportComponent(const std::string &name="default");
        ViewportComponent& reset();

        ViewportComponent& addViewport(const VkViewport& viewport);
        ViewportComponent& addScissor(const VkRect2D& scissor);
        ViewportComponent& addViewportScissor(const ViewportScissor& vpSc);
        ViewportComponent& setViewportScissor(const ViewportScissor& vpSc);

        void apply(VkGraphicsPipelineCreateInfo& pipelineInfo) override {
            updateCreateInfo();
            pipelineInfo.pViewportState = &mCreateInfo;
        }

        std::string getDescription() const override;

        const std::vector<VkViewport>& getViewports() const { return mViewports; }
        const std::vector<VkRect2D>& getScissors() const { return mScissors; }
        bool isValid() const override { return mViewports.size() == mScissors.size(); }
        uint32_t getViewportCount() const { return static_cast<uint32_t>(mViewports.size()); }

    private:
        std::vector<VkViewport> mViewports;
        std::vector<VkRect2D> mScissors;

        VkPipelineViewportStateCreateInfo mCreateInfo{};

        void updateCreateInfo();
    };

} // namespace StarryEngine