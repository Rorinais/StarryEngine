#pragma once 
#include "commandPool.hpp"
#include "commandBuffer.hpp"
#include "sync/fence.hpp"
#include "sync/semaphore.hpp"
#include "../RenderGraph/RenderContext.hpp"
namespace StarryEngine {

    struct FrameContext {
        CommandBuffer::Ptr mainCommandBuffer;
        Semaphore::Ptr imageAvailableSemaphore;
        Semaphore::Ptr renderFinishedSemaphore;
        Fence::Ptr inFlightFence;
        std::unique_ptr<RenderContext> renderContext;
    };
}

