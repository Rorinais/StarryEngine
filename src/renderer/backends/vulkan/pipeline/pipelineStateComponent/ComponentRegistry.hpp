#pragma once
#include "../interface/TypedPipelineComponent.hpp"
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <optional>
#include <unordered_set>

namespace StarryEngine {

    class ComponentRegistry {
    public:
        // 注册组件（使用字符串名称）
        void registerComponent(const std::string& name,
            std::shared_ptr<IPipelineStateComponent> component);

        // 获取组件（通过类型和名称）
        std::shared_ptr<IPipelineStateComponent> getComponent(
            PipelineComponentType type,
            const std::string& name) const;

        // 获取组件名称列表（指定类型）
        std::vector<std::string> getComponentNames(PipelineComponentType type) const;

        // 获取所有已注册的类型
        std::vector<PipelineComponentType> getRegisteredTypes() const;

        // 设置默认组件（当未指定时使用）
        void setDefaultComponent(PipelineComponentType type, const std::string& name);

        // 获取默认组件
        std::shared_ptr<IPipelineStateComponent> getDefaultComponent(
            PipelineComponentType type) const;

        // 获取默认组件名称
        std::optional<std::string> getDefaultComponentName(PipelineComponentType type) const;

        // 检查组件是否存在
        bool hasComponent(PipelineComponentType type, const std::string& name) const;

        // 移除组件
        void removeComponent(PipelineComponentType type, const std::string& name);

        // 清空所有组件
        void clear();

        // 获取组件数量
        size_t getComponentCount() const;

        // 获取指定类型的组件数量
        size_t getComponentCount(PipelineComponentType type) const;

    private:
        // 按类型组织的组件映射
        std::unordered_map<
            PipelineComponentType,
            std::unordered_map<std::string, std::shared_ptr<IPipelineStateComponent>>
        > mComponents;

        // 组件名称集合（便于快速查找）
        std::unordered_map<PipelineComponentType, std::unordered_set<std::string>> mComponentNames;

        // 默认组件映射
        std::unordered_map<PipelineComponentType, std::string> mDefaultComponents;
    };

} // namespace StarryEngine