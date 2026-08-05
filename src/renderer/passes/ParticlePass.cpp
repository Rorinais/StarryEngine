#include "ParticlePass.hpp"
#include "../passExecutor/ParticleRenderExecutor.hpp"
#include "../../assets/material/DefaultMaterialTemplate.hpp"
#include "../../assets/material/MaterialInstance.hpp"
#include "../../logging/Logger.hpp"
#include <cstring>
#include <vector>

namespace StarryEngine {

    // 默认初始粒子数据（发射器不提供 init 时用）
    static void defaultInitParticle(uint32_t i, float* data) {
        auto rnd = [](uint32_t s) { return float((s * 2654435761u) & 0xFFFF) / 65535.0f; };
        data[0] = (rnd(i * 3 + 1) - 0.5f) * 0.8f;   // x
        data[1] = rnd(i * 3 + 2) * 3.0f - 3.0f;      // y
        data[2] = (rnd(i * 3 + 3) - 0.5f) * 0.8f;    // z
        data[3] = rnd(i * 7 + 4) * 0.95f;             // lifetime
    }

    // 渲染 push constant：model(0..64) + 参数(64..120)，与 particle.vert 一致
    constexpr uint32_t kRenderPushSize = 120;

    // 默认 Sprite 材质（emitter 没给材质时用），懒创建一次
    std::shared_ptr<Assets::MaterialInstance> ParticlePass::ensureDefaultMaterial(const CompileContext& ctx) {
        if (m_defaultMaterial) return m_defaultMaterial;

        auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(ctx.resMgr, ctx.globalSetLayout);
        if (!tmpl->loadShaders("assets/shaders/test/particle.vert", "assets/shaders/test/particle.frag")) {
            LOG_ERROR("[{}] default particle material shaders failed", m_name);
            return nullptr;
        }
        RHI::DescriptorPoolDesc poolDesc;
        poolDesc.maxSets = 8;
        poolDesc.poolSizes = {{RHI::DescriptorType::StorageBuffer, 4}};
        m_defaultPool = ctx.resMgr->createDescriptorPool(poolDesc);

        auto mat = std::make_shared<Assets::MaterialInstance>(
            tmpl, m_defaultPool, ctx.resMgr.get(), ctx.globalDescSet);
        RHI::BlendAttachmentState blend;
        blend.blendEnable = true;   // 默认 SrcAlpha/OneMinusSrcAlpha
        mat->setAttachments({ blend });
        mat->setDepthTest(false);
        mat->setDepthWrite(false);
        m_defaultMaterial = mat;
        return mat;
    }

    void ParticlePass::destroyRenderResources(std::shared_ptr<RHI::ResourceManager> resMgr, EmitterState& st) {
        if (st.pool.isValid())       resMgr->destroy(st.pool);
        if (st.renderLayout.isValid()) resMgr->destroy(st.renderLayout);
        if (st.set1Layout.isValid()) resMgr->destroy(st.set1Layout);
        st.pool = {}; st.renderLayout = {}; st.set1Layout = {}; st.particleDescSet = {};
    }

