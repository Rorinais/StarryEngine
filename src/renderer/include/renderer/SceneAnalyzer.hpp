#pragma once
#include <assets/Assets.hpp>
#include <scene/Scene.hpp>
#include <renderer/RenderTypes.hpp>
#include <memory>

namespace StarryEngine {

    class SceneAnalyzer {
    public:
        SceneAnalyzer(
            RHI::ResourceManager* resMgr,
            RHI::DescriptorSetHandle globalDescriptorSet,
            std::shared_ptr<Assets::MaterialInstance> defaultMaterial,
            std::shared_ptr<Assets::MaterialInstance> errorMaterial);

        std::shared_ptr<AnalysisSceneResult> analyze(const Scene::Scene& scene);

    private:
        void processObjects(
            const std::vector<std::shared_ptr<Scene::RenderObject>>& objects,
            AnalysisSceneResult& result,
            std::unordered_map<GraphicsPipelineState, uint32_t, std::hash<GraphicsPipelineState>>& pipelineIndexMap,
            std::unordered_set<Assets::MaterialInstance*>& uniqueMaterials);

        void processProceduralEffects(
            const std::vector<std::shared_ptr<Scene::ProceduralEffect>>& effects,
            AnalysisSceneResult& result,
            std::unordered_map<GraphicsPipelineState, uint32_t, std::hash<GraphicsPipelineState>>& pipelineIndexMap,
            std::unordered_set<Assets::MaterialInstance*>& uniqueMaterials);

        std::shared_ptr<Assets::MaterialInstance> getValidMaterialInstance(
            const std::vector<std::shared_ptr<Assets::MaterialInstance>>& materials,
            uint32_t materialIndex);

        std::shared_ptr<Assets::MaterialInstance> validateMaterialInstance(
            const std::shared_ptr<Assets::MaterialInstance>& material);

        GraphicsPipelineState buildMeshPSO(
            const std::shared_ptr<Assets::MaterialInstance>& materialInst,
            const std::shared_ptr<Assets::Geometry>& geometry,
            const Assets::InstancingLayout* instLayout);

        GraphicsPipelineState buildProceduralPSO(
            const std::shared_ptr<Assets::MaterialInstance>& material);

        std::unordered_map<uint32_t, RHI::DescriptorSetHandle> buildDescriptorSetMap(
            const std::shared_ptr<Assets::MaterialInstance>& materialInst);

        std::shared_ptr<DrawItem> createMeshDrawItem(
            const std::shared_ptr<Scene::RenderObject>& obj,
            const Assets::Submesh& submesh,
            uint32_t pipelineIdx,
            bool instanced,
            const Assets::InstancingLayout* instLayout,
            std::unordered_map<uint32_t, RHI::DescriptorSetHandle> descSets);

        std::shared_ptr<DrawItem> createProceduralDrawItem(
            const std::shared_ptr<Scene::ProceduralEffect>& effect,
            uint32_t pipelineIdx,
            std::unordered_map<uint32_t, RHI::DescriptorSetHandle> descSets);

        void prepareInstanceBuffer(const std::shared_ptr<Scene::RenderObject>& obj);

        uint32_t getOrAllocatePipelineIndex(
            const GraphicsPipelineState& pso,
            AnalysisSceneResult& result,
            std::unordered_map<GraphicsPipelineState, uint32_t, std::hash<GraphicsPipelineState>>& pipelineIndexMap);

        RHI::ResourceManager* m_resMgr;
        RHI::DescriptorSetHandle m_globalDescriptorSet;
        std::shared_ptr<Assets::MaterialInstance> m_defaultMaterial;
        std::shared_ptr<Assets::MaterialInstance> m_errorMaterial;
    };

} // namespace StarryEngine