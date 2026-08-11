add_library(renderer STATIC
    src/Renderer.cpp
    src/SceneAnalyzer.cpp
    src/backend/vulkan/Device.cpp
    src/backend/vulkan/FrameContext.cpp
    src/backend/vulkan/Instance.cpp
    src/backend/vulkan/QueueHandles.cpp
    src/backend/vulkan/Swapchain.cpp
    src/backend/vulkan/VulkanRHI.cpp
    src/graph/PassNode.cpp
    src/graph/RenderGraph.cpp
    src/graph/RenderPassBuilder.cpp
    src/graph/SubpassBuilder.cpp
    src/interface/RHI_RESOURCE_FACTORY.cpp
    src/interface/RHI_RESOURCE_MANAGER.cpp
    src/interface/RHI_UTILS.cpp
    src/interface/RHI_VK_RESOURCE.cpp
    src/passExecutor/ParticleRenderExecutor.cpp
    src/passExecutor/PresentationExecutor.cpp
    src/passes/ComputePass.cpp
    src/passes/GraphicsPass.cpp
    src/passes/ParticlePass.cpp
    src/passes/PassWrapper.cpp
    src/renderPaths/BaseRenderPath.cpp
    src/renderPaths/DeferredRenderPath.cpp
    src/renderPaths/ForwardRenderPath.cpp
    src/utils/FrameCapture.cpp
)

target_include_directories(renderer PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(renderer
PUBLIC
    assets            # 渲染器头引用 assets 类型
    core
    event
    logging
    scene
    utils
    glfw
    Vulkan::Vulkan
    glm::glm
    shaderc
    stb_image        # FrameCapture: stb_image_write.h
    vk_mem_alloc
PRIVATE
    ui                # 仅 cpp 里嵌 imgui 面板（循环，LINK_GROUP 处理）
)
