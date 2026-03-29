#pragma once
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include "../utils/Hash.hpp"

namespace StarryEngine::Scene {
    struct RenderObject;

    enum class RenderQueue {
        Opaque,
        Transparent ,
        UI,
        Skybox
    };

    enum class RenderStage {
        Shadow,       // 阴影投射
        GBuffer,      // 延迟渲染几何体 Pass
        Lighting,     // 延迟渲染光照 Pass
        Forward,      // 前向渲染 Pass
        PostProcess,
        UI            // UI 渲染
    };

    enum class DrawItemType {
        Mesh,        // 普通网格（可含实例化）
        Procedural   // 无网格，直接绘制顶点（全屏三角形等）
    };

    struct DrawItem {
        DrawItemType type = DrawItemType::Mesh;

        // ---------- 通用字段 ----------
        std::unordered_map<uint32_t, RHI::DescriptorSetHandle> descriptorSets;
        uint32_t pipelineIndex = 0;
        RenderQueue queue = RenderQueue::Opaque;
        RenderStage stage = RenderStage::Forward;

        // 对象引用（用于获取变换矩阵）
        std::weak_ptr<RenderObject> object;

        // ---------- 网格相关（仅当 type == Mesh 时有效）----------
        RHI::BufferHandle vertexBuffer;
        RHI::BufferHandle indexBuffer;
        uint32_t indexOffset = 0;
        uint32_t indexCount = 0;

        bool isInstanced = false;
        uint32_t instanceCount = 1;
        RHI::BufferHandle instanceBuffer;
        uint32_t instanceBufferStride = 0;

        // ---------- 过程式绘制相关（仅当 type == Procedural 时有效）----------
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
        uint32_t subpassIndex = -1;   // -1 表示由 RenderGraph 自动绑定

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
}

namespace std {
    template<> struct hash<StarryEngine::Scene::GraphicsPipelineState> {
        size_t operator()(const StarryEngine::Scene::GraphicsPipelineState& state) const {
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
}