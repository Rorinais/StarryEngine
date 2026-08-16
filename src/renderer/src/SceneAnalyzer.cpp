#include <renderer/SceneAnalyzer.hpp>
#include <logging/Logger.hpp>

namespace StarryEngine {

    SceneAnalyzer::SceneAnalyzer(
        RHI::ResourceManager* resMgr,
        const std::array<RHI::DescriptorSetHandle, RHI::kMaxFramesInFlight>& globalDescriptorSets,
        std::shared_ptr<Assets::MaterialInstance> defaultMaterial,
        std::shared_ptr<Assets::MaterialInstance> errorMaterial)
        : m_resMgr(resMgr), m_globalDescriptorSets(globalDescriptorSets),
        m_defaultMaterial(defaultMaterial), m_errorMaterial(errorMaterial) {
    }

    std::shared_ptr<AnalysisSceneResult> SceneAnalyzer::analyze(const Scene::Scene& scene) {
        AnalysisSceneResult result;
        std::unordered_map<GraphicsPipelineState, uint32_t, std::hash<GraphicsPipelineState>> pipelineIndexMap;
        std::unordered_set<Assets::MaterialInstance*> uniqueMaterials;

        processObjects(scene.getOpaqueObjects(), result, pipelineIndexMap, uniqueMaterials);
        processObjects(scene.getTransparentObjects(), result, pipelineIndexMap, uniqueMaterials);
        processProceduralEffects(scene.getProceduralEffects(), result, pipelineIndexMap, uniqueMaterials);

        return std::make_shared<AnalysisSceneResult>(std::move(result));
    }

    void SceneAnalyzer::processObjects(
        const std::vector<std::shared_ptr<Scene::RenderObject>>& objects,
        AnalysisSceneResult& result,
        std::unordered_map<GraphicsPipelineState, uint32_t, std::hash<GraphicsPipelineState>>& pipelineIndexMap,
        std::unordered_set<Assets::MaterialInstance*>& uniqueMaterials) {
        for (auto& obj : objects) {
            auto geometry = obj->geometry;
            if (!geometry || !geometry->getVertexBuffer().isValid() || !geometry->getIndexBuffer().isValid())
                continue;

            bool instanced = obj->isInstanced && !obj->instanceTransforms.empty();
            if (instanced) prepareInstanceBuffer(obj); 

            const auto& submeshes = geometry->getSubmeshes();
            for (size_t i = 0; i < submeshes.size(); ++i) {
                auto mat = getValidMaterialInstance(obj->materials, submeshes[i].materialIndex);
                if (!mat) continue;

                if (uniqueMaterials.insert(mat.get()).second)
                    result.materials.push_back(mat);

                const Assets::InstancingLayout* instLayout = nullptr;
                if (instanced) {
                    instLayout = mat->getInstancingLayout();
                    if (!instLayout) {
                        LOG_WARN("Object requires instancing but material has no instancing layout");
                        continue; 
                    }
                }

                GraphicsPipelineState pso = buildMeshPSO(mat, geometry, instLayout);
                uint32_t idx = getOrAllocatePipelineIndex(pso, result, pipelineIndexMap); 
                auto descSets = buildDescriptorSetMap(mat);
                auto item = createMeshDrawItem(obj, submeshes[i], idx, instanced, instLayout, std::move(descSets));
                result.drawItems.push_back(item);
            }
        }
    }

    void SceneAnalyzer::processProceduralEffects(
        const std::vector<std::shared_ptr<Scene::ProceduralEffect>>& effects,
        AnalysisSceneResult& result,
        std::unordered_map<GraphicsPipelineState, uint32_t, std::hash<GraphicsPipelineState>>& pipelineIndexMap,
        std::unordered_set<Assets::MaterialInstance*>& uniqueMaterials) {
        for (auto& effect : effects) {
            auto material = validateMaterialInstance(effect->material);
            if (!material) continue;

            if (uniqueMaterials.insert(material.get()).second) {
                result.materials.push_back(material);
            }

            GraphicsPipelineState pso = buildProceduralPSO(material);
            uint32_t pipelineIdx = getOrAllocatePipelineIndex(pso, result, pipelineIndexMap);
            auto descSets = buildDescriptorSetMap(material);
            auto item = createProceduralDrawItem(effect, pipelineIdx, std::move(descSets));
            result.drawItems.push_back(item);
        }
    }

