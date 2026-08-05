#pragma once
#include <nlohmann/json.hpp>
#include "../src/logging/Logger.hpp"
#include "../src/renderer/Renderer.hpp"
#include "../src/renderer/graph/RenderGraph.hpp"
#include "../src/renderer/interface/RHI_TYPES.hpp"
#include "../src/assets/geometry/GeometryGenerator.hpp"
#include "../src/renderer/passExecutor/SceneDrawExecutor.hpp"


struct ModelData {
    std::shared_ptr<StarryEngine::Assets::Geometry> geometry;
    std::vector<std::shared_ptr<StarryEngine::Assets::MaterialInstance>> materials;
};

struct DataSet {
    std::shared_ptr<StarryEngine::Scene::Scene> scene;
    std::shared_ptr<StarryEngine::Renderer> renderer;
};

struct GlobalDescriptorData {
    StarryEngine::RHI::DescriptorPoolHandle globalDescriptorPool;
    StarryEngine::RHI::DescriptorSetLayoutHandle globalSetLayout;
    StarryEngine::RHI::DescriptorSetHandle globalDescriptorSet;
};