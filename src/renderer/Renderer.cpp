#include"Renderer.hpp"
#include "../logging/Logger.hpp"
#include "../assets/Assets.hpp"

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
        if (m_renderPath) {
            m_renderPath.reset();
        }
    }

    void Renderer::renderFrame(RHI::RHICommandEncoder* encoder, uint32_t frameIndex, float deltaTime) {
        // 更新所有实例化物体的实例缓冲区（每帧数据可能变化）
        auto updateInstanceBuffers = [&](const std::vector<std::shared_ptr<Scene::RenderObject>>& objects) {
            for (auto& obj : objects) {
                if (obj->isInstanced && obj->instanceBuffer && obj->instanceBuffer->isValid() && !obj->instanceTransforms.empty()) {
                    auto* bufferObj = m_resMgr->getBuffer(*obj->instanceBuffer);
                    if (bufferObj) {
                        size_t dataSize = obj->instanceTransforms.size() * sizeof(glm::mat4);
                        bufferObj->update(obj->instanceTransforms.data(), dataSize, 0);
                    }
                    else {
                        LOG_ERROR("Instance buffer handle is valid but no Buffer object found!");
                    }
                }
            }
            };
        updateInstanceBuffers(m_scene->getOpaqueObjects());
        updateInstanceBuffers(m_scene->getTransparentObjects());

        // 场景内容变化时重新分析并更新 DrawItems
        uint32_t currentVersion = m_scene->getContentVersion();
        if (!m_analysisSceneResult || currentVersion != m_lastAnalyzedVersion) {
            analysisScene();
            m_lastAnalyzedVersion = currentVersion;
            if (m_renderPath && m_analysisSceneResult) {
                m_renderPath->setDrawItems(*m_analysisSceneResult);
            }
        }

        // 更新相机和全局 Uniform
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

        // 执行渲染路径
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

    void Renderer::setRenderPath(std::shared_ptr<IRenderPath> newRenderPath) {
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

        // 合并处理 opaque 和 transparent 物体
        auto processObjects = [&](const std::vector<std::shared_ptr<Scene::RenderObject>>& objects) {
            for (auto& obj : objects) {
                auto geometry = obj->geometry;
                if (!geometry) continue;

                auto vb = geometry->getVertexBuffer();
                auto ib = geometry->getIndexBuffer();
                if (!vb.isValid() || !ib.isValid()) continue;

                // ---------- 实例化检查 ----------
                bool instanced = obj->isInstanced && !obj->instanceTransforms.empty();
                if (instanced) {
                    // 确保实例缓冲区已创建且大小足够
                    size_t requiredSize = obj->instanceTransforms.size() * sizeof(glm::mat4);
                    if (!obj->instanceBuffer || !obj->instanceBuffer->isValid() ||
                        m_resMgr->getBuffer(*obj->instanceBuffer)->getSize() < requiredSize) {
                        RHI::BufferDesc bufDesc;
                        bufDesc.size = requiredSize;
                        bufDesc.type = RHI::BufferType::Vertex;
                        bufDesc.memoryType = RHI::MemoryType::CPU_To_GPU;
                        bufDesc.allowUpdate = true;
                        auto newHandle = m_resMgr->createBuffer(bufDesc, "InstanceBuffer");
                        obj->instanceBuffer = std::make_shared<RHI::BufferHandle>(newHandle);
                    }
                }

                const auto& submeshes = geometry->getSubmeshes();
                for (size_t i = 0; i < submeshes.size(); ++i) {
                    const auto& submesh = submeshes[i];
                    if (submesh.materialIndex >= obj->materials.size()) continue;

                    auto materialInst = obj->materials[submesh.materialIndex];
                    if (!materialInst) continue;
                    // 1. 构造 PSO
                    Scene::GraphicsPipelineState pso;
                    pso.vertexInput = geometry->getVertexInputState();  // 基础布局
                    pso.topology = geometry->getPrimitiveTopology();
                    pso.vertexShader = materialInst->getTemplate()->getVertexShader();
                    pso.fragmentShader = materialInst->getTemplate()->getFragmentShader();
                    pso.layout = materialInst->getTemplate()->getPipelineLayout(m_resMgr.get());
                    pso.cullMode = materialInst->getCullMode();
                    pso.depthTestEnable = materialInst->isDepthTestEnable();
                    pso.depthWriteEnable = materialInst->isDepthWriteEnable();
                    pso.depthCompareOp = materialInst->getDethCompareOp();
                    if (materialInst->isDeferred()) {
                        pso.attachments = {
                            RHI::BlendAttachmentState{}, // Albedo
                            RHI::BlendAttachmentState{}, // Normal
                            RHI::BlendAttachmentState{}  // Material
                        };
                    }
                    else {
                        pso.attachments = materialInst->getAttachments();
                    }

                    // **实例化特殊处理：合并实例布局**
                    if (instanced) {
                        // 定义实例布局的 VertexInputState
                        RHI::VertexInputState instanceState;
                        // 绑定 1：实例数据，步长 64 字节，每实例
                        instanceState.bindings.push_back({ 1, 64, RHI::VertexInputRate::PerInstance });
                        // 属性：矩阵的 4 行，每行一个 vec4
                        instanceState.attributes.push_back({ 3, 1, 0 , RHI::Format::RGBA32_Float }); // 行0
                        instanceState.attributes.push_back({ 4, 1, 16, RHI::Format::RGBA32_Float }); // 行1
                        instanceState.attributes.push_back({ 5, 1, 32 ,RHI::Format::RGBA32_Float }); // 行2
                        instanceState.attributes.push_back({ 6, 1, 48 , RHI::Format::RGBA32_Float });// 行3
                        // 合并到 pso.vertexInput
                        pso.vertexInput = mergeVertexInputStates(pso.vertexInput, instanceState);
                    }

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

                    // 2. 收集描述符集
                    std::unordered_map<uint32_t, RHI::DescriptorSetHandle> descSets;
                    descSets[0] = m_globalDescriptorSet;
                    for (const auto& [setIdx, setHandle] : materialInst->getAllSets()) {
                        if (setIdx != 0) descSets[setIdx] = setHandle;
                    }

                    // 3. 填充 DrawItem
                    Scene::DrawItem item;
                    item.object = obj;
                    item.submeshIndex = static_cast<uint32_t>(i);
                    item.vertexBuffer = vb;
                    item.indexBuffer = ib;
                    item.descriptorSets = std::move(descSets);
                    item.indexOffset = submesh.indexOffset;
                    item.indexCount = submesh.indexCount;
                    item.pipelineIndex = pipelineIdx;
                    item.queue = materialInst->getRenderQueue();
                    item.stage = materialInst->getRenderStage();

                    // ---------- 实例化 ----------
                    if (instanced) {
                        item.isInstanced = true;
                        item.instanceCount = static_cast<uint32_t>(obj->instanceTransforms.size());
                        item.instanceBuffer = *obj->instanceBuffer;
                        item.instanceBufferStride = sizeof(glm::mat4);
                    }
                    else {
                        item.isInstanced = false;
                        item.instanceCount = 1;
                        item.instanceBuffer = RHI::BufferHandle::Null();
                        item.instanceBufferStride = 0;
                    }

                    result.drawItems.push_back(std::make_shared<Scene::DrawItem>(std::move(item)));
                }
            }
            };

        // 处理 opaque 和 transparent 物体
        processObjects(m_scene->getOpaqueObjects());
        processObjects(m_scene->getTransparentObjects());

        m_analysisSceneResult = std::make_shared<Scene::AnalysisSceneResult>(std::move(result));
    }

    RHI::VertexInputState Renderer::mergeVertexInputStates(
        const RHI::VertexInputState& base,
        const RHI::VertexInputState& additional)
    {
        RHI::VertexInputState result = base;

        for (const auto& binding : additional.bindings) {
            bool exists = false;
            for (const auto& b : result.bindings) {
                if (b.binding == binding.binding) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                result.bindings.push_back(binding);
            }
        }

        result.attributes.insert(result.attributes.end(),
            additional.attributes.begin(),
            additional.attributes.end());

        return result;
    }
}