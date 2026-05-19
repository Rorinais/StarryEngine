#include "TextureLoader.hpp"
#include "../../logging/Logger.hpp"
#include <stb_image.h>
#include <vector>
#include <cstring>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace {

    // 立方体面索引 + 像素坐标 → 单位方向向量
    glm::vec3 CubemapFaceToDirection(int face, float u, float v, uint32_t size) {
        // 将像素坐标映射到 [-1, 1]
        float x = (u + 0.5f) / float(size) * 2.0f - 1.0f;
        float y = (v + 0.5f) / float(size) * 2.0f - 1.0f;

        glm::vec3 dir;
        switch (face) {
        case 0: dir = glm::vec3(1.0f, -y, -x);    break; // +X
        case 1: dir = glm::vec3(-1.0f, -y, x);    break; // -X
        case 2: dir = glm::vec3(x, 1.0f, y);    break; // +Y
        case 3: dir = glm::vec3(x, -1.0f, -y);    break; // -Y
        case 4: dir = glm::vec3(x, -y, 1.0f);  break; // +Z
        default:dir = glm::vec3(-x, -y, -1.0f);  break; // -Z
        }
        return glm::normalize(dir);
    }

    // 方向向量 → equirectangular UV
    glm::vec2 DirectionToEquirectUV(const glm::vec3& dir) {
        float phi = std::atan2(dir.z, dir.x);     // [-π, π]
        float theta = std::asin(-dir.y);             // [-π/2, π/2]

        float u = phi / (2.0f * 3.14159265f) + 0.5f;
        float v = theta / 3.14159265f + 0.5f;
        return glm::vec2(u, v);
    }

    // 双线性采样（浮点 RGBA）
    glm::vec4 SampleEquirectBilinear(const float* pixels, uint32_t width, uint32_t height, glm::vec2 uv) {
        uv = glm::clamp(uv, 0.0f, 1.0f);
        float fx = uv.x * (width - 1);
        float fy = uv.y * (height - 1);

        uint32_t x0 = uint32_t(fx);
        uint32_t y0 = uint32_t(fy);
        uint32_t x1 = std::min(x0 + 1, width - 1);
        uint32_t y1 = std::min(y0 + 1, height - 1);

        float tx = fx - float(x0);
        float ty = fy - float(y0);

        auto sample = [&](uint32_t x, uint32_t y) -> glm::vec4 {
            const float* p = pixels + (y * width + x) * 4;
            return glm::vec4(p[0], p[1], p[2], p[3]);
            };

        glm::vec4 c00 = sample(x0, y0);
        glm::vec4 c10 = sample(x1, y0);
        glm::vec4 c01 = sample(x0, y1);
        glm::vec4 c11 = sample(x1, y1);

        return glm::mix(glm::mix(c00, c10, tx), glm::mix(c01, c11, tx), ty);
    }

} // anonymous namespace

namespace StarryEngine::Assets {

    TextureLoader::TextureLoader(std::shared_ptr<RHI::ResourceManager> resMgr)
        : m_resMgr(std::move(resMgr)) {
    }

    // ===== 上传像素数据辅助函数 =====
    bool TextureLoader::uploadPixels(
        const RHI::TextureHandle& texHandle,
        const void* data,
        size_t dataSize,
        uint32_t width,
        uint32_t height,
        uint32_t layer)
    {
        auto* texture = m_resMgr->getTexture(texHandle);
        if (!texture) return false;

        // 子资源范围：单个 mip，单个层
        RHI::ImageSubresourceRange range{
            .aspectMask = RHI::ImageAspect::Color,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = layer,
            .layerCount = 1
        };

        // 更新纹理数据
        texture->update(data, dataSize, range);
 

        texture->transitionLayout(
            RHI::ImageLayout::ShaderReadOnly,
            RHI::PipelineStage::Transfer,
            RHI::PipelineStage::FragmentShader,
            static_cast<RHI::AccessFlags>(RHI::AccessFlag::TransferWrite),
            static_cast<RHI::AccessFlags>(RHI::AccessFlag::ShaderRead),
            range);
        return true;
    }

