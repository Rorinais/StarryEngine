#include "ViewportComponent.hpp"

namespace StarryEngine {
    ViewportScissor::ViewportScissor(float x, float y, float width, float height,
        const VkExtent2D& extent, bool isOpenGLCoord) {
        viewport.x = x;
        viewport.y = isOpenGLCoord ? y + height : y;
        viewport.width = width;
        viewport.height = isOpenGLCoord ? -height : height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        scissor.offset = {
            static_cast<int32_t>(x),
            static_cast<int32_t>(isOpenGLCoord ? extent.height - (y + height) : y)
        };
        scissor.extent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };
    }

    ViewportScissor::ViewportScissor(const VkExtent2D& extent, bool isOpenGLCoord) {
        viewport.x = 0.0f;
        viewport.y = isOpenGLCoord ? static_cast<float>(extent.height) : 0.0f;
        viewport.width = static_cast<float>(extent.width);
        viewport.height = isOpenGLCoord ? -static_cast<float>(extent.height) : static_cast<float>(extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        scissor.offset = { 0, 0 };
        scissor.extent = extent;
    }


    ViewportComponent::ViewportComponent(const std::string&name) {
        setName(name);
        reset();
    }

    ViewportComponent& ViewportComponent::reset() {
        mCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        mCreateInfo.pNext = nullptr;
        mCreateInfo.flags = 0;
        mCreateInfo.viewportCount = 0;
        mCreateInfo.pViewports = nullptr;
        mCreateInfo.scissorCount = 0;
        mCreateInfo.pScissors = nullptr;
        return *this;
    }

    std::string ViewportComponent::getDescription() const {
        return "ViewPorts:" + std::to_string(mViewports.size()) + ",Scissors:" + std::to_string(mScissors.size());
    }

    ViewportComponent& ViewportComponent::addViewport(const VkViewport& viewport) {
        mViewports.push_back(viewport);
        updateCreateInfo();
        return *this;
    }

    ViewportComponent& ViewportComponent::addScissor(const VkRect2D& scissor) {
        mScissors.push_back(scissor);
        updateCreateInfo();
        return *this;
    }

    ViewportComponent& ViewportComponent::addViewportScissor(const ViewportScissor& vpSc) {
        mViewports.push_back(vpSc.viewport);
        mScissors.push_back(vpSc.scissor);
        updateCreateInfo();
        return *this;
    }

    ViewportComponent& ViewportComponent::setViewportScissor(const ViewportScissor& vpSc) {
        mViewports.clear();
        mScissors.clear();
        mViewports.push_back(vpSc.viewport);
        mScissors.push_back(vpSc.scissor);
        updateCreateInfo();
        return *this;
    }

    void ViewportComponent::updateCreateInfo() {
        mCreateInfo.viewportCount = static_cast<uint32_t>(mViewports.size());
        mCreateInfo.pViewports = mViewports.empty() ? nullptr : mViewports.data();
        mCreateInfo.scissorCount = static_cast<uint32_t>(mScissors.size());
        mCreateInfo.pScissors = mScissors.empty() ? nullptr : mScissors.data();
    }

}