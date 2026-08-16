#pragma once

#include <interface/RHIEnums.hpp>
#include <interface/RHIStructs.hpp>

#include <shaderc/shaderc.hpp>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <stdexcept>
#include <string>

namespace StarryEngine::func {

    using namespace RHI; 

    static inline VkShaderStageFlagBits RHI_TO_VK_ShaderStageFlag(ShaderStage stage) {
        switch (stage) {
        case ShaderStage::Vertex:               return VK_SHADER_STAGE_VERTEX_BIT;
        case ShaderStage::Fragment:             return VK_SHADER_STAGE_FRAGMENT_BIT;
        case ShaderStage::Compute:              return VK_SHADER_STAGE_COMPUTE_BIT;
        case ShaderStage::Geometry:             return VK_SHADER_STAGE_GEOMETRY_BIT;
        case ShaderStage::TessellationControl:  return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case ShaderStage::TessellationEvaluation: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        case ShaderStage::Mesh:                 return VK_SHADER_STAGE_MESH_BIT_EXT;
        case ShaderStage::Amplification:        return VK_SHADER_STAGE_TASK_BIT_EXT;
        case ShaderStage::RayGen:               return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        case ShaderStage::AnyHit:               return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        case ShaderStage::ClosestHit:           return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        case ShaderStage::Miss:                 return VK_SHADER_STAGE_MISS_BIT_KHR;
        case ShaderStage::Intersection:         return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
        case ShaderStage::Callable:             return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
        default:
            throw std::runtime_error("Unsupported shader stage: " + std::to_string(static_cast<int>(stage)));
        }
    }

    static inline shaderc_shader_kind Utils_vk_ShadercKind(ShaderStage stage) {
        switch (stage) {
        case ShaderStage::Vertex:               return shaderc_vertex_shader;
        case ShaderStage::Fragment:             return shaderc_fragment_shader;
        case ShaderStage::Compute:              return shaderc_compute_shader;
        case ShaderStage::Geometry:             return shaderc_geometry_shader;
        case ShaderStage::TessellationControl:  return shaderc_tess_control_shader;
        case ShaderStage::TessellationEvaluation: return shaderc_tess_evaluation_shader;
        default:
            throw std::runtime_error("Unsupported shader stage for shaderc: " + std::to_string(static_cast<int>(stage)));
        }
    }

    static inline RHI::Format VK_TO_RHI_Format(VkFormat vkFormat) {
        switch (vkFormat) {
        case VK_FORMAT_R8_UNORM:            return RHI::Format::R8_UNorm;
        case VK_FORMAT_R8_SNORM:            return RHI::Format::R8_SNorm;
        case VK_FORMAT_R8_UINT:             return RHI::Format::R8_UInt;
        case VK_FORMAT_R8_SINT:             return RHI::Format::R8_SInt;
        case VK_FORMAT_R8_SRGB:             return RHI::Format::R8_sRGB;
        case VK_FORMAT_R16_UNORM:           return RHI::Format::R16_UNorm;
        case VK_FORMAT_R16_SNORM:           return RHI::Format::R16_SNorm;
        case VK_FORMAT_R16_UINT:            return RHI::Format::R16_UInt;
        case VK_FORMAT_R16_SINT:            return RHI::Format::R16_SInt;
        case VK_FORMAT_R16_SFLOAT:          return RHI::Format::R16_Float;
        case VK_FORMAT_R8G8_UNORM:          return RHI::Format::RG8_UNorm;
        case VK_FORMAT_R8G8_SNORM:          return RHI::Format::RG8_SNorm;
        case VK_FORMAT_R8G8_UINT:           return RHI::Format::RG8_UInt;
        case VK_FORMAT_R8G8_SINT:           return RHI::Format::RG8_SInt;
        case VK_FORMAT_R32_UINT:            return RHI::Format::R32_UInt;
        case VK_FORMAT_R32_SINT:            return RHI::Format::R32_SInt;
        case VK_FORMAT_R32_SFLOAT:          return RHI::Format::R32_Float;
        case VK_FORMAT_R16G16_UNORM:        return RHI::Format::RG16_UNorm;
        case VK_FORMAT_R16G16_SNORM:        return RHI::Format::RG16_SNorm;
        case VK_FORMAT_R16G16_UINT:         return RHI::Format::RG16_UInt;
        case VK_FORMAT_R16G16_SINT:         return RHI::Format::RG16_SInt;
        case VK_FORMAT_R16G16_SFLOAT:       return RHI::Format::RG16_Float;
        case VK_FORMAT_R8G8B8A8_UNORM:      return RHI::Format::RGBA8_UNorm;
        case VK_FORMAT_R8G8B8A8_SNORM:      return RHI::Format::RGBA8_SNorm;
        case VK_FORMAT_R8G8B8A8_UINT:       return RHI::Format::RGBA8_UInt;
        case VK_FORMAT_R8G8B8A8_SINT:       return RHI::Format::RGBA8_SInt;
        case VK_FORMAT_B8G8R8A8_UNORM:      return RHI::Format::BGRA8_UNorm;
        case VK_FORMAT_B8G8R8A8_SNORM:      return RHI::Format::BGRA8_SNorm;
        case VK_FORMAT_B8G8R8A8_UINT:       return RHI::Format::BGRA8_UInt;
        case VK_FORMAT_B8G8R8A8_SINT:       return RHI::Format::BGRA8_SInt;
        case VK_FORMAT_R8G8B8A8_SRGB:       return RHI::Format::RGBA8_sRGB;
        case VK_FORMAT_B8G8R8A8_SRGB:       return RHI::Format::BGRA8_sRGB;
        case VK_FORMAT_D16_UNORM:           return RHI::Format::D16_UNorm;
        case VK_FORMAT_D32_SFLOAT:          return RHI::Format::D32_Float;
        case VK_FORMAT_D24_UNORM_S8_UINT:   return RHI::Format::D24_UNorm_S8_UInt;
        case VK_FORMAT_D32_SFLOAT_S8_UINT:  return RHI::Format::D32_Float_S8_UInt;
        case VK_FORMAT_R32G32B32_SFLOAT:    return RHI::Format::RGB32_Float;
        case VK_FORMAT_R32G32_SFLOAT:       return RHI::Format::RG32_Float;
        case VK_FORMAT_R32G32B32A32_SFLOAT: return RHI::Format::RGBA32_Float;
        case VK_FORMAT_R16G16B16A16_SFLOAT: return RHI::Format::RGBA16_Float;
        default:                            return RHI::Format::Undefined;
        }
    }