    // ===== 创建默认采样器 =====
    RHI::SamplerHandle TextureLoader::createDefaultSampler(const std::string& debugName) {
        RHI::SamplerDesc desc;
        desc.magFilter = RHI::SamplerFilter::Linear;
        desc.minFilter = RHI::SamplerFilter::Linear;
        desc.mipFilter = RHI::SamplerFilter::Linear;
        desc.addressU = RHI::SamplerAddressMode::Repeat;
        desc.addressV = RHI::SamplerAddressMode::Repeat;
        desc.addressW = RHI::SamplerAddressMode::Repeat;
        desc.maxAnisotropy = 1.0f;
        desc.compareOp = RHI::CompareOp::Never;
        desc.minLod = 0.0f;
        desc.maxLod = 1.0f;
        desc.debugName = debugName;
        return m_resMgr->createSampler(desc);
    }

    // 可增加参数化版本，以适应不同需求
    RHI::SamplerHandle TextureLoader::createSampler(
        RHI::SamplerFilter filter,
        RHI::SamplerAddressMode addressMode,
        float maxAnisotropy,
        float maxLod,
        const std::string& debugName)
    {
        RHI::SamplerDesc desc;
        desc.magFilter = filter;
        desc.minFilter = filter;
        desc.mipFilter = filter;
        desc.addressU = addressMode;
        desc.addressV = addressMode;
        desc.addressW = addressMode;
        desc.maxAnisotropy = maxAnisotropy;
        desc.compareOp = RHI::CompareOp::Never;
        desc.minLod = 0.0f;
        desc.maxLod = maxLod;
        desc.debugName = debugName;
        return m_resMgr->createSampler(desc);
    }

