#include "TextureResource.hpp"
#include <thread>
#include <iostream>

namespace StarryEngine {
    TextureResource::TextureResource(
        const LogicalDevice::Ptr& logicalDevice,
        const CommandPool::Ptr& commandPool,
        const std::string& name,
        const char* imagePath)
        :Resource(ResourceType::Texture, name),
        mLogicalDevice(logicalDevice),
        mCommandPool(commandPool),
        mImagePath(imagePath ? imagePath : "")
    {
    }

    void TextureResource::onSwapchainRecreated(VkExtent2D newExtent) {
        // 如果资源是深度纹理，则重新创建
        if (mTexture && mTexture->getType() == Texture::Type::Depth) {
            mTexture->recreate(newExtent);
        }
    }


    void TextureResource::release() {
        if (mTexture) {
            mTexture->cleanup();
            mTexture.reset();
            mLoaded = false;
        }
    }

    size_t TextureResource::getMemoryUsage() const {
        if (!mTexture) return 0;
        // 简化计算：宽*高*通道数
        return mTexture->getWidth() * mTexture->getHeight() * 4;
    }

    bool TextureResource::isReady() const {
        return mLoaded;
    }

    void TextureResource::loadAsync(Resource::LoadCallback callback) {
        if (mLoaded || mLoading) return;

        mLoading = true;

        // 在实际应用中，应该使用线程池
        std::thread loader([this, callback] {
            this->load();
            if (callback) {
                callback(this);
            }
            mLoading = false;
            });
        loader.detach();
    }

    void TextureResource::load() {
        if (mImagePath.empty()) {
            std::cerr << "TextureResource: No image path provided for " << mName << std::endl;
            return;
        }

        try {
            mTexture = Texture::create(mLogicalDevice, mImagePath.c_str(), mCommandPool);
            mLoaded = true;
        }
        catch (const std::exception& e) {
            std::cerr << "Failed to load texture: " << mImagePath
                << " Error: " << e.what() << std::endl;
            mLoaded = false;
        }
    }
}
