#pragma once
#include <vulkan/vulkan.h>
#include <vector>
namespace StarryEngine {
    class Viewport {
    public:
        struct {
            std::vector<VkViewport> viewports;
            std::vector<VkRect2D> scissors;
        } mViewports;

        Viewport() = default;

		Viewport& reset();
		Viewport& init(const VkExtent2D& extent);

        Viewport& createViewport();

        Viewport& addViewport(const VkViewport& viewport);
        Viewport& addScissor(const VkRect2D& scissor);

        Viewport& IsOpenglCoordinate(bool flag = true) {
            mIsOpenGLCoord = flag;
            return *this;
        }

		VkExtent2D getExtent() const { return mExtent; }
        const VkPipelineViewportStateCreateInfo& getCreateInfo() const;

    private:
        VkPipelineViewportStateCreateInfo mCreateInfo{};
        bool mIsOpenGLCoord = false;
		VkExtent2D mExtent{ 0,0 };
    };
}
