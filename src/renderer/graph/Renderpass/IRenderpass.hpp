#pragma once
#include "../RenderGraph.hpp"
#include "../../subpassRecorder/ISubpassRecorder.hpp"
#include "../../VertexLayout.hpp"

namespace StarryEngine::RenderGraph {
    class IRenderPass {
    public:
        virtual ~IRenderPass() = default;

        // 在 RenderGraph 中设置 PassNode（添加附件、子通道、管线）
        virtual void setup(RenderGraph& renderGraph,
            TextureId input,
            TextureId output) = 0;

        void setViewport(uint32_t width, uint32_t height) {
            this->width = width;
            this->height = height;
        }
        // 可选：每帧更新
        virtual void updateInputAttachment(TextureId texture, RHI::ImageLayout layout) {}

        virtual void update(const Uniforms& data){}
    protected:
        uint32_t width = 0, height = 0;
    };
}