    bool ParticlePass::configure(
        RenderGraph::RenderGraph& graph,
        std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap,
        uint32_t width, uint32_t height,
        std::shared_ptr<RHI::ResourceManager> resMgr,
        RHI::DescriptorSetLayoutHandle globalSetLayout)
    {
        // 从注入的黑板取场景（render path setScene 时 put<Scene::Scene*>；get<T> 返回 T*，对指针类型需解引用）
        auto* scene = m_data ? *m_data->get<Scene::Scene*>() : nullptr;
        if (!globalSetLayout.isValid() || !scene) return false;

        for (auto& st : m_emitterStates) destroyRenderResources(resMgr, st);
        m_emitterStates.clear();
        m_renderPassNode = nullptr;

        // 收集路由匹配的 emitters：passTag 优先 → 材质 subpassTag → 默认 "Particles"。
        // 无材质也行（onAfterCompile 用 pass 的默认 Sprite 材质）
        std::vector<std::shared_ptr<Scene::ParticleEmitter>> emitters;
        for (auto& em : scene->getParticleEmitters()) {
            if (resolveEmitterTag(*em) != m_passTag) continue;
            emitters.push_back(em);
        }
        if (emitters.empty()) return true;

        // 共享 render pass 节点（SceneColor，本 pass 所有 emitter 共用）
        RenderGraph::AttachmentParams colorParams;
        colorParams.loadOp = RHI::AttachmentLoadOp::Load;
        colorParams.storeOp = RHI::AttachmentStoreOp::Store;
        colorParams.initialLayout = RHI::ImageLayout::ShaderReadOnly;
        colorParams.finalLayout = RHI::ImageLayout::ShaderReadOnly;
        auto scId = graph.getTextureId("SceneColor");
        m_renderPassNode = graph.addGraphicsPassNodeShared(m_name + "_Render", "particle:" + m_passTag);
        m_renderPassNode->setRenderArea(width, height);
        std::string colorKey = m_renderPassNode->addColorOutput(scId, colorParams);

        for (auto& em : emitters) {
            EmitterState st;
            st.emitter = em;

            // 粒子存储 buffer（per-emitter GPU 状态，compute 每帧写）
            std::string bufName = em->name + "_Buffer";
            RHI::BufferDesc bufDesc;
            bufDesc.size = em->particleCount * em->perParticleFloats * sizeof(float);
            bufDesc.type = RHI::BufferType::Storage;
            bufDesc.memoryType = RHI::MemoryType::CPU_To_GPU;
            bufDesc.allowUpdate = true;
            st.bufferId = graph.createVirtualBuffer(bufDesc, bufName);

            // 模拟：委托通用 ComputePass（参数来自发射器）
            ComputePassDesc csDesc;
            csDesc.name = em->name + "_Update";
            csDesc.shader = em->computeShader;
            csDesc.dispatchX = (em->particleCount + 255) / 256;
            csDesc.pushConstantSize = 48;
            csDesc.resources = {
                { RHI::DescriptorType::StorageBuffer, 0, bufName, /*write*/ true,
                  em->particleCount * em->perParticleFloats * sizeof(float) }
            };
            csDesc.fillPushConstants = [em](void* out, float deltaTime) {
                float dt = deltaTime > 0.0f ? deltaTime : 0.016f;
                const auto& p = em->params;
                struct CS_PC { float dt; uint32_t n; float g, smin, smax, life, sxz, sf, sa, ey, td, tt; } pc;
                pc = { dt, em->particleCount, p.gravity, p.speedMin, p.speedMax, p.lifetime,
                       p.spreadXZ, p.swayFreq, p.swayAmp, p.emitterY, p.topDiffuse, p.topThreshold };
                memcpy(out, &pc, sizeof(pc));
            };
            st.computePass = std::make_shared<ComputePass>(csDesc);
            st.computePass->configure(graph, texIdMap, width, height, resMgr, globalSetLayout);

            // 渲染 subpass（共享节点，各自一个）
            m_renderPassNode->addReadBuffer(st.bufferId);
            st.renderSubpassIndex = m_renderPassNode->getSubpassCount();
            std::string subpassTag = em->name + "_Draw";
            auto& subpass = m_renderPassNode->addSubpass(subpassTag);
            subpass.addColorAttachmentRef(colorKey);
            subpass.setTag(subpassTag);
            st.executor = std::make_shared<ParticleRenderExecutor>(
                RHI::PipelineHandle{}, RHI::PipelineLayoutHandle{},
                RHI::DescriptorSetHandle{}, RHI::DescriptorSetHandle{}, em->particleCount);  // 占位，onAfterCompile 替换
            subpass.setExecutor(st.executor);
            st.renderPassNode = m_renderPassNode;

            m_emitterStates.push_back(std::move(st));
        }
        LOG_INFO("[{}] Configured {} emitters -> shared render pass", m_name, m_emitterStates.size());
        return true;
    }

    std::vector<PassSubpassInfo> ParticlePass::getSubpasses() const {
        std::vector<PassSubpassInfo> result;
        for (auto& st : m_emitterStates) {
            result.push_back({st.emitter->name + "_Draw", st.renderSubpassIndex, st.executor, st.renderPassNode});
        }
        return result;
    }

