#pragma once
#include <shaderc/shaderc.hpp>
#include "RHI_ENUMS.hpp"
#include "RHI_HANDLES_SYSTEM.hpp"
#include "RHI_STRUCTS_DESC.hpp"

namespace StarryEngine::RHI::FUNC {

    // ==================== 着色器阶段转换 ====================
    static VkShaderStageFlagBits RHI_TO_VK_ShaderStageFlag(ShaderStage stage) {
        switch (stage) {
        case ShaderStage::Vertex:   return VK_SHADER_STAGE_VERTEX_BIT;
        case ShaderStage::Fragment: return VK_SHADER_STAGE_FRAGMENT_BIT;
        case ShaderStage::Compute:  return VK_SHADER_STAGE_COMPUTE_BIT;
        case ShaderStage::Geometry: return VK_SHADER_STAGE_GEOMETRY_BIT;
        case ShaderStage::TessellationControl: return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case ShaderStage::TessellationEvaluation: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        default:
            throw std::runtime_error("Unsupported shader stage: " + std::to_string(static_cast<int>(stage)));
        }
    }

    static shaderc_shader_kind RHI_TO_Shaderc_ShaderKind(ShaderStage stage) {
        switch (stage) {
        case ShaderStage::Vertex:   return shaderc_vertex_shader;
        case ShaderStage::Fragment: return shaderc_fragment_shader;
        case ShaderStage::Compute:  return shaderc_compute_shader;
        case ShaderStage::Geometry: return shaderc_geometry_shader;
        case ShaderStage::TessellationControl: return shaderc_tess_control_shader;
        case ShaderStage::TessellationEvaluation: return shaderc_tess_evaluation_shader;
        default:
            throw std::runtime_error("Unsupported shader stage for shaderc: " + std::to_string(static_cast<int>(stage)));
        }
    }

    // ==================== 格式转换 ====================
    static VkFormat RHI_TO_VK_Format(Format format) {
        switch (format) {
        case Format::R8_UNorm: return VK_FORMAT_R8_UNORM;
        case Format::R8_SNorm: return VK_FORMAT_R8_SNORM;
        case Format::R8_UInt: return VK_FORMAT_R8_UINT;
        case Format::R8_SInt: return VK_FORMAT_R8_SINT;
        case Format::R8_sRGB: return VK_FORMAT_R8_SRGB;
        case Format::R16_UNorm: return VK_FORMAT_R16_UNORM;
        case Format::R16_SNorm: return VK_FORMAT_R16_SNORM;
        case Format::R16_UInt: return VK_FORMAT_R16_UINT;
        case Format::R16_SInt: return VK_FORMAT_R16_SINT;
        case Format::R16_Float: return VK_FORMAT_R16_SFLOAT;
        case Format::RG8_UNorm: return VK_FORMAT_R8G8_UNORM;
        case Format::RG8_SNorm: return VK_FORMAT_R8G8_SNORM;
        case Format::RG8_UInt: return VK_FORMAT_R8G8_UINT;
        case Format::RG8_SInt: return VK_FORMAT_R8G8_SINT;
        case Format::R32_UInt: return VK_FORMAT_R32_UINT;
        case Format::R32_SInt: return VK_FORMAT_R32_SINT;
        case Format::R32_Float: return VK_FORMAT_R32_SFLOAT;
        case Format::RG16_UNorm: return VK_FORMAT_R16G16_UNORM;
        case Format::RG16_SNorm: return VK_FORMAT_R16G16_SNORM;
        case Format::RG16_UInt: return VK_FORMAT_R16G16_UINT;
        case Format::RG16_SInt: return VK_FORMAT_R16G16_SINT;
        case Format::RG16_Float: return VK_FORMAT_R16G16_SFLOAT;
        case Format::RGBA8_UNorm: return VK_FORMAT_R8G8B8A8_UNORM;
        case Format::RGBA8_SNorm: return VK_FORMAT_R8G8B8A8_SNORM;
        case Format::RGBA8_UInt: return VK_FORMAT_R8G8B8A8_UINT;
        case Format::RGBA8_SInt: return VK_FORMAT_R8G8B8A8_SINT;
        case Format::BGRA8_UNorm: return VK_FORMAT_B8G8R8A8_UNORM;
        case Format::BGRA8_SNorm: return VK_FORMAT_B8G8R8A8_SNORM;
        case Format::BGRA8_UInt: return VK_FORMAT_B8G8R8A8_UINT;
        case Format::BGRA8_SInt: return VK_FORMAT_B8G8R8A8_SINT;
        case Format::RGBA8_sRGB: return VK_FORMAT_R8G8B8A8_SRGB;
        case Format::BGRA8_sRGB: return VK_FORMAT_B8G8R8A8_SRGB;
        case Format::D16_UNorm: return VK_FORMAT_D16_UNORM;
        case Format::D32_Float: return VK_FORMAT_D32_SFLOAT;
        case Format::D24_UNorm_S8_UInt: return VK_FORMAT_D24_UNORM_S8_UINT;
        case Format::RGB32_Float: return VK_FORMAT_R32G32B32_SFLOAT;
        case Format::RG32_Float: return VK_FORMAT_R32G32_SFLOAT;
        case Format::RGBA32_Float: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case Format::RGBA16_Float: return VK_FORMAT_R16G16B16A16_SFLOAT;
        default: return VK_FORMAT_UNDEFINED;
        }
    }

