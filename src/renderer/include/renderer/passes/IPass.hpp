#pragma once
#include <renderer/graph/RenderGraph.hpp>
#include <renderer/RenderTypes.hpp>
#include <renderer/RenderBlackboard.hpp>
#include <logging/Logger.hpp>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace StarryEngine {

    class IPassExecutor;

    struct PassSubpassInfo {
        std::string tag;
        uint32_t subpassIndex = 0;
        std::shared_ptr<IPassExecutor> executor;
        RenderGraph::PassNode* passNode = nullptr;
    };

    class IPass {
    public:
        struct CompileContext {
            std::shared_ptr<RHI::ResourceManager> resMgr;
            std::shared_ptr<RHI::IRHI> rhi;
            RenderGraph::RenderGraph* renderGraph = nullptr;
            RHI::DescriptorSetHandle globalDescSet;
            RHI::DescriptorSetLayoutHandle globalSetLayout;
        };

        virtual ~IPass() = default;

        void setDataProvider(RenderBlackboard* data) { m_data = data; }

        virtual bool configure(RenderGraph::RenderGraph& graph,
                               std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap,
                               uint32_t width, uint32_t height,
                               std::shared_ptr<RHI::ResourceManager> resMgr,
                               RHI::DescriptorSetLayoutHandle globalSetLayout) = 0;

        virtual std::vector<PassSubpassInfo> getSubpasses() const { return {}; }

        virtual void onAfterCompile(const CompileContext& ) {}

        virtual void onSceneData(const AnalysisSceneResult& ,const CompileContext& ,const std::string& ) {}

        virtual const std::string& getName() const = 0;

        void addReadTextureByName(const std::string& texName) { m_readTextureNames.push_back(texName); }

    protected:
        void resolveReadTextures(RenderGraph::PassNode* node,
                                 const std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap) {
            if (!node) return;
            for (auto& name : m_readTextureNames) {
                auto tid = texIdMap.find(name);
                if (tid == texIdMap.end()) {
                    LOG_WARN("[{}] addReadTextureByName('{}') 未在 texIdMap 中找到，忽略", getName(), name);
                    continue;
                }
                node->addReadTexture(tid->second);
            }
        }

        RenderBlackboard* m_data = nullptr;   
        std::vector<std::string> m_readTextureNames;   
    };

    using PassList = std::vector<std::shared_ptr<IPass>>;

} // namespace StarryEngine
