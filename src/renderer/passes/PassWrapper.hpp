#pragma once
#include "Type.hpp"

namespace StarryEngine {
    class PassWrapper {
    public:
        using Layout = RHI::ImageLayout;
        using LoadOp = RHI::AttachmentLoadOp;
        using StoreOp = RHI::AttachmentStoreOp;

        // 原有方法...
        static RHI::TextureDesc createTextureDesc(
            RHI::Extent3D extent,
            RHI::Format format,
            RHI::TextureType type,
            bool allowInputAttachment,
            bool allowRenderTarget,
            bool allowDepthStencil,
            uint32_t arrayLayers,
            uint32_t mipLevels,
            uint32_t sampleCount);

        static RHI::TextureDesc createColorTextureDesc(
            RHI::Extent3D extent,
            RHI::Format format,
            RHI::TextureType type = RHI::TextureType::Texture2D);

        static RHI::TextureDesc createDepthTextureDesc(
            RHI::Extent3D extent,
            RHI::Format format,
            RHI::TextureType type = RHI::TextureType::Texture2D);

        static RenderGraph::AttachmentParams createAttachmentParam(
            std::optional<RHI::ImageLayout> initialLayout,
            std::optional<RHI::ImageLayout> finalLayout,
            std::optional<RHI::AttachmentLoadOp> loadOp,
            std::optional<RHI::AttachmentStoreOp> storeOp,
            std::optional<RHI::Color> clearColor,
            std::optional<float> clearDepth,
            std::optional<uint32_t> clearStencil,
            std::optional<RHI::Format> format = std::nullopt);

        static RenderGraph::AttachmentParams createColorAttachment(
            Layout initialLayout,
            Layout finalLayout,
            LoadOp loadOp,
            StoreOp storeOp,
            RHI::Color clearColor = RHI::Color::Gray());

        static RenderGraph::AttachmentParams createDepthAttachment(
            Layout initialLayout,
            Layout finalLayout,
            LoadOp loadOp,
            StoreOp storeOp,
            float clearDepth = 1.0f,
            uint32_t clearStencil = 1);

        // 新增工厂方法
        static SubpassDesc createGeometrySubpassConfig(
            const std::string& name,
            std::shared_ptr<IPassExecutor> executor,
            const std::vector<std::string>& colorTextureNames,
            const std::string& depthTextureName);

        static SubpassDesc createFullscreenSubpassConfig(
            const std::string& name,
            std::shared_ptr<IPassExecutor> executor,
            const std::vector<SubpassAttachment>& colorAttachments,
            const std::vector<SubpassAttachment>& inputAttachments = {},
            std::optional<SubpassAttachment> depthAttachment = std::nullopt);
    };
}