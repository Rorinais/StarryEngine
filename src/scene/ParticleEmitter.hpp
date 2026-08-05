#pragma once
#include <string>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "../assets/Assets.hpp"
#include "ParticleParams.hpp"

namespace StarryEngine::Scene {

    // 粒子渲染方式（Niagara 多渲染器的数据驱动版；避开与引擎 Renderer 类重名）
    // Sprite = 点精灵公告板；Mesh = 网格实例（预留）；以后可加 Ribbon 等
    enum class ParticleRenderMode { Sprite, Mesh };

    // 粒子发射器：模拟（compute 更新 buffer）+ 渲染（通用材质）。
    // 像 RenderObject 一样放进场景，可增删，有自己的局部坐标 transform。
    struct ParticleEmitter {
        std::string name = "ParticleEmitter";
        std::string passTag = "Particles";          // 链接到哪个粒子 pass（同材质 subpassTag 的链接思路）
        glm::mat4 transform = glm::mat4(1.0f);      // 发射器局部坐标 → 世界

        // ── 模拟 ──
        uint32_t particleCount = 1024;
        uint32_t perParticleFloats = 4;
        std::string computeShader = "assets/shaders/test/particle.comp";
        ParticleParams params;                       // 发射器级模拟参数（等价 UE emitter params）

        // ── 渲染（通用材质 + 渲染方式） ──
        ParticleRenderMode renderMode = ParticleRenderMode::Sprite;
        std::shared_ptr<Assets::MaterialInstance> material;   // Sprite/Mesh 的渲染材质
        std::shared_ptr<Assets::Geometry> mesh;               // Mesh 渲染模式用（预留）
    };

} // namespace StarryEngine::Scene