    static RHI::Format VK_TO_RHI_Format(VkFormat vkFormat) {
        switch (vkFormat) {
        case VK_FORMAT_D16_UNORM:          return RHI::Format::D16_UNorm;
        case VK_FORMAT_D32_SFLOAT:         return RHI::Format::D32_Float;
        case VK_FORMAT_D24_UNORM_S8_UINT:  return RHI::Format::D24_UNorm_S8_UInt;
        case VK_FORMAT_R8G8B8A8_UNORM:     return RHI::Format::RGBA8_UNorm;
        case VK_FORMAT_R8G8B8A8_SRGB:      return RHI::Format::RGBA8_sRGB;
        case VK_FORMAT_B8G8R8A8_UNORM:     return RHI::Format::BGRA8_UNorm;
        case VK_FORMAT_B8G8R8A8_SRGB:      return RHI::Format::BGRA8_sRGB;
        default: return RHI::Format::Undefined;
        }
    }

    // ==================== 内存类型转换 ====================
    static VmaMemoryUsage RHI_TO_VK_VmaMemoryUsage(MemoryType memoryType) {
        switch (memoryType) {
        case MemoryType::GPU_Only:      return VMA_MEMORY_USAGE_GPU_ONLY;
        case MemoryType::CPU_To_GPU:    return VMA_MEMORY_USAGE_CPU_TO_GPU;
        case MemoryType::CPU_Only:      return VMA_MEMORY_USAGE_CPU_ONLY;
        case MemoryType::GPU_To_CPU:    return VMA_MEMORY_USAGE_GPU_TO_CPU;
        default:                        return VMA_MEMORY_USAGE_AUTO;
        }
    }

    static VkMemoryPropertyFlags RHI_TO_VK_MemoryProperties(MemoryType memoryType) {
        switch (memoryType) {
        case MemoryType::GPU_Only:
            return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        case MemoryType::CPU_To_GPU:
            return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        case MemoryType::CPU_Only:
            return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        case MemoryType::GPU_To_CPU:
            return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        default:
            return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        }
    }

