#pragma once
#include <shaderc/shaderc.hpp>
#include "RHI_ENUMS.hpp"
#include "RHI_HANDLES_SYSTEM.hpp"
#include "RHI_STRUCTS_DESC.hpp"

namespace StarryEngine::RHI::FUNC {
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
        default: return VK_FORMAT_UNDEFINED;
        }
    }

    static VmaMemoryUsage RHI_TO_VK_VmaMemoryUsage(MemoryType memoryType) {
        switch (memoryType) {
        case MemoryType::GPU_Only:
            return VMA_MEMORY_USAGE_GPU_ONLY;
        case MemoryType::CPU_To_GPU:
            return VMA_MEMORY_USAGE_CPU_TO_GPU;
        case MemoryType::CPU_Only:
            return VMA_MEMORY_USAGE_CPU_ONLY;
        case MemoryType::GPU_To_CPU:
            return VMA_MEMORY_USAGE_GPU_TO_CPU;
        default:
            return VMA_MEMORY_USAGE_AUTO;
        }
    }

    static VkMemoryPropertyFlags RHI_TO_VK_MemoryProperties(MemoryType memoryType) {
        switch (memoryType) {
        case MemoryType::GPU_Only:
            return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        case MemoryType::CPU_To_GPU:
            return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        case MemoryType::CPU_Only:
            return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        case MemoryType::GPU_To_CPU:
            return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        default:
            return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        }
    }

}

