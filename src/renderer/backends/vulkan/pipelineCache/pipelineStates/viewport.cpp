#include "viewport.hpp"

namespace StarryEngine {
    Viewport& Viewport::addViewport(const VkViewport& viewport) {
        mViewports.viewports.push_back(viewport);
        return *this;
    }

    Viewport& Viewport::addScissor(const VkRect2D& scissor) {
        mViewports.scissors.push_back(scissor);
        return *this;
    }
    Viewport& Viewport::reset() {
        mViewports.viewports.clear();
        mViewports.scissors.clear();
		return *this;
    }
    Viewport& Viewport::init(const VkExtent2D& extent) {
		mExtent = extent;
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = mIsOpenGLCoord ? static_cast<float>(extent.height) : 0.0f;
        viewport.width = static_cast<float>(extent.width);
        viewport.height = mIsOpenGLCoord ? -static_cast<float>(extent.height) : static_cast<float>(extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        addViewport(viewport);

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = extent;
        addScissor(scissor);

        createViewport();
		return *this;
    }

    Viewport& Viewport::createViewport() {
        mCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        mCreateInfo.viewportCount = static_cast<uint32_t>(mViewports.viewports.size());
        mCreateInfo.pViewports = mViewports.viewports.data();
        mCreateInfo.scissorCount = static_cast<uint32_t>(mViewports.scissors.size());
        mCreateInfo.pScissors = mViewports.scissors.data();

        return *this;
    }

    const VkPipelineViewportStateCreateInfo& Viewport::getCreateInfo() const {

        return mCreateInfo;
    }
}