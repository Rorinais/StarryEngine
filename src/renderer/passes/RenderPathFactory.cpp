#include"RenderPathFactory.hpp"
#include <stdexcept>

namespace StarryEngine {
	
    std::shared_ptr<DeferredRenderPath> RenderPathFactory::createRenderPathFromJSON(
        const std::string& filePath,
        std::shared_ptr<RHI::IRHI> rhi,
        uint32_t width, uint32_t height)
    {
        // 读取文件
        std::ifstream f(filePath);
        if (!f.is_open()) {
            LOG_ERROR("Failed to open render path config file: {}", filePath);
            return nullptr;
        }

        json j;
        try {
            f >> j;   // 直接从文件流解析
        }
        catch (json::parse_error& e) {
            LOG_ERROR("JSON parse error at byte {}: {}", e.byte, e.what());
            // 打印附近内容帮助定位
            std::string src;
            std::ifstream f2(filePath);
            std::getline(f2, src, '\0');
            LOG_ERROR("Near: `{}`", src.substr(std::max(0, (int)e.byte - 20), 40));
            return nullptr;
        }

        auto renderPath = std::make_shared<DeferredRenderPath>(rhi, width, height);

        // 1. 纹理描述
        for (auto& tex : j.at("textures")) {
            std::string name = tex.at("name");
            std::string type = tex.at("type");
            RHI::Format format = formatFromString(tex.at("format"));

            RHI::TextureDesc desc;
            if (type == "color") {
                desc = PassWrapper::createColorTextureDesc({ width, height, 1 }, format);
            }
            else if (type == "depth") {
                desc = PassWrapper::createDepthTextureDesc({ width, height, 1 }, format);
            }
            else {
                LOG_ERROR("Unknown texture type: {}", type);
                continue;
            }
            renderPath->addTextureDesc(name, desc);
        }

        // 2. 子通道
        for (auto& sp : j.at("subpasses")) {
            auto recorder = createRecorder(sp.at("recorder"));
            if (!recorder) {
                LOG_ERROR("Unknown recorder: {}", sp.at("recorder").get<std::string>());
                continue;
            }

            Subpass subpass(sp.at("name"), recorder);

            for (auto& color : sp.value("colorAttachments", json::array())) {
                auto params = PassWrapper::createColorAttachment(
                    layoutFromString(color.at("initialLayout")),
                    layoutFromString(color.at("finalLayout")),
                    loadOpFromString(color.at("loadOp")),
                    storeOpFromString(color.at("storeOp"))
                );
                subpass.addColorAttachment(color.at("texture"), params);
            }

            if (sp.contains("depthAttachment")) {
                auto& depth = sp.at("depthAttachment");
                auto params = PassWrapper::createDepthAttachment(
                    layoutFromString(depth.at("initialLayout")),
                    layoutFromString(depth.at("finalLayout")),
                    loadOpFromString(depth.at("loadOp")),
                    storeOpFromString(depth.at("storeOp"))
                );
                subpass.setDepthAttachment(depth.at("texture"), params);
            }

            for (auto& input : sp.value("inputAttachments", json::array())) {
                auto params = PassWrapper::createColorAttachment(
                    layoutFromString(input.at("initialLayout")),
                    layoutFromString(input.at("finalLayout")),
                    loadOpFromString(input.at("loadOp")),
                    storeOpFromString(input.at("storeOp"))
                );
                subpass.addInputAttachment(input.at("texture"), params);
            }

            Scene::RenderStage stage = stageFromString(sp.at("stage"));
            Scene::RenderQueue queue = queueFromString(sp.at("queue"));
            renderPath->addSubpass(stage, queue, std::move(subpass));
        }

        if (!renderPath->initialize()) {
            LOG_ERROR("Failed to initialize render path from JSON");
            return nullptr;
        }
        return renderPath;
    }

    RHI::ImageLayout RenderPathFactory::layoutFromString(const std::string& s) {
        if (s == "Undefined")                 return RHI::ImageLayout::Undefined;
        if (s == "ShaderReadOnly")            return RHI::ImageLayout::ShaderReadOnly;
        if (s == "DepthStencilAttachment")    return RHI::ImageLayout::DepthStencilAttachment;
        if (s == "PresentSrc")                return RHI::ImageLayout::PresentSrc;
        throw std::runtime_error("Unknown layout: " + s);
    }

    RHI::AttachmentLoadOp RenderPathFactory::loadOpFromString(const std::string& s) {
        if (s == "Clear") return RHI::AttachmentLoadOp::Clear;
        if (s == "Load")  return RHI::AttachmentLoadOp::Load;
        throw std::runtime_error("Unknown load op: " + s);
    }

    RHI::AttachmentStoreOp RenderPathFactory::storeOpFromString(const std::string& s) {
        if (s == "Store")   return RHI::AttachmentStoreOp::Store;
        if (s == "DontCare") return RHI::AttachmentStoreOp::DontCare;
        throw std::runtime_error("Unknown store op: " + s);
    }

    RHI::Format RenderPathFactory::formatFromString(const std::string& s) {
        if (s == "D32_Float")        return RHI::Format::D32_Float;
        if (s == "RGBA8_UNorm")      return RHI::Format::RGBA8_UNorm;
        if (s == "RGBA16_Float")     return RHI::Format::RGBA16_Float;
        if (s == "BGRA8_sRGB")       return RHI::Format::BGRA8_sRGB;
        throw std::runtime_error("Unknown format: " + s);
    }

    Scene::RenderStage RenderPathFactory::stageFromString(const std::string& s) {
        if (s == "Forward")     return Scene::RenderStage::Forward;
        if (s == "PostProcess") return Scene::RenderStage::PostProcess;
        throw std::runtime_error("Unknown stage: " + s);
    }

    Scene::RenderQueue RenderPathFactory::queueFromString(const std::string& s) {
        if (s == "Opaque")      return Scene::RenderQueue::Opaque;
        if (s == "Skybox")      return Scene::RenderQueue::Skybox;
        if (s == "Transparent") return Scene::RenderQueue::Transparent;
        throw std::runtime_error("Unknown queue: " + s);
    }

    std::shared_ptr<ISubpassRecorder> RenderPathFactory::createRecorder(const std::string& name) {
        if (name == "MeshDrawRecorder")        return std::make_shared<MeshDrawRecorder>();
        if (name == "SkyboxRecorder")          return std::make_shared<SkyboxRecorder>();
        if (name == "CopyToSwapchainRecorder") return std::make_shared<CopyToSwapchainRecorder>();
        return nullptr;
    }
}
