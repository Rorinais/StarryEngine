#pragma once
#include "Resource.hpp"
#include "../../../resources/textures/Texture.hpp"

namespace StarryEngine {
    class TextureResource : public Resource {
    public:
        using Ptr = std::shared_ptr<TextureResource>;

        TextureResource(
            const LogicalDevice::Ptr& logicalDevice,
            const CommandPool::Ptr& commandPool,
            const std::string& name,
            const char* imagePath);

        Texture::Ptr getTexture() const { return mTexture; }

        void release() override;
        void load() override;
        bool isReady() const override;
        size_t getMemoryUsage() const override;
        void loadAsync(LoadCallback callback = nullptr) override;

        // 实现交换链重建方法
        void onSwapchainRecreated(VkExtent2D newExtent) override;

    private:
        LogicalDevice::Ptr mLogicalDevice;
        CommandPool::Ptr mCommandPool;
        std::string mImagePath;
        Texture::Ptr mTexture;
        bool mLoaded = false;
        bool mLoading = false;
    };
}
