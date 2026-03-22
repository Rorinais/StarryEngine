#pragma once
#include "../logging/Logger.hpp"
#include "../renderer/Renderer.hpp"
#include "../renderer/graph/RenderGraph.hpp"
#include "../renderer/interface/RHI_TYPES.hpp"
#include "../assets/geometry/GeomtryCreateFunc.hpp"
#include "../renderer/subpassRecorder/GbufferRecorder.hpp"

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