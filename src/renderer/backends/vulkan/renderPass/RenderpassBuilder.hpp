#pragma once
#include <memory>
#include <vector>

namespace StarryEngine::Builder {
	class SubpassBuilder;
	class LogicalDevice;

	class RenderPassBuilder {
	public:
		RenderPassBuilder(std::shared_ptr<LogicalDevice> logicalDevice)
			: mLogicalDevice(logicalDevice) {
		}
		~RenderPassBuilder() = default;

	private:
		std::vector<SubpassBuilder> mSubpassBuilders;
		std::shared_ptr<LogicalDevice> mLogicalDevice;
	};

}