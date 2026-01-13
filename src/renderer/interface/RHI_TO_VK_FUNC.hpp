#pragma once
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include "RHI_TYPES.hpp"

namespace StarryEngine::RHI {


    VkPipelineLayoutCreateInfo RHI_TO_VK_PipelineLayoutDesc(RHI::PipelineLayoutDesc RHI_Desc) {

        VkPipelineLayoutCreateInfo info{};


        return info;
    }
}