    // ==================== 图像布局转换 ====================
    static VkImageLayout RHI_TO_VK_ImageLayout(ImageLayout layout) {
        switch (layout) {
        case ImageLayout::Undefined:                        return VK_IMAGE_LAYOUT_UNDEFINED;
        case ImageLayout::General:                          return VK_IMAGE_LAYOUT_GENERAL;
        case ImageLayout::ColorAttachment:                  return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case ImageLayout::DepthStencilAttachment:           return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        case ImageLayout::DepthStencilReadOnly:             return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        case ImageLayout::ShaderReadOnly:                   return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        case ImageLayout::TransferSrc:                      return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        case ImageLayout::TransferDst:                      return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        case ImageLayout::Preinitialized:                   return VK_IMAGE_LAYOUT_PREINITIALIZED;
        case ImageLayout::PresentSrc:                       return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        case ImageLayout::DepthReadOnlyStencilAttachment:   return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL_KHR;
        case ImageLayout::DepthAttachmentStencilReadOnly:   return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR;
        case ImageLayout::DepthReadOnly:                    return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL_KHR;
        case ImageLayout::StencilReadOnly:                  return VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL_KHR;
        case ImageLayout::ReadOnly:                         return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR;
        case ImageLayout::Attachment:                       return VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
        case ImageLayout::ReadOnlyAttachment:               return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR; // Vulkan 无独立只读附件布局，使用只读通用布局
        default:                                            return VK_IMAGE_LAYOUT_UNDEFINED;
        }
    }

    // ==================== 附件操作转换 ====================
    static VkAttachmentLoadOp RHI_TO_VK_AttachmentLoadOp(AttachmentLoadOp op) {
        switch (op) {
        case AttachmentLoadOp::Load:     return VK_ATTACHMENT_LOAD_OP_LOAD;
        case AttachmentLoadOp::Clear:    return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case AttachmentLoadOp::DontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        default:                         return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        }
    }

    static VkAttachmentStoreOp RHI_TO_VK_AttachmentStoreOp(AttachmentStoreOp op) {
        switch (op) {
        case AttachmentStoreOp::Store:    return VK_ATTACHMENT_STORE_OP_STORE;
        case AttachmentStoreOp::DontCare: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        case AttachmentStoreOp::None:     return VK_ATTACHMENT_STORE_OP_NONE;
        default:                          return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        }
    }

    // ==================== 管线阶段转换 ====================
    static VkPipelineStageFlags RHI_TO_VK_PipelineStageFlags(PipelineStageFlags flags) {
        VkPipelineStageFlags vkFlags = 0;
        if (flags & static_cast<PipelineStageFlags>(PipelineStage::TopOfPipe))
            vkFlags |= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        if (flags & static_cast<PipelineStageFlags>(PipelineStage::DrawIndirect))
            vkFlags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
        if (flags & static_cast<PipelineStageFlags>(PipelineStage::VertexInput))
            vkFlags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        if (flags & static_cast<PipelineStageFlags>(PipelineStage::VertexShader))
            vkFlags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        if (flags & static_cast<PipelineStageFlags>(PipelineStage::TessellationControlShader))
            vkFlags |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;
        if (flags & static_cast<PipelineStageFlags>(PipelineStage::TessellationEvaluationShader))
            vkFlags |= VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
        if (flags & static_cast<PipelineStageFlags>(PipelineStage::GeometryShader))
            vkFlags |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
        if (flags & static_cast<PipelineStageFlags>(PipelineStage::FragmentShader))
            vkFlags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        if (flags & static_cast<PipelineStageFlags>(PipelineStage::EarlyFragmentTests))
            vkFlags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        if (flags & static_cast<PipelineStageFlags>(PipelineStage::LateFragmentTests))
            vkFlags |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        if (flags & static_cast<PipelineStageFlags>(PipelineStage::ColorAttachmentOutput))
            vkFlags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        if (flags & static_cast<PipelineStageFlags>(PipelineStage::ComputeShader))
            vkFlags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        if (flags & static_cast<PipelineStageFlags>(PipelineStage::Transfer))
            vkFlags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
        if (flags & static_cast<PipelineStageFlags>(PipelineStage::BottomOfPipe))
            vkFlags |= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        if (flags & static_cast<PipelineStageFlags>(PipelineStage::Host))
            vkFlags |= VK_PIPELINE_STAGE_HOST_BIT;
        if (flags & static_cast<PipelineStageFlags>(PipelineStage::AllGraphics))
            vkFlags |= VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
        if (flags & static_cast<PipelineStageFlags>(PipelineStage::AllCommands))
            vkFlags |= VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        if (flags & static_cast<PipelineStageFlags>(PipelineStage::RayTracingShader))
            vkFlags |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
        if (flags & static_cast<PipelineStageFlags>(PipelineStage::AccelerationStructureBuild))
            vkFlags |= VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
        if (flags & static_cast<PipelineStageFlags>(PipelineStage::TaskShader))
            vkFlags |= VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT;
        if (flags & static_cast<PipelineStageFlags>(PipelineStage::MeshShader))
            vkFlags |= VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT;
        return vkFlags;
    }