    std::shared_ptr<Assets::MaterialInstance> SceneAnalyzer::getValidMaterialInstance(const std::vector<std::shared_ptr<Assets::MaterialInstance>>& materials,uint32_t materialIndex){
        std::shared_ptr<Assets::MaterialInstance> inst;
        if (materialIndex < materials.size())
            inst = materials[materialIndex];

        if (!inst) {
            inst = m_defaultMaterial;
            if (inst)
                LOG_WARN("Missing material for submesh, using default");
        }
        if (inst) {
            auto tmpl = inst->getTemplate();
            if (!tmpl || !tmpl->getVertexShader().isValid() || !tmpl->getFragmentShader().isValid()) {
                LOG_ERROR("Invalid material (shader missing), using error material");
                inst = m_errorMaterial;
            }
        }
        return inst;
    }

    std::shared_ptr<Assets::MaterialInstance> SceneAnalyzer::validateMaterialInstance(
        const std::shared_ptr<Assets::MaterialInstance>& material) {
        if (!material) {
            LOG_WARN("ProceduralEffect material is null");
            return nullptr;
        }
        auto tmpl = material->getTemplate();
        if (!tmpl || !tmpl->getVertexShader().isValid() || !tmpl->getFragmentShader().isValid()) {
            LOG_ERROR("ProceduralEffect material has invalid shader, skipping");
            return nullptr;
        }
        return material;
    }

    GraphicsPipelineState SceneAnalyzer::buildMeshPSO(
        const std::shared_ptr<Assets::MaterialInstance>& materialInst,
        const std::shared_ptr<Assets::Geometry>& geometry,
        const Assets::InstancingLayout* instLayout)
    {
        GraphicsPipelineState pso;
        pso.vertexInput = geometry->getVertexInputStateWithInstancing(instLayout);
        pso.topology = geometry->getPrimitiveTopology();
        pso.vertexShader = materialInst->getTemplate()->getVertexShader();
        pso.fragmentShader = materialInst->getTemplate()->getFragmentShader();
        pso.layout = materialInst->getTemplate()->getPipelineLayout(m_resMgr);
        pso.cullMode = materialInst->getCullMode();
        pso.depthTestEnable = materialInst->isDepthTestEnable();
        pso.depthWriteEnable = materialInst->isDepthWriteEnable();
        pso.depthCompareOp = materialInst->getDethCompareOp();
        pso.stencilTestEnable = materialInst->isStencilTestEnable();
        pso.stencilFront = materialInst->getStencilFront();
        pso.stencilBack = materialInst->getStencilBack();
        pso.attachments = materialInst->getAttachments();
        return pso;
    }

    GraphicsPipelineState SceneAnalyzer::buildProceduralPSO(const std::shared_ptr<Assets::MaterialInstance>& material){
        GraphicsPipelineState pso;
        pso.vertexShader = material->getTemplate()->getVertexShader();
        pso.fragmentShader = material->getTemplate()->getFragmentShader();
        pso.layout = material->getTemplate()->getPipelineLayout(m_resMgr);
        pso.vertexInput = {};
        pso.topology = RHI::PrimitiveTopology::TriangleList;
        pso.cullMode = material->getCullMode();
        pso.depthTestEnable = material->isDepthTestEnable();
        pso.depthWriteEnable = material->isDepthWriteEnable();
        pso.depthCompareOp = material->getDethCompareOp();
        pso.stencilTestEnable = material->isStencilTestEnable();
        pso.stencilFront = material->getStencilFront();
        pso.stencilBack = material->getStencilBack();
        pso.attachments = material->getAttachments();
        return pso;
    }

    std::array<std::unordered_map<uint32_t, RHI::DescriptorSetHandle>, RHI::kMaxFramesInFlight> SceneAnalyzer::buildDescriptorSetMap(const std::shared_ptr<Assets::MaterialInstance>& materialInst){
        for (const auto& [setIdx, layout] : materialInst->getTemplate()->getLayouts()) {
            if (setIdx != 0) materialInst->getOrCreateSet(setIdx);
        }
        std::array<std::unordered_map<uint32_t, RHI::DescriptorSetHandle>, RHI::kMaxFramesInFlight> descSets;
        for (uint32_t s = 0; s < RHI::kMaxFramesInFlight; ++s) {
            descSets[s][0] = m_globalDescriptorSets[s];
            for (const auto& [setIdx, layout] : materialInst->getTemplate()->getLayouts()) {
                if (setIdx != 0) {
                    auto h = materialInst->getSet(setIdx, s);
                    if (h.isValid()) descSets[s][setIdx] = h;
                }
            }
        }
        return descSets;
    }

