#include"depthStencil.hpp"
namespace StarryEngine {
    DepthStencil& DepthStencil::enableDepthTest(VkBool32 enable) {
        mCreateInfo.depthTestEnable = enable;
        return *this;
    }

    DepthStencil& DepthStencil::enableDepthWrite(VkBool32 enable) {
        mCreateInfo.depthWriteEnable = enable;
        return *this;
    }

    DepthStencil& DepthStencil::setDepthCompareOp(VkCompareOp compareOp) {
        mCreateInfo.depthCompareOp = compareOp;
        return *this;
    }

    DepthStencil& DepthStencil::enableStencilTest(VkBool32 enable) {
        mCreateInfo.stencilTestEnable = enable;
        return *this;
    }

    DepthStencil& DepthStencil::setStencilFront(const StencilConfig& config) {
        mFrontStencil = config;
        return *this;
    }

    DepthStencil& DepthStencil::setStencilBack(const StencilConfig& config) {
        mBackStencil = config;
        return *this;
    }

    DepthStencil& DepthStencil::enableDepthBoundsTest(VkBool32 enable) {
        mCreateInfo.depthBoundsTestEnable = enable;
        return *this;
    }

    DepthStencil& DepthStencil::setDepthBounds(float min, float max) {
        mMinDepthBounds = min;
        mMaxDepthBounds = max;
        return *this;
    }

    const VkPipelineDepthStencilStateCreateInfo& DepthStencil::getCreateInfo() const {
        static VkPipelineDepthStencilStateCreateInfo info{};
        info = mCreateInfo;

        // 初始化基本结构
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        info.pNext = nullptr;
        info.flags = 0;

        // 深度边界配置
        info.minDepthBounds = mMinDepthBounds;
        info.maxDepthBounds = mMaxDepthBounds;

        // 模板状态配置
        info.front = {
            .failOp = mFrontStencil.failOp,
            .passOp = mFrontStencil.passOp,
            .depthFailOp = mFrontStencil.depthFailOp,
            .compareOp = mFrontStencil.compareOp,
            .compareMask = mFrontStencil.compareMask,
            .writeMask = mFrontStencil.writeMask,
            .reference = mFrontStencil.reference
        };

        info.back = {
            .failOp = mBackStencil.failOp,
            .passOp = mBackStencil.passOp,
            .depthFailOp = mBackStencil.depthFailOp,
            .compareOp = mBackStencil.compareOp,
            .compareMask = mBackStencil.compareMask,
            .writeMask = mBackStencil.writeMask,
            .reference = mBackStencil.reference
        };

        return info;
    }
}