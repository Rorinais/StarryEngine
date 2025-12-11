#pragma once
#include<vulkan/vulkan.h>
#include <string>
#include <memory>
namespace StarryEngine {
	enum class PipelineComponentType :uint32_t {
		SHADER_STAGE=0,
		VERTEX_INPUT,
		INPUT_ASSEMBLY,
		VIEWPORT_STATE,
		RASTERIZATION,
		MULTISAMPLE,
		DEPTH_STENCIL,
		COLOR_BLEND,
		DYNAMIC_STATE,
		TESSELLATION,
		COUNT
	};

	static const char* getComponentTypeName(PipelineComponentType type) {
		switch (type) {
		case PipelineComponentType::SHADER_STAGE:
			return "ShaderStages";
		case PipelineComponentType::VERTEX_INPUT:
			return "VertexInput";
		case PipelineComponentType::INPUT_ASSEMBLY:
			return "InputAssembly";
		case PipelineComponentType::VIEWPORT_STATE:
			return "Viewport";
		case PipelineComponentType::RASTERIZATION:
			return "Rasterization";
		case PipelineComponentType::MULTISAMPLE:
			return "Multisample";
		case PipelineComponentType::DEPTH_STENCIL:
			return "DepthStencil";
		case PipelineComponentType::COLOR_BLEND:
			return "ColorBlend";
		case PipelineComponentType::DYNAMIC_STATE:
			return "DynamicState";
		case PipelineComponentType::TESSELLATION:
			return "Tessellation";
		default:
			return "Unknown";
		}
	}

	static const  char *getComponentTypeDescription(PipelineComponentType type) {
		switch (type) {
		case PipelineComponentType::SHADER_STAGE:
			return "Shader Stages Configuration";
		case PipelineComponentType::VERTEX_INPUT:
			return "Vertex Input Configuration";
		case PipelineComponentType::INPUT_ASSEMBLY:
			return "Input Assembly Configuration";
		case PipelineComponentType::VIEWPORT_STATE:
			return "Viewport and Scissor Configuration";
		case PipelineComponentType::RASTERIZATION:
			return "Rasterization Configuration";
		case PipelineComponentType::MULTISAMPLE:
			return "Multisampling Configuration";
		case PipelineComponentType::DEPTH_STENCIL:
			return "Depth and Stencil Configuration";
		case PipelineComponentType::COLOR_BLEND:
			return "Color Blending Configuration";
		case PipelineComponentType::DYNAMIC_STATE:
			return "Dynamic State Configuration";
		case PipelineComponentType::TESSELLATION:
			return "Tessellation Configuration";
		default:
			return "No Description Available";
		}
	}

	class IPipelineStateComponent {
	public:
		virtual PipelineComponentType getType() const = 0;
		virtual const std::string& getName() const = 0;
		virtual std::string getDescription() const = 0;
		virtual std::shared_ptr<IPipelineStateComponent> clone() const = 0;
		virtual void apply(VkGraphicsPipelineCreateInfo& pipelineInfo) = 0;
		virtual bool isValid() const = 0;
		virtual ~IPipelineStateComponent() = default;
	};
}