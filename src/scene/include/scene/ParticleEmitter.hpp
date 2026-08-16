#pragma once
#include <string>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <assets/Assets.hpp>
#include <scene/ParticleParams.hpp>

namespace StarryEngine::Scene {

    enum class ParticleRenderMode { Sprite, Mesh };

    struct ParticleEmitter {
        std::string name = "ParticleEmitter";
        std::string passTag = "Particles";         
        glm::mat4 transform = glm::mat4(1.0f);     

        uint32_t particleCount = 1024;
        uint32_t perParticleFloats = 4;
        std::string computeShader = "assets/shaders/test/particle.comp";
        ParticleParams params;                  

        ParticleRenderMode renderMode = ParticleRenderMode::Sprite;
        std::shared_ptr<Assets::MaterialInstance> material;   
        std::shared_ptr<Assets::Geometry> mesh;             
    };

} // namespace StarryEngine::Scene
