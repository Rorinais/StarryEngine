#include "RenderPassBuilder.hpp"
#include "SubpassBuilder.hpp"

namespace StarryEngine {
	void createRenderPass() {
        // 创建延迟渲染流程
        auto deferredRenderPass = RenderPassBuilder("mainPass")
            // 1. 定义所有附件
            .addColorAttachment("gbufferAlbedo", VK_FORMAT_R8G8B8A8_UNORM)
            .addColorAttachment("gbufferNormal", VK_FORMAT_R16G16B16A16_SFLOAT)
            .addColorAttachment("gbufferMaterial", VK_FORMAT_R8G8B8A8_UNORM)
            .addDepthAttachment("depth", VK_FORMAT_D24_UNORM_S8_UINT)
            .addColorAttachment("lightingResult", VK_FORMAT_R8G8B8A8_UNORM)
            .addColorAttachment("finalOutput", VK_FORMAT_R8G8B8A8_UNORM)

            // 2. 添加子流程
            // 几何处理子流程
            .addSubpass(
                SubpassBuilder("gbufferpass")
                .addColorAttachment("gbufferAlbedo")
                .addColorAttachment("gbufferNormal")
                .addColorAttachment("gbufferMaterial")
                .setDepthStencilAttachment("depth")
            )
            // 光照计算子流程
            .addSubpass(
                SubpassBuilder("lightpass")
                .addInputAttachment("gbufferAlbedo")
                .addInputAttachment("gbufferNormal")
                .addInputAttachment("gbufferMaterial")
                .addInputAttachment("depth")
                .addColorAttachment("lightingResult")
            )
            // 后处理子流程
            .addSubpass(
                SubpassBuilder("postpass")
                .addInputAttachment("lightingResult")
                .addColorAttachment("finalOutput")
            )

            // 3. 构建（自动推导依赖）
            .build(true);
	}
}