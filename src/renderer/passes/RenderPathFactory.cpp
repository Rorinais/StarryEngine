#include "RenderPathFactory.hpp"
#include <stdexcept>

namespace StarryEngine {

    std::shared_ptr<DeferredRenderPath> RenderPathFactory::createRenderPathFromJSON(
        const std::string& filePath,
        std::shared_ptr<RHI::IRHI> rhi,
        uint32_t width, uint32_t height)
    {
        std::ifstream f(filePath);
        if (!f.is_open()) {
            LOG_ERROR("Failed to open render path config file: {}", filePath);
            return nullptr;
        }

        json j;
        try {
            f >> j;
        }
        catch (json::parse_error& e) {
            LOG_ERROR("JSON parse error at byte {}: {}", e.byte, e.what());
            std::string src;
            std::ifstream f2(filePath);
            std::getline(f2, src, '\0');
            LOG_ERROR("Near: `{}`", src.substr(std::max(0, (int)e.byte - 20), 40));
            return nullptr;
        }

        auto renderPath = std::make_shared<DeferredRenderPath>(rhi, width, height);

        try {
            // 1. 纹理描述（与旧版相同）
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

            // 2. 构建新的 RenderPathConfig
            RenderPathConfig config;           // std::vector<PassDesc>
            for (auto& pass : j.at("passes")) {
                PassDesc passDesc;
                passDesc.name = pass.at("name");

                for (auto& sp : pass.at("subpasses")) {
                    SubpassDesc subpassDesc;
                    subpassDesc.tag = sp.at("tag");                 // ★ 从 JSON 读取标签

                    auto recorder = createRecorder(sp.at("recorder"));
                    if (!recorder) {
                        LOG_ERROR("Unknown recorder: {}", sp.at("recorder").get<std::string>());
                        continue;
                    }
                    subpassDesc.recorder = recorder;

                    // 颜色附件
                    for (auto& color : sp.value("colorAttachments", json::array())) {
                        subpassDesc.colorAttachments.push_back({
                            color.at("texture"),
                            parseAttachmentParams(color)
                            });
                    }

                    // 深度附件
                    if (sp.contains("depthAttachment")) {
                        auto& depth = sp.at("depthAttachment");
                        subpassDesc.depthAttachment = SubpassAttachment{
                            depth.at("texture"),
                            parseAttachmentParams(depth)
                        };
                    }

                    // 输入附件
                    for (auto& input : sp.value("inputAttachments", json::array())) {
                        subpassDesc.inputAttachments.push_back({
                            input.at("texture"),
                            parseAttachmentParams(input)
                            });
                    }

                    // 解析附件
                    for (auto& res : sp.value("resolveAttachments", json::array())) {
                        subpassDesc.resolveAttachments.push_back({
                            res.at("texture"),
                            parseAttachmentParams(res)
                            });
                    }

                    // 保留附件
                    for (auto& pres : sp.value("preserveAttachments", json::array())) {
                        // preserve 直接是字符串数组
                        subpassDesc.preserveAttachments.push_back(pres.get<std::string>());
                    }

                    passDesc.subpasses.push_back(std::move(subpassDesc));
                }

                config.push_back(std::move(passDesc));
            }
            renderPath->setConfig(std::move(config));

            if (!renderPath->initialize()) {
                LOG_ERROR("Failed to initialize render path from JSON");
                return nullptr;
            }
        }
        catch (json::out_of_range& e) {
            LOG_ERROR("JSON key not found: {}", e.what());
            return nullptr;
        }
        catch (json::exception& e) {
            LOG_ERROR("JSON exception: {}", e.what());
            return nullptr;
        }
        return renderPath;
    }

    // 从 JSON 附件对象创建 AttachmentParams
    RenderGraph::AttachmentParams RenderPathFactory::parseAttachmentParams(const json& att) {
        RenderGraph::AttachmentParams params;
        if (att.contains("initialLayout"))
            params.initialLayout = layoutFromString(att.at("initialLayout"));
        if (att.contains("finalLayout"))
            params.finalLayout = layoutFromString(att.at("finalLayout"));
        if (att.contains("loadOp"))
            params.loadOp = loadOpFromString(att.at("loadOp"));
        if (att.contains("storeOp"))
            params.storeOp = storeOpFromString(att.at("storeOp"));
        // clearColor / clearDepth / clearStencil 可选
        if (att.contains("clearColor")) {
            auto& cc = att.at("clearColor");
            params.clearColor = RHI::Color{ cc[0], cc[1], cc[2], cc[3] };
        }
        if (att.contains("clearDepth"))
            params.clearDepth = att.at("clearDepth");
        if (att.contains("clearStencil"))
            params.clearStencil = att.at("clearStencil");
        return params;
    }

    RHI::ImageLayout RenderPathFactory::layoutFromString(const std::string& s) {
        if (s == "Undefined")                 return RHI::ImageLayout::Undefined;
        if (s == "ShaderReadOnly")            return RHI::ImageLayout::ShaderReadOnly;
        if (s == "ColorAttachment")           return RHI::ImageLayout::ColorAttachment;
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
        if (s == "Store")    return RHI::AttachmentStoreOp::Store;
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

    std::shared_ptr<ISubpassRecorder> RenderPathFactory::createRecorder(const std::string& name) {
        if (name == "MeshDrawRecorder")           return std::make_shared<MeshDrawRecorder>();
        if (name == "SkyboxRecorder")             return std::make_shared<SkyboxRecorder>();
        if (name == "CopyToSwapchainRecorder")    return std::make_shared<CopyToSwapchainRecorder>();
        return nullptr;
    }

} // namespace StarryEngine