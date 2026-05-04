#include "Renderer.hpp"
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
        // ====== 阶段1：更新实例缓冲（每帧可能变化） ======
        auto updateInstanceBuffers = [&](auto& objects) {
            for (auto& obj : objects) {
                if (obj->isInstanced && obj->instanceBuffer && obj->instanceBuffer->isValid() && !obj->instanceTransforms.empty()) {
                    auto* buf = m_resMgr->getBuffer(*obj->instanceBuffer);
                    if (buf) {
                        buf->update(obj->instanceTransforms.data(), obj->instanceTransforms.size() * sizeof(glm::mat4), 0);
                    }
                }
            }
            };
        updateInstanceBuffers(m_scene->getOpaqueObjects());
        updateInstanceBuffers(m_scene->getTransparentObjects());

        // ====== 阶段2：场景分析 & 渲染资源构建（仅在场景变化时执行） ======
        uint32_t version = m_scene->getContentVersion();
        if (!m_analysisSceneResult || version != m_lastAnalyzedVersion) {
            analysisScene();                                       // 生成 DrawItems + PSO
            m_lastAnalyzedVersion = version;
            if (m_renderPath && m_analysisSceneResult) {
                m_renderPath->rebuildResources(*m_analysisSceneResult); // 分发+管线+纹理绑定
            }
        }

        // ====== 阶段3：每帧动态数据上传 ======
        auto camera = m_scene->getActiveCamera();
        if (camera) {
            camera->update();  // 内部更新 view/proj 矩阵
            Assets::GlobalUniforms globals;
            globals.view = camera->getViewMatrix();
            globals.proj = camera->getProjMatrix();
            globals.invView = glm::inverse(globals.view);
            globals.invProj = glm::inverse(globals.proj);
            globals.time = deltaTime;  // 或改为累积时间，见文末建议

            auto* buf = m_resMgr->getBuffer(m_globalUniformBuffer);
            if (buf) buf->update(&globals, sizeof(globals), 0);
        }

        // 统一提交所有材质的脏 UBO
        if (m_analysisSceneResult) {
            for (auto& mat : m_analysisSceneResult->materials) {
                mat->applyAllDirtyBlocks();
            }
        }

        // ====== 阶段4：执行渲染 ======
        if (m_renderPath) {
            m_renderPath->render(encoder, frameIndex);
        }
    }

    void Renderer::onResize(uint32_t width, uint32_t height) {
        m_rhi->waitIdle();
        if (m_renderPath) {
            m_renderPath->onResize(width, height);
        }

        m_lastAnalyzedVersion = UINT32_MAX;
    }

    void Renderer::setRenderPath(std::shared_ptr<IRenderPath> newRenderPath) {
        if (!newRenderPath) return;
        destroy();
        m_renderPath = std::move(newRenderPath);
    }

    void Renderer::createGlobalSetLayout() {
        RHI::DescriptorSetLayoutDesc layoutDesc;
        layoutDesc.bindings = {
            {0, RHI::DescriptorType::UniformBuffer, 1, RHI::ShaderStage::Vertex | RHI::ShaderStage::Fragment}
        };
        m_globalSetLayout = m_resMgr->createDescriptorSetLayout(layoutDesc);
        if (!m_globalSetLayout.isValid()) {
            LOG_ERROR("Failed to create global descriptor set layout");
            return;
        }

        RHI::DescriptorSetDesc setDesc;
        setDesc.descriptorSetLayout = m_globalSetLayout;
        setDesc.descriptorPool = m_globalPool;
        m_globalDescriptorSet = m_resMgr->createDescriptorSet(setDesc);
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
        m_globalUniformBuffer = m_resMgr->createBuffer(bufDesc);
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

    void Renderer::initDefaultMaterials() {
        if (m_materialsInitialized) return;

        m_defaultMaterial = Assets::MaterialInstance::createDefault(m_resMgr, m_globalSetLayout, m_globalPool, m_globalDescriptorSet);
        if (!m_defaultMaterial) {
            LOG_ERROR("Failed to create default material");
        }

        m_errorMaterial = Assets::MaterialInstance::createError(m_resMgr, m_globalSetLayout, m_globalPool, m_globalDescriptorSet);
        if (!m_errorMaterial) {
            LOG_ERROR("Failed to create error material");
        }

        m_materialsInitialized = true;
    }

    void Renderer::analysisScene() {
        Scene::AnalysisSceneResult result;
        std::unordered_map<Scene::GraphicsPipelineState, uint32_t, std::hash<Scene::GraphicsPipelineState>> pipelineIndexMap;
        std::unordered_set<Assets::MaterialInstance*> uniqueMaterials; 

        // 处理普通物体（不透明 + 透明）
        auto processObjects = [&](const std::vector<std::shared_ptr<Scene::RenderObject>>& objects) {
            for (auto& obj : objects) {
                auto geometry = obj->geometry;
                if (!geometry) continue;

                auto vb = geometry->getVertexBuffer();
                auto ib = geometry->getIndexBuffer();
                if (!vb.isValid() || !ib.isValid()) continue;

                bool instanced = obj->isInstanced && !obj->instanceTransforms.empty();
                if (instanced) {
                    size_t requiredSize = obj->instanceTransforms.size() * sizeof(glm::mat4);
                    if (!obj->instanceBuffer || !obj->instanceBuffer->isValid() ||
                        m_resMgr->getBuffer(*obj->instanceBuffer)->getSize() < requiredSize) {
                        RHI::BufferDesc bufDesc;
                        bufDesc.size = requiredSize;
                        bufDesc.type = RHI::BufferType::Vertex;
                        bufDesc.memoryType = RHI::MemoryType::CPU_To_GPU;
                        bufDesc.allowUpdate = true;
                        auto newHandle = m_resMgr->createBuffer(bufDesc);
                        obj->instanceBuffer = std::make_shared<RHI::BufferHandle>(newHandle);
                    }
                }

                const auto& submeshes = geometry->getSubmeshes();
                for (size_t i = 0; i < submeshes.size(); ++i) {
                    const auto& submesh = submeshes[i];

                    // 获取材质：优先使用模型提供的材质，否则 fallback 到默认材质
                    std::shared_ptr<Assets::MaterialInstance> materialInst;
                    if (submesh.materialIndex < obj->materials.size()) {
                        materialInst = obj->materials[submesh.materialIndex];
                    }

                    // ========== Fallback 逻辑开始 ==========
                    if (!materialInst) {
                        materialInst = m_defaultMaterial;
                        if (materialInst) {
                            LOG_WARN("Missing material for submesh, using default");
                        }
                        else {
                            LOG_ERROR("Default material not available, skipping submesh");
                            continue;
                        }
                    }
                    else {
                        // 检查材质是否有效（shader 是否加载成功）
                        auto tmpl = materialInst->getTemplate();
                        if (!tmpl || !tmpl->getVertexShader().isValid() || !tmpl->getFragmentShader().isValid()) {
                            LOG_ERROR("Invalid material (shader missing), using error material");
                            materialInst = m_errorMaterial;
                            if (!materialInst) {
                                LOG_ERROR("Error material not available, skipping submesh");
                                continue;
                            }
                        }
                    }
                    // ========== Fallback 逻辑结束 ==========

                    if (uniqueMaterials.insert(materialInst.get()).second) {
                        result.materials.push_back(materialInst);
                    }

                    // ---------- 实例化布局处理 ----------
                    bool submeshInstanced = instanced;
                    const Assets::InstancingLayout* instLayout = nullptr;
                    if (submeshInstanced) {
                        instLayout = materialInst->getInstancingLayout();
                        if (!instLayout) {
                            LOG_WARN("Object requires instancing but material has no instancing layout. Falling back to non-instanced.");
                            submeshInstanced = false;
                        }
                    }

                    Scene::GraphicsPipelineState pso;
                    pso.vertexInput = geometry->getVertexInputStateWithInstancing(instLayout);
                    pso.topology = geometry->getPrimitiveTopology();
                    pso.vertexShader = materialInst->getTemplate()->getVertexShader();
                    pso.fragmentShader = materialInst->getTemplate()->getFragmentShader();
                    pso.layout = materialInst->getTemplate()->getPipelineLayout(m_resMgr.get());
                    pso.cullMode = materialInst->getCullMode();
                    pso.depthTestEnable = materialInst->isDepthTestEnable();
                    pso.depthWriteEnable = materialInst->isDepthWriteEnable();
                    pso.depthCompareOp = materialInst->getDethCompareOp();
                    pso.attachments = materialInst->getAttachments();

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

                    for (const auto& [setIdx, layout] : materialInst->getTemplate()->getLayouts()) {
                        if (setIdx != 0) {
                            materialInst->getOrCreateSet(setIdx); 
                        }
                    }
                    std::unordered_map<uint32_t, RHI::DescriptorSetHandle> descSets;
                    descSets[0] = m_globalDescriptorSet;
                    for (const auto& [setIdx, setHandle] : materialInst->getAllSets()) {
                        if (setIdx != 0) descSets[setIdx] = setHandle;
                    }

                    auto item = std::make_shared<Scene::DrawItem>();
                    item->type = Scene::DrawItemType::Mesh;
                    item->object = obj;
                    item->vertexBuffer = vb;
                    item->indexBuffer = ib;
                    item->indexOffset = submesh.indexOffset;
                    item->indexCount = submesh.indexCount;
                    item->descriptorSets = std::move(descSets);
                    item->pipelineIndex = pipelineIdx;
                    item->passTag = materialInst->getSubpassTag();

                    if (submeshInstanced) {
                        item->isInstanced = true;
                        item->instanceCount = static_cast<uint32_t>(obj->instanceTransforms.size());
                        item->instanceBuffer = *obj->instanceBuffer;
                        item->instanceBufferStride = instLayout->stride;
                    }
                    else {
                        item->isInstanced = false;
                        item->instanceCount = 1;
                        item->instanceBuffer = RHI::BufferHandle::Null();
                        item->instanceBufferStride = 0;
                    }

                    result.drawItems.push_back(item);
                }
            }
            };

        processObjects(m_scene->getOpaqueObjects());
        processObjects(m_scene->getTransparentObjects());

        // 处理过程式特效
        for (auto& effect : m_scene->getProceduralEffects()) {
            auto material = effect->material;
            if (!material) continue;

            if (uniqueMaterials.insert(material.get()).second) {
                result.materials.push_back(material);
            }

            Scene::GraphicsPipelineState pso;
            pso.vertexShader = material->getTemplate()->getVertexShader();
            pso.fragmentShader = material->getTemplate()->getFragmentShader();
            pso.layout = material->getTemplate()->getPipelineLayout(m_resMgr.get());
            pso.vertexInput = {};   
            pso.topology = RHI::PrimitiveTopology::TriangleList;
            pso.cullMode = material->getCullMode();
            pso.depthTestEnable = material->isDepthTestEnable();
            pso.depthWriteEnable = material->isDepthWriteEnable();
            pso.depthCompareOp = material->getDethCompareOp();
            pso.attachments = material->getAttachments();

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

            for (const auto& [setIdx, layout] : material->getTemplate()->getLayouts()) {
                if (setIdx != 0) {
                    material->getOrCreateSet(setIdx);
                }
            }
            std::unordered_map<uint32_t, RHI::DescriptorSetHandle> descSets;
            descSets[0] = m_globalDescriptorSet;
            for (const auto& [setIdx, setHandle] : material->getAllSets()) {
                if (setIdx != 0) descSets[setIdx] = setHandle;
            }

            auto item = std::make_shared<Scene::DrawItem>();
            item->type = Scene::DrawItemType::Procedural;
            item->vertexCount = effect->vertexCount;
            item->instanceCount = effect->instanceCount;
            item->firstVertex = 0;
            item->firstInstance = 0;
            item->descriptorSets = std::move(descSets);
            item->pipelineIndex = pipelineIdx;
            item->passTag = material->getSubpassTag();

            result.drawItems.push_back(item);
        }

        m_analysisSceneResult = std::make_shared<Scene::AnalysisSceneResult>(std::move(result));
    }

} // namespace StarryEngine