    std::shared_ptr<DrawItem> SceneAnalyzer::createMeshDrawItem(
        const std::shared_ptr<Scene::RenderObject>& obj,
        const Assets::Submesh& submesh,
        uint32_t pipelineIdx,
        bool instanced,
        const Assets::InstancingLayout* instLayout,
        std::array<std::unordered_map<uint32_t, RHI::DescriptorSetHandle>, RHI::kMaxFramesInFlight> descSets)
    {
        auto item = std::make_shared<DrawItem>();
        item->type = DrawItemType::Mesh;
        item->object = obj;
        item->vertexBuffer = obj->geometry->getVertexBuffer();
        item->indexBuffer = obj->geometry->getIndexBuffer();
        item->indexOffset = submesh.indexOffset;
        item->indexCount = submesh.indexCount;
        item->descriptorSets = std::move(descSets);
        item->pipelineIndex = pipelineIdx;
        item->passTag = obj->materials[submesh.materialIndex]->getSubpassTag();

        item->isInstanced = instanced;
        if (instanced) {
            item->instanceCount = static_cast<uint32_t>(obj->instanceTransforms.size());
            item->instanceBuffer = obj->instanceBuffers;
            item->instanceBufferStride = instLayout->stride;
        }
        else {
            item->instanceCount = 1;
            item->instanceBuffer = {};   
            item->instanceBufferStride = 0;
        }
        return item;
    }

    std::shared_ptr<DrawItem> SceneAnalyzer::createProceduralDrawItem(
        const std::shared_ptr<Scene::ProceduralEffect>& effect,
        uint32_t pipelineIdx,
        std::array<std::unordered_map<uint32_t, RHI::DescriptorSetHandle>, RHI::kMaxFramesInFlight> descSets)
    {
        auto item = std::make_shared<DrawItem>();
        item->type = DrawItemType::Procedural;
        item->vertexCount = effect->vertexCount;
        item->instanceCount = effect->instanceCount;
        item->firstVertex = 0;
        item->firstInstance = 0;
        item->descriptorSets = std::move(descSets);
        item->pipelineIndex = pipelineIdx;
        item->passTag = effect->material->getSubpassTag();
        return item;
    }

    void SceneAnalyzer::prepareInstanceBuffer(const std::shared_ptr<Scene::RenderObject>& obj) {
        size_t requiredSize = obj->instanceTransforms.size() * sizeof(glm::mat4);

        for (uint32_t s = 0; s < RHI::kMaxFramesInFlight; ++s) {
            bool needCreate = !obj->instanceBuffers[s].isValid();
            if (!needCreate) {
                auto* buf = m_resMgr->getBuffer(obj->instanceBuffers[s]);
                needCreate = !buf || buf->getSize() < requiredSize;
            }
            if (needCreate) {
                RHI::BufferDesc bufDesc;
                bufDesc.size = requiredSize;
                bufDesc.type = RHI::BufferType::Vertex;
                bufDesc.memoryType = RHI::MemoryType::CPU_To_GPU;
                bufDesc.allowUpdate = true;
                obj->instanceBuffers[s] = m_resMgr->createBuffer(bufDesc);
            }
        }
    }

    uint32_t SceneAnalyzer::getOrAllocatePipelineIndex(
        const GraphicsPipelineState& pso,
        AnalysisSceneResult& result,
        std::unordered_map<GraphicsPipelineState, uint32_t, std::hash<GraphicsPipelineState>>& pipelineIndexMap) {
        auto it = pipelineIndexMap.find(pso);
        if (it != pipelineIndexMap.end())
            return it->second;

        uint32_t idx = static_cast<uint32_t>(result.PSO.size());
        result.PSO.push_back(std::make_shared<GraphicsPipelineState>(pso));
        pipelineIndexMap[pso] = idx;
        return idx;
    }
}