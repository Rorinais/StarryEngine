#pragma once
#include "../VulkanRenderer.hpp"
#include "backends/SimpleVulkanBackend.hpp"
#include "RenderGraph/DecoupledRenderGraph.hpp"
#include "UnifiedResourceManager.hpp"
#include <memory>

namespace StarryEngine {

    class RenderSystemFactory {
    public:
        static std::unique_ptr<VulkanRenderer> createDefaultRenderer(
            VulkanCore::Ptr core,
            WindowContext::Ptr window
        ) {
            // 创建后端
            auto backend = std::make_unique<SimpleVulkanBackend>();
            if (!backend->initialize(core, window)) {
                return nullptr;
            }

            // 创建资源管理器
            auto resourceManager = std::make_unique<UnifiedResourceManager>();
            if (!resourceManager->initialize(core)) {
                return nullptr;
            }

            // 创建渲染图
            auto renderGraph = std::make_unique<DecoupledRenderGraph>();
            if (!renderGraph->initialize(core->getLogicalDeviceHandle(), core->getAllocator())) {
                return nullptr;
            }

            // 创建渲染器并组装
            auto renderer = std::make_unique<VulkanRenderer>();
            renderer->init(
                std::move(backend),
                std::move(renderGraph),
                std::move(resourceManager)
            );

            return renderer;
        }

        // 创建自定义渲染器
        template<typename BackendT, typename RenderGraphT, typename ResourceManagerT>
        static std::unique_ptr<VulkanRenderer> createCustomRenderer(
            VulkanCore::Ptr core,
            WindowContext::Ptr window
        ) {
            auto backend = std::make_unique<BackendT>();
            if (!backend->initialize(core, window)) {
                return nullptr;
            }

            auto resourceManager = std::make_unique<ResourceManagerT>();
            if (!resourceManager->initialize(core)) {
                return nullptr;
            }

            auto renderGraph = std::make_unique<RenderGraphT>();
            if (!renderGraph->initialize(core->getLogicalDeviceHandle(), core->getAllocator())) {
                return nullptr;
            }

            auto renderer = std::make_unique<VulkanRenderer>();
            renderer->init(
                std::move(backend),
                std::move(renderGraph),
                std::move(resourceManager)
            );

            return renderer;
        }
    };

} // namespace StarryEngine