#include"DescriptorManager.hpp"
#include"../vulkanCore/VulkanCore.hpp"
#include "../renderPass/RenderPassBuilder.hpp"

namespace StarryEngine {
	void test() {
		auto device = LogicalDevice::create();

		auto descriptorManager = std::make_shared<DescriptorManager>(device);

		// 定义set 0的布局
		descriptorManager->beginSetLayout(0);
		descriptorManager->addUniformBuffer(0, VK_SHADER_STAGE_VERTEX_BIT); // binding 0: uniform buffer
		descriptorManager->addCombinedImageSampler(1, VK_SHADER_STAGE_FRAGMENT_BIT); // binding 1: texture
		descriptorManager->endSetLayout();

		// 定义set 1的布局
		descriptorManager->beginSetLayout(1);
		descriptorManager->addStorageBuffer(0, VK_SHADER_STAGE_COMPUTE_BIT); // binding 0: storage buffer
		descriptorManager->endSetLayout();

		// 分配描述符集，每个set分配2个实例（用于双缓冲）
		descriptorManager->allocateSets(2);
	}

	void render(std::shared_ptr<DescriptorManager> descriptorManager,uint32_t frameIndex) {
		descriptorManager->updateUniformBuffer(0, 0, frameIndex, uniformBuffer0);
		descriptorManager->updateUniformBuffer(0, 1, frameIndex, uniformBuffer1);
		descriptorManager->updateStorageBuffer(1, 0, frameIndex, storageBuffer0);
	}

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
            );
        auto renderPassResult = deferredRenderPass.build();
        renderPassResult->name;
        renderPassResult->renderPass;
        renderPassResult->pipelineNameToSubpassIndexMap["gbufferpass"];
    }
}