    // ==================== 访问掩码转换 ====================
    static VkAccessFlags RHI_TO_VK_AccessFlags(AccessFlags flags) {
        if (flags == static_cast<AccessFlags>(AccessFlag::None))
            return 0;

        VkAccessFlags vkFlags = 0;
        if (flags & static_cast<AccessFlags>(AccessFlag::IndirectCommandRead))
            vkFlags |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        if (flags & static_cast<AccessFlags>(AccessFlag::IndexRead))
            vkFlags |= VK_ACCESS_INDEX_READ_BIT;
        if (flags & static_cast<AccessFlags>(AccessFlag::VertexAttributeRead))
            vkFlags |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        if (flags & static_cast<AccessFlags>(AccessFlag::UniformRead))
            vkFlags |= VK_ACCESS_UNIFORM_READ_BIT;
        if (flags & static_cast<AccessFlags>(AccessFlag::InputAttachmentRead))
            vkFlags |= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
        if (flags & static_cast<AccessFlags>(AccessFlag::ShaderRead))
            vkFlags |= VK_ACCESS_SHADER_READ_BIT;
        if (flags & static_cast<AccessFlags>(AccessFlag::ShaderWrite))
            vkFlags |= VK_ACCESS_SHADER_WRITE_BIT;
        if (flags & static_cast<AccessFlags>(AccessFlag::ColorAttachmentRead))
            vkFlags |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        if (flags & static_cast<AccessFlags>(AccessFlag::ColorAttachmentWrite))
            vkFlags |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        if (flags & static_cast<AccessFlags>(AccessFlag::DepthStencilAttachmentRead))
            vkFlags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        if (flags & static_cast<AccessFlags>(AccessFlag::DepthStencilAttachmentWrite))
            vkFlags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        if (flags & static_cast<AccessFlags>(AccessFlag::TransferRead))
            vkFlags |= VK_ACCESS_TRANSFER_READ_BIT;
        if (flags & static_cast<AccessFlags>(AccessFlag::TransferWrite))
            vkFlags |= VK_ACCESS_TRANSFER_WRITE_BIT;
        if (flags & static_cast<AccessFlags>(AccessFlag::HostRead))
            vkFlags |= VK_ACCESS_HOST_READ_BIT;
        if (flags & static_cast<AccessFlags>(AccessFlag::HostWrite))
            vkFlags |= VK_ACCESS_HOST_WRITE_BIT;
        if (flags & static_cast<AccessFlags>(AccessFlag::MemoryRead))
            vkFlags |= VK_ACCESS_MEMORY_READ_BIT;
        if (flags & static_cast<AccessFlags>(AccessFlag::MemoryWrite))
            vkFlags |= VK_ACCESS_MEMORY_WRITE_BIT;
        if (flags & static_cast<AccessFlags>(AccessFlag::AccelerationStructureRead))
            vkFlags |= VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        if (flags & static_cast<AccessFlags>(AccessFlag::AccelerationStructureWrite))
            vkFlags |= VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        if (flags & static_cast<AccessFlags>(AccessFlag::ShaderSampledRead))
            vkFlags |= VK_ACCESS_SHADER_READ_BIT;
        if (flags & static_cast<AccessFlags>(AccessFlag::ShaderStorageRead))
            vkFlags |= VK_ACCESS_SHADER_READ_BIT;
        if (flags & static_cast<AccessFlags>(AccessFlag::ShaderStorageWrite))
            vkFlags |= VK_ACCESS_SHADER_WRITE_BIT;
        return vkFlags;
    }

