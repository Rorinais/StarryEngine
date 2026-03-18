#pragma once
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include "../utils/Hash.hpp"

namespace StarryEngine::Scene {
    struct DrawItem {
        glm::mat4 transform = glm::mat4(1.0f);
        RHI::BufferHandle vertexBuffer;
        RHI::BufferHandle indexBuffer;
        std::vector<RHI::DescriptorSetHandle> descriptorSet;
        uint32_t indexOffset = 0;
        uint32_t indexCount = 0;
        uint32_t pipelineIndex = 0;
    };

    struct BasePipelineState {
        RHI::PipelineType type = RHI::PipelineType::Graphics;
        RHI::PipelineLayoutHandle layout;
        std::string debugName;

        //默认开启动态视口
        std::vector<RHI::DynamicState> dynamicStates = { RHI::DynamicState::Viewport, RHI::DynamicState::Scissor };
        virtual ~BasePipelineState() = default;
    };

    struct GraphicsPipelineState : BasePipelineState {
        // 着色器
        RHI::ShaderHandle vertexShader;
        RHI::ShaderHandle fragmentShader;

        // 渲染通道关联
        RHI::RenderPassHandle renderPass;
        uint32_t subpassIndex = -1;   // -1 表示由 RenderGraph 自动绑定

        // 顶点输入
        RHI::VertexInputState vertexInput;

        // 光栅化
        RHI::CullMode cullMode = RHI::CullMode::Back;
        RHI::FrontFace frontFace = RHI::FrontFace::CounterClockwise;
        float lineWidth = 1.0f;

        // 深度模板
        bool depthTestEnable = true;
        bool depthWriteEnable = true;
        RHI::CompareOp depthCompareOp = RHI::CompareOp::Less;

        // 输入装配
        RHI::PrimitiveTopology topology = RHI::PrimitiveTopology::TriangleList;

        // 视口与裁剪（即使开启动态，创建时仍需默认值）
        std::vector<RHI::Viewport> viewports = { RHI::Viewport{} };
        std::vector<RHI::Rect2D> scissors = { RHI::Rect2D{} };

        // 颜色混合（每个颜色附件的默认设置）
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
            // 注意：不比较 renderPass 和 subpassIndex
        }
    };


    struct AnalysisSceneResult {
        std::vector<std::shared_ptr<GraphicsPipelineState>> PSO;
        std::vector<std::shared_ptr<DrawItem>> drawItems;
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