    void ParticlePass::onAfterCompile(const CompileContext& ctx) {
        auto resMgr = ctx.resMgr;
        for (auto& st : m_emitterStates) {
            auto& em = st.emitter;

            // 初始粒子数据
            auto physBuf = ctx.renderGraph->getPhysicalBuffer(st.bufferId);
            auto* bufObj = resMgr->getBuffer(physBuf);
            if (bufObj) {
                uint32_t stride = em->perParticleFloats;
                std::vector<float> init(em->particleCount * stride, 0.0f);
                for (uint32_t i = 0; i < em->particleCount; ++i)
                    defaultInitParticle(i, init.data() + i * stride);
                bufObj->update(init.data(), init.size() * sizeof(float), 0);
            }

            // 模拟管线
            if (st.computePass) st.computePass->onAfterCompile(ctx);

            RHI::RenderPassHandle rp = st.renderPassNode ? st.renderPassNode->getRenderPassHandle() : RHI::RenderPassHandle{};
            if (!rp.isValid()) continue;
            // 无材质 → pass 的默认 Sprite 材质（particle.vert/frag + alpha 混合）
            auto* mat = em->material ? em->material.get() : ensureDefaultMaterial(ctx).get();
            if (!mat) continue;

            // 渲染管线布局：set0 global + set1 storage buffer + push(0,120)
            RHI::DescriptorSetLayoutDesc set1Desc;
            set1Desc.bindings = {{0, RHI::DescriptorType::StorageBuffer, 1, RHI::ShaderStage::Vertex}};
            st.set1Layout = resMgr->createDescriptorSetLayout(set1Desc);
            RHI::PipelineLayoutDesc plDesc;
            plDesc.descriptorSetLayouts = { ctx.globalSetLayout, st.set1Layout };
            plDesc.pushConstants = {{RHI::ShaderStage::Vertex, 0, kRenderPushSize}};
            st.renderLayout = resMgr->createPipelineLayout(plDesc);

            // PSO：材质提供 shader + 混合（通用材质渲染）
            GraphicsPipelineState pso;
            pso.vertexShader = mat->getTemplate()->getVertexShader();
            pso.fragmentShader = mat->getTemplate()->getFragmentShader();
            pso.layout = st.renderLayout;
            pso.cullMode = mat->getCullMode();
            pso.depthTestEnable = false;
            pso.depthWriteEnable = false;
            pso.topology = RHI::PrimitiveTopology::PointList;
            pso.dynamicStates = {RHI::DynamicState::Viewport, RHI::DynamicState::Scissor};
            pso.vertexInput = {};
            pso.attachments = mat->getAttachments();   // 材质的混合状态（可换 additive 等）
            auto pipeline = Assets::PipelineCache::getOrCreateGraphicsPipeline(
                resMgr.get(), pso, rp, st.renderSubpassIndex);
            if (!pipeline.isValid()) { LOG_ERROR("[{}] pipeline failed", em->name); continue; }

            // set1：粒子 buffer
            if (physBuf.isValid()) {
                RHI::DescriptorPoolDesc poolDesc;
                poolDesc.maxSets = 2;
                poolDesc.poolSizes = {{RHI::DescriptorType::UniformBuffer, 1},
                                      {RHI::DescriptorType::StorageBuffer, 1}};
                st.pool = resMgr->createDescriptorPool(poolDesc);
                RHI::DescriptorSetDesc dsDesc;
                dsDesc.descriptorSetLayout = st.set1Layout;
                dsDesc.descriptorPool = st.pool;
                st.particleDescSet = resMgr->createDescriptorSet(dsDesc);
                if (st.particleDescSet.isValid()) {
                    auto* ds = resMgr->getDescriptorSet(st.particleDescSet);
                    ds->writeBuffer(0, 0, bufObj, 0, em->particleCount * em->perParticleFloats * sizeof(float));
                    ds->update();
                }
            }

            auto rec = std::make_shared<ParticleRenderExecutor>(
                pipeline, st.renderLayout, ctx.globalDescSet, st.particleDescSet, em->particleCount);
            rec->setVSParams(em->params);
            rec->setTransform(em->transform);
            st.executor = rec;
            st.renderPassNode->setPassExecutor(st.renderSubpassIndex, rec);

            LOG_INFO("[{}] emitter '{}' ready ({} points)", m_name, em->name, em->particleCount);
        }
    }

} // namespace StarryEngine