    // ==================== 新增：图像方面转换 ====================
    static VkImageAspectFlags RHI_TO_VK_ImageAspect(ImageAspect aspect) {
        switch (aspect) {
        case ImageAspect::Color:        return VK_IMAGE_ASPECT_COLOR_BIT;
        case ImageAspect::Depth:        return VK_IMAGE_ASPECT_DEPTH_BIT;
        case ImageAspect::Stencil:      return VK_IMAGE_ASPECT_STENCIL_BIT;
        case ImageAspect::DepthStencil: return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        default:                        return VK_IMAGE_ASPECT_COLOR_BIT;
        }
    }

    // ==================== 新增：图像视图类型转换 ====================
    static VkImageViewType RHI_TO_VK_ImageViewType(ImageViewType viewType, TextureType texType) {
        if (viewType != ImageViewType::Auto) {
            switch (viewType) {
            case ImageViewType::Texture1D:          return VK_IMAGE_VIEW_TYPE_1D;
            case ImageViewType::Texture1DArray:     return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
            case ImageViewType::Texture2D:          return VK_IMAGE_VIEW_TYPE_2D;
            case ImageViewType::Texture2DArray:     return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            case ImageViewType::Texture3D:          return VK_IMAGE_VIEW_TYPE_3D;
            case ImageViewType::TextureCube:        return VK_IMAGE_VIEW_TYPE_CUBE;
            case ImageViewType::TextureCubeArray:   return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
            default: break;
            }
        }
        // 根据纹理类型自动推导
        switch (texType) {
        case TextureType::Texture1D:        return VK_IMAGE_VIEW_TYPE_1D;
        case TextureType::Texture1DArray:   return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
        case TextureType::Texture2D:        return VK_IMAGE_VIEW_TYPE_2D;
        case TextureType::Texture2DArray:   return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        case TextureType::Texture3D:        return VK_IMAGE_VIEW_TYPE_3D;
        case TextureType::TextureCube:      return VK_IMAGE_VIEW_TYPE_CUBE;
        case TextureType::TextureCubeArray: return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        default:                            return VK_IMAGE_VIEW_TYPE_2D;
        }
    }
    static VkFilter RHI_TO_VK_Filter(SamplerFilter filter) {
        switch (filter) {
        case SamplerFilter::Nearest: return VK_FILTER_NEAREST;
        case SamplerFilter::Linear:  return VK_FILTER_LINEAR;
        default: return VK_FILTER_LINEAR;
        }
    }

    static VkSamplerMipmapMode RHI_TO_VK_MipmapMode(SamplerFilter filter) {
        switch (filter) {
        case SamplerFilter::Nearest: return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        case SamplerFilter::Linear:  return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        default: return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        }
    }

    static VkSamplerAddressMode RHI_TO_VK_AddressMode(SamplerAddressMode mode) {
        switch (mode) {
        case SamplerAddressMode::Repeat:            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case SamplerAddressMode::MirrorRepeat:      return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case SamplerAddressMode::ClampToEdge:       return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case SamplerAddressMode::ClampToBorder:     return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case SamplerAddressMode::MirrorClampToEdge: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
        default: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        }
    }

    static VkCompareOp RHI_TO_VK_CompareOp(CompareOp op) {
        switch (op) {
        case CompareOp::Never:          return VK_COMPARE_OP_NEVER;
        case CompareOp::Less:           return VK_COMPARE_OP_LESS;
        case CompareOp::Equal:          return VK_COMPARE_OP_EQUAL;
        case CompareOp::LessOrEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
        case CompareOp::Greater:        return VK_COMPARE_OP_GREATER;
        case CompareOp::NotEqual:       return VK_COMPARE_OP_NOT_EQUAL;
        case CompareOp::GreaterOrEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case CompareOp::Always:         return VK_COMPARE_OP_ALWAYS;
        default: return VK_COMPARE_OP_ALWAYS;
        }
    }

