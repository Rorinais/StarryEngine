#pragma once
#include <string>
#include <vector>
#include <memory>
#include "../../renderer/interface/RHI_RESOURCE_MANAGER.hpp"
#include "../AssetType.hpp"

namespace StarryEngine::Assets {

    class TextureLoader {
    public:
        explicit TextureLoader(std::shared_ptr<RHI::ResourceManager> resMgr);
        ~TextureLoader() = default;

        // 加载单张 2D 纹理，自动创建采样器
        TextureLoadResult loadTexture2D(
            const std::string& filepath,
            RHI::Format format,
            const std::string& debugName = "");

        // 加载立方体贴图（需要 6 个文件，顺序：+X, -X, +Y, -Y, +Z, -Z 或根据约定）
        TextureLoadResult loadTextureCube(
            const std::vector<std::string>& faceFilepaths,
            RHI::Format format,
            const std::string& debugName = "");

        // 将纹理数据保存到文件
        bool saveTextureToFile(
            const RHI::TextureHandle& texture,
            const std::string& filepath);

        RHI::SamplerHandle createSampler(
            RHI::SamplerFilter filter = RHI::SamplerFilter::Linear,
            RHI::SamplerAddressMode addressMode = RHI::SamplerAddressMode::Repeat,
            float maxAnisotropy = 1.0f,
            float maxLod = 1000.0f,
            const std::string& debugName = "");

        RHI::SamplerHandle createDefaultSampler(const std::string& debugName = "");

    private:
        std::shared_ptr<RHI::ResourceManager> m_resMgr;

        // 上传像素数据到纹理
        bool uploadPixels(
            const RHI::TextureHandle& texHandle,
            const void* data,
            size_t dataSize,
            uint32_t width,
            uint32_t height,
            uint32_t layer = 0);
    };

} // namespace StarryEngine::Assets