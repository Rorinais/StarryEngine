#pragma once
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include"../assets/Assets.hpp"
#include"camera/OrthographicCamera.hpp"
#include"camera/PerspectiveCamera.hpp"

namespace StarryEngine::Scene {
    struct RenderObject {
        glm::mat4 transform = glm::mat4(1.0f);
        std::shared_ptr<Assets::Geometry> geometry;
        std::vector<std::shared_ptr<Assets::Material>> materials;
    };

    struct DrawItem {
        glm::mat4 transform = glm::mat4(1.0f);
        std::shared_ptr<Assets::Geometry> geometry;
        std::shared_ptr<Assets::Material> material;
        uint32_t indexOffset = 0;
        uint32_t indexCount = 0;
        uint32_t pipelineIndex = 0;
    };

    //struct DrawItem {
    //    glm::mat4 transform = glm::mat4(1.0f);
    //    RHI::BufferHandle vertexBuffer;
    //    RHI::BufferHandle indexBuffer;
    //    std::vector<RHI::DescriptorSetHandle> descriptorSet;
    //    uint32_t indexOffset = 0;
    //    uint32_t indexCount = 0;
    //    uint32_t pipelineIndex = 0;
    //};

    struct BasePipelineState {
        RHI::PipelineType type = RHI::PipelineType::Graphics;
        RHI::PipelineLayoutHandle layout;
        std::string debugName;

        //默认开启动态视口
        std::vector<RHI::DynamicState> dynamicStates = { RHI::DynamicState::Viewport, RHI::DynamicState::Scissor };
        virtual ~BasePipelineState() = default;
    };

    struct GraphicsPipelineState : BasePipelineState {
        // 着色器
        RHI::ShaderHandle vertexShader;
        RHI::ShaderHandle fragmentShader;

        // 渲染通道关联
        RHI::RenderPassHandle renderPass;
        uint32_t subpassIndex = -1;   // -1 表示由 RenderGraph 自动绑定

        // 顶点输入
        RHI::VertexInputState vertexInput;

        // 光栅化
        RHI::CullMode cullMode = RHI::CullMode::Back;
        RHI::FrontFace frontFace = RHI::FrontFace::CounterClockwise;
        float lineWidth = 1.0f;

        // 深度模板
        bool depthTestEnable = true;
        bool depthWriteEnable = true;
        RHI::CompareOp depthCompareOp = RHI::CompareOp::Less;

        // 输入装配
        RHI::PrimitiveTopology topology = RHI::PrimitiveTopology::TriangleList;

        // 视口与裁剪（即使开启动态，创建时仍需默认值）
        std::vector<RHI::Viewport> viewports = { RHI::Viewport{} };   
        std::vector<RHI::Rect2D> scissors = { RHI::Rect2D{} };

        // 颜色混合（每个颜色附件的默认设置）
        std::vector<RHI::BlendAttachmentState> attachments = { RHI::BlendAttachmentState{} };
    };


    struct AnalysisSecneResult {
        std::vector<std::shared_ptr<GraphicsPipelineState>> PSO;
        std::vector<std::shared_ptr<DrawItem>> drawItems;
    };

    class Scene {
    public:
        void addObject(std::shared_ptr<RenderObject> object);
        bool removeObject(std::shared_ptr<RenderObject> object);
        void clear();
        void update(float deltaTime);

        const std::vector<std::shared_ptr<RenderObject>>& getAllObjects() const { return m_allObjects; }
        const std::vector<std::shared_ptr<RenderObject>>& getOpaqueObjects() const { return m_opaqueObjects; }
        const std::vector<std::shared_ptr<RenderObject>>& getTransparentObjects() const { return m_transparentObjects; }

        void addCamera(std::shared_ptr<ICamera> camera) { m_cameras.push_back(camera); }
        const std::vector<std::shared_ptr<ICamera>>& getCameras() const { return m_cameras; }
        const std::shared_ptr<ICamera>& getCamera(uint32_t& index) { return m_cameras[index]; }

        void setActiveCamera(std::shared_ptr<ICamera> camera) { m_activeCamera = camera; }
        std::shared_ptr<ICamera> getActiveCamera() const { return m_activeCamera; }

    private:
        void updateObjectClassification(std::shared_ptr<RenderObject> object);

        std::vector<std::shared_ptr<RenderObject>> m_allObjects;
        std::vector<std::shared_ptr<RenderObject>> m_opaqueObjects;
        std::vector<std::shared_ptr<RenderObject>> m_transparentObjects;

        std::vector<std::shared_ptr<ICamera>> m_cameras;
        std::shared_ptr<ICamera> m_activeCamera;
    };

} // namespace StarryEngine::Scene