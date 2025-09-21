#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include <string>
#include <functional>
#include <vector>
#include <unordered_map>

namespace StarryEngine {
	//泛型句柄
	template<typename TypeName>
	class Handle {
	private:
		uint32_t m_id;
	public:
		Handle(uint32_t id = UINT32_MAX) : m_id(id) {}
		~Handle() = default;

		bool operator==(const Handle& other) const { return m_id == other.m_id; }
		bool operator!=(const Handle& other) const { return m_id != other.m_id; }

		uint32_t getId() const { return m_id; }
		uint32_t setId(uint32_t id) { return m_id = id; }

		bool isValid() const { return m_id != UINT32_MAX; }

		struct Hash {
			size_t operator()(const Handle& handle) const
			{
				return std::hash<uint32_t>()(handle.getId());
			}
		};
	};

	//渲染通道句柄和资源句柄
	struct RenderPassTag {};
	struct ReSourceTag {};

	using RenderPassHandle = Handle<RenderPassTag>;
	using ResourceHandle = Handle<ReSourceTag>;

	//资源使用类型
	enum class ResourceType {
		Undefined,
		ColorAttachment,
		DepthStencilAttachment,
		InputAttachment,
		SampledImage,
		UniformBuffer,
		StorageBuffer,
		IndirectBuffer,
		VertexBuffer,
		IndexBuffer,
	};

	//资源描述符
	struct ResourceDescription{
		ResourceType type = ResourceType::Undefined;
		//仅对图像资源有效
		VkFormat format = VK_FORMAT_UNDEFINED;
		VkExtent3D extent = { 1,1,1 }; 
		uint32_t arrayLayers = 1; 
		uint32_t mipLevels = 1; 
		VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT; 
		VkImageUsageFlags imageUsage = 0; 

		//仅对缓冲区资源有效
		VkBufferUsageFlags bufferUsage = 0; 
		VkMemoryPropertyFlags memoryProperties = 0; 
		size_t size = 0;
		bool isTransient = false; //是否为临时资源，如深度图

		bool operator==(const ResourceDescription& other) const {
			return type == other.type &&
				format == other.format &&
				extent.width == other.extent.width &&
				extent.height == other.extent.height &&
				extent.depth == other.extent.depth &&
				arrayLayers == other.arrayLayers &&
				mipLevels == other.mipLevels &&
				samples == other.samples &&
				imageUsage == other.imageUsage &&
				bufferUsage == other.bufferUsage &&
				memoryProperties == other.memoryProperties &&
				size == other.size &&
				isTransient == other.isTransient;
		}
	};

	size_t hash(const ResourceDescription& desc) {
		size_t hash = std::hash<int>()(static_cast<int>(desc.type));
		hash ^= std::hash<int>()(static_cast<int>(desc.format)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<uint32_t>()(desc.extent.width) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<uint32_t>()(desc.extent.height) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<uint32_t>()(desc.extent.depth) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<uint32_t>()(desc.arrayLayers) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<uint32_t>()(desc.mipLevels) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<int>()(static_cast<int>(desc.samples)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<uint32_t>()(desc.imageUsage) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<uint32_t>()(desc.bufferUsage) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<uint32_t>()(desc.memoryProperties) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<uint32_t>()(desc.size) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<bool>()(desc.isTransient) + 0x9e3779b9 + (hash << 6) + (hash >> 2);

		return hash;
	}

	struct ResourceState {
		VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED; //图像资源布局
		VkAccessFlags accessMask = 0; //访问掩码
		VkPipelineStageFlags stageMask = 0; //管线阶段掩码

		bool isWrite() const {
			return accessMask && (
					VK_ACCESS_SHADER_WRITE_BIT |
					VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
					VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
					VK_ACCESS_TRANSFER_WRITE_BIT
					);
		}
	};

	struct BarrierBatch {
		std::vector<VkMemoryBarrier> memoryBarriers;
		std::vector<VkBufferMemoryBarrier> bufferBarriers;
		std::vector<VkImageMemoryBarrier> imageBarriers;

		bool empty() const {
			return memoryBarriers.empty() &&
				bufferBarriers.empty() &&
				imageBarriers.empty();
		}
	};

	struct Dependency {
		RenderPassHandle producer;
		RenderPassHandle consumer;
		ResourceHandle resource;
		ResourceState stateBefore;
		ResourceState stateAfter;
	};
}


namespace std {
	template<>
	class hash<StarryEngine::ResourceDescription> {
	public:
		size_t operator()(const StarryEngine::ResourceDescription& desc)const {
			return StarryEngine::hash(desc);
		}
	};

	template<typename Tag>
	class hash<StarryEngine::Handle<Tag>> {
	public:
		size_t operator()(const StarryEngine::Handle<Tag>& handle) {
			return StarryEngine::hash<uint32_t>()(handle.getId());
		}
	};
}