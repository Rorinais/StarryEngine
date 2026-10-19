#pragma once
#include <assets/material/MaterialInstance.hpp>
#include <memory>
#include <string>

namespace StarryEngine::Content {

    /**
     * 标准基础材质（上层控制）：天空盒 / 网格地面。
     *
     * 这些不是"材质系统"的一部分，而是内容层用 MaterialInstance API 搭出来的
     * 常用材质样板：参数（贴图、层、subpass）完全由调用方决定，
     * 想改行为直接改这里或自己用 API 重写，不占引擎的材质接口。
     */
    class BuiltinMaterials {
    public:
        /** 天空盒：环境 cubemap（IBL 产物或用户自带）→ 天空盒材质（后处理 subpass） */
        static std::shared_ptr<Assets::MaterialInstance> createSkybox(
            std::shared_ptr<RHI::ResourceManager> resMgr,
            RHI::DescriptorSetLayoutHandle globalSetLayout,
            RHI::DescriptorPoolHandle pool,
            RHI::DescriptorSetHandle globalSet,
            RHI::TextureHandle envCubemap,
            RHI::SamplerHandle cubeSampler);

        /** 网格地面：编辑器/调试用基础件，无贴图，深度写入。 */
        static std::shared_ptr<Assets::MaterialInstance> createGrid(
            std::shared_ptr<RHI::ResourceManager> resMgr,
            RHI::DescriptorSetLayoutHandle globalSetLayout,
            RHI::DescriptorPoolHandle pool,
            RHI::DescriptorSetHandle globalSet);
    };

} // namespace StarryEngine::Content