    static VkBorderColor RHI_TO_VK_BorderColor(SamplerBorderColor color) {
        switch (color) {
        case SamplerBorderColor::TransparentBlack: return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        case SamplerBorderColor::OpaqueBlack:      return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        case SamplerBorderColor::OpaqueWhite:      return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        case SamplerBorderColor::Custom:
        default: return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK; // 降级
        }
    }

    static VkDescriptorType RHI_TO_VK_DescriptorType(DescriptorType type) {
        switch (type) {
        case DescriptorType::Sampler:               return VK_DESCRIPTOR_TYPE_SAMPLER;
        case DescriptorType::CombinedImageSampler:  return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case DescriptorType::SampledImage:          return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case DescriptorType::StorageImage:          return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case DescriptorType::UniformTexelBuffer:    return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        case DescriptorType::StorageTexelBuffer:    return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
        case DescriptorType::UniformBuffer:         return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case DescriptorType::StorageBuffer:         return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case DescriptorType::UniformBufferDynamic:  return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        case DescriptorType::StorageBufferDynamic:  return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        case DescriptorType::InputAttachment:       return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        case DescriptorType::AccelerationStructure: return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR; // 需检查扩展
        case DescriptorType::InlineUniformBlock:    return VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT;  // 需检查扩展
        default:
            throw std::runtime_error("Unsupported DescriptorType");
        }
    }

    static VkDescriptorSetLayoutCreateFlags RHI_TO_VK_DescriptorSetLayoutFlags(bool pushDescriptors,bool updateAfterBind) {
        VkDescriptorSetLayoutCreateFlags flags = 0;
        if (pushDescriptors) {
            flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
        }
        if (updateAfterBind) {
            flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        }
        return flags;
    }

    static RHI::PipelineStage layoutToSrcStage(RHI::ImageLayout layout) {
        switch (layout) {
        case RHI::ImageLayout::ColorAttachment:
            return RHI::PipelineStage::ColorAttachmentOutput;
        case RHI::ImageLayout::DepthStencilAttachment:
            return RHI::PipelineStage::LateFragmentTests;
        case RHI::ImageLayout::ShaderReadOnly:
            return RHI::PipelineStage::FragmentShader;
        case RHI::ImageLayout::TransferSrc:
        case RHI::ImageLayout::TransferDst:
            return RHI::PipelineStage::Transfer;
        default:
            return RHI::PipelineStage::AllCommands;
        }
    }

    // 将 ImageLayout 转换为目标阶段（通常与源阶段相同，但读操作可能需不同阶段）
    static RHI::PipelineStage layoutToDstStage(RHI::ImageLayout layout) {
        switch (layout) {
        case RHI::ImageLayout::ShaderReadOnly:
            return RHI::PipelineStage::FragmentShader;
        case RHI::ImageLayout::ColorAttachment:
            return RHI::PipelineStage::ColorAttachmentOutput;
        case RHI::ImageLayout::DepthStencilAttachment:
            return RHI::PipelineStage::LateFragmentTests;
        case RHI::ImageLayout::TransferSrc:
        case RHI::ImageLayout::TransferDst:
            return RHI::PipelineStage::Transfer;
        default:
            return RHI::PipelineStage::AllCommands;
        }
    }

    // 将 ImageLayout 转换为访问掩码
    static RHI::AccessFlag layoutToAccessMask(RHI::ImageLayout layout) {
        switch (layout) {
        case RHI::ImageLayout::ColorAttachment:
            return RHI::AccessFlag::ColorAttachmentWrite;
        case RHI::ImageLayout::DepthStencilAttachment:
            return RHI::AccessFlag::DepthStencilAttachmentWrite;
        case RHI::ImageLayout::ShaderReadOnly:
            return RHI::AccessFlag::ShaderRead;
        case RHI::ImageLayout::TransferSrc:
            return RHI::AccessFlag::TransferRead;
        case RHI::ImageLayout::TransferDst:
            return RHI::AccessFlag::TransferWrite;
        default:
            return RHI::AccessFlag::None;
        }
    }

} // namespace StarryEngine::RHI::FUNC