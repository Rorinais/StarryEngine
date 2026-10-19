#include <content/renderpath/StandardPipeline.hpp>
#include <renderer/passes/ShadowPass.hpp>
#include <renderer/passes/MeshPass.hpp>
#include <renderer/passes/ParticlePass.hpp>
#include <renderer/passes/GraphicsPass.hpp>
#include <renderer/passExecutor/SceneDrawExecutor.hpp>
#include <logging/Logger.hpp>
#include <renderer/passes/PassWrapper.hpp>

namespace StarryEngine::Content {

    namespace {
        // ── 标准延迟管线的默认 pass（层 → pass）──
        void addShadow(PassBuildContext& c) {
            c.setSlot(RenderLayers::Background, std::make_shared<ShadowPass>("ShadowPass"));
        }
        void addForward(PassBuildContext& c) {
            auto pass = std::make_shared<MeshPass>("ForwardPass", RHI::Color::Transparent());
            pass->addReadTextureByName("ShadowMap");
            c.setSlot(RenderLayers::Opaque, pass);
        }
        // Surface：一个 render pass，两个 subpass（天空盒 queue0 / 网格 queue1）
        void addSurface(PassBuildContext& c) {
            auto post = std::make_shared<GraphicsPass>("PostProcessPass");
            auto makeSubpass = [&post](int queue) {
                SubpassDesc sp;
                sp.layer = RenderLayers::Surface;
                sp.queue = queue;
                sp.executor = std::make_shared<SceneDrawExecutor>();
                sp.colorAttachments.push_back({ "SceneColor" });
                sp.depthAttachment = { "Depth" };
                post->addSubpass(sp);
            };
            makeSubpass(0);   // 天空盒
            makeSubpass(1);   // 网格地面
            c.setSlot(RenderLayers::Surface, post);
        }
        void addStencil(PassBuildContext& c) {
            auto pass = std::make_shared<GraphicsPass>("StencilPass");
            RenderGraph::AttachmentParams ds;
            ds.clearDepth = 1.0f;
            ds.clearStencil = 0;

            SubpassDesc sw;
            sw.layer = RenderLayers::Outline; sw.queue = 0;
            sw.executor = std::make_shared<SceneDrawExecutor>();
            sw.colorAttachments.push_back({ "SceneColor", RenderGraph::AttachmentParams{} });
            sw.depthAttachment = { "Depth", ds };
            pass->addSubpass(sw);

            SubpassDesc st;
            st.layer = RenderLayers::Outline; st.queue = 1;
            st.executor = std::make_shared<SceneDrawExecutor>();
            st.colorAttachments.push_back({ "SceneColor", RenderGraph::AttachmentParams{} });
            st.depthAttachment = { "Depth", ds };
            pass->addSubpass(st);

            c.setSlot(RenderLayers::Outline, pass);
        }
        void addParticles(PassBuildContext& c) {
            c.setSlot(RenderLayers::Particles, std::make_shared<ParticlePass>("Particles"));
        }
    } // namespace


    void StandardPipeline::declareAttachments(uint32_t width, uint32_t height) {
        auto& textures = RenderTextures::instance();
        textures.define("SceneColor", PassWrapper::createColorTextureDesc({width, height, 1}, RHI::Format::RGBA16_Float));
        textures.define("Depth",      PassWrapper::createDepthTextureDesc({width, height, 1}, RHI::Format::D24_UNorm_S8_UInt));
        textures.define("Swapchain",  PassWrapper::createColorTextureDesc({width, height, 1}, RHI::Format::BGRA8_sRGB));
        textures.define("ShadowMap",  PassWrapper::createDepthTextureDesc({ShadowPass::kShadowMapSize, ShadowPass::kShadowMapSize, 1}, RHI::Format::D24_UNorm_S8_UInt));
    }

    bool StandardPipeline::declare(uint32_t width, uint32_t height) {
        declareAttachments(width, height);   // 附件声明（幂等：外部先 define 的值保留）
        auto& layers = RenderLayers::instance();
        if (layers.passOf(RenderLayers::Background)) {
            LOG_DEBUG("[pipeline] 标准 pass 已声明过，跳过");
            return false;
        }
        layers.setPass(RenderLayers::Background, &addShadow);
        layers.setPass(RenderLayers::Opaque,     &addForward);
        layers.setPass(RenderLayers::Surface,    &addSurface);
        layers.setPass(RenderLayers::Outline,    &addStencil);
        layers.setPass(RenderLayers::Particles,  &addParticles);
        LOG_INFO("[pipeline] 标准 pass 已声明: Background=shadow Opaque=forward Surface=skybox+grid Outline=stencil Particles=particles");
        return true;
    }

} // namespace StarryEngine::Content
