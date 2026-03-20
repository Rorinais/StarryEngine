#include"Renderer.hpp"
#include "../logging/Logger.hpp"

namespace StarryEngine {
    Renderer::Renderer(std::shared_ptr<RHI::IRHI> rhi,
        RHI::DescriptorPoolHandle globalPool,
        std::shared_ptr<Scene::Scene> scene)
        : m_rhi(rhi), m_resMgr(rhi->getResourceManager()),
        m_globalPool(globalPool), m_scene(scene) {
    }

    Renderer::~Renderer() {
        destroy();
    }

    void Renderer::destroy() {
        m_rhi->waitIdle();
        if (m_renderPath){
            m_renderPath.reset();
        }
    }

    void Renderer::renderFrame(RHI::RHICommandEncoder* encoder, uint32_t frameIndex, float deltaTime) {
        // 检查场景版本，必要时重新分析
        uint32_t currentVersion = m_scene->getContentVersion();
        if (!m_analysisSceneResult || currentVersion != m_lastAnalyzedVersion) {
            analysisScene();   // 内部更新 m_analysisSceneResult
            m_lastAnalyzedVersion = currentVersion;
            // 将新的绘制项传递给渲染路径（只需在变化时更新）
            if (m_renderPath && m_analysisSceneResult) {
                m_renderPath->setDrawItems(*m_analysisSceneResult);
            }
        }

        auto camera = m_scene->getActiveCamera();
        if (camera) {
            camera->update();

            Assets::GlobalUniforms globals;
            globals.view = camera->getViewMatrix();
            globals.proj = camera->getProjMatrix();

            auto* buf = m_resMgr->getBuffer(m_globalUniformBuffer);
            if (buf) {
                buf->update(&globals, sizeof(globals), 0);
            }

            if (m_renderPath) {
                m_renderPath->update(globals.view, globals.proj, deltaTime);
            }
        }

        if (m_renderPath) {
            m_renderPath->render(encoder, frameIndex);
        }
    }

    void Renderer::onResize(uint32_t width, uint32_t height) {
        m_rhi->waitIdle();
        if (m_renderPath) {
            m_renderPath->onResize(width, height);
        }
    }

    void Renderer::setRenderPath(std::unique_ptr<IRenderPath> newRenderPath) {
        if (!newRenderPath) return;
        destroy();
        m_renderPath = std::move(newRenderPath);
    }

    void Renderer::createGlobalSetLayout() {
        RHI::DescriptorSetLayoutDesc layoutDesc;
        layoutDesc.bindings = {
            {0, RHI::DescriptorType::UniformBuffer, 1,RHI::ShaderStage::Vertex | RHI::ShaderStage::Fragment}
        };
        m_globalSetLayout = m_resMgr->createDescriptorSetLayout(layoutDesc);
        if (!m_globalSetLayout.isValid()) {
            LOG_ERROR("Failed to create global descriptor set layout");
            return;
        }

        RHI::DescriptorSetDesc setDesc;
        setDesc.descriptorSetLayout = m_globalSetLayout;
        setDesc.descriptorPool = m_globalPool;
        m_globalDescriptorSet = m_resMgr->createDescriptorSet(setDesc, "GlobalSet");
        if (!m_globalDescriptorSet.isValid()) {
            LOG_ERROR("Failed to create global descriptor set");
        }
    }

    void Renderer::createGlobalUniformBuffer() {
        RHI::BufferDesc bufDesc;
        bufDesc.size = sizeof(Assets::GlobalUniforms);
        bufDesc.type = RHI::BufferType::Uniform;
        bufDesc.memoryType = RHI::MemoryType::CPU_To_GPU;
        bufDesc.allowUpdate = true;
        m_globalUniformBuffer = m_resMgr->createBuffer(bufDesc, "GlobalUBO");
        if (!m_globalUniformBuffer.isValid()) {
            LOG_ERROR("Failed to create global uniform buffer");
            return;
        }

        auto* descSet = m_resMgr->getDescriptorSet(m_globalDescriptorSet);
        if (!descSet) {
            LOG_ERROR("Global descriptor set is invalid");
            return;
        }
        descSet->writeBuffer(0, 0, m_resMgr->getBuffer(m_globalUniformBuffer),
            0, sizeof(Assets::GlobalUniforms));
        descSet->update();
    }