    static inline VkFormat RHI_TO_VK_Format(Format format) {
        switch (format) {
        case Format::R8_UNorm:       return VK_FORMAT_R8_UNORM;
        case Format::R8_SNorm:       return VK_FORMAT_R8_SNORM;
        case Format::R8_UInt:        return VK_FORMAT_R8_UINT;
        case Format::R8_SInt:        return VK_FORMAT_R8_SINT;
        case Format::R8_sRGB:        return VK_FORMAT_R8_SRGB;
        case Format::R16_UNorm:      return VK_FORMAT_R16_UNORM;
        case Format::R16_SNorm:      return VK_FORMAT_R16_SNORM;
        case Format::R16_UInt:       return VK_FORMAT_R16_UINT;
        case Format::R16_SInt:       return VK_FORMAT_R16_SINT;
        case Format::R16_Float:      return VK_FORMAT_R16_SFLOAT;
        case Format::RG8_UNorm:      return VK_FORMAT_R8G8_UNORM;
        case Format::RG8_SNorm:      return VK_FORMAT_R8G8_SNORM;
        case Format::RG8_UInt:       return VK_FORMAT_R8G8_UINT;
        case Format::RG8_SInt:       return VK_FORMAT_R8G8_SINT;
        case Format::R32_UInt:       return VK_FORMAT_R32_UINT;
        case Format::R32_SInt:       return VK_FORMAT_R32_SINT;
        case Format::R32_Float:      return VK_FORMAT_R32_SFLOAT;
        case Format::RG16_UNorm:     return VK_FORMAT_R16G16_UNORM;
        case Format::RG16_SNorm:     return VK_FORMAT_R16G16_SNORM;
        case Format::RG16_UInt:      return VK_FORMAT_R16G16_UINT;
        case Format::RG16_SInt:      return VK_FORMAT_R16G16_SINT;
        case Format::RG16_Float:     return VK_FORMAT_R16G16_SFLOAT;
        case Format::RGBA8_UNorm:    return VK_FORMAT_R8G8B8A8_UNORM;
        case Format::RGBA8_SNorm:    return VK_FORMAT_R8G8B8A8_SNORM;
        case Format::RGBA8_UInt:     return VK_FORMAT_R8G8B8A8_UINT;
        case Format::RGBA8_SInt:     return VK_FORMAT_R8G8B8A8_SINT;
        case Format::BGRA8_UNorm:    return VK_FORMAT_B8G8R8A8_UNORM;
        case Format::BGRA8_SNorm:    return VK_FORMAT_B8G8R8A8_SNORM;
        case Format::BGRA8_UInt:     return VK_FORMAT_B8G8R8A8_UINT;
        case Format::BGRA8_SInt:     return VK_FORMAT_B8G8R8A8_SINT;
        case Format::RGBA8_sRGB:     return VK_FORMAT_R8G8B8A8_SRGB;
        case Format::BGRA8_sRGB:     return VK_FORMAT_B8G8R8A8_SRGB;
        case Format::D16_UNorm:      return VK_FORMAT_D16_UNORM;
        case Format::D32_Float:      return VK_FORMAT_D32_SFLOAT;
        case Format::D24_UNorm_S8_UInt: return VK_FORMAT_D24_UNORM_S8_UINT;
        case Format::D32_Float_S8_UInt: return VK_FORMAT_D32_SFLOAT_S8_UINT;
        case Format::RGB32_Float:    return VK_FORMAT_R32G32B32_SFLOAT;
        case Format::RG32_Float:     return VK_FORMAT_R32G32_SFLOAT;
        case Format::RGBA32_Float:   return VK_FORMAT_R32G32B32A32_SFLOAT;
        case Format::RGBA16_Float:   return VK_FORMAT_R16G16B16A16_SFLOAT;
        default:                     return VK_FORMAT_UNDEFINED;
        }
    }

    static inline VmaMemoryUsage RHI_TO_VK_VmaMemoryUsage(MemoryType memoryType) {
        switch (memoryType) {
        case MemoryType::GPU_Only:   return VMA_MEMORY_USAGE_GPU_ONLY;
        case MemoryType::CPU_To_GPU: return VMA_MEMORY_USAGE_CPU_TO_GPU;
        case MemoryType::CPU_Only:   return VMA_MEMORY_USAGE_CPU_ONLY;
        case MemoryType::GPU_To_CPU: return VMA_MEMORY_USAGE_GPU_TO_CPU;
        default:                     return VMA_MEMORY_USAGE_AUTO;
        }
    }

