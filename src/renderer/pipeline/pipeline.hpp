#pragma once
#include"../../base.hpp"
#include"../resources/shaders/shaderUtils.hpp"
#include"../resources/shaders/shaderProgram.hpp"
#include"../core/VulkanCore/VulkanCore.hpp"
#include "../../renderer/core/RenderGraph/RenderPassSystem.hpp"
#include"pipelineStates/colorBlend.hpp"
#include"pipelineStates/depthStencil.hpp"
#include"pipelineStates/inputAssembly.hpp"
#include"pipelineStates/multiSample.hpp"
#include"pipelineStates/rasterization.hpp"
#include"pipelineStates/vertexInput.hpp"
#include"pipelineStates/viewport.hpp"
#include"pipelineStates/dynamic.hpp"

namespace StarryEngine {
	class PipelineLayout {
	public:
		using Ptr = std::shared_ptr<PipelineLayout>;
		static Ptr create(const LogicalDevice::Ptr& logicalDevice, std::vector<VkDescriptorSetLayout> descriptorSetLayout) {
			return std::make_shared<PipelineLayout>(logicalDevice, descriptorSetLayout);
		}
		PipelineLayout(const LogicalDevice::Ptr& logicalDevice, std::vector<VkDescriptorSetLayout> descriptorSetLayout) :mLogicalDevice(logicalDevice) {
			VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
			pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayout.size());
			pipelineLayoutInfo.pSetLayouts = descriptorSetLayout.data();
			if (vkCreatePipelineLayout(mLogicalDevice->getHandle(), &pipelineLayoutInfo, nullptr, &mPipelineLayout) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create pipeline layout!");
			}
		}

		~PipelineLayout() {
			if (mPipelineLayout != VK_NULL_HANDLE) {
				vkDestroyPipelineLayout(mLogicalDevice->getHandle(), mPipelineLayout, nullptr);
				mPipelineLayout = VK_NULL_HANDLE;
			}
		}

		VkPipelineLayout getHandle() { return mPipelineLayout; }

	private:
		LogicalDevice::Ptr mLogicalDevice;
		VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
	};


	class Pipeline {
	public:
		struct Config {
			VertexInput vertexInputState;
			InputAssembly inputAssemblyState;
			Viewport viewportState;
			Rasterization rasterizationState;
			MultiSample multisampleState;
			DepthStencil depthStencilState;
			ColorBlend colorBlendState;
			Dynamic dynamicState;
		};

		using Ptr = std::shared_ptr<Pipeline>;
		static Ptr create(const LogicalDevice::Ptr& logicalDevice, Pipeline::Config config = {}) {
			return std::make_shared<Pipeline>(logicalDevice, config);
		}

		Pipeline(const LogicalDevice::Ptr& logicalDevice, Pipeline::Config config = {});
		~Pipeline();

		void cleanup();
		void createGraphicsPipeline();

		//必须指定
		void setShaderStage(ShaderProgram::Ptr shaderStates);
		void setRenderPass(VkRenderPass renderpass);
		void setPipelineLayout(PipelineLayout::Ptr pipelineLayout);
		void setViewportState(Viewport viewport);
		void setVertexInputState(VertexInput vertexInput);

		//按需求设置
		void setInputAssemblyState(InputAssembly inputAssembly);
		void setRasterizationState(Rasterization rasterization);
		void setMultiSampleState(MultiSample multiSample);
		void setColorBlendState(ColorBlend colorBlend);
		void setDepthStencilState(DepthStencil depthStencil);
		void setDynamicStates(Dynamic dynamic);

		//有默认参数
		void setSubPass(uint32_t count = 0);
		void setBasePipelineHandle(VkPipeline basePipeline = VK_NULL_HANDLE);
		void setBasePipelineIndex(uint32_t index = -1);

		VkPipeline& getHandle() { return mGraphicsPipeline; }
		Pipeline::Config  getPipelineStateConfig() { return mPipelineStageConfig; }
		PipelineLayout::Ptr getPipelineLayout() { return mPipelineLayout; }
		Dynamic getDynamic() { return mPipelineStageConfig.dynamicState; }

	private:
		Pipeline::Config mPipelineStageConfig;

		LogicalDevice::Ptr mLogicalDevice;
		PipelineLayout::Ptr mPipelineLayout;
		ShaderProgram::Ptr mShaderStages;

		VkRenderPass mRenderPass = VK_NULL_HANDLE;
		VkPipeline mGraphicsPipeline = VK_NULL_HANDLE;
		VkPipeline mBasePipelineHandle = VK_NULL_HANDLE;
		int32_t  mBasePipelineIndex = -1;
		uint32_t mSubpass = 0;
	};
}