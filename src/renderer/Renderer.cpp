#include"Renderer.hpp"
#include "../logging/Logger.hpp"

namespace StarryEngine {
    Renderer::Renderer(std::shared_ptr<RHI::IRHI> rhi,
        RHI::DescriptorPoolHandle globalPool,
        std::shared_ptr<Scene::Scene> scene)
        : m_rhi(rhi), m_resMgr(rhi->getResourceManager()),m_globalPool(globalPool), m_scene(scene) {

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

            m_renderPath->update(globals.view, globals.proj, deltaTime);
        }

        analysisScene();
        m_renderPath->render(encoder, frameIndex);
    }

    void Renderer::onResize(uint32_t width, uint32_t height) {
        m_rhi->waitIdle();
        m_renderPath->onResize(width, height);
    }

    void Renderer::setRenderPath(std::unique_ptr<IRenderPath> newRenderPath) {
        if (!newRenderPath) return;
        destroy();
        m_renderPath = std::move(newRenderPath);
    }

    void Renderer::createGlobalSetLayout() {
        RHI::DescriptorSetLayoutDesc layoutDesc;
        layoutDesc.bindings = {
            {0, RHI::DescriptorType::UniformBuffer, 1, RHI::ShaderStage::Vertex | RHI::ShaderStage::Fragment},
            //{1, RHI::DescriptorType::CombinedImageSampler, 1, RHI::ShaderStage::Fragment} // 天空盒贴图，暂时不设置
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
        descSet->writeBuffer(0, 0, m_resMgr->getBuffer(m_globalUniformBuffer), 0, sizeof(Assets::GlobalUniforms));
        descSet->update();
    }

    void Renderer::analysisScene() {
        Scene::AnalysisSceneResult result;
        std::unordered_map<Scene::GraphicsPipelineState, uint32_t, std::hash<Scene::GraphicsPipelineState>> pipelineIndexMap;

        for (auto& obj : m_scene->getOpaqueObjects()) {
            auto geometry = obj->geometry;
            if (!geometry) continue;

            auto vb = geometry->getVertexBuffer();
            auto ib = geometry->getIndexBuffer();
            if (!vb.isValid() || !ib.isValid()) continue;

            const auto& submeshes = geometry->getSubmeshes();
            for (size_t i = 0; i < submeshes.size(); ++i) {
                const auto& submesh = submeshes[i];
                if (submesh.materialIndex >= obj->materials.size()) continue;

                auto material = obj->materials[submesh.materialIndex];
                if (!material) continue;

                // 1. 生成 PSO（不含 renderPass/subpass）
                Scene::GraphicsPipelineState pso = material->generatePipelineState(geometry->getVertexInputState());

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

                // 2. 收集描述符集句柄（set 0 全局，set 1 材质私有）
                std::vector<RHI::DescriptorSetHandle> descSets;
                descSets.push_back(m_globalDescriptorSet);
                if (auto privateSet = material->getDescriptorSet(); privateSet.isValid()) {
                    descSets.push_back(privateSet);
                }
                else {
                    LOG_WARN("Material {} has no private descriptor set", submesh.materialIndex);
                }

                // 3. 填充 DrawItem
                Scene::DrawItem item;
                item.transform = obj->transform;
                item.vertexBuffer = vb;
                item.indexBuffer = ib;
                item.descriptorSet = std::move(descSets);
                item.indexOffset = submesh.indexOffset;
                item.indexCount = submesh.indexCount;
                item.pipelineIndex = pipelineIdx;

                result.drawItems.push_back(std::make_shared<Scene::DrawItem>(item));
            }
        }

        // 将结果存储到成员变量，并传递给渲染路径
        m_analysisSceneResult = std::make_shared<Scene::AnalysisSceneResult>(std::move(result));
        if (m_renderPath) {
            m_renderPath->setDrawItems(*m_analysisSceneResult);
        }
    }

}