#pragma once

#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include "../utils/Hash.hpp"
#include "interface/RHI_TYPES.hpp"

namespace StarryEngine::Scene {
    struct RenderObject;   
}

namespace StarryEngine::Assets {
    class MaterialInstance;   
}

namespace StarryEngine {

    enum class DrawItemType {
        Mesh,        // 普通网格
        Procedural   // 无网格，直接绘制顶点
    };

    // 单次 draw call 的渲染数据。由 SceneAnalyzer 从 RenderObject 生成。
    struct DrawItem {
        DrawItemType type = DrawItemType::Mesh;

        std::string passTag;
        std::unordered_map<uint32_t, RHI::DescriptorSetHandle> descriptorSets;
        uint32_t pipelineIndex = 0;
        std::weak_ptr<Scene::RenderObject> object;

        RHI::BufferHandle vertexBuffer;
        RHI::BufferHandle indexBuffer;
        uint32_t indexOffset = 0;
        uint32_t indexCount = 0;

        bool isInstanced = false;
        uint32_t instanceCount = 1;
        RHI::BufferHandle instanceBuffer;
        uint32_t instanceBufferStride = 0;

        uint32_t vertexCount = 0;
        uint32_t firstVertex = 0;
        uint32_t firstInstance = 0;
    };

    struct BasePipelineState {
        RHI::PipelineType type = RHI::PipelineType::Graphics;
        RHI::PipelineLayoutHandle layout;
        std::string debugName;

        std::vector<RHI::DynamicState> dynamicStates = { RHI::DynamicState::Viewport, RHI::DynamicState::Scissor };
        virtual ~BasePipelineState() = default;
    };

    struct GraphicsPipelineState : BasePipelineState {
        RHI::ShaderHandle vertexShader;
        RHI::ShaderHandle fragmentShader;

        RHI::RenderPassHandle renderPass;
        uint32_t subpassIndex = -1;

        RHI::VertexInputState vertexInput;

        RHI::CullMode cullMode = RHI::CullMode::Back;
        RHI::FrontFace frontFace = RHI::FrontFace::CounterClockwise;
        float lineWidth = 1.0f;

        bool depthTestEnable = true;
        bool depthWriteEnable = true;
        RHI::CompareOp depthCompareOp = RHI::CompareOp::Less;

        RHI::PrimitiveTopology topology = RHI::PrimitiveTopology::TriangleList;

        std::vector<RHI::Viewport> viewports = { RHI::Viewport{} };
        std::vector<RHI::Rect2D> scissors = { RHI::Rect2D{} };

        std::vector<RHI::BlendAttachmentState> attachments = { RHI::BlendAttachmentState{} };

        bool operator==(const GraphicsPipelineState& other) const {
            return type == other.type &&
                layout == other.layout &&
                debugName == other.debugName &&
                dynamicStates == other.dynamicStates &&
                vertexShader == other.vertexShader &&
                fragmentShader == other.fragmentShader &&
                vertexInput == other.vertexInput &&
                cullMode == other.cullMode &&
                frontFace == other.frontFace &&
                lineWidth == other.lineWidth &&
                depthTestEnable == other.depthTestEnable &&
                depthWriteEnable == other.depthWriteEnable &&
                depthCompareOp == other.depthCompareOp &&
                topology == other.topology &&
                viewports == other.viewports &&
                scissors == other.scissors &&
                attachments == other.attachments;
        }
    };

    // 场景分析产出：渲染器每帧消费
    struct AnalysisSceneResult {
        std::vector<std::shared_ptr<GraphicsPipelineState>> PSO;   // 用于实时创建管线
        std::vector<std::shared_ptr<Assets::MaterialInstance>> materials; // 更新纹理和 uniform
        std::vector<std::shared_ptr<DrawItem>> drawItems;          // 真正的渲染数据
    };

} // namespace StarryEngine

namespace std {
    template<> struct hash<StarryEngine::GraphicsPipelineState> {
        size_t operator()(const StarryEngine::GraphicsPipelineState& state) const {
            size_t seed = 0;
            StarryEngine::Utils::hash_combine(seed, state.type);
            StarryEngine::Utils::hash_combine(seed, state.layout);
            StarryEngine::Utils::hash_combine(seed, state.debugName);
            StarryEngine::Utils::hash_combine(seed, state.dynamicStates);
            StarryEngine::Utils::hash_combine(seed, state.vertexShader);
            StarryEngine::Utils::hash_combine(seed, state.fragmentShader);
            StarryEngine::Utils::hash_combine(seed, state.vertexInput);
            StarryEngine::Utils::hash_combine(seed, state.cullMode);
            StarryEngine::Utils::hash_combine(seed, state.frontFace);
            StarryEngine::Utils::hash_combine(seed, state.lineWidth);
            StarryEngine::Utils::hash_combine(seed, state.depthTestEnable);
            StarryEngine::Utils::hash_combine(seed, state.depthWriteEnable);
            StarryEngine::Utils::hash_combine(seed, state.depthCompareOp);
            StarryEngine::Utils::hash_combine(seed, state.topology);
            StarryEngine::Utils::hash_combine(seed, state.viewports);
            StarryEngine::Utils::hash_combine(seed, state.scissors);
            StarryEngine::Utils::hash_combine(seed, state.attachments);
            return seed;
        }
    };
} // namespace std