    // ===== 加载 2D 纹理 =====
    TextureLoadResult TextureLoader::loadTexture2D(
        const std::string& filepath,
        RHI::Format format,
        const std::string& debugName)
    {
        TextureLoadResult result{ RHI::TextureHandle::Null(), RHI::SamplerHandle::Null() };

        int width, height, channels;
        stbi_uc* pixels = stbi_load(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        if (!pixels) {
            LOG_ERROR("Failed to load texture: {}", filepath);
            return result;
        }

        // 创建纹理描述
        RHI::TextureDesc texDesc;
        texDesc.extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
        texDesc.format = format;
        texDesc.type = RHI::TextureType::Texture2D;
        texDesc.mipLevels = 1;
        texDesc.arrayLayers = 1;
        texDesc.sampleCount = 1;
        texDesc.allowRenderTarget = false;
        texDesc.allowDepthStencil = false;
        texDesc.allowUnorderedAccess = false;
        texDesc.debugName = debugName.empty() ? filepath : debugName;

        result.texture = m_resMgr->createTexture(texDesc);
        if (!result.texture.isValid()) {
            stbi_image_free(pixels);
            return result;
        }

        // 上传数据
        size_t dataSize = width * height * 4; 
        if (!uploadPixels(result.texture, pixels, dataSize, width, height, 0)) {
            m_resMgr->destroy(result.texture);
            result.texture = RHI::TextureHandle::Null();
            stbi_image_free(pixels);
            return result;
        }

        stbi_image_free(pixels);

        // 创建采样器
        result.sampler = createDefaultSampler(debugName + "_Sampler");
        return result;
    }

    TextureLoadResult TextureLoader::loadTextureFromMemory(
        const void* data,
        uint32_t width,
        uint32_t height,
        RHI::Format format,
        const std::string& debugName)
    {
        TextureLoadResult result{ RHI::TextureHandle::Null(), RHI::SamplerHandle::Null() };

        if (!data || width == 0 || height == 0) {
            LOG_ERROR("Invalid parameters for loadTextureFromMemory");
            return result;
        }

        // 创建纹理描述
        RHI::TextureDesc texDesc;
        texDesc.extent = { width, height, 1 };
        texDesc.format = format;
        texDesc.type = RHI::TextureType::Texture2D;
        texDesc.mipLevels = 1;
        texDesc.arrayLayers = 1;
        texDesc.sampleCount = 1;
        texDesc.allowRenderTarget = false;
        texDesc.allowDepthStencil = false;
        texDesc.allowUnorderedAccess = false;
        texDesc.debugName = debugName.empty() ? "MemoryTexture" : debugName;

        result.texture = m_resMgr->createTexture(texDesc);
        if (!result.texture.isValid()) {
            LOG_ERROR("Failed to create texture from memory");
            return result;
        }

        // 上传数据（假设像素为 RGBA8，dataSize = width * height * 4）
        size_t dataSize = width * height * 4;
        if (!uploadPixels(result.texture, data, dataSize, width, height, 0)) {
            m_resMgr->destroy(result.texture);
            result.texture = RHI::TextureHandle::Null();
            return result;
        }

        // 创建默认采样器
        result.sampler = createDefaultSampler(debugName + "_Sampler");
        return result;
    }

    // ===== 加载立方体贴图 =====
    TextureLoadResult TextureLoader::loadTextureCube(
        const std::vector<std::string>& faceFilepaths,
        RHI::Format format,
        const std::string& debugName)
    {
        TextureLoadResult result{ RHI::TextureHandle::Null(), RHI::SamplerHandle::Null() };

        if (faceFilepaths.size() != 6) {
            LOG_ERROR("Cube map requires exactly 6 face filepaths, got {}", faceFilepaths.size());
            return result;
        }

        // 加载第一个面以获取尺寸，并验证所有面尺寸一致
        int width = 0, height = 0, channels;
        std::vector<stbi_uc*> facePixels(6, nullptr);
        bool success = true;

        for (int i = 0; i < 6; ++i) {
            int w, h, c;
            stbi_uc* pixels = stbi_load(faceFilepaths[i].c_str(), &w, &h, &c, STBI_rgb_alpha);
            if (!pixels) {
                LOG_ERROR("Failed to load cube face: {}", faceFilepaths[i]);
                success = false;
                break;
            }
            if (i == 0) {
                width = w;
                height = h;
            }
            else if (w != width || h != height) {
                LOG_ERROR("Cube face dimensions mismatch: {}x{} vs {}x{}", width, height, w, h);
                stbi_image_free(pixels);
                success = false;
                break;
            }
            facePixels[i] = pixels;
        }

        if (!success) {
            for (auto* p : facePixels) stbi_image_free(p);
            return result;
        }

        // 创建立方体贴图纹理
        RHI::TextureDesc texDesc;
        texDesc.flags = RHI::ImageCreateFlags::CubeCompatible;
        texDesc.extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
        texDesc.format = format;
        texDesc.type = RHI::TextureType::TextureCube;
        texDesc.mipLevels = 1;
        texDesc.arrayLayers = 6; 
        texDesc.sampleCount = 1;
        texDesc.allowRenderTarget = false;
        texDesc.allowDepthStencil = false;
        texDesc.allowUnorderedAccess = false;
        texDesc.debugName = debugName.empty() ? "CubeMap" : debugName;

        result.texture = m_resMgr->createTexture(texDesc);
        if (!result.texture.isValid()) {
            for (auto* p : facePixels) stbi_image_free(p);
            return result;
        }

        // 逐个面上传
        size_t faceSize = width * height * 4;
        for (int i = 0; i < 6; ++i) {
            if (!uploadPixels(result.texture, facePixels[i], faceSize, width, height, i)) {
                LOG_ERROR("Failed to upload cube face {}", i);
                m_resMgr->destroy(result.texture);
                result.texture = RHI::TextureHandle::Null();
                break;
            }
        }

        // 释放像素内存
        for (auto* p : facePixels) stbi_image_free(p);

        if (!result.texture.isValid()) {
            return result;
        }

        result.sampler = createDefaultSampler(debugName + "_Sampler");
        return result;
    }

    // ===== 保存纹理到文件 =====
    bool TextureLoader::saveTextureToFile(
        const RHI::TextureHandle& texture,
        const std::string& filepath)
    {
        auto* tex = m_resMgr->getTexture(texture);
        if (!tex) {
            LOG_ERROR("Invalid texture handle");
            return false;
        }

        if (tex->getType() != RHI::TextureType::Texture2D) {
            LOG_ERROR("Save only supports 2D textures currently");
            return false;
        }

        // 1. 创建 staging buffer（CPU 可见）
        auto extent = tex->getExtent();
        size_t dataSize = extent.width * extent.height * 4; // RGBA8 每像素 4 字节
        RHI::BufferDesc stagingDesc;
        stagingDesc.size = dataSize;
        stagingDesc.type = RHI::BufferType::Staging;          // 使用 type 而非 usage
        stagingDesc.memoryType = RHI::MemoryType::CPU_To_GPU; // CPU 可见
        stagingDesc.debugName = "StagingBuffer_TextureSave";

        auto stagingBuffer = m_resMgr->createBuffer(stagingDesc);
        if (!stagingBuffer.isValid()) {
            LOG_ERROR("Failed to create staging buffer");
            return false;
        }

        // 2. 定义子资源范围（单个 mip 级别，单个层）
        RHI::ImageSubresourceRange range{
            .aspectMask = RHI::ImageAspect::Color,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        };

        // 3. 将纹理布局转换为 TransferSrc
        tex->transitionLayout(
            RHI::ImageLayout::TransferSrc,
            RHI::PipelineStage::FragmentShader,
            RHI::PipelineStage::Transfer,
            static_cast<RHI::AccessFlags>(RHI::AccessFlag::ShaderRead),
            static_cast<RHI::AccessFlags>(RHI::AccessFlag::TransferRead),
            range);

        // 4. 执行拷贝：纹理 → staging buffer
        RHI::BufferImageCopyRegion copyRegion;
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;        // 0 表示紧密打包
        copyRegion.bufferImageHeight = 0;
        copyRegion.imageSubresource = range;
        copyRegion.imageOffset = { 0, 0, 0 };
        copyRegion.imageExtent = extent;

        // 关键修正：通过 ResourceManager 获取 RHIBuffer*，而不是调用 get()
        tex->copyToBuffer(m_resMgr->getBuffer(stagingBuffer), { copyRegion });

        // 5. 等待拷贝完成（假设 copyToBuffer 内部已同步）

        // 6. 映射 staging buffer 读取数据
        auto* buffer = m_resMgr->getBuffer(stagingBuffer);
        if (!buffer) {
            m_resMgr->destroy(stagingBuffer);
            return false;
        }

        void* mappedData = buffer->map();  // 需要 RHIBuffer 支持 map
        if (!mappedData) {
            LOG_ERROR("Failed to map staging buffer");
            m_resMgr->destroy(stagingBuffer);
            return false;
        }

        // 7. 使用 stb_image_write 写入 PNG
        int success = stbi_write_png(
            filepath.c_str(),
            static_cast<int>(extent.width),
            static_cast<int>(extent.height),
            4, // 通道数
            mappedData,
            static_cast<int>(extent.width) * 4);

        buffer->unmap();

        // 8. 销毁 staging buffer
        m_resMgr->destroy(stagingBuffer);

        // 9. 可选：将纹理布局恢复为 ShaderReadOnly
        tex->transitionLayout(
            RHI::ImageLayout::ShaderReadOnly,
            RHI::PipelineStage::Transfer,
            RHI::PipelineStage::FragmentShader,
            static_cast<RHI::AccessFlags>(RHI::AccessFlag::TransferRead),
            static_cast<RHI::AccessFlags>(RHI::AccessFlag::ShaderRead),
            range);

        if (!success) {
            LOG_ERROR("Failed to write PNG file: {}", filepath);
            return false;
        }

        LOG_INFO("Texture saved to {}", filepath);
        return true;
    }

    TextureLoadResult TextureLoader::loadTextureHDR(
        const std::string& filepath,
        const std::string& debugName)
    {
        TextureLoadResult result{ RHI::TextureHandle::Null(), RHI::SamplerHandle::Null() };

        int width, height, channels;
        float* hdrPixels = stbi_loadf(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        if (!hdrPixels) {
            LOG_ERROR("Failed to load HDR texture: {}", filepath);
            return result;
        }

        // ✅ 直接用 RGBA32_Float，和 float* 数据一致
        RHI::TextureDesc texDesc;
        texDesc.extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
        texDesc.format = RHI::Format::RGBA32_Float;
        texDesc.type = RHI::TextureType::Texture2D;
        texDesc.mipLevels = 1;
        texDesc.arrayLayers = 1;
        texDesc.sampleCount = 1;
        texDesc.debugName = debugName.empty() ? filepath : debugName;

        result.texture = m_resMgr->createTexture(texDesc);
        if (!result.texture.isValid()) {
            stbi_image_free(hdrPixels);
            return result;
        }

        // ✅ 上传 float 数据
        size_t dataSize = width * height * 4 * sizeof(float);
        uploadPixels(result.texture, hdrPixels, dataSize, width, height, 0);

        // ✅ 转换为 cubemap
        auto cubemapTex = convertEquirectToCubemap(
            hdrPixels, width, height, 512, "EnvironmentCubemap");
        stbi_image_free(hdrPixels);

        result.texture = cubemapTex;

        // ✅ 采样器：cubemap 用 ClampToEdge
        result.sampler = createSampler(
            RHI::SamplerFilter::Linear,
            RHI::SamplerAddressMode::ClampToEdge,
            1.0f, 1.0f,
            debugName + "_Sampler");

        return result;
    }

    RHI::TextureHandle TextureLoader::convertEquirectToCubemap(
        const float* hdrPixels,
        uint32_t      hdrWidth,
        uint32_t      hdrHeight,
        uint32_t      faceSize,
        const std::string& debugName)
    {
        // 1. 创建空的 cubemap
        RHI::TextureDesc cubemapDesc;
        cubemapDesc.extent = { faceSize, faceSize, 1 };
        cubemapDesc.format = RHI::Format::RGBA32_Float;   // 半精度浮点即可
        cubemapDesc.type = RHI::TextureType::TextureCube;
        cubemapDesc.mipLevels = 1;
        cubemapDesc.arrayLayers = 6;
        cubemapDesc.sampleCount = 1;
        cubemapDesc.flags = RHI::ImageCreateFlags::CubeCompatible;
        cubemapDesc.allowRenderTarget = false;
        cubemapDesc.allowDepthStencil = false;
        cubemapDesc.allowUnorderedAccess = false;
        cubemapDesc.debugName = debugName;

        auto cubemapHandle = m_resMgr->createTexture(cubemapDesc);
        if (!cubemapHandle.isValid()) {
            LOG_ERROR("Failed to create cubemap: {}", debugName);
            return RHI::TextureHandle::Null();
        }

        // 2. 逐面生成像素并上传
        size_t facePixelCount = faceSize * faceSize;
        std::vector<float> faceData(facePixelCount * 4); // RGBA

        for (uint32_t face = 0; face < 6; ++face) {
            float* dst = faceData.data();
            for (uint32_t y = 0; y < faceSize; ++y) {
                for (uint32_t x = 0; x < faceSize; ++x) {
                    glm::vec3 dir = CubemapFaceToDirection(face, float(x), float(y), faceSize);
                    glm::vec2 uv = DirectionToEquirectUV(dir);
                    glm::vec4 c = SampleEquirectBilinear(hdrPixels, hdrWidth, hdrHeight, uv);

                    *dst++ = c.r;
                    *dst++ = c.g;
                    *dst++ = c.b;
                    *dst++ = c.a;
                }
            }

            // 上传到 cubemap 的当前面
            size_t dataSize = facePixelCount * 4 * sizeof(float);
            if (!uploadPixels(cubemapHandle, faceData.data(), dataSize, faceSize, faceSize, face)) {
                LOG_ERROR("Failed to upload cubemap face {}", face);
                m_resMgr->destroy(cubemapHandle);
                return RHI::TextureHandle::Null();
            }
        }

        LOG_INFO("Cubemap '{}' created: {}×{} ×6 faces", debugName, faceSize, faceSize);
        return cubemapHandle;
    }

    RHI::TextureHandle TextureLoader::createRenderableCubemap(
        uint32_t faceSize,
        RHI::Format format,
        const std::string& debugName)
    {
        RHI::TextureDesc desc;
        desc.extent = { faceSize, faceSize, 1 };
        desc.format = format;
        desc.type = RHI::TextureType::TextureCube;
        desc.mipLevels = 1;
        desc.arrayLayers = 6;
        desc.sampleCount = 1;
        desc.flags = RHI::ImageCreateFlags::CubeCompatible;
        desc.allowRenderTarget = true;   
        desc.allowDepthStencil = false;
        desc.allowUnorderedAccess = false;
        desc.debugName = debugName;

        return m_resMgr->createTexture(desc);
    }
} // namespace StarryEngine::Assets