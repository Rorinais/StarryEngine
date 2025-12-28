#pragma once
#include "RHI_ENUMS.hpp"
#include "RHI_STRUCTS_BASE.hpp"
#include "RHI_STRUCTS_RESOURCE.hpp"
#include "RHI_STRUCTS_SHADER_PIPELINE.hpp"
#include "RHI_STRUCTS_RENDER_PASS.hpp"
#include "RHI_STRUCTS_SYNC.hpp"
#include "RHI_STRUCTS_COMMAND.hpp"
#include "RHI_STRUCTS_RT.hpp"
#include "RHI_STRUCTS_CONFIG.hpp"
#include "RHI_STRUCTS_COPY_OPERATIONS.hpp"
#include "RHI_STRUCTS_OPERATIONS.hpp"
#include "RHI_STRUCTS_MISC.hpp"

namespace StarryEngine::RHI {

    // 前向声明
    class IResource;
    class RHIBuffer;
    class RHITexture;
    class RHISampler;
    class RHIShaderModule;
    class RHIPipelineLayout;
    class RHIPipeline;
    class RHIRenderPass;
    class RHIFramebuffer;
    class RHICommandBuffer;
    class RHICommandPool;
    class RHIFence;
    class RHISemaphore;
    class RHIEvent;
    class RHIQueryPool;
    class RHIAccelerationStructure;
    class RHIDescriptorSetLayout;
    class RHIDescriptorPool;
    class RHIDescriptorSet;
    class RHISwapChain;
    class RHIQueue;
    class IRHIContext;

    // 工具类
    class RHIUtils;
    class RHIFactory;
    class RHICallbackManager;
}