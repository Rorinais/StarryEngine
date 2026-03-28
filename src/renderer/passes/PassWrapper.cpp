#include"PassWrapper.hpp"

namespace StarryEngine {
	RHI::TextureDesc PassWrapper::createTextureDesc(
		RHI::Extent3D extent,
		RHI::Format format,
		RHI::TextureType type,
		bool allowInputAttachment,
		bool allowRenderTarget,
		bool allowDepthStencil,
		uint32_t arrayLayers,
		uint32_t mipLevels,
		uint32_t sampleCount
	) {
		RHI::TextureDesc desc;
		desc.extent = extent;
		desc.format = format;
		desc.type = type;
		desc.allowInputAttachment = allowInputAttachment;
		desc.allowRenderTarget = allowRenderTarget;
		desc.allowDepthStencil = allowDepthStencil;
		desc.arrayLayers = arrayLayers;
		desc.mipLevels = mipLevels;
		desc.sampleCount = sampleCount;
		return desc;
	}

	RHI::TextureDesc PassWrapper::createColorTextureDesc(
		RHI::Extent3D extent,
		RHI::Format format,
		RHI::TextureType type
	) {
		return createTextureDesc(
			extent,format,type,
			true,true,false,
			1,1,1
		);
	}

	RHI::TextureDesc PassWrapper::createDepthTextureDesc(
		RHI::Extent3D extent,
		RHI::Format format,
		RHI::TextureType type
	) {
		return createTextureDesc(
			extent, format, type,
			true, false, true,
			1, 1, 1
		);
	}

	RenderGraph::AttachmentParams PassWrapper::createAttachmentParam(
		std::optional<RHI::ImageLayout> initialLayout,
		std::optional<RHI::ImageLayout> finalLayout,
		std::optional<RHI::AttachmentLoadOp> loadOp,
		std::optional<RHI::AttachmentStoreOp> storeOp,
		std::optional<RHI::Color> clearColor,
		std::optional<float> clearDepth,
		std::optional<uint32_t> clearStencil,
		std::optional<RHI::Format> format
	) {
		RenderGraph::AttachmentParams attachment;
		attachment.initialLayout = initialLayout;
		attachment.finalLayout = finalLayout;
		attachment.loadOp = loadOp;
		attachment.storeOp = storeOp;
		attachment.clearColor = clearColor;
		attachment.clearDepth = clearDepth;
		attachment.clearStencil = clearStencil;
		attachment.format = format;
		return attachment;
	}

	RenderGraph::AttachmentParams PassWrapper::createColorAttachment(
		Layout initialLayout,
		Layout finalLayout,
		LoadOp loadOp,
		StoreOp storeOp,
		RHI::Color clearColor
	) {
		return createAttachmentParam(
			initialLayout,
			finalLayout,
			loadOp,
			storeOp,
			clearColor,
			std::nullopt,
			std::nullopt,
			std::nullopt
		);
	}

	RenderGraph::AttachmentParams PassWrapper::createDepthAttachment(
		Layout initialLayout,
		Layout finalLayout,
		LoadOp loadOp,
		StoreOp storeOp,
		float clearDepth,
		uint32_t clearStencil
	) {
		return createAttachmentParam(
			initialLayout,
			finalLayout,
			loadOp,
			storeOp,
			std::nullopt,
			clearDepth,
			clearStencil,
			std::nullopt
		);
	}

	SubpassConfig PassWrapper::createGeometrySubpassConfig(
		const std::string& name,
		std::shared_ptr<ISubpassRecorder> recorder,
		const std::vector<std::string>& colorTextureNames,
		const std::string& depthTextureName) {

		SubpassConfig config;
		config.name = name;
		config.recorder = recorder;

		auto colorAttachParams = createColorAttachment(
			RHI::ImageLayout::Undefined,
			RHI::ImageLayout::ColorAttachment,
			RHI::AttachmentLoadOp::Clear,
			RHI::AttachmentStoreOp::Store);
		for (const auto& tex : colorTextureNames) {
			config.colorAttachments.push_back({ tex, colorAttachParams });
		}

		auto depthAttachParams = createDepthAttachment(
			RHI::ImageLayout::Undefined,
			RHI::ImageLayout::DepthStencilAttachment,
			RHI::AttachmentLoadOp::Clear,
			RHI::AttachmentStoreOp::DontCare);
		config.depthAttachment = { depthTextureName, depthAttachParams };

		return config;
	}

	SubpassConfig PassWrapper::createFullscreenSubpassConfig(
		const std::string& name,
		std::shared_ptr<ISubpassRecorder> recorder,
		const std::vector<SubpassAttachment>& colorAttachments,
		const std::vector<SubpassAttachment>& inputAttachments,
		std::optional<SubpassAttachment> depthAttachment) {

		SubpassConfig config;
		config.name = name;
		config.recorder = recorder;
		config.colorAttachments = colorAttachments;
		config.inputAttachments = inputAttachments;
		if (depthAttachment) config.depthAttachment = depthAttachment;
		return config;
	}
}