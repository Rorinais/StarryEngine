#pragma once
#include "../assets/Assets.hpp"
#include "../scene/Scene.hpp"
#include <memory>

namespace StarryEngine {

    class SceneAnalyzer {
    public:
        SceneAnalyzer(
            RHI::ResourceManager* resMgr,
            RHI::DescriptorSetHandle globalDescriptorSet,
            std::shared_ptr<Assets::MaterialInstance> defaultMaterial,
            std::shared_ptr<Assets::MaterialInstance> errorMaterial);

        std::shared_ptr<Scene::AnalysisSceneResult> analyze(const Scene::Scene& scene);

    private:
        void processObjects(
            const std::vector<std::shared_ptr<Scene::RenderObject>>& objects,
            Scene::AnalysisSceneResult& result,
            std::unordered_map<Scene::GraphicsPipelineState, uint32_t, std::hash<Scene::GraphicsPipelineState>>& pipelineIndexMap,
            std::unordered_set<Assets::MaterialInstance*>& uniqueMaterials);

        void processProceduralEffects(
            const std::vector<std::shared_ptr<Scene::ProceduralEffect>>& effects,
            Scene::AnalysisSceneResult& result,
            std::unordered_map<Scene::GraphicsPipelineState, uint32_t, std::hash<Scene::GraphicsPipelineState>>& pipelineIndexMap,
            std::unordered_set<Assets::MaterialInstance*>& uniqueMaterials);

        std::shared_ptr<Assets::MaterialInstance> getValidMaterialInstance(
            const std::vector<std::shared_ptr<Assets::MaterialInstance>>& materials,
            uint32_t materialIndex);

        std::shared_ptr<Assets::MaterialInstance> validateMaterialInstance(
            const std::shared_ptr<Assets::MaterialInstance>& material);

        Scene::GraphicsPipelineState buildMeshPSO(
            const std::shared_ptr<Assets::MaterialInstance>& materialInst,
            const std::shared_ptr<Assets::Geometry>& geometry,
            const Assets::InstancingLayout* instLayout);

        Scene::GraphicsPipelineState buildProceduralPSO(
            const std::shared_ptr<Assets::MaterialInstance>& material);

        std::unordered_map<uint32_t, RHI::DescriptorSetHandle> buildDescriptorSetMap(
            const std::shared_ptr<Assets::MaterialInstance>& materialInst);

        std::shared_ptr<Scene::DrawItem> createMeshDrawItem(
            const std::shared_ptr<Scene::RenderObject>& obj,
            const Assets::Submesh& submesh,
            uint32_t pipelineIdx,
            bool instanced,
            const Assets::InstancingLayout* instLayout,
            std::unordered_map<uint32_t, RHI::DescriptorSetHandle> descSets);

        std::shared_ptr<Scene::DrawItem> createProceduralDrawItem(
            const std::shared_ptr<Scene::ProceduralEffect>& effect,
            uint32_t pipelineIdx,
            std::unordered_map<uint32_t, RHI::DescriptorSetHandle> descSets);

        void prepareInstanceBuffer(const std::shared_ptr<Scene::RenderObject>& obj);

        uint32_t getOrAllocatePipelineIndex(
            const Scene::GraphicsPipelineState& pso,
            Scene::AnalysisSceneResult& result,
            std::unordered_map<Scene::GraphicsPipelineState, uint32_t, std::hash<Scene::GraphicsPipelineState>>& pipelineIndexMap);

        RHI::ResourceManager* m_resMgr;
        RHI::DescriptorSetHandle m_globalDescriptorSet;
        std::shared_ptr<Assets::MaterialInstance> m_defaultMaterial;
        std::shared_ptr<Assets::MaterialInstance> m_errorMaterial;
    };

} // namespace StarryEngine