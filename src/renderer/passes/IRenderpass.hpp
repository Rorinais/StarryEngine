#pragma once
#include "../graph/RenderGraph.hpp"
#include "../subpassRecorder/ISubpassRecorder.hpp"
#include "../../assets/geometry/VertexLayout.hpp"

namespace StarryEngine::RenderGraph {
    class IRenderPass {
    public:
        virtual ~IRenderPass() = default;

        virtual void setup(RenderGraph& renderGraph, TextureId input, TextureId output,
            RHI::ImageLayout depthInitial = RHI::ImageLayout::Undefined,
            RHI::ImageLayout depthFinal = RHI::ImageLayout::DepthStencilAttachment,
            RHI::ImageLayout colorInitial = RHI::ImageLayout::Undefined,
            RHI::ImageLayout colorFinal = RHI::ImageLayout::ShaderReadOnly) = 0;

        void setViewport(uint32_t width, uint32_t height) {
            this->width = width;
            this->height = height;
        }
        // 可选：每帧更新
        virtual void updateInputAttachment(TextureId texture, RHI::ImageLayout layout) {}

        virtual void update(const Assets::Uniforms& data){}
    protected:
        uint32_t width = 0, height = 0;
    };
}