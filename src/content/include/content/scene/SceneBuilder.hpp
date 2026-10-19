#pragma once
// L4 内容层 · 创建层：场景创建（只创建，不含业务逻辑）
//
// 默认自带引擎基础件（天空盒 + 网格地面），业务物体/相机通过链式 API 追加。
// 不订阅事件、不写帧循环、不读全局设置 —— 参数由外部显式传入。
#include <scene/Scene.hpp>
#include <assets/loader/IBLBuilder.hpp>
#include <renderer/interface/IRHI.hpp>
#include <glm/glm.hpp>
#include <memory>
#include <string>

namespace StarryEngine::Content {

    /// 创建场景所需的引擎全局资源（来自渲染器）
    struct SceneResources {
        std::shared_ptr<RHI::IRHI> rhi;
        RHI::DescriptorPoolHandle  pool{};
        RHI::DescriptorSetLayoutHandle globalSetLayout{};
        RHI::DescriptorSetHandle   globalSet{};
    };

    struct CameraParams {
        float     fovDeg = 45.0f;
        float     aspect = 16.0f / 9.0f;   ///< 宽高比（窗口尺寸变化时重建相机即可）
        float     nearZ  = 0.1f;
        float     farZ   = 100.0f;
        glm::vec3 eye    {0.0f, 1.2f, 4.0f};
        glm::vec3 target {0.0f, 0.0f, 0.0f};
        glm::vec3 up     {0.0f, 1.0f, 0.0f};
    };

    class SceneBuilder {
    public:
        /// 填充外部已有的场景（渲染器已持有该场景指针）
        SceneBuilder(std::shared_ptr<Scene::Scene> scene, const SceneResources& res);

        /// 天空盒（引擎自带材质）：默认用自己的 HDR；返回自身便于链式
        SceneBuilder& addSkybox(const std::string& hdrPath =
            "assets/textures/pbr/citrus_orchard_road_puresky_1k.hdr");

        /// 网格地面（引擎自带图元 + 材质）
        SceneBuilder& addGrid(float size = 10.0f, uint32_t divisions = 10);

        /// 追加业务物体
        SceneBuilder& addObject(std::shared_ptr<Assets::Geometry> geometry,
                                std::shared_ptr<Assets::MaterialInstance> material,
                                const glm::mat4& transform = glm::mat4(1.0f));

        /// 相机（参数可调，必须在 build() 之前调用，鼠标控制才能绑定）
        SceneBuilder& addCamera(const CameraParams& params = {});

        /// 返回被填充的场景
        std::shared_ptr<Scene::Scene> build();

    private:
        std::shared_ptr<Scene::Scene> m_scene;
        SceneResources m_res;
        std::unique_ptr<Assets::IBLBuilder> m_ibl;
    };

} // namespace StarryEngine::Content
