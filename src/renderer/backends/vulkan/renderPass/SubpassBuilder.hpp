#pragma once
#include<string>
#include<unordered_map>
#include<optional>
#include"Subpass.hpp"

namespace StarryEngine::Builder {

    enum class SubpassAttachFormat {
        // 8位无符号归一化格式 
        RGBA8 = VK_FORMAT_R8G8B8A8_UNORM,           // 37 - 标准 RGBA 8位
        BGRA8 = VK_FORMAT_B8G8R8A8_UNORM,           // 44 - BGRA 顺序 (Windows 常用)
        SRGBA8 = VK_FORMAT_R8G8B8A8_SRGB,           // 43 - sRGB 颜色空间

        // 16位浮点格式 (HDR 渲染)
        RGBA16F = VK_FORMAT_R16G16B16A16_SFLOAT,    // 97 - 16位浮点 RGBA

        // 32位浮点格式 (高精度 HDR)
        RGBA32F = VK_FORMAT_R32G32B32A32_SFLOAT,    // 109 - 32位浮点 RGBA

        // 深度格式
        DEPTH16 = VK_FORMAT_D16_UNORM,              // 124 - 16位深度
        DEPTH32F = VK_FORMAT_D32_SFLOAT,            // 126 - 32位浮点深度
        DEPTH24STENCIL8 = VK_FORMAT_D24_UNORM_S8_UINT, // 129 - 24位深度 + 8位模板
        DEPTH32FSTENCIL8 = VK_FORMAT_D32_SFLOAT_S8_UINT, // 130 - 32位深度 + 8位模板

        // 其他常用颜色格式
        R8 = VK_FORMAT_R8_UNORM,                    // 9 - 单通道 8位
        RG8 = VK_FORMAT_R8G8_UNORM,                 // 16 - 双通道 8位
        RGB8 = VK_FORMAT_R8G8B8_UNORM,              // 23 - RGB 8位
        R16F = VK_FORMAT_R16_SFLOAT,                // 76 - 单通道 16位浮点
        R32F = VK_FORMAT_R32_SFLOAT,                // 100 - 单通道 32位浮点

        // 10-10-10-2 格式 
        A2R10G10B10 = VK_FORMAT_A2R10G10B10_UNORM_PACK32, // 58 - 2+10+10+10 位打包

        // 压缩格式 
        BC1_RGBA = VK_FORMAT_BC1_RGBA_UNORM_BLOCK,  // 133 - DXT1 压缩
        BC3_RGBA = VK_FORMAT_BC3_UNORM_BLOCK,       // 137 - DXT5 压缩
        BC7_UNORM = VK_FORMAT_BC7_UNORM_BLOCK       // 145 - BC7 压缩
    };

    struct SubpassAttachmentInfo {
        std::string name;
        SubpassAttachFormat format;
        VkImageLayout layout{ VK_IMAGE_LAYOUT_UNDEFINED };
        uint32_t index{ 0 };
    };

    class SubpassBuilder {
    public:
        SubpassBuilder() = default;
        ~SubpassBuilder() = default;

        // 构建接口
        SubpassBuilder& addColorAttachment(const std::string& name, SubpassAttachFormat format);
        SubpassBuilder& addInputAttachment(const std::string& name, SubpassAttachFormat format);
        SubpassBuilder& addResolveAttachment(const std::string& name, SubpassAttachFormat format);
        SubpassBuilder& setDepthStencilAttachment(const std::string& name, SubpassAttachFormat format);
        SubpassBuilder& addPreserveAttachment(const std::string& name);
        SubpassBuilder& setAttachmentIndex(const std::string& name, uint32_t index);

        std::unique_ptr<Subpass> build(VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS);

        const std::vector<SubpassAttachmentInfo>& getColorAttachments() const { return mColorAttachments; }
        const std::vector<SubpassAttachmentInfo>& getInputAttachments() const { return mInputAttachments; }
        const std::vector<SubpassAttachmentInfo>& getResolveAttachments() const { return mResolveAttachments; }
        const std::optional<SubpassAttachmentInfo>& getDepthStencilAttachment() const { return mDepthStencilAttachment; }
        const std::vector<std::string>& getPreserveAttachments() const { return mPreserveAttachments; }

    private:
        std::vector<SubpassAttachmentInfo> mColorAttachments;
        std::vector<SubpassAttachmentInfo> mInputAttachments;
        std::vector<SubpassAttachmentInfo> mResolveAttachments;
        std::vector<std::string> mPreserveAttachments;
        std::optional<SubpassAttachmentInfo> mDepthStencilAttachment;

        std::unordered_map<std::string, uint32_t> mNameToIndexMap;

        VkImageLayout getDefaultLayout(SubpassAttachFormat format, bool isInput = false);
    };
}