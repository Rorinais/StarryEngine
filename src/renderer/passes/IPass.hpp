#pragma once
#include "../graph/RenderGraph.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace StarryEngine {

    class IPassExecutor;

    // PassSubpassInfo — 由 IPass::getSubpasses() 返回，用于注册 subpass 到 RenderPath
    struct PassSubpassInfo {
        std::string tag;
        uint32_t subpassIndex = 0;
        std::shared_ptr<IPassExecutor> executor;
        RenderGraph::PassNode* passNode = nullptr;
    };

    // IPass — 渲染 Pass 抽象基类
    // 每个 Pass 子类负责在 configure() 中声明自己的资源读写和附件
    class IPass {
    public:
        virtual ~IPass() = default;

        // 在 RenderGraph 上构建 PassNode 和 Subpass
        // 返回 false 表示配置失败，此 pass 将被跳过
        virtual bool configure(RenderGraph::RenderGraph& graph,
                               std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap,
                               uint32_t width, uint32_t height,
                               std::shared_ptr<RHI::ResourceManager> resMgr,
                               RHI::DescriptorSetLayoutHandle globalSetLayout) = 0;

        // 返回此 Pass 贡献的 subpass 信息（configure() 之后调用）
        virtual std::vector<PassSubpassInfo> getSubpasses() const { return {}; }

        // onAfterCompile 上下文（RenderPass handle 等资源在编译后才有效）
        struct CompileContext {
            std::shared_ptr<RHI::ResourceManager> resMgr;
            std::shared_ptr<RHI::IRHI> rhi;
            RenderGraph::RenderGraph* renderGraph = nullptr;
            RHI::DescriptorSetHandle globalDescSet;
            RHI::DescriptorSetLayoutHandle globalSetLayout;
        };

        // 在 RenderGraph 编译之后调用，此时 render pass handle 已有效，
        // 可创建 graphics pipeline、descriptor set 等依赖 render pass 的资源
        virtual void onAfterCompile(const CompileContext& /*ctx*/) {}

        // Pass 名称（用于 RenderGraph 中的 PassNode 命名）
        virtual const std::string& getName() const = 0;
    };

    using PassList = std::vector<std::shared_ptr<IPass>>;

} // namespace StarryEngine
