add_library(renderer STATIC
    src/Renderer.cpp
    src/SceneAnalyzer.cpp
    src/graph/PassNode.cpp
    src/graph/RenderGraph.cpp
    src/graph/RenderPassBuilder.cpp
    src/graph/SubpassBuilder.cpp
    src/interface/RHIManager.cpp
    src/interface/vulkan/VulkanCommandEncoder.cpp
    src/interface/vulkan/VulkanDevice.cpp
    src/interface/vulkan/VulkanFactory.cpp
    src/interface/vulkan/VulkanFrameContext.cpp
    src/interface/vulkan/VulkanInstance.cpp
    src/interface/vulkan/VulkanQueueHandles.cpp
    src/interface/vulkan/VulkanRHI.cpp
    src/interface/vulkan/VulkanResources.cpp
    src/interface/vulkan/VulkanSwapchain.cpp
    src/passExecutor/ParticleRenderExecutor.cpp
    src/passExecutor/PresentationExecutor.cpp
    src/passes/ComputePass.cpp
    src/passes/GraphicsPass.cpp
    src/passes/ParticlePass.cpp
    src/passes/PassWrapper.cpp
    src/passes/ShadowPass.cpp
    src/renderPaths/BaseRenderPath.cpp
    src/renderPaths/DeferredRenderPath.cpp
    src/renderPaths/ForwardRenderPath.cpp
    src/utils/FrameCapture.cpp
)

target_include_directories(renderer PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/renderer>
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
