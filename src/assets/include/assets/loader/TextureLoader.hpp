#pragma once
#include <string>
#include <vector>
#include <memory>
#include <renderer/interface/RHIManager.hpp>
#include <assets/AssetType.hpp>

namespace StarryEngine::Assets {

    class TextureLoader {
    public:
        explicit TextureLoader(std::shared_ptr<RHI::ResourceManager> resMgr);
        ~TextureLoader() = default;

        TextureLoadResult loadTexture2D(const std::string& filepath,RHI::Format format,const std::string& debugName = "");

        TextureLoadResult loadTextureFromMemory(const void* data,uint32_t width,uint32_t height,RHI::Format format,const std::string& debugName = "");

        TextureLoadResult loadTextureCube(const std::vector<std::string>& faceFilepaths,RHI::Format format,const std::string& debugName = "");

        TextureLoadResult loadTextureHDR(const std::string& filepath,const std::string& debugName = "");

        RHI::TextureHandle convertEquirectToCubemap(const float* hdrPixels,uint32_t hdrWidth,uint32_t hdrHeight,uint32_t faceSize,const std::string& debugName = "EnvCubemap");

        RHI::TextureHandle createRenderableCubemap(uint32_t faceSize,RHI::Format format,const std::string& debugName);

        bool saveTextureToFile(const RHI::TextureHandle& texture,const std::string& filepath);

        RHI::SamplerHandle createSampler(
            RHI::SamplerFilter filter = RHI::SamplerFilter::Linear,
            RHI::SamplerAddressMode addressMode = RHI::SamplerAddressMode::Repeat,
            float maxAnisotropy = 1.0f,
            float maxLod = 1000.0f,
            const std::string& debugName = "");

        RHI::SamplerHandle createDefaultSampler(const std::string& debugName = "");

    private:
        std::shared_ptr<RHI::ResourceManager> m_resMgr;

        bool uploadPixels(const RHI::TextureHandle& texHandle,const void* data,size_t dataSize,uint32_t width,uint32_t height,uint32_t layer = 0);
    };

} // namespace StarryEngine::Assets