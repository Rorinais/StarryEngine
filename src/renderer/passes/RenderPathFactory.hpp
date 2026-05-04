#pragma once
#include <string>
#include <memory>
#include <fstream>
#include <nlohmann/json.hpp>
#include "PassWrapper.hpp"
#include "../renderPaths/DeferredRenderPath.hpp"
#include "../subpassRecorder/GbufferRecorder.hpp"
#include "../subpassRecorder/SkyboxRecorder.hpp"
#include "../subpassRecorder/DeferredLightingRecorder.hpp"
using json = nlohmann::json;

namespace StarryEngine {

    class RenderPathFactory {
    public:
        static std::shared_ptr<DeferredRenderPath> createRenderPathFromJSON(
            const std::string& filePath,
            std::shared_ptr<RHI::IRHI> rhi,
            uint32_t width, uint32_t height);

    private:
        static RHI::ImageLayout layoutFromString(const std::string& s);
        static RHI::AttachmentLoadOp loadOpFromString(const std::string& s);
        static RHI::AttachmentStoreOp storeOpFromString(const std::string& s);
        static RHI::Format formatFromString(const std::string& s);
        static std::shared_ptr<ISubpassRecorder> createRecorder(const std::string& name);

        // 从 JSON 对象创建 AttachmentParams
        static RenderGraph::AttachmentParams parseAttachmentParams(const json& att);
    };
}