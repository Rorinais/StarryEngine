#pragma once
// 渲染纹理注册表（与 RenderLayers 同一模式）：名字 → TextureDesc，外部可注册/覆盖。
//
// 用途：pass 工厂或外部代码在渲染路径初始化前声明附件（G-Buffer、MSAA、自定义目标等），
// 路径建图时把它们与自身默认附件合并后创建纹理节点。
#include <renderer/interface/IRHI.hpp>
#include <string>
#include <unordered_map>

namespace StarryEngine {

    class RenderTextures {
    public:
        static RenderTextures& instance();

        /// 注册/覆盖一张纹理描述（overwrite=false 时已存在则告警保留旧值）
        void define(const std::string& name, RHI::TextureDesc desc, bool overwrite = false);
        bool has(const std::string& name) const;
        RHI::TextureDesc get(const std::string& name) const;
        std::unordered_map<std::string, RHI::TextureDesc> all() const;
        void clear();

    private:
        RenderTextures() = default;
        std::unordered_map<std::string, RHI::TextureDesc> m_textures;
    };

} // namespace StarryEngine
