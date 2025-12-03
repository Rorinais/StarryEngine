#pragma once
#include<vulkan/vulkan.h>


namespace StarryEngine {
	class IPipeline {
		public:
		virtual VkPipeline getHanlde() const = 0;
		virtual VkPipelineLayout getPipelineLayout() const = 0;
		virtual ~IPipeline() = default;
	};

}