    void Renderer::analysisScene() {
        Scene::AnalysisSceneResult result;
        std::unordered_map<Scene::GraphicsPipelineState, uint32_t, std::hash<Scene::GraphicsPipelineState>> pipelineIndexMap;

        // 合并处理 opaque 和 transparent 物体（或者分开存储，但绘制项列表统一）
        auto processObjects = [&](const std::vector<std::shared_ptr<Scene::RenderObject>>& objects) {
            for (auto& obj : objects) {
                auto geometry = obj->geometry;
                if (!geometry) continue;

                auto vb = geometry->getVertexBuffer();
                auto ib = geometry->getIndexBuffer();
                if (!vb.isValid() || !ib.isValid()) continue;

                const auto& submeshes = geometry->getSubmeshes();
                for (size_t i = 0; i < submeshes.size(); ++i) {
                    const auto& submesh = submeshes[i];
                    if (submesh.materialIndex >= obj->materials.size()) continue;

                    auto materialInst = obj->materials[submesh.materialIndex];
                    if (!materialInst) continue;

                    // 1. 构造 PSO（不含 renderPass/subpass，由渲染路径决定）
                    Scene::GraphicsPipelineState pso;
                    pso.vertexInput = geometry->getVertexInputState();
                    pso.topology = geometry->getPrimitiveTopology();
                    pso.vertexShader = materialInst->getTemplate()->getVertexShader();
                    pso.fragmentShader = materialInst->getTemplate()->getFragmentShader();
                    pso.layout = materialInst->getTemplate()->getPipelineLayout(m_resMgr.get());
                    pso.cullMode = materialInst->getCullMode();
                    pso.depthTestEnable = materialInst->isDepthTestEnable();
                    pso.depthWriteEnable = materialInst->isDepthWriteEnable();
                    pso.depthCompareOp = materialInst->getDethCompareOp();
                    pso.attachments = { materialInst->getBlendAttachmentState() };

                    uint32_t pipelineIdx;
                    auto it = pipelineIndexMap.find(pso);
                    if (it == pipelineIndexMap.end()) {
                        pipelineIdx = static_cast<uint32_t>(result.PSO.size());
                        result.PSO.push_back(std::make_shared<Scene::GraphicsPipelineState>(pso));
                        pipelineIndexMap[pso] = pipelineIdx;
                    }
                    else {
                        pipelineIdx = it->second;
                    }

                    // 2. 收集描述符集句柄（set0 全局，set1 材质私有）
                    std::unordered_map<uint32_t, RHI::DescriptorSetHandle> descSets;
                    descSets[0] = m_globalDescriptorSet; // 全局 set0
                    for (const auto& [setIdx, setHandle] : materialInst->getAllSets()) {
                        if (setIdx != 0) descSets[setIdx] = setHandle;
                    }

                    // 3. 填充 DrawItem（不含 transform）
                    Scene::DrawItem item;
                    item.object = obj;                          // 弱指针
                    item.submeshIndex = static_cast<uint32_t>(i);
                    item.vertexBuffer = vb;
                    item.indexBuffer = ib;
                    item.descriptorSets = std::move(descSets);
                    item.indexOffset = submesh.indexOffset;
                    item.indexCount = submesh.indexCount;
                    item.pipelineIndex = pipelineIdx;

                    result.drawItems.push_back(std::make_shared<Scene::DrawItem>(std::move(item)));
                }
            }
            };

        // 处理 opaque 和 transparent 物体（可根据需要合并到一个列表中，或保持分类）
        processObjects(m_scene->getOpaqueObjects());
        processObjects(m_scene->getTransparentObjects());

        // 存储结果
        m_analysisSceneResult = std::make_shared<Scene::AnalysisSceneResult>(std::move(result));
    }

}