    static inline VkMemoryPropertyFlags RHI_TO_VK_MemoryProperties(MemoryType memoryType) {
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

    static inline RHI::ImageLayout VK_TO_RHI_ImageLayout(VkImageLayout vkLayout) {
        switch (vkLayout) {
        case VK_IMAGE_LAYOUT_UNDEFINED:                        return RHI::ImageLayout::Undefined;
        case VK_IMAGE_LAYOUT_GENERAL:                          return RHI::ImageLayout::General;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:         return RHI::ImageLayout::ColorAttachment;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return RHI::ImageLayout::DepthStencilAttachment;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:  return RHI::ImageLayout::DepthStencilReadOnly;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:         return RHI::ImageLayout::ShaderReadOnly;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:             return RHI::ImageLayout::TransferSrc;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:             return RHI::ImageLayout::TransferDst;
        case VK_IMAGE_LAYOUT_PREINITIALIZED:                   return RHI::ImageLayout::Preinitialized;
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:                  return RHI::ImageLayout::PresentSrc;
        default:                                               return RHI::ImageLayout::Undefined;
        }
    }

    static inline VkImageLayout RHI_TO_VK_ImageLayout(ImageLayout layout) {
        switch (layout) {
        case ImageLayout::Undefined:                      return VK_IMAGE_LAYOUT_UNDEFINED;
        case ImageLayout::General:                        return VK_IMAGE_LAYOUT_GENERAL;
        case ImageLayout::ColorAttachment:                return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case ImageLayout::DepthStencilAttachment:         return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        case ImageLayout::DepthStencilReadOnly:           return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        case ImageLayout::ShaderReadOnly:                 return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        case ImageLayout::TransferSrc:                    return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        case ImageLayout::TransferDst:                    return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        case ImageLayout::Preinitialized:                 return VK_IMAGE_LAYOUT_PREINITIALIZED;
        case ImageLayout::PresentSrc:                     return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        case ImageLayout::DepthReadOnlyStencilAttachment: return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL_KHR;
        case ImageLayout::DepthAttachmentStencilReadOnly: return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR;
        case ImageLayout::DepthReadOnly:                  return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL_KHR;
        case ImageLayout::StencilReadOnly:                return VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL_KHR;
        case ImageLayout::ReadOnly:                       return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR;
        case ImageLayout::Attachment:                     return VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
        case ImageLayout::ReadOnlyAttachment:             return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR;
        default:                                          return VK_IMAGE_LAYOUT_UNDEFINED;
        }
    }

    static inline VkAttachmentLoadOp RHI_TO_VK_AttachmentLoadOp(AttachmentLoadOp op) {
        switch (op) {
        case AttachmentLoadOp::Load:     return VK_ATTACHMENT_LOAD_OP_LOAD;
        case AttachmentLoadOp::Clear:    return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case AttachmentLoadOp::DontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        default:                         return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        }
    }

    static inline VkAttachmentStoreOp RHI_TO_VK_AttachmentStoreOp(AttachmentStoreOp op) {
        switch (op) {
        case AttachmentStoreOp::Store:    return VK_ATTACHMENT_STORE_OP_STORE;
        case AttachmentStoreOp::DontCare: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        case AttachmentStoreOp::None:     return VK_ATTACHMENT_STORE_OP_NONE;
        default:                          return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        }
    }

    static inline VkPipelineStageFlags RHI_TO_VK_PipelineStageFlags(PipelineStage flags) {
        VkPipelineStageFlags vkFlags = 0;
        if (hasFlag(flags, PipelineStage::TopOfPipe))                    vkFlags |= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        if (hasFlag(flags, PipelineStage::DrawIndirect))                 vkFlags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
        if (hasFlag(flags, PipelineStage::VertexInput))                  vkFlags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        if (hasFlag(flags, PipelineStage::VertexShader))                 vkFlags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        if (hasFlag(flags, PipelineStage::TessellationControlShader))    vkFlags |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;
        if (hasFlag(flags, PipelineStage::TessellationEvaluationShader)) vkFlags |= VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
        if (hasFlag(flags, PipelineStage::GeometryShader))               vkFlags |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
        if (hasFlag(flags, PipelineStage::FragmentShader))               vkFlags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        if (hasFlag(flags, PipelineStage::EarlyFragmentTests))           vkFlags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        if (hasFlag(flags, PipelineStage::LateFragmentTests))            vkFlags |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        if (hasFlag(flags, PipelineStage::ColorAttachmentOutput))        vkFlags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        if (hasFlag(flags, PipelineStage::ComputeShader))                vkFlags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        if (hasFlag(flags, PipelineStage::Transfer))                     vkFlags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
        if (hasFlag(flags, PipelineStage::BottomOfPipe))                 vkFlags |= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        if (hasFlag(flags, PipelineStage::Host))                         vkFlags |= VK_PIPELINE_STAGE_HOST_BIT;
        if (hasFlag(flags, PipelineStage::AllGraphics))                  vkFlags |= VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
        if (hasFlag(flags, PipelineStage::AllCommands))                  vkFlags |= VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        if (hasFlag(flags, PipelineStage::RayTracingShader))             vkFlags |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
        if (hasFlag(flags, PipelineStage::AccelerationStructureBuild))   vkFlags |= VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
        if (hasFlag(flags, PipelineStage::TaskShader))                   vkFlags |= VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT;
        if (hasFlag(flags, PipelineStage::MeshShader))                   vkFlags |= VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT;
        return vkFlags;
    }

    static inline VkAccessFlags RHI_TO_VK_AccessFlags(AccessFlag flags) {
        if (flags == AccessFlag::None)
            return 0;

        VkAccessFlags vkFlags = 0;
        if (hasFlag(flags, AccessFlag::IndirectCommandRead))           vkFlags |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        if (hasFlag(flags, AccessFlag::IndexRead))                     vkFlags |= VK_ACCESS_INDEX_READ_BIT;
        if (hasFlag(flags, AccessFlag::VertexAttributeRead))           vkFlags |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        if (hasFlag(flags, AccessFlag::UniformRead))                   vkFlags |= VK_ACCESS_UNIFORM_READ_BIT;
        if (hasFlag(flags, AccessFlag::InputAttachmentRead))           vkFlags |= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
        if (hasFlag(flags, AccessFlag::ShaderRead))                    vkFlags |= VK_ACCESS_SHADER_READ_BIT;
        if (hasFlag(flags, AccessFlag::ShaderWrite))                   vkFlags |= VK_ACCESS_SHADER_WRITE_BIT;
        if (hasFlag(flags, AccessFlag::ColorAttachmentRead))           vkFlags |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        if (hasFlag(flags, AccessFlag::ColorAttachmentWrite))          vkFlags |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        if (hasFlag(flags, AccessFlag::DepthStencilAttachmentRead))    vkFlags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        if (hasFlag(flags, AccessFlag::DepthStencilAttachmentWrite))   vkFlags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        if (hasFlag(flags, AccessFlag::TransferRead))                  vkFlags |= VK_ACCESS_TRANSFER_READ_BIT;
        if (hasFlag(flags, AccessFlag::TransferWrite))                 vkFlags |= VK_ACCESS_TRANSFER_WRITE_BIT;
        if (hasFlag(flags, AccessFlag::HostRead))                      vkFlags |= VK_ACCESS_HOST_READ_BIT;
        if (hasFlag(flags, AccessFlag::HostWrite))                     vkFlags |= VK_ACCESS_HOST_WRITE_BIT;
        if (hasFlag(flags, AccessFlag::MemoryRead))                    vkFlags |= VK_ACCESS_MEMORY_READ_BIT;
        if (hasFlag(flags, AccessFlag::MemoryWrite))                   vkFlags |= VK_ACCESS_MEMORY_WRITE_BIT;
        if (hasFlag(flags, AccessFlag::AccelerationStructureRead))     vkFlags |= VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        if (hasFlag(flags, AccessFlag::AccelerationStructureWrite))    vkFlags |= VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        if (hasFlag(flags, AccessFlag::ShaderSampledRead))             vkFlags |= VK_ACCESS_SHADER_READ_BIT;
        if (hasFlag(flags, AccessFlag::ShaderStorageRead))             vkFlags |= VK_ACCESS_SHADER_READ_BIT;
        if (hasFlag(flags, AccessFlag::ShaderStorageWrite))            vkFlags |= VK_ACCESS_SHADER_WRITE_BIT;
        return vkFlags;
    }

    static inline VkImageAspectFlags RHI_TO_VK_ImageAspect(ImageAspect aspect) {
        switch (aspect) {
        case ImageAspect::Color:        return VK_IMAGE_ASPECT_COLOR_BIT;
        case ImageAspect::Depth:        return VK_IMAGE_ASPECT_DEPTH_BIT;
        case ImageAspect::Stencil:      return VK_IMAGE_ASPECT_STENCIL_BIT;
        case ImageAspect::DepthStencil: return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        default:                        return VK_IMAGE_ASPECT_COLOR_BIT;
        }
    }

    static inline VkImageViewType RHI_TO_VK_ImageViewType(ImageViewType viewType, TextureType texType) {
        if (viewType != ImageViewType::Auto) {
            switch (viewType) {
            case ImageViewType::Texture1D:        return VK_IMAGE_VIEW_TYPE_1D;
            case ImageViewType::Texture1DArray:   return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
            case ImageViewType::Texture2D:        return VK_IMAGE_VIEW_TYPE_2D;
            case ImageViewType::Texture2DArray:   return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            case ImageViewType::Texture3D:        return VK_IMAGE_VIEW_TYPE_3D;
            case ImageViewType::TextureCube:      return VK_IMAGE_VIEW_TYPE_CUBE;
            case ImageViewType::TextureCubeArray: return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
            default: break;
            }
        }
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

    static inline VkFilter RHI_TO_VK_Filter(SamplerFilter filter) {
        switch (filter) {
        case SamplerFilter::Nearest: return VK_FILTER_NEAREST;
        case SamplerFilter::Linear:  return VK_FILTER_LINEAR;
        default:                     return VK_FILTER_LINEAR;
        }
    }

    static inline VkSamplerMipmapMode RHI_TO_VK_MipmapMode(SamplerFilter filter) {
        switch (filter) {
        case SamplerFilter::Nearest: return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        case SamplerFilter::Linear:  return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        default:                     return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        }
    }

    static inline VkSamplerAddressMode RHI_TO_VK_AddressMode(SamplerAddressMode mode) {
        switch (mode) {
        case SamplerAddressMode::Repeat:            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case SamplerAddressMode::MirrorRepeat:      return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case SamplerAddressMode::ClampToEdge:       return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case SamplerAddressMode::ClampToBorder:     return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case SamplerAddressMode::MirrorClampToEdge: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
        default:                                    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        }
    }

    static inline VkCompareOp RHI_TO_VK_CompareOp(CompareOp op) {
        switch (op) {
        case CompareOp::Never:          return VK_COMPARE_OP_NEVER;
        case CompareOp::Less:           return VK_COMPARE_OP_LESS;
        case CompareOp::Equal:          return VK_COMPARE_OP_EQUAL;
        case CompareOp::LessOrEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
        case CompareOp::Greater:        return VK_COMPARE_OP_GREATER;
        case CompareOp::NotEqual:       return VK_COMPARE_OP_NOT_EQUAL;
        case CompareOp::GreaterOrEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case CompareOp::Always:         return VK_COMPARE_OP_ALWAYS;
        default:                        return VK_COMPARE_OP_ALWAYS;
        }
    }

    static inline VkBorderColor RHI_TO_VK_BorderColor(SamplerBorderColor color) {
        switch (color) {
        case SamplerBorderColor::TransparentBlack: return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        case SamplerBorderColor::OpaqueBlack:      return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        case SamplerBorderColor::OpaqueWhite:      return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        case SamplerBorderColor::Custom:
        default:                                   return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        }
    }


    static inline VkDescriptorType RHI_TO_VK_DescriptorType(DescriptorType type) {
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
        case DescriptorType::AccelerationStructure: return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        case DescriptorType::InlineUniformBlock:    return VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT;
        default:
            throw std::runtime_error("Unsupported DescriptorType");
        }
    }

    static inline VkDescriptorSetLayoutCreateFlags RHI_TO_VK_DescriptorSetLayoutFlags(bool pushDescriptors, bool updateAfterBind) {
        VkDescriptorSetLayoutCreateFlags flags = 0;
        if (pushDescriptors) {
            flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
        }
        if (updateAfterBind) {
            flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        }
        return flags;
    }

    static inline VkImageCreateFlags RHI_TO_VK_ImageCreateFlags(ImageCreateFlags flags) {
        VkImageCreateFlags vkFlags = 0;
        if (hasFlag(flags, ImageCreateFlags::CubeCompatible))  vkFlags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        if (hasFlag(flags, ImageCreateFlags::MutableFormat))   vkFlags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
        if (hasFlag(flags, ImageCreateFlags::Protected))       vkFlags |= VK_IMAGE_CREATE_PROTECTED_BIT;
        return vkFlags;
    }

    static inline RHI::PipelineStage Utils_vk_LayoutToSrcStage(RHI::ImageLayout layout) {
        switch (layout) {
        case RHI::ImageLayout::ColorAttachment:        return RHI::PipelineStage::ColorAttachmentOutput;
        case RHI::ImageLayout::DepthStencilAttachment: return RHI::PipelineStage::LateFragmentTests;
        case RHI::ImageLayout::ShaderReadOnly:         return RHI::PipelineStage::FragmentShader;
        case RHI::ImageLayout::TransferSrc:
        case RHI::ImageLayout::TransferDst:            return RHI::PipelineStage::Transfer;
        default:                                       return RHI::PipelineStage::AllCommands;
        }
    }

    static inline RHI::PipelineStage Utils_vk_LayoutToDstStage(RHI::ImageLayout layout) {
        switch (layout) {
        case RHI::ImageLayout::ShaderReadOnly:         return RHI::PipelineStage::FragmentShader;
        case RHI::ImageLayout::ColorAttachment:        return RHI::PipelineStage::ColorAttachmentOutput;
        case RHI::ImageLayout::DepthStencilAttachment: return RHI::PipelineStage::LateFragmentTests;
        case RHI::ImageLayout::TransferSrc:
        case RHI::ImageLayout::TransferDst:            return RHI::PipelineStage::Transfer;
        default:                                       return RHI::PipelineStage::AllCommands;
        }
    }


    static inline RHI::AccessFlag Utils_vk_LayoutToAccessMask(RHI::ImageLayout layout) {
        switch (layout) {
        case RHI::ImageLayout::ColorAttachment:        return RHI::AccessFlag::ColorAttachmentWrite;
        case RHI::ImageLayout::DepthStencilAttachment: return RHI::AccessFlag::DepthStencilAttachmentWrite;
        case RHI::ImageLayout::ShaderReadOnly:         return RHI::AccessFlag::ShaderRead;
        case RHI::ImageLayout::TransferSrc:            return RHI::AccessFlag::TransferRead;
        case RHI::ImageLayout::TransferDst:            return RHI::AccessFlag::TransferWrite;
        default:                                       return RHI::AccessFlag::None;
        }
    }


    static inline VkPrimitiveTopology RHI_TO_VK_PrimitiveTopology(PrimitiveTopology topology) {
        switch (topology) {
        case PrimitiveTopology::PointList:                   return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case PrimitiveTopology::LineList:                    return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case PrimitiveTopology::LineStrip:                   return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case PrimitiveTopology::TriangleList:                return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case PrimitiveTopology::TriangleStrip:               return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case PrimitiveTopology::TriangleFan:                 return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
        case PrimitiveTopology::LineListWithAdjacency:       return VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
        case PrimitiveTopology::LineStripWithAdjacency:      return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY;
        case PrimitiveTopology::TriangleListWithAdjacency:   return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
        case PrimitiveTopology::TriangleStripWithAdjacency:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;
        default:                                             return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        }
    }

    static inline VkCullModeFlags RHI_TO_VK_CullMode(CullMode mode) {
        switch (mode) {
        case CullMode::None:  return VK_CULL_MODE_NONE;
        case CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
        case CullMode::Back:  return VK_CULL_MODE_BACK_BIT;
        default:              return VK_CULL_MODE_BACK_BIT;
        }
    }

    static inline VkPolygonMode RHI_TO_VK_PolygonMode(PolygonMode mode) {
        switch (mode) {
        case PolygonMode::Fill: return VK_POLYGON_MODE_FILL;
        case PolygonMode::Line: return VK_POLYGON_MODE_LINE;
        default:                return VK_POLYGON_MODE_FILL;
        }
    }

    static inline VkFrontFace RHI_TO_VK_FrontFace(FrontFace face) {
        switch (face) {
        case FrontFace::Clockwise:        return VK_FRONT_FACE_CLOCKWISE;
        case FrontFace::CounterClockwise: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
        default:                          return VK_FRONT_FACE_COUNTER_CLOCKWISE;
        }
    }

    static inline VkBlendFactor RHI_TO_VK_BlendFactor(BlendFactor factor) {
        switch (factor) {
        case BlendFactor::Zero:                  return VK_BLEND_FACTOR_ZERO;
        case BlendFactor::One:                   return VK_BLEND_FACTOR_ONE;
        case BlendFactor::SrcColor:              return VK_BLEND_FACTOR_SRC_COLOR;
        case BlendFactor::OneMinusSrcColor:      return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case BlendFactor::DstColor:              return VK_BLEND_FACTOR_DST_COLOR;
        case BlendFactor::OneMinusDstColor:      return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case BlendFactor::SrcAlpha:              return VK_BLEND_FACTOR_SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha:      return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case BlendFactor::DstAlpha:              return VK_BLEND_FACTOR_DST_ALPHA;
        case BlendFactor::OneMinusDstAlpha:      return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case BlendFactor::ConstantColor:         return VK_BLEND_FACTOR_CONSTANT_COLOR;
        case BlendFactor::OneMinusConstantColor: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        case BlendFactor::ConstantAlpha:         return VK_BLEND_FACTOR_CONSTANT_ALPHA;
        case BlendFactor::OneMinusConstantAlpha: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
        case BlendFactor::SrcAlphaSaturate:      return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
        default:                                 return VK_BLEND_FACTOR_ONE;
        }
    }

    static inline VkBlendOp RHI_TO_VK_BlendOp(BlendOp op) {
        switch (op) {
        case BlendOp::Add:             return VK_BLEND_OP_ADD;
        case BlendOp::Subtract:        return VK_BLEND_OP_SUBTRACT;
        case BlendOp::ReverseSubtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
        case BlendOp::Min:             return VK_BLEND_OP_MIN;
        case BlendOp::Max:             return VK_BLEND_OP_MAX;
        default:                       return VK_BLEND_OP_ADD;
        }
    }

    static inline VkStencilOp RHI_TO_VK_StencilOp(StencilOp op) {
        switch (op) {
        case StencilOp::Keep:              return VK_STENCIL_OP_KEEP;
        case StencilOp::Zero:              return VK_STENCIL_OP_ZERO;
        case StencilOp::Replace:           return VK_STENCIL_OP_REPLACE;
        case StencilOp::IncrementAndClamp: return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case StencilOp::DecrementAndClamp: return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case StencilOp::Invert:            return VK_STENCIL_OP_INVERT;
        case StencilOp::IncrementAndWrap:  return VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case StencilOp::DecrementAndWrap:  return VK_STENCIL_OP_DECREMENT_AND_WRAP;
        default:                           return VK_STENCIL_OP_KEEP;
        }
    }

    static inline VkLogicOp RHI_TO_VK_LogicOp(LogicOp op) {
        switch (op) {
        case LogicOp::Clear:         return VK_LOGIC_OP_CLEAR;
        case LogicOp::And:           return VK_LOGIC_OP_AND;
        case LogicOp::AndReverse:    return VK_LOGIC_OP_AND_REVERSE;
        case LogicOp::Copy:          return VK_LOGIC_OP_COPY;
        case LogicOp::AndInverted:   return VK_LOGIC_OP_AND_INVERTED;
        case LogicOp::NoOp:          return VK_LOGIC_OP_NO_OP;
        case LogicOp::Xor:           return VK_LOGIC_OP_XOR;
        case LogicOp::Or:            return VK_LOGIC_OP_OR;
        case LogicOp::Nor:           return VK_LOGIC_OP_NOR;
        case LogicOp::Equivalent:    return VK_LOGIC_OP_EQUIVALENT;
        case LogicOp::Invert:        return VK_LOGIC_OP_INVERT;
        case LogicOp::OrReverse:     return VK_LOGIC_OP_OR_REVERSE;
        case LogicOp::CopyInverted:  return VK_LOGIC_OP_COPY_INVERTED;
        case LogicOp::OrInverted:    return VK_LOGIC_OP_OR_INVERTED;
        case LogicOp::Nand:          return VK_LOGIC_OP_NAND;
        case LogicOp::Set:           return VK_LOGIC_OP_SET;
        default:                     return VK_LOGIC_OP_COPY;
        }
    }

    static inline VkIndexType RHI_TO_VK_IndexType(IndexType type) {
        switch (type) {
        case IndexType::UInt16: return VK_INDEX_TYPE_UINT16;
        case IndexType::UInt32: return VK_INDEX_TYPE_UINT32;
        default:                return VK_INDEX_TYPE_UINT32;
        }
    }

    static inline VkVertexInputRate RHI_TO_VK_VertexInputRate(VertexInputRate rate) {
        switch (rate) {
        case VertexInputRate::PerVertex:   return VK_VERTEX_INPUT_RATE_VERTEX;
        case VertexInputRate::PerInstance: return VK_VERTEX_INPUT_RATE_INSTANCE;
        default:                           return VK_VERTEX_INPUT_RATE_VERTEX;
        }
    }

    static inline VkPipelineBindPoint RHI_TO_VK_PipelineBindPoint(PipelineBindPoint bindPoint) {
        switch (bindPoint) {
        case PipelineBindPoint::Graphics: return VK_PIPELINE_BIND_POINT_GRAPHICS;
        case PipelineBindPoint::Compute:  return VK_PIPELINE_BIND_POINT_COMPUTE;
        default:                          return VK_PIPELINE_BIND_POINT_GRAPHICS;
        }
    }

    static inline VkDynamicState RHI_TO_VK_DynamicState(DynamicState state) {
        switch (state) {
        case DynamicState::Viewport:                return VK_DYNAMIC_STATE_VIEWPORT;
        case DynamicState::Scissor:                 return VK_DYNAMIC_STATE_SCISSOR;
        case DynamicState::LineWidth:               return VK_DYNAMIC_STATE_LINE_WIDTH;
        case DynamicState::DepthBias:               return VK_DYNAMIC_STATE_DEPTH_BIAS;
        case DynamicState::BlendConstants:          return VK_DYNAMIC_STATE_BLEND_CONSTANTS;
        case DynamicState::DepthBounds:             return VK_DYNAMIC_STATE_DEPTH_BOUNDS;
        case DynamicState::StencilCompareMask:      return VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK;
        case DynamicState::StencilWriteMask:        return VK_DYNAMIC_STATE_STENCIL_WRITE_MASK;
        case DynamicState::StencilReference:        return VK_DYNAMIC_STATE_STENCIL_REFERENCE;
        case DynamicState::VertexInputBindingStride: return VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE;
        case DynamicState::PrimitiveTopology:       return VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY;
        case DynamicState::CullMode:                return VK_DYNAMIC_STATE_CULL_MODE;
        case DynamicState::FrontFace:               return VK_DYNAMIC_STATE_FRONT_FACE;
        default:                                    return VK_DYNAMIC_STATE_VIEWPORT;
        }
    }

    static inline VkDependencyFlags RHI_TO_VK_DependencyFlags(DependencyFlags flags) {
        VkDependencyFlags vkFlags = 0;
        if (hasFlag(flags, DependencyFlags::ByRegion))    vkFlags |= VK_DEPENDENCY_BY_REGION_BIT;
        if (hasFlag(flags, DependencyFlags::DeviceGroup)) vkFlags |= VK_DEPENDENCY_DEVICE_GROUP_BIT;
        if (hasFlag(flags, DependencyFlags::ViewLocal))   vkFlags |= VK_DEPENDENCY_VIEW_LOCAL_BIT;
        if (hasFlag(flags, DependencyFlags::ViewGlobal))  vkFlags |= VK_DEPENDENCY_VIEW_LOCAL_BIT; // VK 无独立 ViewGlobal，与 ViewLocal 同 bit
        return vkFlags;
    }

    static inline VkQueryType RHI_TO_VK_QueryType(QueryType type) {
        switch (type) {
        case QueryType::Occlusion:                          return VK_QUERY_TYPE_OCCLUSION;
        case QueryType::PipelineStatistics:                 return VK_QUERY_TYPE_PIPELINE_STATISTICS;
        case QueryType::Timestamp:                          return VK_QUERY_TYPE_TIMESTAMP;
        case QueryType::Performance:                        return VK_QUERY_TYPE_PERFORMANCE_QUERY_KHR;
        case QueryType::AccelerationStructureCompactedSize: return VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
        default:                                            return VK_QUERY_TYPE_OCCLUSION;
        }
    }

    static inline VkQueryControlFlags RHI_TO_VK_QueryControlFlags(QueryControlFlags flags) {
        VkQueryControlFlags vkFlags = 0;
        if (hasFlag(flags, QueryControlFlags::Precise)) vkFlags |= VK_QUERY_CONTROL_PRECISE_BIT;
        return vkFlags;
    }

    static inline VkQueryResultFlags RHI_TO_VK_QueryResultFlags(QueryResultFlags flags) {
        VkQueryResultFlags vkFlags = 0;
        if (hasFlag(flags, QueryResultFlags::_64Bit))          vkFlags |= VK_QUERY_RESULT_64_BIT;
        if (hasFlag(flags, QueryResultFlags::Wait))            vkFlags |= VK_QUERY_RESULT_WAIT_BIT;
        if (hasFlag(flags, QueryResultFlags::WithAvailability)) vkFlags |= VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
        if (hasFlag(flags, QueryResultFlags::Partial))         vkFlags |= VK_QUERY_RESULT_PARTIAL_BIT;
        return vkFlags;
    }

    static inline RHI::QueueType Utils_vk_QueueFamilyToType(uint32_t queueFamilyIndex, uint32_t graphicsFamily, uint32_t computeFamily, uint32_t transferFamily) {
        if (queueFamilyIndex == graphicsFamily) return RHI::QueueType::Graphics;
        if (queueFamilyIndex == computeFamily)  return RHI::QueueType::Compute;
        if (queueFamilyIndex == transferFamily) return RHI::QueueType::Transfer;
        return RHI::QueueType::Graphics;
    }

    static inline VkCommandBufferLevel RHI_TO_VK_CommandBufferLevel(CommandBufferLevel level) {
        switch (level) {
        case CommandBufferLevel::Primary:   return VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        case CommandBufferLevel::Secondary: return VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        default:                            return VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        }
    }

    static inline VkColorSpaceKHR RHI_TO_VK_ColorSpace(ColorSpace colorSpace) {
        switch (colorSpace) {
        case ColorSpace::SRGBNonlinear:     return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        case ColorSpace::ExtendedSRGBLinear: return VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
        case ColorSpace::HDR10_ST2084:      return VK_COLOR_SPACE_HDR10_ST2084_EXT;
        case ColorSpace::HDR10_HLG:         return VK_COLOR_SPACE_HDR10_HLG_EXT;
        case ColorSpace::DCI_P3:            return VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT;
        case ColorSpace::DisplayP3:         return VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT;
        case ColorSpace::AdobeRGB:          return VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT;
        case ColorSpace::BT2020:            return VK_COLOR_SPACE_BT2020_LINEAR_EXT;
        default:                            return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        }
    }

    static inline VkFormatFeatureFlags RHI_TO_VK_FormatFeatureFlags(FormatFeatureFlags flags) {
        VkFormatFeatureFlags vkFlags = 0;
        if (hasFlag(flags, FormatFeatureFlags::SampledImage))            vkFlags |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        if (hasFlag(flags, FormatFeatureFlags::StorageImage))            vkFlags |= VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
        if (hasFlag(flags, FormatFeatureFlags::StorageImageAtomic))      vkFlags |= VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT;
        if (hasFlag(flags, FormatFeatureFlags::UniformTexelBuffer))      vkFlags |= VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT;
        if (hasFlag(flags, FormatFeatureFlags::StorageTexelBuffer))      vkFlags |= VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT;
        if (hasFlag(flags, FormatFeatureFlags::VertexBuffer))            vkFlags |= VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT;
        if (hasFlag(flags, FormatFeatureFlags::ColorAttachment))         vkFlags |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
        if (hasFlag(flags, FormatFeatureFlags::ColorAttachmentBlend))    vkFlags |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
        if (hasFlag(flags, FormatFeatureFlags::DepthStencilAttachment))  vkFlags |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
        if (hasFlag(flags, FormatFeatureFlags::BlitSrc))                 vkFlags |= VK_FORMAT_FEATURE_BLIT_SRC_BIT;
        if (hasFlag(flags, FormatFeatureFlags::BlitDst))                 vkFlags |= VK_FORMAT_FEATURE_BLIT_DST_BIT;
        if (hasFlag(flags, FormatFeatureFlags::SampledImageFilterLinear)) vkFlags |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
        if (hasFlag(flags, FormatFeatureFlags::TransferSrc))             vkFlags |= VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
        if (hasFlag(flags, FormatFeatureFlags::TransferDst))             vkFlags |= VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
        return vkFlags;
    }

    static inline VkShaderStageFlags RHI_TO_VK_ShaderStageFlags(ShaderStageFlags stageFlags) {
        VkShaderStageFlags vkFlags = 0;
        if (stageFlags.Has(ShaderStage::Vertex))                vkFlags |= VK_SHADER_STAGE_VERTEX_BIT;
        if (stageFlags.Has(ShaderStage::Fragment))              vkFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
        if (stageFlags.Has(ShaderStage::Compute))               vkFlags |= VK_SHADER_STAGE_COMPUTE_BIT;
        if (stageFlags.Has(ShaderStage::Geometry))              vkFlags |= VK_SHADER_STAGE_GEOMETRY_BIT;
        if (stageFlags.Has(ShaderStage::TessellationControl))   vkFlags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        if (stageFlags.Has(ShaderStage::TessellationEvaluation)) vkFlags |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        if (stageFlags.Has(ShaderStage::Mesh))                  vkFlags |= VK_SHADER_STAGE_MESH_BIT_EXT;
        if (stageFlags.Has(ShaderStage::Amplification))         vkFlags |= VK_SHADER_STAGE_TASK_BIT_EXT;
        if (stageFlags.Has(ShaderStage::RayGen))                vkFlags |= VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        if (stageFlags.Has(ShaderStage::AnyHit))                vkFlags |= VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        if (stageFlags.Has(ShaderStage::ClosestHit))            vkFlags |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        if (stageFlags.Has(ShaderStage::Miss))                  vkFlags |= VK_SHADER_STAGE_MISS_BIT_KHR;
        if (stageFlags.Has(ShaderStage::Intersection))          vkFlags |= VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
        if (stageFlags.Has(ShaderStage::Callable))              vkFlags |= VK_SHADER_STAGE_CALLABLE_BIT_KHR;
        return vkFlags;
    }

} // namespace StarryEngine::func
